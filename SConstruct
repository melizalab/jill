import glob
import os
import subprocess

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
Type: 'scons modules' to build the JILL modules
      'scons library' to build the library the modules link against
      'scons test' to build the test programs
      'scons install' to install the module binaries under %s

      The library and headers are used only within this source tree,
      so 'install' deploys the binaries and scripts and nothing else.

Options:
      debug=1      to enable debug compilation
      debug=2      as debug=1, and emit DBG log messages at runtime
      --no-arf     skip jrecord and the rest of the ARF/HDF5 code
      --prefix     installation prefix (default /usr/local)
      --bindir     binary installation directory (default PREFIX/bin)
"""
    % install_bindir
)

env = Environment(
    ENV=os.environ,
    PREFIX=install_prefix,
    LIBDIR=install_libdir,
    BINDIR=install_bindir,
    CXXFLAGS=["-std=c++17"],
    tools=["default"],
)

if system == "Darwin":
    env.Replace(CXX="clang++")
    env.Append(
        CPPPATH=["/opt/local/include/"],
        # CXXFLAGS=["-stdlib=libc++"],
        # LINKFLAGS=["-stdlib=libc++"],
        LIBPATH=["/opt/local/lib/"],
    )


def boost_version_key(path):
    """Sort key for a versioned boost directory name, e.g. '1.87' -> (1, 87)."""
    try:
        return tuple(int(part) for part in os.path.basename(path).split("."))
    except ValueError:
        return ()


# Boost has no pkg-config metadata, so locate it by convention. BOOST_ROOT
# overrides; otherwise MacPorts keeps versioned boost ports under libexec, and
# we take the newest rather than pinning to one release.
boost_root = os.environ.get("BOOST_ROOT")
if boost_root is None and system == "Darwin":
    candidates = sorted(glob.glob("/opt/local/libexec/boost/*"), key=boost_version_key)
    boost_root = candidates[-1] if candidates else None
if boost_root:
    env.Append(
        CPPPATH=[os.path.join(boost_root, "include")],
        LIBPATH=[os.path.join(boost_root, "lib")],
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

# Warnings are on in every configuration, not just debug builds, so problems
# surface during ordinary development. -Wunused-parameter is switched back off
# because the JACK callback signatures oblige every callback to accept
# arguments that most of them ignore: it accounts for ~94 of the ~106 warnings
# this codebase produces, and leaving it on buries the ones that matter.
env.Append(CCFLAGS=["-Wall", "-Wextra", "-Wno-unused-parameter"])

if int(debug):
    env.Append(CCFLAGS=["-g2", "-DDEBUG=%s" % debug])
else:
    env.Append(CCFLAGS=["-O2", "-DNDEBUG"])


def require_pkgconfig(env, *packages):
    """Add cflags/libs for each package, failing early with a clear message."""
    for pkg in packages:
        if subprocess.call(["pkg-config", "--exists", pkg]) != 0:
            print("error: required dependency '%s' was not found by pkg-config." % pkg)
            print("       Install its development package (see doc/debian-installation.md")
            print("       or doc/osx-installation.md), or set PKG_CONFIG_PATH.")
            Exit(1)
        env.ParseConfig("pkg-config --cflags --libs '%s'" % pkg)


# These all ship pkg-config metadata, which supplies include and library paths
# rather than assuming they are on the default search path. ZMQ 4.x is required;
# earlier releases are not API-compatible.
require_pkgconfig(env, "jack", "samplerate", "sndfile", "libzmq >= 4.0")

if GetOption("compile_arf"):
    if not os.path.exists(Dir("#/arf/c++").abspath):
        print("error: the ARF headers are missing from arf/.")
        print("       Run 'git submodule update --init' to fetch them,")
        print("       or build without jrecord using 'scons --no-arf'.")
        Exit(1)
    if system != "Darwin":
        require_pkgconfig(env, "hdf5")
    env.Append(CPPPATH=["#/arf/c++"])

# Boost library names vary by platform: MacPorts has historically used an -mt
# suffix, Debian has not. Detect rather than guess. Boost.System is deliberately
# absent -- it has been header-only since Boost 1.69 and recent distributions no
# longer ship the stub library at all.
BOOST_LIBS = ["boost_date_time", "boost_program_options", "boost_filesystem"]

if not GetOption("help") and not GetOption("clean"):
    conf = Configure(env)
    resolved = []
    for base in BOOST_LIBS:
        for name in (base, base + "-mt"):
            # autoadd=0: record the name, but let the SConscripts decide which
            # targets actually link against Boost.
            if conf.CheckLib(name, language="C++", autoadd=0):
                resolved.append(name)
                break
        else:
            print("error: could not find the Boost library '%s'." % base)
            print("       Install the Boost development packages, or point")
            print("       BOOST_ROOT at your Boost installation prefix.")
            Exit(1)
    env = conf.Finish()
    env.Replace(BOOST_LIBS=resolved)
else:
    env.Replace(BOOST_LIBS=BOOST_LIBS)

lib = SConscript("jill/SConscript", exports="env libname")
SConscript("modules/SConscript", exports="env lib")
SConscript("test/SConscript", exports="env lib")
SConscript("scripts/SConscript", exports="env")

if hasattr(env, "Doxygen"):
    dox = env.Doxygen("doc/doxy.cfg")
    env.Alias("docs", dox)
