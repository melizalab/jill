#include <unistd.h>
#include <math.h>
#include <string.h>
#include <getopt.h>

#include "JILL.h"


static void usage () {

  fprintf (stderr, "\n"
"usage: makeSignalFile \n"
"                --outfile OR -o outfile \n"
"                --v1 OR -v open voltage \n"
"                --v2 OR -w close voltage \n"
"                --dt1 OR -t open duration (in sec) \n"
"                --dt2 OR -s close duration (in sec) \n"
"                --samplerate OR -r sample rate (in Hz) \n"
"\n"
"default values: 'signal.wav', +4V, -4V, 0.1s, 0.01s, 44100Hz\n"
"\n"
	   );

}


int main(int argc, char **argv) {

  float dt1 = 0.1, dt2 = 0.01, v1 = 4.0, v2 = -4.0, sr = 44100.;
  float dt_samp;
  int i;
 
  char filename[JILL_MAX_FILENAME_LEN];
  SNDFILE *sf;
  int table_size;
  float *table;
  int num_frames_written_to_disk;

  int opt, option_index;
  const char *options = "r:t:s:v:w:o:";
  struct option long_options[] =
    {
      {"outfile", 1, 0, 'o'},
      {"dt1", 1, 0, 't'},
      {"dt2", 1, 0, 's'},
      {"v1", 1, 0, 'v'},
      {"v2", 1, 0, 'w'},
      {"samplerate", 1, 0, 'r'},
      {0, 0, 0, 0}
    };

  strncpy (filename, "signal.wav", JILL_MAX_FILENAME_LEN-1);
  filename[JILL_MAX_FILENAME_LEN-1] = '\0';


  while ((opt = getopt_long (argc, argv, options, long_options, &option_index)) != EOF) {
    switch (opt) {
    case 'o':
      strncpy(filename, optarg, JILL_MAX_FILENAME_LEN-1);
      filename[JILL_MAX_FILENAME_LEN-1] = '\0';
      break;
    case 'r':
      sr = atof(optarg);
      break;
    case 't':
      dt1 = atof(optarg);
      break;
    case 's':
      dt2 = atof(optarg);
      break;
    case 'v':
      v1 = atof(optarg);
      break;
    case 'w':
      v2 = atof(optarg);
      break;
    default:
      fprintf (stderr, "unknown option %c\n", opt); 
    case 'h':
      usage ();
      return -1;
    }
  }

  sf = JILL_open_soundfile_for_write(filename);
  if (sf == NULL) {
    printf("could not open file, '%s' for writing\n", filename);
  }

  dt_samp = 1 / sr;

  table_size = ceil(( dt1 + dt2 ) * sr);

  
  printf (
	  "filename:      %s\n"
	  "ontime:        %f\n"
	  "offtime:       %f\n"
	  "open voltage:  %f\n"
	  "close voltage: %f\n"
	  "sample rate:   %f\n", 
	  filename, dt1, dt2, v1, v2, sr);
  
  table = (float *) calloc(sizeof(float), table_size);
  for (i = 0; i < table_size; i++) {
    if (i * dt_samp < dt1) {
      table[i] = v1;
    } else {
      table[i] = v2;
    }
  }

  num_frames_written_to_disk = JILL_soundfile_write(sf, table, table_size);
  if (num_frames_written_to_disk != table_size) {
    printf("TROUBLE: wanted to write %d frames, but call only wrote %d frames\n", table_size, num_frames_written_to_disk);
  }

  JILL_close_soundfile(sf);

  free(table);


}
