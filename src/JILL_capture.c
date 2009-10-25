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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

long crossings = 0;
long nframes_written = 0;

sample_t threshhold = 0.5;

jack_nframes_t calc_jack_latency(jack_client_t *client) {

  jack_nframes_t  latency = 0;

  latency += jack_port_get_latency (jack_port_by_name (client, "system:capture_1"));
  latency += jack_port_get_latency (jack_port_by_name (client, "system:playback_1"));

  return 2*latency;
}


static void signal_handler(int sig) {
	jack_client_close(client);
	JILL_close_soundfile(sf_out);
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);
}

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client follows a simple rule: when the JACK transport is
 * running, copy the input port to the output.  When it stops, exit.
 */

int process (jack_nframes_t nframes, void *arg) {
  sample_t *in;
  int rc;
  int jack_sample_size;
  jack_nframes_t  num_frames_left_to_write, 
    num_frames_to_write, num_frames_able_to_write,
    num_frames_written;
  size_t num_bytes_to_write, num_bytes_written, num_bytes_able_to_write;

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
void
jack_shutdown (void *arg) {
	exit (1);
}

static void
usage () {

  fprintf (stderr, "\n"
"usage: delay \n"
"                --delay OR -d delay (in sec) ]\n"
"                --input_port OR -i <jack port name from which to read signal> \n"
"                --input_port OR -i <jack port name from which to read signal> \n"
);
}

int
main (int argc, char *argv[])
{
  char *client_name;
  jack_options_t jack_options = JackNullOption;
  jack_status_t status;
  int option_index;
  int opt;
  float SR;
  char his_output_port_name[80];
  int rc;

  int jack_sample_size;
  size_t jrb_size_in_bytes, jrb_trigger_size_in_bytes;
  char out_filename[JILL_MAX_FILENAME_LEN];

  size_t num_bytes_available_to_read, num_bytes_to_read, num_bytes_read;
  sf_count_t num_frames_to_write_to_disk, num_frames_written_to_disk;
  sample_t *writer_read_buf, *trigger_read_buf;
  int jack_nframes_per_buf;

  jack_ringbuffer_t *jrb_trigger;
  int trigger_nframes;
  trigger_data_t trigger;
  float trigger_delay_secs = 2.0;
  float trigger_after_secs = 2.0;

  jack_time_t last_on, start_time;

  const char *options = "i:f:";
  struct option long_options[] =
    {
      {"inputPort", 1, 0, 'i'},
      {"filename", 1, 0, 'f'},
      {0, 0, 0, 0}
    };

  his_output_port_name[0] = '\0';
  out_filename[0] = '\0';

  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'i':
      strncpy (his_output_port_name, optarg, 80);
      his_output_port_name[79] = '\0';
      break;
    case 'f':
      strncpy (out_filename, optarg, 80);
      out_filename[79] = '\0';
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

  if (out_filename[0] == '\0') {
    fprintf(stderr, "No output file specified, will use default.\n");
    JILL_get_outfilename(out_filename, "JILL_capture", his_output_port_name);
  } 
  
  printf ("capture input port to try: %s\n", his_output_port_name);
  printf ("output file to try: %s\n", out_filename);

  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(80 * sizeof(char));
  strcpy(client_name, "JILL_capture");
  client = jack_client_open (client_name, jack_options, &status);
  if (client == NULL) {
    fprintf (stderr, "jack_client_open() failed, "
	     "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf (stderr, "Unable to connect to JACK server\n");
    }
    exit (1);
  }
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf (stderr, "unique name `%s' assigned\n", client_name);
  }

  jack_nframes_per_buf = jack_get_buffer_size(client);
  SR = jack_get_sample_rate (client);

  jrb_size_in_bytes = SR*sizeof(sample_t);

  jrb = jack_ringbuffer_create(jrb_size_in_bytes);
  writer_read_buf = (float *) malloc(jrb_size_in_bytes);

  /* just cover delay time */
  trigger_nframes = SR * trigger_delay_secs; 

  printf("tdelay nframes %d\n", trigger_nframes);
  
  jrb_trigger = jack_ringbuffer_create(trigger_nframes * sizeof(sample_t)); 
  jrb_trigger_size_in_bytes = jack_ringbuffer_write_space(jrb_trigger);
  printf("asked for %d, got %d bytes in trigger jrb\n", jrb_trigger_size_in_bytes, trigger_nframes * sizeof(sample_t));
  trigger_read_buf = (sample_t *) malloc(jrb_trigger_size_in_bytes);

  rc = JILL_trigger_create(&trigger, 0.005, 0.03, 5, SR, jack_get_buffer_size(client));
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

  sf_out = JILL_open_soundfile_for_write(out_filename);
  if (sf_out == NULL) {
    printf("could not open file '%s' for writting\n", out_filename);
    jack_client_close(client);
    exit(1);
  }
  
  jack_sample_size = sizeof(sample_t);

  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_mutex_lock(&mutex);
 
  start_time = jack_get_time();
  last_on = start_time;

  while (1) {

    num_bytes_available_to_read = jack_ringbuffer_read_space(jrb);

    //    printf("num bytes available to disk writer %d\n", num_bytes_available_to_read);
    while (num_bytes_available_to_read >= (jack_nframes_per_buf * jack_sample_size)) {

      num_bytes_to_read = jack_nframes_per_buf * jack_sample_size;
      num_bytes_read = jack_ringbuffer_read(jrb, (char *) writer_read_buf, num_bytes_to_read);
      num_bytes_available_to_read -= num_bytes_read;
	
      if (jack_ringbuffer_write_space(jrb_trigger) < num_bytes_read) {
	jack_ringbuffer_read_advance(jrb_trigger, num_bytes_read);
      }

      jack_ringbuffer_write(jrb_trigger, (char *) writer_read_buf, num_bytes_read);

      if (JILL_trigger_calc_new_state(&trigger, writer_read_buf, jack_nframes_per_buf) == 1) {
	printf("ON\n");
	last_on = jack_get_time();
      } else {
	printf("OFF\n");
      }
      
      if (JILL_trigger_get_state(&trigger) == 1 || ((last_on > start_time) && ((jack_get_time() - last_on) < trigger_after_secs * 1000000))) {
	printf("RECORDING\n");
	num_bytes_to_read = jack_ringbuffer_read_space(jrb_trigger);
	printf("num bytes available to read from trigger delay: %d\n", num_bytes_to_read);
	num_bytes_read = jack_ringbuffer_read(jrb_trigger, (char *) trigger_read_buf, num_bytes_to_read); 
	printf("bytes read from trigger delay: %d; frames: %d\n", num_bytes_read, num_bytes_read / jack_sample_size);
	printf("tdnframes %d, jssize %d\n", trigger_nframes, jack_sample_size);
	num_frames_to_write_to_disk = (float) num_bytes_read / jack_sample_size;
	printf("%d num f to wr to disk\n", (int) num_frames_to_write_to_disk);
	num_frames_written_to_disk = JILL_soundfile_write(sf_out, trigger_read_buf, num_frames_to_write_to_disk);
	if(num_frames_written_to_disk != num_frames_to_write_to_disk) {
	  printf("number of frames written to disk is not equal to number requested\n");
	}
	printf("num frames to disk %d\n", (int) num_frames_written_to_disk);
      } 
    }

    pthread_cond_wait(&cond, &mutex);
  }

  pthread_mutex_unlock(&mutex);
  
  jack_client_close (client);
  exit (0);
}

