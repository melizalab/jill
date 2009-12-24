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
/* Time and tempo variables.  These are global to the entire,
 * transport timeline.  There is no attempt to keep a true tempo map.
 * The default time signature is: "march time", 4/4, 120bpm
 */
float time_beats_per_bar = 4.0;
float time_beat_type = 4.0;
double time_ticks_per_beat = 1920.0;
double time_beats_per_minute = 120.0;
volatile int time_reset = 1;		/* true when time values change */
int am_time_master = 0;

struct timeval tv_start_process;
jack_nframes_t jtime_start;

jack_nframes_t jtime_start_cur_cycle;
double start_frame_time;

/* JACK timebase callback.
 *
 * Runs in the process thread.  Realtime, must not wait.
 */
static void timebase(jack_transport_state_t state, jack_nframes_t nframes, 
	      jack_position_t *pos, int new_pos, void *arg)
{
  double min;			/* minutes since frame 0 */
  long abs_tick;			/* ticks since frame 0 */
  long abs_beat;			/* beats since frame 0 */
  static int first_call = 1;
  

	
  if (first_call) {
    first_call = 0;
    gettimeofday(&tv_start_process, NULL);
    start_frame_time = tv_start_process.tv_sec + (double) tv_start_process.tv_usec / 1000000.0; 
    jtime_start = jack_frame_time(client);
  }

  //pos->frame_time = tv.tv_sec + tv.tv_usec * 1000000.;
	
  if (new_pos || time_reset) {

    pos->valid = JackPositionBBT|JackPositionTimecode;
    pos->beats_per_bar = time_beats_per_bar;
    pos->beat_type = time_beat_type;
    pos->ticks_per_beat = time_ticks_per_beat;
    pos->beats_per_minute = time_beats_per_minute;
   
    time_reset = 0;		/* time change complete */
    
    /* Compute BBT info from frame number.  This is relatively
     * simple here, but would become complex if we supported tempo
     * or time signature changes at specific locations in the
     * transport timeline. */
    
    pos->frame_time = start_frame_time + (jack_frame_time(client) - jtime_start) / (double) pos->frame_rate;
    min = pos->frame / ((double) pos->frame_rate * 60.0);
    abs_tick = min * pos->beats_per_minute * pos->ticks_per_beat;
    abs_beat = abs_tick / pos->ticks_per_beat;
    
    pos->bar = abs_beat / pos->beats_per_bar;
    pos->beat = abs_beat - (pos->bar * pos->beats_per_bar) + 1;
    pos->tick = abs_tick - (abs_beat * pos->ticks_per_beat);
    pos->bar_start_tick = pos->bar * pos->beats_per_bar *
      pos->ticks_per_beat;
    pos->bar++;		/* adjust start to bar 1 */
    
#if 0
    /* some debug code... */
    fprintf(stderr, "\nnew position: %" PRIu32 "\tBBT: %3"
	    PRIi32 "|%" PRIi32 "|%04" PRIi32 "\n",
	    pos->frame, pos->bar, pos->beat, pos->tick);
#endif
    
  } else {
    
    pos->frame_time += (double) nframes / pos->frame_rate;
    
    /* Compute BBT info based on previous period. */
    pos->tick +=
      nframes * pos->ticks_per_beat * pos->beats_per_minute
      / (pos->frame_rate * 60);
    
    while (pos->tick >= pos->ticks_per_beat) {
      pos->tick -= pos->ticks_per_beat;
      if (++pos->beat > pos->beats_per_bar) {
	pos->beat = 1;
	++pos->bar;
	pos->bar_start_tick +=
	  pos->beats_per_bar
	  * pos->ticks_per_beat;
      }
    }
  }
  
}

void jack_shutdown (void *arg) {
  exit (1);
}

static void signal_handler(int sig) {

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
  return 0;      
}


int main (int argc, char *argv[]) {
  char *client_name;
  int option_index;
  int opt;
  int rc;

  const char *options = "";
  struct option long_options[] =
    {
      {0, 0, 0, 0}
    };
  
  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    default:
      fprintf (stderr, "unknown option %c\n", opt); 
    case 'h':
      //usage ();
      return -1;
    }
  }


  /* open a client connection to the JACK server */
    
  client_name = (char *) malloc(JILL_MAX_STRING_LEN * sizeof(char));
  strcpy(client_name, "JILL_transport");

  client = JILL_connect_server(client_name);

  if (client == NULL) {
    fprintf(stderr, "could not connect to jack server\n");
    exit(1);
  }

  /* tell the JACK server to call `process()' whenever
     there is work to be done.
  */

  jack_set_process_callback (client, process, NULL);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us.
  */

  jack_on_shutdown (client, jack_shutdown, 0);
  rc = jack_set_timebase_callback(client, 1, timebase, NULL);
  
  if (rc == 0) {
    am_time_master = 1;
  } else {
    am_time_master = 0;
  }

  if (!am_time_master)
    fprintf(stderr, "Unable to take over timebase.\n");
   
  /* Tell the JACK server that we are ready to roll.  Our
   * process() callback will start running now. */

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }


  signal(SIGQUIT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);
  while (1) {
    sleep(1);
  }
  
  jack_client_close (client);
  exit (0);
}
