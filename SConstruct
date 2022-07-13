import os

if hasattr(os, "uname"):
    system = os.uname()[0]
else:
    system = "Windows"

libname = "jill"

# install location
AddOption(
    "--prefix",
    dest="prefix",
    type="string",
    nargs=1,
    action="store",
    metavar="DIR",
    help="installation prefix",
)
AddOption(
    "--bindir",
    dest="bindir",
    type="string",
    nargs=1,
    action="store",
    metavar="DIR",
    help="binary installation dir",
)
AddOption(
    "--libdir",
    dest="libdir",
    type="string",
    nargs=1,
    action="store",
    metavar="DIR",
    help="library installation",
)
AddOption(
    "--no-arf",
    dest="compile_arf",
    action="store_false",
    default=True,
    help="skip compilation of ARF/HDF5-dependent code",
)
# debug flags for compliation
debug = ARGUMENTS.get("debug", 0)

if not GetOption("prefix") == None:
    install_prefix = GetOption("prefix")
else:
    install_prefix = "/usr/local/"
if not GetOption("bindir") == None:
    install_bindir = GetOption("bindir")
else:
    install_bindir = os.path.join(install_prefix, "bin")
if not GetOption("libdir") == None:
    install_libdir = GetOption("libdir")
else:
    install_libdir = os.path.join(install_prefix, "lib")


Help(
    """
Type: 'scons library' to build the library
      'scons install' to install library and headers under %s
      'scons examples' to compile examples
      (use --prefix  to change library installation location)

Options:
      debug=1      to enable debug compliation
"""
    % install_prefix
)

env = Environment(
    ENV=os.environ,
    PREFIX=install_prefix,
    LIBDIR=install_libdir,
    BINDIR=install_bindir,
    CXXFLAGS=["-std=c++14"],
    tools=["default"],
)

if system == "Darwin":
    env.Replace(CXX="clang++")
    env.Append(
        CPPPATH=["/opt/local/include"],
        # CXXFLAGS=["-stdlib=libc++"],
        # LINKFLAGS=["-stdlib=libc++"],
        LIBPATH=["/opt/local/lib"],
    )

if "CXX" in os.environ:
    env.Replace(CXX=os.environ["CXX"])
if "CFLAGS" in os.environ:
    env.Append(CCFLAGS=os.environ["CFLAGS"].split())
if "CXXFLAGS" in os.environ:
    env.Append(CXXFLAGS=os.environ["CXXFLAGS"].split())
if "LDFLAGS" in os.environ:
    env.Append(LINKFLAGS=os.environ["LDFLAGS"].split())

print(env.subst("using $CXX $CXXVERSION"))

if GetOption("compile_arf"):
    if system != "Darwin":
        env.ParseConfig("pkg-config --cflags --libs hdf5")
    env.Append(CPPPATH=["#/arf"])

if int(debug):
    env.Append(CCFLAGS=["-g2", "-Wall", "-DDEBUG=%s" % debug])
else:
    env.Append(CCFLAGS=["-O2", "-DNDEBUG"])

lib = SConscript("jill/SConscript", exports="env libname")
SConscript("modules/SConscript", exports="env lib")
SConscript("test/SConscript", exports="env lib")
SConscript("util/SConscript", exports="env")

if hasattr(env, "Doxygen"):
    dox = env.Doxygen("doc/doxy.cfg")
    env.Alias("docs", dox)
