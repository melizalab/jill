#ifndef JILL_H
#define JILL_H

#ifdef __cplusplus
extern "C" {
#endif 


#include <sndfile.h>
#include <stdio.h>
#include <math.h>
#include <jack/jack.h>

#define JILL_MAX_FILENAME_LEN 80


  typedef struct {
    bool state;
    jack_nframes_t 
  } trigger_data_t;

  SNDFILE* JILL_open_soundfile_for_write(const char *filename);
  sf_count_t JILL_soundfile_write(SNDFILE *sf, float *buf, sf_count_t frames);
  int JILL_close_soundfile(SNDFILE *sf);
  int JILL_get_crossings(float threshhold, float *buf, jack_nframes_t nframes);
    
  bool JILL_get_trigger_state(trigger_data_t *trigger) { return trigger->state };
  jack_nframes_t JILL_get_trigger_start_frame(trigger);
  
#ifdef __cplusplus
}
#endif


#endif // JILL_H
