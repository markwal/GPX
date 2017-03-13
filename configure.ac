#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gpx], [2.5.3], [https://github.com/markwal/GPX/issues],,[https://github.com/markwal/GPX])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE([disable])
AM_EXTRA_RECURSIVE_TARGETS([test])
AC_CONFIG_SRCDIR([src/gpx/gpx.c])
AC_CONFIG_HEADERS([src/shared/config.h])

# Checks for cross compiling and arbitrary platform specifics
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_SUBST(PLATFORM, [`$ac_aux_dir/archive-name.sh $host_os $host_cpu`])
case $PLATFORM in
    win*)
        BDIST_TARGET="bdist-zip"
        CFLAGS="$CFLAGS -pthread"
        LDFLAGS="$LDFLAGS -static -static-libgcc -lpthread"
        ;;
    osx)
        BDIST_TARGET="bdist-dmg"
        ;;
    *)
        BDIST_TARGET="bdist-gzip"
        ;;
esac
AC_SUBST(BDIST_TARGET)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_CHECK_PROG([SIGNOSX], [sign-osx.sh], [sign-osx.sh])
AM_CONDITIONAL([HAVE_SIGNOSX], [test -n "$SIGNOSX"])
AC_CHECK_PROG([CREATEDMG], [hdiutil], [hdiutil create -format UDZO -srcfolder])
AM_CONDITIONAL([HAVE_CREATEDMG], [test -n "$CREATEDMG"])
AC_CHECK_PROG([PYTHON], [python], [python])
AM_CONDITIONAL([HAVE_PYTHON], [test -n "$PYTHON"])
AC_CHECK_PROG([DIFF], [diff], [diff -b])
AM_CONDITIONAL([HAVE_DIFF], [test -n "$DIFF"])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h inttypes.h limits.h stdint.h stdlib.h string.h unistd.h poll.h])
AC_CHECK_HEADERS([windows.h], [HAVE_WINDOWS_H=yes])
AM_CONDITIONAL([HAVE_WINDOWS_H], [test -n "$HAVE_WINDOWS_H"])
AM_CONDITIONAL([CROSS_COMPILING], [test "$cross_compiling" != no]) 

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit memmove memset select sqrt strcasecmp strchr strdup strerror strrchr strtol nanosleep posix_openpt grantpt unlockpt])

AC_CONFIG_FILES([Makefile
                 src/gpx/Makefile
                 src/utils/Makefile])
AC_OUTPUT
