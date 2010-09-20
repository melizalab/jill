import os

if hasattr(os,'uname'):
    system = os.uname()[0]
else:
    system = 'Windows'

version = '1.1.0rc1'
libname = 'jill'

# install location
AddOption('--prefix',
          dest='prefix',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='installation prefix')
# whether to use resampling
do_resampling = ARGUMENTS.get('resample',1)
# debug flags for compliation
debug = ARGUMENTS.get('debug',0)

if not GetOption('prefix')==None:
    install_prefix = GetOption('prefix')
else:
    install_prefix = '/usr/local/'

Help("""
Type: 'scons library' to build the library
      'scons install' to install library and headers under %s
      'scons examples' to compile examples
      (use --prefix  to change library installation location)

Options:
      debug=1      to enable debug compliation
      resample=0   to turn off resampling for soundfile output (requires libsamplerate)
""" % install_prefix)


    
env = Environment(CCFLAGS=['-Wall'],
                  CPPDEFINES = [('VERSION', '\\"%s\\"' % version)],
                  LIBS=['jack'],
                  PREFIX=install_prefix,
                  tools=['default'])

if system=='Darwin':
    env.Append(CPPPATH=['/opt/local/include'],
               LIBPATH=['/opt/local/lib'])
if int(debug):
    env.Append(CCFLAGS=['-g2'])
else:
    env.Append(CCFLAGS=['-O2'])

if int(do_resampling):
    env.Append(CPPDEFINES=[('HAVE_SRC', 1)],
               LIBS=['samplerate'])
    

lib = SConscript('jill/SConscript', exports='env libname')
examples = SConscript('examples/SConscript', exports='env lib')

env.Alias('examples',examples)
