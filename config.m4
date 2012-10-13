dnl $Id: config.m4 305831 2010-11-29 13:30:49Z martynas $
dnl config.m4 for extension htscanner

PHP_ARG_ENABLE(htscanner, whether to enable htscanner support,
[  --enable-htscanner           Enable htscanner support])

AC_ARG_WITH(htscanner-debug,
[  --with-htscanner-debug       Include htscanner debug code],[
  htscanner_debug=$withval
],[
  htscanner_debug=no
])

if test "$PHP_HTSCANNER" != "no"; then
  PHP_NEW_EXTENSION(htscanner, htscanner.c, $ext_shared)

  if test "$htscanner_debug" = "yes"; then
    AC_DEFINE(HTSCANNER_DEBUG, 1, [Define if you like to include htscanner debug code])
  fi
fi
