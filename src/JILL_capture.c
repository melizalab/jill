#include "JILL.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <signal.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

jack_port_t  *my_input_port, *his_output_port;
jack_client_t *client;
size_t TABLE_SIZE;
jack_ringbuffer_t *jrb;
SNDFILE *sf_out;
struct timeval start_tv;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

long crossings = 0;
long nframes_written = 0;

int finish_up = 0;
long long samples_processed = 0;

sample_t threshhold = 0.5;

void samples_to_timeval(struct timeval *tv, long long samples, int sr) {
  long long start_micro, now_micro;

  start_micro =  (long long) start_tv.tv_sec * 1000000 + start_tv.tv_usec;
  now_micro = start_micro + floor(1000000 * (float)samples / (float)sr);
  
  tv->tv_sec = (time_t)floor(now_micro / 1000000.);
  tv->tv_usec = (suseconds_t) (now_micro - 1000000 * tv->tv_sec);
}
 
float timeval_to_seconds_since_midnight(struct timeval *tv) {
  struct tm *lt;
  float ssm = 0;
  
  lt = localtime(&(tv->tv_sec));

  ssm =lt->tm_sec + 60. * lt->tm_min + 3600 * lt->tm_hour + tv->tv_usec / 1000000.;

  return ssm;
}

double samples_to_seconds_since_epoch(long long samples, int sr) {
  struct timeval tv;

  samples_to_timeval(&tv, samples, sr);
  return (double) tv.tv_sec + tv.tv_usec / 1000000.;
}


jack_nframes_t calc_jack_latency(jack_client_t *client) {

  jack_nframes_t  latency = 0;

  latency += jack_port_get_latency (jack_port_by_name (client, "system:capture_1"));
  latency += jack_port_get_latency (jack_port_by_name (client, "system:playback_1"));

  return 2*latency;
}


static void signal_handler(int sig) {
  int rc;
	jack_deactivate(client);
	finish_up = 1;
	rc = pthread_mutex_trylock(&mutex);
	if (rc == 0) {
	  pthread_cond_signal(&cond);
	  pthread_mutex_unlock(&mutex);
	}


	/*	jack_client_close(client);
	JILL_close_soundfile(sf_out);
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);*/
}

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 */

int process (jack_nframes_t nframes, void *arg) {
  sample_t *in;
  int rc;
  int jack_sample_size;
  jack_nframes_t  num_frames_left_to_write, 
    num_frames_to_write, num_frames_able_to_write,
    num_frames_written;
  size_t num_bytes_to_write, num_bytes_written, num_bytes_able_to_write;

  if (!timerisset(&start_tv)) {
    gettimeofday(&start_tv, NULL);
  }

  jack_sample_size = sizeof(sample_t);

  in = jack_port_get_buffer (my_input_port, nframes);

  num_frames_left_to_write = nframes;

  while (num_frames_left_to_write > 0) {
    num_bytes_able_to_write = jack_ringbuffer_write_space(jrb);
    num_frames_able_to_write = floor(num_bytes_able_to_write / jack_sample_size);
      
    num_frames_to_write = num_frames_able_to_write < num_frames_left_to_write ? num_frames_able_to_write : num_frames_left_to_write; 
    num_bytes_to_write = num_frames_to_write * jack_sample_size;
      
    num_bytes_written = jack_ringbuffer_write(jrb, (char*)in, num_bytes_to_write); 
      
    if (num_bytes_written != num_bytes_to_write) { 
      printf("DANGER!!! number of bytes written to ringbuffer not the number requested\n"); 
    } 
      
    num_frames_written = num_bytes_written / jack_sample_size;
    num_frames_left_to_write -= num_frames_written; 
    nframes_written += num_frames_written; 
    samples_processed += num_frames_written;
  }
  /* signal we have more data to write */
  rc = pthread_mutex_trylock(&mutex);
  if (rc == 0) {
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
  }

  return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg) {
	JILL_close_soundfile(sf_out);
	exit (1);
}

static void usage () {

  fprintf (stderr, "\n"
"usage: JILL_capture \n"
"                --input_port OR -i <jack input port name (as listed by the jack_lsp command)> \n"
"                --name OR -n <string identifier for recording> \n"
"                --prebuf OR -p <record this number of seconds before the trigger opening> \n"
"                --postbuf OR -s <record this number of seconds after the trigger closing> \n"
);
}

int
main (int argc, char *argv[])
{
  char *client_name;
  int option_index;
  int opt;
  float SR;
  char his_output_port_name[JILL_MAX_STRING_LEN];
  int rc;

  
  int jack_sample_size;
  size_t jrb_size_in_bytes, jrb_trigger_size_in_bytes;
  char out_filename[JILL_MAX_STRING_LEN];
  char recording_id[JILL_MAX_STRING_LEN];

  size_t num_bytes_available_to_read, num_bytes_to_read, num_bytes_read;
  sf_count_t num_frames_to_write_to_disk, num_frames_written_to_disk;
  sample_t *writer_read_buf, *trigger_read_buf;
  int jack_nframes_per_buf;

  jack_ringbuffer_t *jrb_trigger;
  int trigger_nframes;
  trigger_data_t trigger;
  float trigger_prebuf_secs = 2.0;
  float trigger_postbuf_secs = 2.0;
  sample_t open_threshold = 0.1, close_threshold = 0.01;
  float open_window = 0.1, close_window = 0.250;
  int ncrossings_open = 1, ncrossings_close = 1000000;
  int last_state, new_state;
  
  struct timeval tv;

  double last_on, start_time;

  const char *options = "i:n:p:s:o:c:w:x:a:b:";
  struct option long_options[] =
    {
      {"inputPort", 1, 0, 'i'},
      {"name", 1, 0, 'n'},
      {"prebuf", 1, 0, 'p'},
      {"postbuf", 1, 0, 's'},
      {"open_threshold", 1, 0, 'o'},
      {"close_threshold", 1, 0, 'c'},
      {"open_window", 1, 0, 'w'},
      {"close_window", 1, 0, 'x'},
      {"ncrossings_open", 1, 0, 'a'},
      {"ncrossings_close", 1, 0, 'b'},
      {0, 0, 0, 0}
    };

  memset(his_output_port_name, '\0', JILL_MAX_STRING_LEN);
  memset(out_filename, '\0', JILL_MAX_STRING_LEN);
  memset(recording_id, '\0', JILL_MAX_STRING_LEN);

  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'i':
      strncpy (his_output_port_name, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'n':
      strncpy (recording_id, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'p':
      trigger_prebuf_secs = atof(optarg);
      break;
    case 's':
      trigger_postbuf_secs = atof(optarg);
      break;
    case 'o':
      open_threshold = atof(optarg);
      break;
    case 'c':
      close_threshold = atof(optarg);
      break;
    case 'w':
      open_window = atof(optarg);
      break;
    case 'x':
      close_window = atof(optarg);
      break;
    case 'a':
      ncrossings_open = atof(optarg);
      break;
    case 'b':
      ncrossings_close = atof(optarg);
      break;
    default:
      fprintf (stderr, "unknown option %c\n", opt); 
    case 'h':
      usage ();
      return -1;
    }
  }
	 
  if (his_output_port_name[0] == '\0') {
    fprintf(stderr, "No input port specified, will use default.\n");
    strcpy(his_output_port_name, "system:capture_1");
  } 

  if (recording_id[0] == '\0') {
    fprintf(stderr, "No output name specified, will use default.\n");
    strcpy(recording_id, "default");
  }
 
  printf ("capture input port to try: %s\n", his_output_port_name);
  printf ("prebuf (secs): %f\n", trigger_prebuf_secs);
  printf ("postbuf (secs): %f\n", trigger_postbuf_secs);


  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(JILL_MAX_STRING_LEN * sizeof(char));
  strcpy(client_name, recording_id);
  strcat(client_name, "_jill_capture");

  client = JILL_connect_server(client_name);

  if (client == NULL) {
    fprintf(stderr, "could not connect to jack server\n");
    exit(1);
  }

  jack_nframes_per_buf = jack_get_buffer_size(client);
  SR = jack_get_sample_rate (client);

  jrb_size_in_bytes = SR*sizeof(sample_t);

  jrb = jack_ringbuffer_create(jrb_size_in_bytes);
  writer_read_buf = (float *) malloc(jrb_size_in_bytes);
  printf("asked for %d, got %d bytes in jrb\n", jrb_size_in_bytes, jack_ringbuffer_write_space(jrb));
  /* just cover delay time */
  trigger_nframes = SR * trigger_prebuf_secs; 

  printf("trigger prebuf nframes %d\n", trigger_nframes);

  jrb_trigger_size_in_bytes = trigger_nframes * sizeof(sample_t);  
  jrb_trigger = jack_ringbuffer_create(jrb_trigger_size_in_bytes); 

  trigger_read_buf = (sample_t *) malloc(jrb_trigger_size_in_bytes);

  //  exit(0);


  rc = JILL_trigger_create(&trigger, open_threshold, close_threshold, open_window, close_window, 
			   ncrossings_open, ncrossings_close, SR, jack_get_buffer_size(client));

  /* tell the JACK server to call `process()' whenever
     there is work to be done.
  */

  jack_set_process_callback (client, process, NULL);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us.
  */

  jack_on_shutdown (client, jack_shutdown, 0);

  my_input_port = jack_port_register (client, "input",
				      JACK_DEFAULT_AUDIO_TYPE,
				      JackPortIsInput, 0);

  if (my_input_port == NULL) {
    fprintf(stderr, "no more JACK ports available\n");
    exit (1);
  }
   
  /* Tell the JACK server that we are ready to roll.  Our
   * process() callback will start running now. */

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }
      
  if (jack_connect (client, his_output_port_name, jack_port_name(my_input_port))) {
    fprintf (stderr, "cannot connect to port '%s'\n", his_output_port_name);
    exit(1);
  }

  signal(SIGQUIT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);

  
  jack_sample_size = sizeof(sample_t);

  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_mutex_lock(&mutex);
 
  start_time = samples_to_seconds_since_epoch(0, SR);
  last_on = start_time;

  printf("Starting at %f\n", start_time);

  while (1) {

    /* check for samples in ringbuffer that process fills */
    num_bytes_available_to_read = jack_ringbuffer_read_space(jrb);

    /* will empty ringbuffer into the trigger prebuf ringbuffer jack_nframes_per_buf at a time */
    while (num_bytes_available_to_read >= (jack_nframes_per_buf * jack_sample_size)) {

      num_bytes_to_read = jack_nframes_per_buf * jack_sample_size;
      num_bytes_read = jack_ringbuffer_read(jrb, (char *) writer_read_buf, num_bytes_to_read);
      num_bytes_available_to_read -= num_bytes_read;
	
      /* restrict prebuf size to delay length (the ringbuffer has more space than we need) */
      if (jack_ringbuffer_read_space(jrb_trigger) + num_bytes_read > jrb_trigger_size_in_bytes) {
	jack_ringbuffer_read_advance(jrb_trigger, num_bytes_read);
      }

      jack_ringbuffer_write(jrb_trigger, (char *) writer_read_buf, num_bytes_read);

      last_state = JILL_trigger_get_state(&trigger);
      new_state = JILL_trigger_calc_new_state(&trigger, writer_read_buf, jack_nframes_per_buf);

      if (last_state != new_state) {

	if (new_state == 1) {

	  samples_to_timeval(&tv, trigger.samples_processed, SR);	  
	  printf("OPEN trigger at %f\n", samples_to_seconds_since_epoch(trigger.samples_processed, SR));

	  if (sf_out == NULL) {


	    JILL_get_outfilename(out_filename, recording_id, his_output_port_name, &tv);
	    printf("opening '%s'\n", out_filename);
	    sf_out = JILL_open_soundfile_for_write(out_filename, SR);
	    if (sf_out == NULL) {
	      printf("could not open file '%s' for writing\n", out_filename);
	      jack_client_close(client);
	      exit(1);
	    }
	  }
	} else {
	  printf("CLOSE trigger at %f\n", samples_to_seconds_since_epoch(trigger.samples_processed, SR));
	}
      }

      if (new_state == 1) {
	last_on = samples_to_seconds_since_epoch(trigger.samples_processed, SR);
      } 
      
      if (JILL_trigger_get_state(&trigger) == 1 || 
	  ( (last_on > start_time) && 
	    ((samples_to_seconds_since_epoch(trigger.samples_processed, SR) - last_on) < trigger_postbuf_secs) )) {

	num_bytes_to_read = jack_ringbuffer_read_space(jrb_trigger);

	num_bytes_read = jack_ringbuffer_read(jrb_trigger, (char *) trigger_read_buf, num_bytes_to_read); 

	num_frames_to_write_to_disk = (float) num_bytes_read / jack_sample_size;

	num_frames_written_to_disk = JILL_soundfile_write(sf_out, trigger_read_buf, num_frames_to_write_to_disk);
	if(num_frames_written_to_disk != num_frames_to_write_to_disk) {
	  printf("number of frames written to disk is not equal to number requested\n");
	}

      } else {
	if (sf_out != NULL) {
	  printf("closing %s at %f\n", out_filename, samples_to_seconds_since_epoch(trigger.samples_processed, SR));
	  JILL_close_soundfile(sf_out);
	  sf_out = NULL;
	}
      }
    }
    if (finish_up == 1) {
      printf("process says %lld trigger says %lld\n", samples_processed, trigger.samples_processed);
      exit(0);
    }
    pthread_cond_wait(&cond, &mutex);    
  }

  pthread_mutex_unlock(&mutex);
  
  jack_client_close (client);
  exit (0);
}

