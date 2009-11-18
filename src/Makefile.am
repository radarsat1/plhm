
lib_LTLIBRARIES = libplhm-1.0.la
libplhm_1_0_la_CFLAGS = -Wall -I$(top_srcdir)/include $(liblo_CFLAGS)
libplhm_1_0_la_SOURCES = plhm.c OSC-client.c OSC-timetag.c

bin_PROGRAMS = libertyd
libertyd_CFLAGS = -Wall -I$(top_srcdir)/include $(liblo_CFLAGS)
libertyd_SOURCES = libertyd.c
libertyd_LDADD = libplhm-1.0.la $(liblo_LIBS)
