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
jack_ringbuffer_data_t jrb_write_vec[2], jrb_read_vec[2];
SNDFILE *sf_out;

long nframes_written = 0;

typedef struct {
    jack_default_audio_sample_t *delay;
    int read_phase;
    int write_phase;
}
paDelayData;



void *capture_thread(void *arg) {
  printf("Hello World! It's me, capture_thread!\n");
   pthread_exit(NULL);
}

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
  jack_default_audio_sample_t *in;
  int i;
  int jack_sample_size;
  jack_nframes_t  num_frames_left_to_write, 
    num_frames_to_write, num_frames_able_to_write,
    num_frames_written;
  size_t num_bytes_to_write, num_bytes_written, num_bytes_able_to_write;

  jack_sample_size = sizeof(jack_default_audio_sample_t);

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
    printf("num frames left to write after write %d\n", num_frames_left_to_write);
    nframes_written += num_frames_written; 
  }

  printf("total frames written %ld\n", nframes_written);
  printf("exiting process\n");


/*   while(num_frames_left_to_write > 0) { */
/*     jack_ringbuffer_get_write_vector(jrb, jrb_write_vec); */

/*     num_frames_able_to_write = floor(jrb_write_vec[0].len / jack_sample_size); */

/*     printf("frames able to write %d\n", num_frames_able_to_write); */
/*     if (num_frames_able_to_write <= 0) { */
/*       printf("filled up the buffer, please read it faster.\n"); */
/*     } else { */

/*       /* write to the buffer */ 
/*       num_frames_to_write = num_frames_able_to_write < num_frames_left_to_write ? num_frames_able_to_write : num_frames_left_to_write; */
/*       num_bytes_to_write = num_frames_to_write * jack_sample_size; */

/*       num_bytes_written = jack_ringbuffer_write(jrb, (char*)in, num_bytes_to_write); */

/*       if (num_bytes_written != num_bytes_to_write) { */
/* 	printf("DANGER!!! number of bytes written to ringbuffer not the number requested\n"); */
/*       } */
/*       num_frames_left_to_write -= num_frames_to_write; */
/*       nframes_written += num_frames_to_write; */
/*       //      jack_ringbuffer_write_advance(jrb, num_bytes_written); */
       
/*       if (num_frames_left_to_write > 0) { */
	
/* 	num_frames_able_to_write = floor(jrb_write_vec[1].len / jack_sample_size); */

/* 	if (num_frames_able_to_write > 0) { */

/* 	  /* write to the buffer  */
/* 	  num_frames_to_write = num_frames_able_to_write < num_frames_left_to_write ? num_frames_able_to_write : num_frames_left_to_write; */
/* 	  num_bytes_to_write = num_frames_to_write * jack_sample_size; */

/* 	  num_bytes_written = jack_ringbuffer_write(jrb, (char*) in, num_bytes_to_write); */

/* 	  if (num_bytes_written != num_bytes_to_write) { */
/* 	    printf("DANGER!!! number of bytes written to ringbuffer not the number requested\n"); */
/* 	  } */
/* 	  num_frames_left_to_write -= num_frames_to_write; */
/* 	  nframes_written += num_frames_to_write; */
/* 	  //jack_ringbuffer_write_advance(jrb, num_bytes_written); */
/* 	} */
/*       } */
/*     } */
/*   } */
   
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
"              [ --delay OR -d delay (in sec) ]\n"
"              [ --output_port OR -o jack port to send delayed signal ]\n"
"              [ --input_port OR -i jack port from which to read signal ]\n"
);
}

int
main (int argc, char *argv[])
{
  const char **ports;
  const char *client_name;
  const char *server_name = NULL;
  jack_options_t jack_options = JackNullOption;
  jack_status_t status;
  paDelayData data;
  int option_index;
  int opt;
  int i;
  jack_nframes_t SR;
  double delay; /* in seconds */
  char *his_input_port_name, *his_output_port_name;
  pthread_t capture;
  int rc;

  int jack_sample_size;
  size_t jrb_size_in_bytes;
  char out_filename[JILL_MAX_FILENAME_LEN];
  size_t num_bytes_available_to_read, num_bytes_read;
  sf_count_t num_frames_to_write_to_disk, num_frames_written_to_disk;
  float *read_buf;

  rc = pthread_create(&capture, NULL, capture_thread, NULL);
  if (rc){
    printf("ERROR; return code from pthread_create() is %d\n", rc);
    exit(-1);
  }
  
  const char *options = "i:";
  struct option long_options[] =
    {
      {"inputPort", 1, 0, 'i'},
      {0, 0, 0, 0}
    };
  
  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'i':
      his_output_port_name = (char *) malloc ((strlen (optarg) + 1) * sizeof (char));
      strcpy (his_output_port_name, optarg);
      break;
    default:
      fprintf (stderr, "unknown option %c\n", opt); 
    case 'h':
      usage ();
      return -1;
    }
  }
	  
  printf ("capture input port: %s\n", his_output_port_name);

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

  SR = jack_get_sample_rate (client);

  jrb_size_in_bytes = SR*sizeof(jack_default_audio_sample_t);

  printf("about to create a jack_ringbuffer with size %d\n", jrb_size_in_bytes);
  jrb = jack_ringbuffer_create(jrb_size_in_bytes);
  read_buf = (float *) malloc(jrb_size_in_bytes);

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
  }

    
  /* install a signal handler to properly quits jack client */
#ifdef WIN32
  signal(SIGINT, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGTERM, signal_handler);
#else
  signal(SIGQUIT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);
#endif

  /* keep running until the Ctrl+C */

  
  strcpy(out_filename, "JILL_test.wav"); 
  sf_out = JILL_open_soundfile_for_write(out_filename);
  if (sf_out == NULL) {
    printf("could not open file '%s' for writting\n", out_filename);
    jack_client_close(client);
    exit(1);
  }
  
  jack_sample_size = sizeof(jack_default_audio_sample_t);

  while (1) {

    
    //jack_ringbuffer_get_read_vector(jrb, jrb_read_vec);
    num_bytes_available_to_read = jack_ringbuffer_read_space(jrb);

    while (num_bytes_available_to_read > 0) {
      num_bytes_read = jack_ringbuffer_read(jrb, (char *) read_buf, num_bytes_available_to_read);
      printf ("%d bytes available, %d bytes read from jrb\n", num_bytes_available_to_read, num_bytes_read);
      num_bytes_available_to_read -= num_bytes_read;

      num_frames_to_write_to_disk = num_bytes_read / jack_sample_size;

      printf("jack sample size %d\n", jack_sample_size);
      printf("about to write %d frames to disk\n", num_frames_to_write_to_disk);

      num_frames_written_to_disk = JILL_soundfile_write(sf_out, read_buf, num_frames_to_write_to_disk);
      if(num_frames_written_to_disk != num_frames_to_write_to_disk) {
	printf("number of frames written to disk is not equal to number requested\n");
      }
    }
/* #ifdef WIN32  */
/*     Sleep(1000); */
/* #else */
/*    sleep (0.5);*/ 
/* #endif */
  }
    
  jack_client_close (client);
  exit (0);
}

