#ifndef JILL_H
#define JILL_H

#ifdef __cplusplus
extern "C" {
#endif 


#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <jack/jack.h>

#define JILL_MAX_FILENAME_LEN 80

  typedef jack_default_audio_sample_t sample_t;

  typedef struct {
    int state;
    sample_t threshhold;
    float window;
    int crossings_per_window;
    int buffers_per_window;
    int *ncrossings;
    int c_idx;
  } trigger_data_t;

  SNDFILE* JILL_open_soundfile_for_write(const char *filename);
  sf_count_t JILL_soundfile_write(SNDFILE *sf, float *buf, sf_count_t frames);
  int JILL_close_soundfile(SNDFILE *sf);
  void JILL_wait_for_keystroke();
 
  int JILL_trigger_create(trigger_data_t *trigger, float threshhold, float window, int crossings_per_window, float sr, int buf_len);
  void JILL_trigger_free(trigger_data_t *trigger);
  int JILL_trigger_get_crossings(trigger_data_t *trigger, float *buf, jack_nframes_t nframes);
  int JILL_trigger_get_state(trigger_data_t *trigger);
  int JILL_trigger_set_event_seen(trigger_data_t *trigger);
  int JILL_trigger_calc_new_state(trigger_data_t *trigger, sample_t *buf, jack_nframes_t nframes);

#ifdef __cplusplus
}
#endif


#endif /* JILL_H */
