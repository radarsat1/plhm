
lib_LTLIBRARIES = libplhm-@MAJOR_VERSION@.la
libplhm_@MAJOR_VERSION@_la_CFLAGS = -Wall -I$(top_srcdir)/include $(liblo_CFLAGS)
libplhm_@MAJOR_VERSION@_la_SOURCES = libplhm.c
libplhm_@MAJOR_VERSION@_la_LDFLAGS = -export-dynamic -version-info @SO_VERSION@

bin_PROGRAMS = plhm
plhm_CFLAGS = -Wall -I$(top_srcdir)/include $(liblo_CFLAGS)
plhm_SOURCES = plhm.c
plhm_LDADD = libplhm-@MAJOR_VERSION@.la $(liblo_LIBS)
