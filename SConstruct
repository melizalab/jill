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
# debug flags for compilation
debug = ARGUMENTS.get("debug", 0)
# runtime sanitizers, e.g. sanitize=address or sanitize=thread. Passed straight
# through to -fsanitize, so the compiler's own combinations work too.
# sanitize=realtime is special-cased below because it is clang-only.
sanitize = ARGUMENTS.get("sanitize", None)

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
      sanitize=X   build with -fsanitize=X, e.g. address, thread, or
                   realtime. realtime checks the process callbacks for
                   allocation, locking and blocking syscalls, and needs clang.
                   Combine with debug=1, or the assertions are compiled
                   out of the very code being checked.
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

def brew_prefix(package=None):
    """Ask Homebrew where it installs things, or None if brew is not present.

    The answer varies by machine -- /opt/homebrew on Apple Silicon,
    /usr/local on Intel -- so it has to be queried rather than assumed.
    """
    command = ["brew", "--prefix"]
    if package:
        command.append(package)
    try:
        prefix = subprocess.check_output(
            command, stderr=subprocess.DEVNULL, universal_newlines=True
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    return prefix if prefix and os.path.isdir(prefix) else None


def add_prefix(env, prefix):
    """Add a package manager's include, library and pkg-config directories."""
    env.Append(
        CPPPATH=[os.path.join(prefix, "include")],
        LIBPATH=[os.path.join(prefix, "lib")],
    )
    pkgconfig_dir = os.path.join(prefix, "lib", "pkgconfig")
    if os.path.isdir(pkgconfig_dir):
        existing = env["ENV"].get("PKG_CONFIG_PATH")
        env["ENV"]["PKG_CONFIG_PATH"] = (
            pkgconfig_dir if not existing else pkgconfig_dir + os.pathsep + existing
        )


if system == "Darwin":
    env.Replace(CXX="clang++")
    # env.Append(CXXFLAGS=["-stdlib=libc++"], LINKFLAGS=["-stdlib=libc++"])

    # Support both package managers rather than assuming MacPorts: the GitHub
    # Actions runners have Homebrew and no /opt/local at all.
    if os.path.isdir("/opt/local"):
        add_prefix(env, "/opt/local")
    homebrew = brew_prefix()
    if homebrew:
        add_prefix(env, homebrew)


def boost_version_key(path):
    """Sort key for a versioned boost directory name, e.g. '1.87' -> (1, 87)."""
    try:
        return tuple(int(part) for part in os.path.basename(path).split("."))
    except ValueError:
        return ()


# Boost has no pkg-config metadata, so locate it by convention. BOOST_ROOT
# overrides everything; otherwise MacPorts keeps versioned boost ports under
# libexec, and we take the newest rather than pinning to one release. Homebrew
# keeps its own prefix per formula.
boost_root = os.environ.get("BOOST_ROOT")
if boost_root is None and system == "Darwin":
    candidates = sorted(glob.glob("/opt/local/libexec/boost/*"), key=boost_version_key)
    boost_root = candidates[-1] if candidates else brew_prefix("boost")
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

if sanitize == "realtime":
    # RealtimeSanitizer is clang-only, and it only checks functions carrying
    # clang's nonblocking attribute -- which is what JILL_RT expands to. A gcc
    # build silently drops the attribute, so without this check the build would
    # succeed, run, and verify nothing at all.
    cxx = env.subst("$CXX")
    is_clang = False
    try:
        probe = subprocess.run([cxx, "--version"], capture_output=True, text=True)
        is_clang = "clang" in probe.stdout.lower()
    except OSError:
        pass
    if not is_clang:
        if "CXX" in os.environ:
            print("error: sanitize=realtime needs clang, but CXX is set to '%s'." % cxx)
            print("       RealtimeSanitizer is a clang feature and JILL_RT expands to")
            print("       nothing on other compilers, so the build would check nothing.")
            print("       Unset CXX to let the build pick clang++, or set CXX=clang++.")
            Exit(1)
        print("sanitize=realtime: switching to clang (RealtimeSanitizer is clang-only)")
        # jmonitor is C, so the C compiler has to move too or the sanitizer
        # flag reaches a gcc that does not know it
        env.Replace(CXX="clang++", CC="clang")

if sanitize:
    # -fno-omit-frame-pointer keeps the reports readable, and the flag has to
    # reach the linker as well as the compiler. Worth combining with debug=1:
    # a sanitizer build with NDEBUG set has all its assertions compiled out.
    env.Append(
        CCFLAGS=["-fsanitize=%s" % sanitize, "-fno-omit-frame-pointer", "-g"],
        LINKFLAGS=["-fsanitize=%s" % sanitize],
    )


def require_pkgconfig(env, *packages):
    """Add cflags/libs for each package, failing early with a clear message."""
    for pkg in packages:
        # run the probe with the same environment ParseConfig will use, so a
        # PKG_CONFIG_PATH added for Homebrew or MacPorts is honoured by both
        if subprocess.call(["pkg-config", "--exists", pkg], env=env["ENV"]) != 0:
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
