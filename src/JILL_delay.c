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

#include "JILL.h"

jack_port_t *my_output_port, *my_input_port, *his_input_port, *his_output_port;
jack_client_t *client;
size_t TABLE_SIZE;

struct timeval tv_start_process;
jack_nframes_t jtime_start;

jack_nframes_t jtime_start_cur_cycle;
struct timeval tv_cur;
double jill_time_cur;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

typedef struct
{
    sample_t *delay;
    int read_phase;
    int write_phase;
}
paDelayData;

jack_nframes_t calc_jack_latency(jack_client_t *client, jack_port_t *input_port, jack_port_t *output_port) {

  jack_nframes_t  latency = 0;

  latency += jack_port_get_latency (jack_port_by_name (client, "system:capture_1"));

  return latency;
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
 */

int process (jack_nframes_t nframes, void *arg) {
  sample_t *out, *in;
  paDelayData *data = (paDelayData*)arg;
  int i, rc;

  if (!timerisset(&tv_start_process)) {
    gettimeofday(&tv_start_process, NULL);
    jtime_start = jack_last_frame_time(client);


    rc = pthread_mutex_trylock(&mutex);
    if (rc == 0) {
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&mutex);
    }
  }

  out = jack_port_get_buffer (my_output_port, nframes);
  in = jack_port_get_buffer (my_input_port, nframes);
  
  for( i=0; i<nframes; i++ ) {
    out[i] = data->delay[data->write_phase];

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
void jack_shutdown (void *arg) {
  exit (1);
}

static void usage () {

  fprintf (stderr, "\n"
	   "usage: JILL_delay \n"
	   "              [ --name OR -n <string identifier for delay> ]\n"
	   "              [ --logfile OR -l <logfile name> ]\n"
	   "              [ --delay OR -d delay (in sec) ]\n"
	   "              [ --output_port OR -o <name of jack port to send delayed signal> ]\n"
	   "              [ --input_port OR -i <name of jack port from which to read signal> ]\n"
	   );
}

int main (int argc, char *argv[]) {
  char *client_name;
  paDelayData data;
  int option_index;
  int opt;
  int i;
  jack_nframes_t SR;
  double delay = 0.2; /* in seconds */
  char his_input_port_name[JILL_MAX_STRING_LEN], his_output_port_name[JILL_MAX_STRING_LEN];

  char jill_id[JILL_MAX_STRING_LEN];

  int logfile_fd;
  char logfile_name[JILL_MAX_STRING_LEN];  

  double jill_time_start;

  const char *options = "n:d:i:o:l:";
  struct option long_options[] =
    {
      {"delay", 1, 0, 'd'},
      {"name", 1, 0, 'n'},
      {"input_port", 1, 0, 'i'},
      {"output_port", 1, 0, 'o'},
      {"logfile", 1, 0, 'l'},
      {0, 0, 0, 0}
    };

  memset(his_output_port_name, '\0', JILL_MAX_STRING_LEN);
  memset(his_input_port_name, '\0', JILL_MAX_STRING_LEN);
  memset(logfile_name, '\0', JILL_MAX_STRING_LEN);
  memset(jill_id, '\0', JILL_MAX_STRING_LEN);

  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'd':
      delay = atof (optarg);
	break;
    case 'n':
      strncpy (jill_id, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'i':
      strncpy (his_output_port_name, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'o':
      strncpy (his_input_port_name, optarg, JILL_MAX_STRING_LEN-1);
      break;
    case 'l':
      strncpy (logfile_name, optarg, JILL_MAX_STRING_LEN-1);
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
    fprintf(stderr, "No input port specified, will not connect.\n");
  } 

  if (his_input_port_name[0] == '\0') {
    fprintf(stderr, "No output port specified, will not connect.\n");
  } 

  if (jill_id[0] == '\0') {
    fprintf(stderr, "No name specified, will use default.\n");
    strcpy(jill_id, "default_id");
  }

  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(JILL_MAX_STRING_LEN * sizeof(char));
  strcpy(client_name, "JILL_delay__");
  strcat(client_name, jill_id);

  client = JILL_connect_server(client_name);

  if (client == NULL) {
    fprintf (stderr, "%s could not connect to jack server", client_name);
    exit (1);
  }

  /* create input and output ports */

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

  SR = jack_get_sample_rate (client);
  TABLE_SIZE = ceil(SR * delay);
  TABLE_SIZE -= calc_jack_latency(client, my_input_port, my_output_port);

  data.delay = (sample_t *) malloc(sizeof(sample_t) * TABLE_SIZE); 
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

   
  /* Tell the JACK server that we are ready to roll.  Our
   * process() callback will start running now. */

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }


  if (strcmp(his_input_port_name, "") != 0) {
    if (jack_connect (client, jack_port_name(my_output_port), his_input_port_name)) {
      fprintf (stderr, "cannot connect to port '%s'\n", his_input_port_name);
    }
  }

  if (strcmp(his_output_port_name, "") != 0) {
    if (jack_connect (client, his_output_port_name, jack_port_name(my_input_port))) {
      fprintf (stderr, "cannot connect to port '%s'\n", his_output_port_name);
    }
  }
  calc_jack_latency(client, my_input_port, my_output_port);

  /* install a signal handler to properly quit jack client */

  signal(SIGQUIT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);

  /* wait until process goes through once (to set the date) then wake up */
  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_mutex_lock(&mutex);

  pthread_cond_wait(&cond, &mutex); 


/*     case 'd': */
/*       delay = atof (optarg); */
/* 	break; */
/*     case 'n': */
/*       strncpy (jill_id, optarg, JILL_MAX_STRING_LEN-1); */
/*       break; */
/*     case 'i': */
/*       strncpy (his_output_port_name, optarg, JILL_MAX_STRING_LEN-1); */
/*       break; */
/*     case 'o': */
/*       strncpy (his_input_port_name, optarg, JILL_MAX_STRING_LEN-1); */
/*       break; */
/*     case 'l': */
/*       strncpy (logfile_name, optarg, JILL_MAX_STRING_LEN-1); */
/*       break; */
/*     default: */
/*       fprintf (stderr, "unknown option %c\n", opt);  */

  jill_time_start = JILL_samples_to_seconds_since_epoch(0, &tv_start_process, SR);
  JILL_log_writef(logfile_fd, "[JILL_delay:%s] %f %u START\n", 
		  jill_id, jill_time_start, jtime_start);
  JILL_log_writef(logfile_fd, "[JILL_delay:%s] %f %u PARAMETER name = %s\n", 
		  jill_id, jill_time_start, jtime_start, jill_id);
  JILL_log_writef(logfile_fd, "[JILL_delay:%s] %f %u PARAMETER logfile = %s\n", 
		  jill_id, jill_time_start, jtime_start, logfile_name);
  JILL_log_writef(logfile_fd, "[JILL_delay:%s] %f %u PARAMETER delay = %f\n", 
		  jill_id, jill_time_start, jtime_start, delay);
  JILL_log_writef(logfile_fd, "[JILL_delay:%s] %f %u PARAMETER input_port = %s\n", 
		  jill_id, jill_time_start, jtime_start, his_output_port_name);
  JILL_log_writef(logfile_fd, "[JILL_delay:%s] %f %u PARAMETER output_port = %s\n", 
		  jill_id, jill_time_start, jtime_start, his_input_port_name);

  pthread_mutex_unlock(&mutex);


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
