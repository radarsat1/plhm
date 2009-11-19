SUBDIRS = src include # @DOXYGEN@

EXTRA_DIST = libtool ltmain.sh autogen.sh plhm.pc.in

plhm-@MAJOR_VERSION@.pc: plhm.pc
	@if ! [ -e $@ ]; then ln -v $< $@; fi

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = plhm-@MAJOR_VERSION@.pc
