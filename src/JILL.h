#ifndef JILL_H
#define JILL_H

#ifdef __cplusplus
extern "C" {
#endif 


#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
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
    long long samples_processed;
  } trigger_data_t;

  void JILL_soundfile_get_name(char* outfilename, const char *name, double seconds_since_epoch);
  SNDFILE* JILL_soundfile_open_for_write(const char *filename, int samplerate);
  sf_count_t JILL_soundfile_write(SNDFILE *sf, sample_t *buf, sf_count_t frames);
  int JILL_soundfile_close(SNDFILE *sf);
  void JILL_wait_for_keystroke();
 
  jack_client_t *JILL_connect_server(char *client_name);

  int JILL_trigger_create(trigger_data_t *trigger, 
			  sample_t open_threshold, sample_t close_threshold, 
			  float open_window, float close_window, 
			  int crossings_per_open_window, int crossings_per_close_window, 
			  float sr, int buf_len);

  void JILL_trigger_free(trigger_data_t *trigger);
  int JILL_trigger_get_crossings(sample_t threshold, sample_t *buf, jack_nframes_t nframes);
  int JILL_trigger_get_state(trigger_data_t *trigger);
  int JILL_trigger_calc_new_state(trigger_data_t *trigger, sample_t *buf, jack_nframes_t nframes);


  int JILL_log_open(char *filename);
  int JILL_log_writef(int fd, char *fmt, ...);

  void JILL_samples_to_timeval(long long samples, struct timeval *tv_start_process, int sr, struct timeval *tv);
  double JILL_timeval_to_seconds_since_midnight(struct timeval *tv);
  double JILL_samples_to_seconds_since_epoch(long long samples, struct timeval *tv_start_process, int sr);


#ifdef __cplusplus
}
#endif


#endif /* JILL_H */
