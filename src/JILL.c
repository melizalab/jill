#include "JILL.h"
#include <time.h>

void JILL_get_outfilename(char* outfilename, const char *name, const char *portname) {
  char timestring[JILL_MAX_STRING_LEN];
  time_t t;
  struct tm *tm_time;
  
  t = time(NULL);
  tm_time = localtime(&t);
  strftime(timestring, JILL_MAX_STRING_LEN, "%Y%m%d_%H%M%S", tm_time);
  sprintf(outfilename, "%s_%s_%s.wav", name, portname, timestring);
  outfilename[79] = '\0';

}

SNDFILE* JILL_open_soundfile_for_write(const char *filename, int samplerate) {
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

sf_count_t JILL_soundfile_write(SNDFILE *sf, float *buf, sf_count_t frames) {
  return sf_write_float (sf, buf, frames);
}

int JILL_close_soundfile(SNDFILE *sf) {
  int ret_val;

  ret_val = sf_close(sf);
  return ret_val;
}


int JILL_trigger_get_crossings(sample_t threshold, sample_t *buf, jack_nframes_t nframes) {
  int i;
  int ncrossings = 0;
  
  sample_t threshold_pos, threshold_neg;
  sample_t mine, before, after;

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
  trigger->nopen_crossings = (int *) calloc(trigger->buffers_per_open_window, buf_len * sizeof(int));
  trigger->nclose_crossings = (int *) calloc(trigger->buffers_per_close_window, buf_len * sizeof(int));
  trigger->open_idx = 0;
  trigger->close_idx = 0;
  
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
      tot_ncrossings += trigger->nopen_crossings[i];
    }
    
    trigger->state = tot_ncrossings <= trigger->crossings_per_close_window ? 0 : 1;
  }

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
