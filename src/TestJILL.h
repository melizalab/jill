#include <cxxtest/TestSuite.h>
#include "JILL.h"

class JILLSuite : public CxxTest::TestSuite {
  
public:

  void testOpenSoundFile() {
    SNDFILE *sf;
    char cmd[80];
    int ret;

    const char *filename = "/tmp/blah.wav";
    sf = JILL_open_soundfile_for_write(filename);
    TS_ASSERT(sf != NULL);

    sprintf(cmd, "rm -rf %s", filename);
    system (cmd);
  }

  void testCloseSoundFile() {
    SNDFILE *sf;
    char cmd[80];
    int ret;

    const char *filename = "/tmp/blah.wav";
    sf = JILL_open_soundfile_for_write(filename);

    ret = JILL_close_soundfile(sf);

    TS_ASSERT(ret == 0);

    sprintf(cmd, "rm -rf %s", filename);
    system (cmd);
  }


  void testGetCrossings() {

    float buf1[] = { 0.7, 0.6, 0.55, 0.54, -0.4, -0.7, -0.8, -0.9 }; // 2
    float buf2[] = { 0.4, 0.45, 0.3, -0.3, -0.4, -0.7, -0.8, -0.45 }; // 2
    float buf3[] = { 0., 0., 0., 0., 0., 0., 0., 0. }; // 0
    
    jack_nframes_t nframes = 8;
    float threshhold = 0.5;
    float window = 16;
    int crossings_per_window = 3;
    float sr = 1;
    int buf_len = 8;
    int state = 0;
    
    int rc;
    trigger_data_t trigger;

    rc = JILL_trigger_create(&trigger, threshhold, window, crossings_per_window, sr, buf_len);

    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(&trigger, buf1, nframes), 2);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(&trigger, buf2, nframes), 2);
    TS_ASSERT_EQUALS(JILL_trigger_get_crossings(&trigger, buf3, nframes), 0);

    JILL_trigger_free(&trigger);
  }


  void testTriggerCalcNewState() {

    float buf1[] = { 0.7, 0.6, 0.55, 0.54, -0.4, -0.7, -0.8, -0.9 }; // 2
    float buf2[] = { 0.4, 0.45, 0.3, -0.3, -0.4, -0.7, -0.8, -0.45 }; // 2
    float buf3[] = { 0., 0., 0., 0., 0., 0., 0., 0. }; // 0
    
    jack_nframes_t nframes = 8;
    float threshhold = 0.5;
    float window = 16;
    int crossings_per_window = 3;
    float sr = 1;
    int buf_len = 8;
    int state = 0;
    
    int rc;
    trigger_data_t trigger;

    rc = JILL_trigger_create(&trigger, threshhold, window, crossings_per_window, sr, buf_len);

    
    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf1, buf_len), 0);
    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf2, buf_len), 1);
    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf3, buf_len), 0);
    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf1, buf_len), 0);
    TS_ASSERT_EQUALS(JILL_trigger_calc_new_state(&trigger, buf1, buf_len), 1);

    JILL_trigger_free(&trigger);
  }

  void testTriggerCreateFree() {

    trigger_data_t trigger;
    
    int rc;
    sample_t threshhold = 0.3;
    float window = 0.03;
    int crossings_per_window = 53;
    float sr = 44100.;
    int buf_len = 64;

    rc = JILL_trigger_create(&trigger, threshhold, window, crossings_per_window, sr, buf_len);

    //    trigger = (trigger_data_t *) malloc (sizeof(trigger_data_t));
    TS_ASSERT_EQUALS(rc, 0);

    JILL_trigger_free(&trigger);

  }
};
