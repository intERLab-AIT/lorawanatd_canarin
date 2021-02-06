AC_INIT([lorawanatd], 1.0, [raunak.mukhia@gmail.com])
AC_CONFIG_AUX_DIR([build_aux])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC_STDC
AC_CONFIG_HEADERS([config.h])

PKG_CHECK_MODULES(EVENTCORE, libevent_core)

AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_OUTPUT
