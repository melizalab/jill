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
jack_ringbuffer_t *jrb_process;
SNDFILE *sf_out;
struct timeval tv_start_process;
jack_nframes_t jtime_start;

jack_nframes_t jtime_start_cur_cycle;
struct timeval tv_cur;
double jill_time_cur;
double start_frame_time;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

long crossings = 0;
long nframes_written = 0;

long long samples_processed = 0;

sample_t threshhold = 0.5;

static void signal_handler(int sig) {

  jack_client_close(client);
  JILL_soundfile_close(sf_out);
  fprintf(stderr, "signal received, exiting ...\n");
  exit(0);
}

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 */

int process (jack_nframes_t nframes, void *arg) {
  sample_t *in;
  //  int rc;
  int jack_sample_size;
  jack_nframes_t  num_frames_left_to_write, 
    num_frames_to_write, num_frames_able_to_write,
    num_frames_written;
  size_t num_bytes_to_write, num_bytes_written, num_bytes_able_to_write;
  
  jack_position_t pos;
  int rc;


  if(jack_transport_query(client, &pos) == JackTransportRolling) {


    jtime_start_cur_cycle = pos.frame;
  } else {
    return 0;
  }
  //  jtime_start_cur_cycle = jack_last_frame_time(client);

  jack_sample_size = sizeof(sample_t);

  in = jack_port_get_buffer (my_input_port, nframes);

  num_frames_left_to_write = nframes;
 
  while (num_frames_left_to_write > 0) {
    num_bytes_able_to_write = jack_ringbuffer_write_space(jrb_process);
    num_frames_able_to_write = floor(num_bytes_able_to_write / jack_sample_size);
      
    num_frames_to_write = num_frames_able_to_write < num_frames_left_to_write ? num_frames_able_to_write : num_frames_left_to_write; 
    num_bytes_to_write = num_frames_to_write * jack_sample_size;
      
    num_bytes_written = jack_ringbuffer_write(jrb_process, (char*)in, num_bytes_to_write); 
      
    if (num_bytes_written != num_bytes_to_write) { 
      fprintf(stderr, "DANGER!!! number of bytes written to ringbuffer not the number requested\n"); 
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
  JILL_soundfile_close(sf_out);
  exit (1);
}

static void usage () {

  fprintf (stderr, "\n"
"usage: JILL_capture \n"
"                --name OR -n <string identifier for recording> \n"
"                --logfile OR -l <logfile filename> \n"
"                --input_port OR -i <jack input port name (as listed by the jack_lsp command)> \n"
"                --open_threshold OR -o <value [0-1] for opening crossings> \n"
"                --close_threshold OR -c <value [0-1] for closing crossings> \n"
"                --open_window OR -w <seconds in open trigger window> \n"
"                --close_window OR -x <seconds in close trigger window> \n"
"                --ncrossings_open OR -a <number of threshold crossings in window above which to open> \n"
"                --ncrossings_close OR -b <number of threshold crossings in window below which to close> \n"
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
  size_t jrb_size_in_bytes, jrb_prebuf_size_in_bytes;
  char soundfile_name[JILL_MAX_STRING_LEN];
  char jill_id[JILL_MAX_STRING_LEN];

  size_t num_bytes_available_to_read, num_bytes_to_read, num_bytes_read;
  sf_count_t num_frames_to_write_to_disk, num_frames_written_to_disk;
  sample_t *writer_read_buf, *trigger_read_buf;
  int jack_nframes_per_buf;

  jack_ringbuffer_t *jrb_prebuf;
  int trigger_nframes;
  trigger_data_t trigger;
  float trigger_prebuf_secs = 2.0;
  float trigger_postbuf_secs = 2.0;
  sample_t open_threshold = 0.1, close_threshold = 0.01;
  float open_window = 0.1, close_window = 0.250;
  int ncrossings_open = 1, ncrossings_close = 1000000;
  int trigger_last_state, trigger_new_state;
  
  jack_position_t pos;
  double jill_time_trigger_first_close, jill_time_start;

  int logfile_fd;
  char logfile_name[JILL_MAX_STRING_LEN];
  
  const char *options = "i:n:p:s:o:c:w:x:a:b:l:";
  struct option long_options[] =
    {
      {"input_port", 1, 0, 'i'},
      {"name", 1, 0, 'n'},
      {"prebuf", 1, 0, 'p'},
      {"postbuf", 1, 0, 's'},
      {"open_threshold", 1, 0, 'o'},
      {"close_threshold", 1, 0, 'c'},
      {"open_window", 1, 0, 'w'},
      {"close_window", 1, 0, 'x'},
      {"ncrossings_open", 1, 0, 'a'},
      {"ncrossings_close", 1, 0, 'b'},
      {"logfile", 1, 0, 'l'},
      {0, 0, 0, 0}
    };
  
  memset(&tv_start_process, '\0', sizeof(struct timeval));
  memset(his_output_port_name, '\0', JILL_MAX_STRING_LEN);
  memset(soundfile_name, '\0', JILL_MAX_STRING_LEN);
  memset(jill_id, '\0', JILL_MAX_STRING_LEN);
  memset(logfile_name, '\0', JILL_MAX_STRING_LEN);

  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'i':
      strncpy (his_output_port_name, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'n':
      strncpy (jill_id, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'l':
      strncpy (logfile_name, optarg, JILL_MAX_STRING_LEN-1);
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

  if (logfile_name[0] == '\0') {
    fprintf(stderr, "No logfile specified, will use stdout.\n");
    strcpy(logfile_name, "stdout");
  }

  logfile_fd = JILL_log_open(logfile_name);

  if (his_output_port_name[0] == '\0') {
    fprintf(stderr, "No input port specified, will use default.\n");
    strcpy(his_output_port_name, "system:capture_1");
  } 

  if (jill_id[0] == '\0') {
    fprintf(stderr, "No name specified, will use default.\n");
    strcpy(jill_id, "default_id");
  }
   

  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(JILL_MAX_STRING_LEN * sizeof(char));
  strcpy(client_name, "JILL_capture__");
  strcat(client_name, jill_id);

  client = JILL_connect_server(client_name);

  if (client == NULL) {
    fprintf(stderr, "could not connect to jack server\n");
    exit(1);
  }

  
  jack_nframes_per_buf = jack_get_buffer_size(client);
  SR = jack_get_sample_rate (client);

  jrb_size_in_bytes = SR*sizeof(sample_t);

  jrb_process = jack_ringbuffer_create(jrb_size_in_bytes);
  writer_read_buf = (float *) malloc(jrb_size_in_bytes);

  /* just cover delay time */
  trigger_nframes = SR * trigger_prebuf_secs; 

  jrb_prebuf_size_in_bytes = trigger_nframes * sizeof(sample_t);  
  jrb_prebuf = jack_ringbuffer_create(jrb_prebuf_size_in_bytes); 

  trigger_read_buf = (sample_t *) malloc(jrb_prebuf_size_in_bytes);

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
				      JackPortIsInput|JackPortIsTerminal, 0);

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

  /* wait until process goes through once (to set the date) then wake up and roll */
  pthread_cond_wait(&cond, &mutex); 
  jack_transport_query(client, &pos);

  start_frame_time = pos.frame_time;
  jtime_start = pos.frame;

  
  jill_time_start = start_frame_time;

  time_t  secs = (time_t)floor(jill_time_start);
  printf("%s\n", ctime(&secs)); 
  /* because we check for triggering based on being within a certain time of the 
     first close after being open, initialize the first close to be far in the past */
  jill_time_trigger_first_close = 0.0;


  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u START\n", 
		  jill_id, jill_time_start, jtime_start);

  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER name = %s\n", 
		  jill_id, jill_time_start, jtime_start, jill_id);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER logfile = %s\n", 
		  jill_id, jill_time_start, jtime_start, logfile_name);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER input_port = %s\n", 
		  jill_id, jill_time_start, jtime_start, his_output_port_name);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER open_window = %.4f\n", 
		  jill_id, jill_time_start, jtime_start, open_window);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER close_window = %.4f\n", 
		  jill_id, jill_time_start, jtime_start, close_window);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER open_threshold = %.4f\n", 
		  jill_id, jill_time_start, jtime_start, open_threshold);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER close_threshold = %.4f\n", 
		  jill_id, jill_time_start, jtime_start, close_threshold);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER ncrossings_open = %d\n", 
		  jill_id, jill_time_start, jtime_start, ncrossings_open);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER ncrossings_close = %d\n", 
		  jill_id, jill_time_start, jtime_start, ncrossings_close);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER prebuf = %.4f\n", 
		  jill_id, jill_time_start, jtime_start, trigger_prebuf_secs);
  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u PARAMETER postbuf = %.4f\n", 
		  jill_id, jill_time_start, jtime_start, trigger_postbuf_secs);

  while (1) {

    /* we here for the first time or else we've been woken up
       by the process function */

    /* check for samples in ringbuffer filled by the process function */
    num_bytes_available_to_read = jack_ringbuffer_read_space(jrb_process);

    /* will empty ringbuffer into the trigger prebuf ringbuffer jack_nframes_per_buf at a time */
    while (num_bytes_available_to_read >= (jack_nframes_per_buf * jack_sample_size)) {

      num_bytes_to_read = jack_nframes_per_buf * jack_sample_size;
      num_bytes_read = jack_ringbuffer_read(jrb_process, (char *) writer_read_buf, num_bytes_to_read);
      num_bytes_available_to_read -= num_bytes_read;
	
      /* restrict prebuf ringbuffer data size by discarding old samples
	 (the ringbuffer has more space than we need) */
      if (jack_ringbuffer_read_space(jrb_prebuf) + num_bytes_read > jrb_prebuf_size_in_bytes) {
	jack_ringbuffer_read_advance(jrb_prebuf, num_bytes_read);
      }

      /* put new samples into prebuf ringbuffer */
      jack_ringbuffer_write(jrb_prebuf, (char *) writer_read_buf, num_bytes_read);

      /* calculate a new trigger state based on the new samples */
      trigger_last_state = JILL_trigger_get_state(&trigger);
      trigger_new_state = JILL_trigger_calc_new_state(&trigger, writer_read_buf, jack_nframes_per_buf);

      jack_transport_query(client, &pos);
      jill_time_cur = pos.frame_time;

      /* now begin  deciding what to do with the new state */
      if (trigger_last_state != trigger_new_state) {
	if (trigger_new_state == 1) {
	  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u TRIGGER_OPEN\n", jill_id, jill_time_cur, jtime_start_cur_cycle);

	  if (sf_out == NULL) {

	    JILL_soundfile_get_name(soundfile_name, jill_id, his_output_port_name, jill_time_cur);
	    JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u OUTFILE_OPEN %s\n", jill_id, jill_time_cur, jtime_start_cur_cycle, soundfile_name);

	    sf_out = JILL_soundfile_open_for_write(soundfile_name, SR);
	    if (sf_out == NULL) {
	      fprintf(stderr, "could not open file '%s' for writing\n", soundfile_name);
	      jack_client_close(client);
	      exit(1);
	    }
	  }
	} else { 
	jill_time_trigger_first_close = jill_time_cur; 
	JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u TRIGGER_CLOSE\n", jill_id, jill_time_cur, jtime_start_cur_cycle); 
	}
      }
      
      if ( trigger_new_state == 1 || ( (jill_time_cur - jill_time_trigger_first_close) < trigger_postbuf_secs) ) {

	num_bytes_to_read = jack_ringbuffer_read_space(jrb_prebuf); 
	num_bytes_read = jack_ringbuffer_read(jrb_prebuf, (char *) trigger_read_buf, num_bytes_to_read); 	num_frames_to_write_to_disk = (float) num_bytes_read / jack_sample_size;
	num_frames_written_to_disk = JILL_soundfile_write(sf_out, trigger_read_buf, num_frames_to_write_to_disk);
	if(num_frames_written_to_disk != num_frames_to_write_to_disk) {
	  fprintf(stderr, "number of frames written to disk is not equal to number requested\n");
	}

      } else {
	if (sf_out != NULL) {
	  JILL_log_writef(logfile_fd, "[JILL_capture:%s] %f %u OUTFILE_CLOSE %s\n", jill_id, jill_time_cur, jtime_start_cur_cycle, soundfile_name);

	  JILL_soundfile_close(sf_out);
	  sf_out = NULL;
	}
      }
    }

    pthread_cond_wait(&cond, &mutex);    

  }

  pthread_mutex_unlock(&mutex);
  
  jack_client_close (client);
  exit (0);
}

