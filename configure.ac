AC_INIT([msgbus], [0.1], [dugsong@monkey.org])
AC_CONFIG_SRCDIR(src/msgbus.c)
AC_PREREQ(2.59)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL
AC_CANONICAL_HOST
AC_REPLACE_FUNCS(strlcat strlcpy)

case "$host" in
  *linux*)
    OS="linux"
    AC_DEFINE(OS_LINUX, 1, [compiling under linux])
    ;;
  *cygwin* | *mingw*)
    OS="win32"
    CFLAGS="$CFLAGS -mno-cygwin"
    LDFLAGS="$LDFLAGS --strip-all"
    AC_DEFINE(OS_WIN32, 1, [compiling under windows])
    AC_DEFINE(WIN32_LEAN_AND_MEAN, 1, [fast code generation])
    ;;
  *darwin* | *macos10*)
    OS="bsd"
    if test -d /opt/local ; then
      PATH="/opt/local/bin:$PATH"
      CFLAGS="$CFLAGS -I/opt/local/include"
      LDFLAGS="$LDFLAGS -L/opt/local/lib"
    elif test -d /sw ; then
      PATH="/sw/bin:$PATH"
      CFLAGS="$CFLAGS -I/sw/include"
      LDFLAGS="$LDFLAGS -L/sw/lib"
    fi
    AC_DEFINE(OS_BSD, 1, [compiling under bsd])
    AC_DEFINE(BIND_8_COMPAT, 1, [enable bind8 compatibility])
    ;;
  *bsd*)
    OS="bsd"
    AC_DEFINE(OS_BSD, 1, [compiling under bsd])
    ;;
  *)
    OS="bsd"
    AC_DEFINE(OS_BSD, 1, [compiling under unknown, assuming BSD])
    ;;
esac

AC_SUBST(OS)

AC_OUTPUT([Makefile src/Makefile python/Makefile])
