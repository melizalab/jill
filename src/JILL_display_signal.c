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
#include "gnuplot_i.h"

jack_port_t  *my_input_port, *his_output_port;
jack_client_t *client;
jack_ringbuffer_t *jrb;

static void signal_handler(int sig) {
  jack_client_close(client);
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
  int jack_sample_size;
  size_t num_bytes_to_write, num_bytes_written, num_bytes_able_to_write;

  jack_sample_size = sizeof(sample_t);

  in = jack_port_get_buffer (my_input_port, nframes);

  num_bytes_to_write = nframes * jack_sample_size;

  /* force enough space on the ring buffer.
   * we don't care about losing the data */
  num_bytes_able_to_write = jack_ringbuffer_write_space(jrb);
  if (num_bytes_able_to_write < num_bytes_to_write) {
    jack_ringbuffer_read_advance(jrb, num_bytes_to_write);
  }

  num_bytes_written = jack_ringbuffer_write(jrb, (char*) in, num_bytes_to_write); 

  /* fix me: should do something smarter */      
  if (num_bytes_written != num_bytes_to_write) { 
    printf("DANGER!!! number of bytes written to ringbuffer not the number requested\n"); 
  } 
 
  return 0;      
}

void jack_shutdown (void *arg) {
  exit (1);
}

static void
usage () {

  fprintf (stderr, "\n"
"usage: JILL_display_signal \n"
"           ( --input_port | -i ) <jack port from which to read signal> \n"
"           [ ( --duration | -d )  <seconds to display> ]\n"
);
}

int main (int argc, char *argv[]) {
  char *client_name;
  jack_options_t jack_options = JackNullOption;
  jack_status_t status;
  int option_index;
  int opt;
  int i;
  float SR;
  char *his_output_port_name;

  size_t jrb_size_in_bytes;
  size_t num_bytes_available_to_read, num_bytes_to_read, num_bytes_read;
  size_t num_frames_read;
  sample_t *read_buf;
  int jack_nframes_per_buf;
  int count;

  float display_secs = 0.01;

  gnuplot_ctrl *gp_handle;
  double *gp_input;

  const char *options = "i:d:";
  struct option long_options[] =
    {
      {"inputPort", 1, 0, 'i'},
      {"duration", 1, 0, 'd'},
      {0, 0, 0, 0}
    };

  his_output_port_name = (char *) calloc (80, sizeof (char));
  
  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'i':
      strcpy (his_output_port_name, optarg);
      break;
    case 'd':
      display_secs = atof(optarg);
      break;
    default:
      fprintf (stderr, "unknown option %c\n", opt); 
    case 'h':
      usage ();
      return -1;
    }
  }

  gp_handle = gnuplot_init();
  if (gp_handle == NULL) {
    fprintf (stderr, "\nproblem with gnuplot, exiting\n");
    exit(1);
  }

  if (display_secs <= 0 || display_secs > 10) {
    fprintf(stderr, "please make duration between 0 and 10 seconds\n");
    exit(1);
  }

  if (strlen(his_output_port_name) == 0) {
    usage();
    exit(1);
  }

  printf("setting display window to %f seconds\n", display_secs);


  printf ("capture input port: %s\n", his_output_port_name);

  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(80 * sizeof(char));
  strcpy(client_name, "JILL_display_signal");
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

  jrb_size_in_bytes = ceil(display_secs * SR * sizeof(sample_t));

  jrb = jack_ringbuffer_create(jrb_size_in_bytes);
  jrb_size_in_bytes = jack_ringbuffer_write_space(jrb);

  read_buf = (float *) calloc(jrb_size_in_bytes / sizeof(sample_t), sizeof(sample_t));

  gp_input = (double *) calloc(jrb_size_in_bytes / sizeof(sample_t), sizeof(double));

  jack_set_process_callback (client, process, NULL);

  jack_on_shutdown (client, jack_shutdown, 0);

  my_input_port = jack_port_register (client, "input",
				      JACK_DEFAULT_AUDIO_TYPE,
				      JackPortIsInput, 0);

  if (my_input_port == NULL) {
    fprintf(stderr, "no more JACK ports available\n");
    exit (1);
  }
   
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

  gnuplot_setstyle(gp_handle, "lines");

  while (1) {

    JILL_wait_for_keystroke();

    num_bytes_available_to_read = jack_ringbuffer_read_space(jrb);

    num_bytes_to_read = num_bytes_available_to_read;
    num_bytes_read = jack_ringbuffer_peek(jrb, (char *) read_buf, num_bytes_to_read);

    num_frames_read = num_bytes_read / sizeof(sample_t);
 
    printf("Wait for it...\n");
        
    count = 0;

    /* downsample */
    for (i = 0; i < num_frames_read; i += 1) {
      gp_input[count++] = (double) read_buf[i];
    }
    
    gnuplot_plot_x(gp_handle, gp_input, count, "signal");
  }

  jack_client_close (client);
  exit (0);
}

