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

};
