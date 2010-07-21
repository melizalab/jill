import os

env = Environment(CCFLAGS=['-O2','-g','-Wall'],
                  LIBS=['jack'],
                  tools=['default'])

SConscript('lib/SConscript', exports='env')
