#include "JILL.h"

SNDFILE* JILL_open_soundfile_for_write(const char *filename) {
  SNDFILE *sf;

  SF_INFO sf_info;
  int sr = 44100;
  int channels = 1;
  int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  int mode = SFM_WRITE;

  sf_info.samplerate = sr;
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


int JILL_trigger_get_crossings(trigger_data_t *trigger, float *buf, jack_nframes_t nframes) {
  int i;
  int ncrossings = 0;

  float mine, before, after;

  for (i = 0; i < nframes-1; i++) {
    mine = fabs(trigger->threshhold);
    before = fabs(buf[i]);
    after = fabs(buf[i+1]);
    if ((before < mine && mine < after) || (after < mine && mine < before)) {
      ncrossings++;
    }
  }

  return ncrossings;
}


int JILL_get_trigger_state(trigger_data_t *trigger) { 
  return trigger->state; 
}

int JILL_trigger_create(trigger_data_t *trigger, float threshhold, float window, int crossings_per_window, float sr, int buf_len) {
  int ret = 0;

  trigger->threshhold = threshhold;
  trigger->window = window;
  trigger->crossings_per_window = crossings_per_window;
  trigger->buffers_per_window = ceil(sr * trigger->window / buf_len);
  trigger->ncrossings = (int *) calloc(trigger->buffers_per_window, buf_len * sizeof(int));
  trigger->c_idx = 0;
    
  if (trigger->ncrossings == 0) {
    ret = 1;
  }

  return ret;
}

void JILL_trigger_free(trigger_data_t *trigger) {
  free(trigger->ncrossings);
}

/* expects input buffer to be of size one jack buffer long */
int JILL_trigger_calc_new_state(trigger_data_t *trigger, sample_t *buf, jack_nframes_t nframes) {
  int tot_ncrossings = 0;
  int i;
  
  trigger->ncrossings[trigger->c_idx++] = JILL_trigger_get_crossings(trigger, buf, nframes);
  
  if (trigger->c_idx >= trigger->buffers_per_window) {
    trigger->c_idx -= trigger->buffers_per_window;
  }
  
  for (i = 0; i < trigger->buffers_per_window; i++) {
    tot_ncrossings += trigger->ncrossings[i];
  }
  
  trigger->state = tot_ncrossings >= trigger->crossings_per_window ? 1 : 0;
  
  return trigger->state;
  
}

