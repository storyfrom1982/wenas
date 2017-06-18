lib_LTLIBRARIES=libsr.la

libsr_la_SOURCES= \
        sr_log.c \
        sr_common.c \
        sr_pipe.c \
        sr_queue.c \
        sr_mutex.c \
        sr_malloc.c \
		sr_message.c


libsr_la_CPPFLAGS= \
        -I$(top_srcdir)/include

libsr_la_LIBADD= \
        -lpthread

libsr_la_CFLAGS= \
		-g \
        -std=gnu99

libsr_la_LDFLAGS= \
		-rdynamic
