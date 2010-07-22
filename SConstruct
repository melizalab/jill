import os

if hasattr(os,'uname'):
    system = os.uname()[0]
else:
    system = 'Windows'
    
env = Environment(CCFLAGS=['-O2','-g','-Wall'],
                  LIBS=['jack'],
                  tools=['default'])

if system=='Darwin':
    env.Append(CPPPATH=['/opt/local/include'],
               LIBPATH=['/opt/local/lib'])

SConscript('lib/SConscript', exports='env')
