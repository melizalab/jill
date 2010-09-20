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

if not GetOption('prefix')==None:
    install_prefix = GetOption('prefix')
else:
    install_prefix = '/usr/local/'

Help("""
Type: 'scons library' to build the library
      'scons install' to install library and headers under %s
      'scons examples' to compile examples
      (use --prefix  to change library installation location)
""" % install_prefix)


    
env = Environment(CCFLAGS=['-O2','-g','-Wall'], # add this to debug counter,'-DDEBUG_COUNTER'],
                  CPPDEFINES = [('VERSION', '\\"%s\\"' % version)],
                  LIBS=['jack'],
                  PREFIX=install_prefix,
                  tools=['default'])

if system=='Darwin':
    env.Append(CPPPATH=['/opt/local/include'],
               LIBPATH=['/opt/local/lib'])

lib = SConscript('jill/SConscript', exports='env libname')
examples = SConscript('examples/SConscript', exports='env lib')

env.Alias('examples',examples)
