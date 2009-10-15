/** @file simple_client.c
 *
 * @brief This simple client demonstrates the basic features of JACK
 * as they would be used by many applications.
 */

/* compile with:
 *  gcc -o delay delay.c -ljack -lpthread -lrt       
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <signal.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <jack/jack.h>

jack_port_t *my_output_port, *my_input_port, *his_input_port, *his_output_port;
jack_client_t *client;
size_t TABLE_SIZE;

typedef struct
{
    jack_default_audio_sample_t *delay;
    int read_phase;
    int write_phase;
}
paDelayData;

jack_nframes_t calc_jack_latency(jack_client_t *client) {

  jack_nframes_t  latency = 0;

  latency += jack_port_get_latency (jack_port_by_name (client, "system:capture_1"));
  latency += jack_port_get_latency (jack_port_by_name (client, "system:playback_1"));

  return 2*latency;
}


static void signal_handler(int sig)
{
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
  jack_default_audio_sample_t *out, *in;
  paDelayData *data = (paDelayData*)arg;
  int i;
  jack_default_audio_sample_t test;


  out = jack_port_get_buffer (my_output_port, nframes);
  in = jack_port_get_buffer (my_input_port, nframes);
  
  for( i=0; i<nframes; i++ ) {
    out[i] = data->delay[data->write_phase];


/*     test = data->delay[data->write_phase] * 0.8 + in[i] * 10; */
/*     if (test < 0.8) { */
/*       out[i] = test; */
/*     } else { */
/*       out[i] = 0.8; */
/*     } */
                
    data->delay[data->read_phase] = in[i];
    data->read_phase++;
    data->write_phase++;
    if (data->read_phase >= TABLE_SIZE) {
      data->read_phase -= TABLE_SIZE;
    }
    if (data->write_phase >= TABLE_SIZE) {
      data->write_phase -= TABLE_SIZE;
    }
  }  
  return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
	exit (1);
}

static void
usage ()

{
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
  
  const char *options = "d:i:o:";
  struct option long_options[] =
    {
      {"delay", 1, 0, 'd'},
      {"inputPort", 1, 0, 'i'},
      {"outputPort", 1, 0, 'o'},
      {0, 0, 0, 0}
    };
  
  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'd':
      if ((delay = atof (optarg)) < 0) {
	fprintf (stderr, "invalid delay\n");
	return -1;
      }
      break;
    case 'i':
      his_output_port_name = (char *) malloc (strlen (optarg) * sizeof (char));
      strcpy (his_output_port_name, optarg);
      break;
    case 'o':
      his_input_port_name = (char *) malloc (strlen (optarg) * sizeof (char));
      strcpy (his_input_port_name, optarg);
      break;
    default:
      fprintf (stderr, "unknown option %c\n", opt); 
    case 'h':
      usage ();
      return -1;
    }
  }
	  
  printf ("delay: %f, input port: %s, output port: %s\n", delay, his_output_port_name, his_input_port_name);

  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(9 * sizeof(char));
  strcpy(client_name, "delay");
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
  TABLE_SIZE = ceil(SR * delay);
  TABLE_SIZE -= calc_jack_latency(client);
  printf("%d\n", calc_jack_latency(client));

  printf("sample rate: %d, TABLE_SIZE: %d\n", SR, TABLE_SIZE);
  data.delay = (jack_default_audio_sample_t *) malloc(sizeof(jack_default_audio_sample_t) * TABLE_SIZE); 
  for( i=0; i<TABLE_SIZE; i++ ) {
    data.delay[i] = 0.0;
  }
  data.read_phase = 0;
  data.write_phase = 0;
	  
  /* tell the JACK server to call `process()' whenever
     there is work to be done.
  */

  jack_set_process_callback (client, process, &data);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us.
  */

  jack_on_shutdown (client, jack_shutdown, 0);

  /* create two ports */

  my_output_port = jack_port_register (client, "output",
				       JACK_DEFAULT_AUDIO_TYPE,
				       JackPortIsOutput, 0);

  my_input_port = jack_port_register (client, "input",
				      JACK_DEFAULT_AUDIO_TYPE,
				      JackPortIsInput, 0);

  if ((my_output_port == NULL) || (my_input_port == NULL)) {
    fprintf(stderr, "no more JACK ports available\n");
    exit (1);
  }
   
  /* Tell the JACK server that we are ready to roll.  Our
   * process() callback will start running now. */

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }

  /* Connect the ports.  You can't do this before the client is
   * activated, because we can't make connections to clients
   * that aren't running.  Note the confusing (but necessary)
   * orientation of the driver backend ports: playback ports are
   * "input" to the backend, and capture ports are "output" from
   * it.
   */
 	
  /* 	ports = jack_get_ports (client, NULL, NULL, */
  /* 				JackPortIsPhysical|JackPortIsInput); */
  /* 	if (ports == NULL) { */
  /* 		fprintf(stderr, "no physical playback ports\n"); */
  /* 		exit (1); */
  /* 	} */

  printf ("me: %s, him: %s\n", jack_port_name(my_output_port), his_input_port_name);
  if (jack_connect (client, jack_port_name(my_output_port), his_input_port_name)) {
    fprintf (stderr, "cannot connect to port '%s'\n", his_input_port_name);
  }
  
  /* 	free (ports); */
    
  /* 	ports = jack_get_ports (client, NULL, NULL, */
  /* 				JackPortIsPhysical|JackPortIsOutput); */
  /* 	if (ports == NULL) { */
  /* 		fprintf(stderr, "no physical capture ports\n"); */
  /* 		exit (1); */
  /* 	} */

  if (jack_connect (client, his_output_port_name, jack_port_name(my_input_port))) {
    fprintf (stderr, "cannot connect to port '%s'\n", his_output_port_name);
  }

  /* 	free (ports); */
    
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

  while (1) {
#ifdef WIN32 
    Sleep(1000);
#else
    sleep (1);
#endif
  }
    
  jack_client_close (client);
  exit (0);
}
