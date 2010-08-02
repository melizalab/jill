import os

if hasattr(os,'uname'):
    system = os.uname()[0]
else:
    system = 'Windows'

version = '1.0.0rc3'
libname = 'jill'
    
env = Environment(CCFLAGS=['-O2','-g','-Wall'],
                  CPPDEFINES = [('VERSION', '\\"%s\\"' % version)],
                  LIBS=['jack'],
                  tools=['default'])

if system=='Darwin':
    env.Append(CPPPATH=['/opt/local/include'],
               LIBPATH=['/opt/local/lib'])

lib = SConscript('jill/SConscript', exports='env libname')
examples = SConscript('examples/SConscript', exports='env lib')

env.Alias('examples',examples)
