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

    float buf[] = { 0.4, 0.6, 0.3, -0.3, -0.4, -0.7, -0.8 };
    jack_nframes_t nframes = 7;
    float threshhold = 0.5;

    TS_ASSERT_EQUALS(JILL_get_crossings(threshhold, buf, nframes), 3);
  }

};
