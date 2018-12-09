lib_LTLIBRARIES=libsr.la

libsr_la_SOURCES= \
        sr_lib.c \
        sr_malloc.c


libsr_la_CPPFLAGS= \
        -I$(top_srcdir)/include

libsr_la_LIBADD= \
        -lpthread

libsr_la_CFLAGS= \
		-g \
        -std=gnu99

libsr_la_LDFLAGS= \
		-rdynamic
