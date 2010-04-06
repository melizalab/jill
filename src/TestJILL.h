#include <cxxtest/TestSuite.h>
#include "JILL.h"

class JILLSuite : public CxxTest::TestSuite {
  
public:

  void testOpenSoundFile() {
    SNDFILE *sf;
    char cmd[JILL_MAX_STRING_LEN];
    int ret;

    const char *filename = "/tmp/blah.wav";
    sf = JILL_soundfile_open_for_write(filename, 44100);
    TS_ASSERT(sf != NULL);

    sprintf(cmd, "rm -rf %s", filename);
    system (cmd);
  }

  void testCloseSoundFile() {
    SNDFILE *sf;
    char cmd[JILL_MAX_STRING_LEN];
    int ret;

    const char *filename = "/tmp/blah.wav";
    sf = JILL_soundfile_open_for_write(filename, 44100);

    ret = JILL_soundfile_close(sf);

    TS_ASSERT(ret == 0);

    sprintf(cmd, "rm -rf %s", filename);
    system (cmd);
  }


  void testGetCrossings() {

    float buf1[] = { 0.7, -0.7, 0.55, 0.54, -0.4, -0.7, -0.8, -0.9 }; // 6
    float buf2[] = { 0.4, 0.45, 0.3, -0.3, -0.4, -0.7, -0.8, -0.45 }; // 2
    float buf3[] = { 0.7, -0.7, 0., 0., 0., 0., 0., 0. }; // 3
    
    jack_nframes_t nframes = 8;
    float threshold = 0.5;

    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(threshold, buf1, nframes), 6);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(threshold, buf2, nframes), 2);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(threshold, buf3, nframes), 3);

  }


  void testTriggerCalcNewState() {

    float buf4[] = { 0.7, 0.6, 0.55, 0.45, 0.55, -0.7, -0.8, -0.9 }; // 4
    float buf2[] = { 0.4, 0.45, 0.3, -0.3, -0.4, -0.7, -0.8, -0.45 }; // 2
    float buf0[] = { 0., 0., 0., 0., 0., 0., 0., 0. }; // 0
    float buf6[] = { 0.7, -0.7, 0.55, 0.54, -0.4, -0.7, -0.8, -0.9 }; // 6

    jack_nframes_t nframes = 8;
    float open_threshold = 0.5, close_threshold = 0.1;
    float open_window = 8, close_window = 16;
    int crossings_per_open_window = 5, crossings_per_close_window = 3;
    float sr = 1;
    int buf_len = 8;
    int state = 0;
    
    int rc;
    trigger_data_t trigger;

    rc = JILL_trigger_create(&trigger, 
			     open_threshold, 
			     close_threshold,
			     open_window, close_window,
			     crossings_per_open_window, 
			     crossings_per_close_window,
			     sr, buf_len);

    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(open_threshold, buf4, buf_len), 4);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(open_threshold, buf6, buf_len), 6);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(open_threshold, buf2, buf_len), 2);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(open_threshold, buf0, buf_len), 0);

    TS_ASSERT_EQUALS(trigger.buffers_per_open_window, 1);
    TS_ASSERT_EQUALS(trigger.buffers_per_close_window, 2);

    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf4, buf_len), 0);
    TS_ASSERT_EQUALS(trigger.nopen_crossings[0], 4);
    TS_ASSERT_EQUALS(trigger.nclose_crossings[0], 2);
    TS_ASSERT_EQUALS(trigger.nclose_crossings[1], 0);

    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf6, buf_len), 1);
    TS_ASSERT_EQUALS(trigger.nopen_crossings[0], 6);
    TS_ASSERT_EQUALS(trigger.nclose_crossings[0], 2);
    TS_ASSERT_EQUALS(trigger.nclose_crossings[1], 6);

    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf0, buf_len), 1);
    TS_ASSERT_EQUALS(trigger.nopen_crossings[0], 0);
    TS_ASSERT_EQUALS(trigger.nclose_crossings[0], 0);
    TS_ASSERT_EQUALS(trigger.nclose_crossings[1], 6);

    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf2, buf_len), 0);
    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf0, buf_len), 0);

    JILL_trigger_free(&trigger);
  }

  void testTriggerCreateFree() {

    trigger_data_t trigger;
    
    int rc;
    sample_t open_threshold = 0.3, close_threshold = 0.05;
    float open_window = 0.1, close_window = 0.25;
    int crossings_per_open_window = 53, crossings_per_close_window = 10;
    float sr = 44100.;
    int buf_len = 64;

    rc = JILL_trigger_create(&trigger, 
			     open_threshold, close_threshold,
			     open_window, close_window,
			     crossings_per_open_window, crossings_per_close_window,
			     sr, buf_len);

    TS_ASSERT_EQUALS(rc, 0);

    JILL_trigger_free(&trigger);

  }

  void testOutFilename() {
    char filename[JILL_MAX_STRING_LEN];
    struct timeval tv;
    double secs;
    
    gettimeofday(&tv, NULL);
    secs = tv.tv_sec;
    JILL_soundfile_get_name(filename, "testing", secs);
    TS_ASSERT(strlen(filename) <= JILL_MAX_STRING_LEN);
  }
};
