
# Automake-style multiple target wrapper for BSD make.
#
# this should be mostly backwards-compatible with
# bsd.{lib,prog}.mk... i think. ;-)
# 
# PROGS = foo bar
# LIBS  = baz
# foo_SRCS = foo.c
# bar_SRCS = bar.c
# baz_SRCS = baz.c
# SRCS = common.c
# foo_LDADD = -lquux
# LDADD = -lcommon

CFLAGS		?= -g -O2 -Wall

.if defined(LIB)

.for t in lib ${LIB}
.  if defined(${t}_CFLAGS)
_CFLAGS		:= ${${t}_CFLAGS}
CFLAGS		+= ${_CFLAGS}
.  endif
.  if defined(${t}_SRCS)
_SRCS		:= ${${t}_SRCS}
SRCS		+= ${_SRCS}
.  endif
.  if defined(${t}_LDFLAGS)
_LDFLAGS	:= ${${t}_LDFLAGS}
LDFLAGS		+= ${_LDFLAGS}
.  endif
.endfor
.include <bsd.lib.mk>

.elif defined(PROG)

.for t in prog ${PROG}
.  if defined(${t}_CFLAGS)
_CFLAGS		:= ${${t}_CFLAGS}
CFLAGS		+= ${_CFLAGS}
.  endif
.  if defined(${t}_SRCS)
_SRCS		:= ${${t}_SRCS}
SRCS		+= ${_SRCS}
.  endif
.  if defined(${t}_LDADD)
_LDADD		:= ${${t}_LDADD}
LDADD		+= ${_LDADD}
.  endif
.  if defined(${t}_DPADD)
_DPADD		:= ${${t}_DPADD}
DPADD		+= ${_DPADD}
.  endif
.endfor
.include <bsd.prog.mk>

.elif defined(SUBDIR)

# XXX - gross recursion hack
all clean cleandir depend test pylint:
	@cd ${SUBDIR} && env -i `env | egrep -v 'SUBDIR| '` ${.MAKE} $@

.else

### all ${.TARGETS}:
# XXX - only recurse certain targets
all clean depend:
.for var in LIBS PROGS SUBDIRS
.  for val in ${${var}}
	${MAKE} ${var:S/S$//}=${val} $@
.  endfor
.endfor

.endif
