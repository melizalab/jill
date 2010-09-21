import os

if hasattr(os,'uname'):
    system = os.uname()[0]
else:
    system = 'Windows'

version = '1.1.0rc2'
libname = 'jill'

# install location
AddOption('--prefix',
          dest='prefix',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='installation prefix')
# debug flags for compliation
debug = ARGUMENTS.get('debug',1)

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
""" % install_prefix)


    
env = Environment(CCFLAGS=['-Wall'],
                  CPPDEFINES = [('VERSION', '\\"%s\\"' % version)],
                  LIBS=['jack','samplerate'],
                  PREFIX=install_prefix,
                  tools=['default'])

if system=='Darwin':
    env.Append(CPPPATH=['/opt/local/include'],
               LIBPATH=['/opt/local/lib'])
if int(debug):
    env.Append(CCFLAGS=['-g2'])
else:
    env.Append(CCFLAGS=['-O2'])

lib = SConscript('jill/SConscript', exports='env libname')
examples = SConscript('examples/SConscript', exports='env lib')

env.Alias('examples',examples)
