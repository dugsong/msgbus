AC_INIT([msgbus], [0.1], [msgbus@googlegroups.com])
AC_CONFIG_SRCDIR(src/msgbus.c)
AC_PREREQ(2.59)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL
AM_PATH_PYTHON
AM_CONDITIONAL(HAVE_PYTHON, [test "x$PYTHON" != "x"])
AC_REPLACE_FUNCS(strlcat strlcpy)

# Checks for libevent
AC_MSG_CHECKING(for libevent)
AC_ARG_WITH(libevent,
[  --with-libevent=DIR     use libevent in DIR],
[ case "$withval" in
  yes|no)
     AC_MSG_RESULT(no)
     ;;
  *)
     AC_MSG_RESULT($withval)
     if test -f $withval/include/event.h -a -f $withval/lib/libevent.a; then
        owd=`pwd`
        if cd $withval; then withval=`pwd`; cd $owd; fi
	EVENTINC="-I$withval/include"
	EVENTLIB="-L$withval/lib -levent"
     elif test -f $withval/event.h -a -f $withval/libevent.a; then
        owd=`pwd`
        if cd $withval; then withval=`pwd`; cd $owd; fi
	EVENTINC="-I$withval"
	EVENTLIB="-L$withval -levent"
     else
        AC_ERROR(event.h or libevent.a not found in $withval)
     fi
     ;;
  esac ],
[ if test -f /usr/include/event.h -a -f /usr/lib/libevent.a; then
     EVENTLIB="-levent"
  elif test -f ${prefix}/include/event.h -a -f ${prefix}/lib/libevent.a; then
     EVENTINC="-I${prefix}/include"
     EVENTLIB="-L${prefix}/lib -levent"
  else
     AC_MSG_RESULT(no)
     AC_ERROR(libevent not found)
  fi
  AC_MSG_RESULT(yes) ]
)
AC_SUBST(EVENTINC)
AC_SUBST(EVENTLIB)

AC_OUTPUT([Makefile chatdemo/Makefile src/Makefile python/Makefile python/setup.py])
