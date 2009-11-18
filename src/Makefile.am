
lib_LTLIBRARIES = libplhm-1.0.la
libplhm_1_0_la_CFLAGS = -Wall -I$(top_srcdir)/include $(liblo_CFLAGS)
libplhm_1_0_la_SOURCES = libplhm.c OSC-client.c OSC-timetag.c

bin_PROGRAMS = plhm
plhm_CFLAGS = -Wall -I$(top_srcdir)/include $(liblo_CFLAGS)
plhm_SOURCES = plhm.c
plhm_LDADD = libplhm-1.0.la $(liblo_LIBS)
