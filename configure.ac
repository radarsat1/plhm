AC_PREREQ(2.63)

AC_INIT([plhm],[0.2],[sinclair@music.mcgill.ca])

# libtool version: current:revision:age
#
# If the library source code has changed at all since the last update, then
# increment revision (`c:r:a' becomes `c:r+1:a').
#
# If any interfaces have been added, removed, or changed since the last update,
# increment current, and set revision to 0.
#
# If any interfaces have been added since the last public release, then
# increment age.
#
# If any interfaces have been removed since the last public release, then set
# age to 0.
SO_VERSION=1:0:0

AC_CONFIG_SRCDIR([src/plhm.c])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_ENABLE_STATIC(no)
AC_ENABLE_SHARED(yes)
AC_SUBST(SO_VERSION)

MAJOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f1)
AC_SUBST(MAJOR_VERSION)

AM_SANITY_CHECK

# Checks for programs.
AC_PROG_CC
LT_INIT
AM_PROG_CC_C_O
AC_CHECK_PROG([DOXYGEN], [doxygen], [doc], [])
AC_SUBST(DOXYGEN)
PKG_PROG_PKG_CONFIG

# Check for liblo
AC_ARG_WITH([liblo],
  AS_HELP_STRING([--without-liblo],[compile without liblo, disable OSC]))
AS_IF([test x$with_liblo != xno],[
  PKG_CHECK_MODULES([liblo], [liblo >= 0.26])])
AS_IF([test "x$liblo_LIBS" = x],
  [with_liblo=no])
AS_IF([test x$with_liblo != xno],[
  AC_DEFINE([HAVE_LIBLO],[],[Define to enable Open Sound Control support with liblo])
  AC_SUBST(LIBLO,liblo)
])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/time.h unistd.h termios.h fcntl.h errno.h sys/stat.h \
                  getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Check for features
AC_CHECK_FUNC([select], [AC_DEFINE(HAVE_SELECT, [1], [Define to 1 if select() is available.])], [])
AC_CHECK_FUNC([poll], [AC_DEFINE(HAVE_POLL, [1], [Define to 1 if poll() is available.])])

AC_C_BIGENDIAN([LO_BIGENDIAN="1"], [LO_BIGENDIAN="0"])
AC_DEFINE_UNQUOTED(LO_BIGENDIAN, "$LO_BIGENDIAN", [If machine is bigendian])
AC_SUBST(LO_BIGENDIAN)

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    include/Makefile
    plhm.pc
])
AC_OUTPUT
