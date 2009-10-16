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


int JILL_get_crossings(float threshhold, float *buf, jack_nframes_t nframes) {
  int i;
  int ncrossings = 0;

  float mine, before, after;

  for (i = 0; i < nframes-1; i++) {
    mine = fabs(threshhold);
    before = fabs(buf[i]);
    after = fabs(buf[i+1]);
    if ((before < mine && mine < after) || (after < mine && mine < before)) {
      ncrossings++;
    }
  }

  return ncrossings;
}
