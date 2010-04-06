#include "JILL.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>

void JILL_soundfile_get_name(char* outfilename, const char *name, double seconds_since_epoch) {
  char timestring[JILL_MAX_STRING_LEN];
  struct tm *tm_time;
  time_t secs;
  
  secs = (time_t) floor(seconds_since_epoch);
  tm_time = localtime(&secs);
  strftime(timestring, JILL_MAX_STRING_LEN, "%Y-%m-%d__%Hh%Mm%Ss", tm_time);
  sprintf(outfilename, "%s__%s.wav", name, timestring);
  outfilename[79] = '\0';

}

SNDFILE* JILL_soundfile_open_for_write(const char *filename, int samplerate) {
  SNDFILE *sf;

  SF_INFO sf_info;
  int channels = 1;
  int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  int mode = SFM_WRITE;

  sf_info.samplerate = samplerate;
  sf_info.channels = channels;
  sf_info.format = format;

  sf = sf_open (filename, mode, &sf_info);

  return sf;
}

sf_count_t JILL_soundfile_write(SNDFILE *sf, sample_t *buf, sf_count_t frames) {
  return sf_write_float (sf, buf, frames);
}

int JILL_soundfile_close(SNDFILE *sf) {
  int ret_val;

  ret_val = sf_close(sf);
  return ret_val;
}


int JILL_trigger_get_crossings(sample_t threshold, sample_t *buf, jack_nframes_t nframes) {
  int i;
  int ncrossings = 0;
  
  sample_t threshold_pos, threshold_neg;
  sample_t before, after;

  threshold_pos = fabs(threshold);
  threshold_neg = -threshold;

  for (i = 0; i < nframes-1; i++) {
    
    before = buf[i];
    after = buf[i+1];
    if ((before < threshold_neg && threshold_neg < after) || 
	(after < threshold_neg && threshold_neg < before)) {
      ncrossings++;
    }
    if ((before < threshold_pos && threshold_pos < after) || 
	(after < threshold_pos && threshold_pos < before)) {
      ncrossings++;
    } 

  }

  return ncrossings;
}


int JILL_get_trigger_state(trigger_data_t *trigger) { 
  return trigger->state; 
}

int JILL_trigger_create(trigger_data_t *trigger, sample_t open_threshold, sample_t close_threshold, float open_window, float close_window, int crossings_per_open_window, int crossings_per_close_window, float sr, int buf_len) {
  int ret = 0;

  trigger->open_threshold = open_threshold;
  trigger->close_threshold = close_threshold;
  trigger->open_window = open_window;
  trigger->close_window = close_window;
  trigger->crossings_per_open_window = crossings_per_open_window;
  trigger->crossings_per_close_window = crossings_per_close_window;
  trigger->buffers_per_open_window = ceil(sr * trigger->open_window / buf_len);
  trigger->buffers_per_close_window = ceil(sr * trigger->close_window / buf_len);
  trigger->nopen_crossings = (int *) calloc(trigger->buffers_per_open_window, sizeof(int));
  trigger->nclose_crossings = (int *) calloc(trigger->buffers_per_close_window, sizeof(int));
  trigger->open_idx = 0;
  trigger->close_idx = 0;
  trigger->state = 0;
  trigger->samples_processed = 0;

  if (trigger->nopen_crossings == 0 || trigger->nclose_crossings == 0) {
    ret = 1;
  }

  return ret;
}

void JILL_trigger_free(trigger_data_t *trigger) {
  free(trigger->nopen_crossings);
  free(trigger->nclose_crossings);
}


int JILL_trigger_calc_new_state(trigger_data_t *trigger, sample_t *buf, jack_nframes_t nframes) {
  int tot_ncrossings;
  int open_test;
  int i;

  /* count crossings in our open window */
  trigger->nopen_crossings[trigger->open_idx++] = JILL_trigger_get_crossings(trigger->open_threshold, buf, nframes);
  
  if (trigger->open_idx >= trigger->buffers_per_open_window) {
    trigger->open_idx -= trigger->buffers_per_open_window;
  }

  /* count crossings in our close window */
  trigger->nclose_crossings[trigger->close_idx++] = JILL_trigger_get_crossings(trigger->close_threshold, buf, nframes);
  
  if (trigger->close_idx >= trigger->buffers_per_close_window) {
    trigger->close_idx -= trigger->buffers_per_close_window;
  }
  
  /* see if we should open */
  tot_ncrossings = 0;

  for (i = 0; i < trigger->buffers_per_open_window; i++) {
    tot_ncrossings += trigger->nopen_crossings[i];
  }

  open_test = tot_ncrossings >= trigger->crossings_per_open_window ? 1 : 0;

  if (trigger->state == 0) { /* we are closed, so new state is result of open test */
    trigger->state = open_test;
  } else if (open_test == 0) { /* we are open and didn't pass open test, so test if should close */    
    tot_ncrossings = 0;
    for (i = 0; i < trigger->buffers_per_close_window; i++) {
      tot_ncrossings += trigger->nclose_crossings[i];
    }
    
    trigger->state = tot_ncrossings <= trigger->crossings_per_close_window ? 0 : 1;
  }
  //  printf("BEFORE trigger samples %lld\n", trigger->samples_processed);
  trigger->samples_processed += nframes;
  //printf("AFTER trigger samples %lld\n", trigger->samples_processed);
  return trigger->state;  
}

int JILL_trigger_get_state(trigger_data_t *trigger) {
  return trigger->state;
}

void JILL_wait_for_keystroke(char *prompt) {
  printf("Press any key to continue.\n");
  printf("here?\n");
  fgetc(stdin);
  return;
}


jack_client_t *JILL_connect_server(char *name) {

  jack_client_t *client;
  jack_status_t status;
  jack_options_t jack_options = JackNullOption;

  jack_options |= JackUseExactName;

  client = jack_client_open (name, jack_options, &status);
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    name = jack_get_client_name(client);
    fprintf (stderr, "unique name `%s' assigned\n", name);
  }

  return client;
}

int JILL_log_open(char *filename) {
  if (strcmp(filename, "stdout") == 0) {
    return 0;
  } else {
    return open(filename, O_APPEND|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  }
}

int JILL_log_writef(int fd, char *fmt, ...) {
  
  char message[256];
  int count = 0;

  va_list argp;
  va_start(argp, fmt);
  vsprintf(message, fmt, argp);
  va_end(argp);
  
  count = strlen(message);
  if (fd == 0) {
    count = write(fd, message, count);
  } else {
    flock(fd, LOCK_EX);

    count = write(fd, message, count);

    flock(fd, LOCK_UN);
  }

  return count;
}


void JILL_samples_to_timeval(long long samples, struct timeval *tv_start_process, int sr, struct timeval *tv) {
  long long start_micro, now_micro;

  start_micro =  (long long) tv_start_process->tv_sec * 1000000 + tv_start_process->tv_usec;
  now_micro = start_micro + floor(1000000 * (float)samples / (float)sr);
  
  tv->tv_sec = (time_t)floor(now_micro / 1000000.);
  tv->tv_usec = (suseconds_t) (now_micro - 1000000 * tv->tv_sec);
}
 
double JILL_timeval_to_seconds_since_midnight(struct timeval *tv) {
  struct tm *lt;
  double ssm = 0;
  
  lt = localtime(&(tv->tv_sec));

  ssm =lt->tm_sec + 60. * lt->tm_min + 3600 * lt->tm_hour + tv->tv_usec / 1000000.;

  return ssm;
}

double JILL_samples_to_seconds_since_epoch(long long samples, struct timeval *tv_start_process, int sr) {
  struct timeval tv;

  JILL_samples_to_timeval(samples, tv_start_process, sr, &tv);
  return (double) tv.tv_sec + tv.tv_usec / 1000000.;
}
