dnl $Id$
dnl config.m4 for extension drizzle

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(drizzle, for drizzle support,
Make sure that the comment is aligned:
[  --with-drizzle             Include drizzle support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(drizzle, whether to enable drizzle support,
dnl Make sure that the comment is aligned:
dnl [  --enable-drizzle           Enable drizzle support])

if test "$PHP_DRIZZLE" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-drizzle -> check with-path
  SEARCH_PATH="/usr/local /usr"     # you might want to change this
  SEARCH_FOR="/include/libdrizzle-5.1/drizzle.h"  # you most likely want to change this
  if test -r $PHP_DRIZZLE/$SEARCH_FOR; then # path given as parameter
    DRIZZLE_DIR=$PHP_DRIZZLE
  else # search default path list
    AC_MSG_CHECKING([for drizzle files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        DRIZZLE_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi
  
  if test -z "$DRIZZLE_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the drizzle distribution])
  fi

  dnl # --with-drizzle -> add include path
  PHP_ADD_INCLUDE($DRIZZLE_DIR/include)

  dnl # --with-drizzle -> check for lib and symbol presence
  LIBNAME=drizzle
  LIBSYMBOL=drizzle_create
  LIBS="$LIBS -lpthread"
  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $DRIZZLE_DIR/lib, DRIZZLE_SHARED_LIBADD)
    AC_DEFINE(HAVE_DRIZZLELIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong drizzle lib version or lib not found])
  ],[
    $DRIZZLE_SHARED_LIBADD -lpthread
  ])
  dnl
  PHP_SUBST(DRIZZLE_SHARED_LIBADD)
  PHP_ADD_LIBRARY(stdc++, "", EXTERN_NAME_LIBADD)
  PHP_REQUIRE_CXX
  PHP_NEW_EXTENSION(drizzle, drizzle.cc, $ext_shared)
fi
