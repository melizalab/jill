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

#define JILL_MAX_STRING_LEN 80

  typedef jack_default_audio_sample_t sample_t;

  typedef struct {
    int state;
    sample_t open_threshold;
    sample_t close_threshold;
    float open_window;
    float close_window;
    int crossings_per_open_window;
    int crossings_per_close_window;
    int buffers_per_open_window;
    int buffers_per_close_window;
    int *nopen_crossings;
    int *nclose_crossings;
    int open_idx;
    int close_idx;
  } trigger_data_t;

  void JILL_get_outfilename(char* outfilename, const char *name, const char *portname);
  SNDFILE* JILL_open_soundfile_for_write(const char *filename, int samplerate);
  sf_count_t JILL_soundfile_write(SNDFILE *sf, float *buf, sf_count_t frames);
  int JILL_close_soundfile(SNDFILE *sf);
  void JILL_wait_for_keystroke();
 
  int JILL_trigger_create(trigger_data_t *trigger, 
			  sample_t open_threshold, sample_t close_threshold, 
			  float open_window, float close_window, 
			  int crossings_per_open_window, int crossings_per_close_window, 
			  float sr, int buf_len);

  void JILL_trigger_free(trigger_data_t *trigger);
  int JILL_trigger_get_crossings(sample_t threshold, sample_t *buf, jack_nframes_t nframes);
  int JILL_trigger_get_state(trigger_data_t *trigger);
  int JILL_trigger_calc_new_state(trigger_data_t *trigger, sample_t *buf, jack_nframes_t nframes);

#ifdef __cplusplus
}
#endif


#endif /* JILL_H */
