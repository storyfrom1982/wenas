bin_PROGRAMS = queue pipe malloc memory_leak media aligned_malloc


queue_SOURCES=queue.c
queue_LDADD=$(top_srcdir)/src/libsr.la
queue_CFLAGS=-I$(top_srcdir)/include -std=gnu99 -g
queue_LDFLAGS=-rdynamic


pipe_SOURCES=pipe.c
pipe_LDADD=$(top_srcdir)/src/libsr.la
pipe_CFLAGS=-I$(top_srcdir)/include -std=gnu99 -g
pipe_LDFLAGS=-rdynamic


malloc_SOURCES=malloc.c
malloc_LDADD=$(top_srcdir)/src/libsr.la
malloc_CFLAGS=-I$(top_srcdir)/include -std=gnu99 -g
malloc_LDFLAGS=-rdynamic


aligned_malloc_SOURCES=aligned_malloc.c
aligned_malloc_LDADD=$(top_srcdir)/src/libsr.la
aligned_malloc_CFLAGS=-I$(top_srcdir)/include -std=gnu99 -g
aligned_malloc_LDFLAGS=-rdynamic


memory_leak_SOURCES=memory_leak.c
memory_leak_LDADD=$(top_srcdir)/src/libsr.la
memory_leak_CFLAGS=-I$(top_srcdir)/include -std=gnu99 -g
memory_leak_LDFLAGS=-rdynamic


media_SOURCES=media.c
media_LDADD=$(top_srcdir)/src/libsr.la
media_CFLAGS=-I$(top_srcdir)/include -std=gnu99 -g
media_LDFLAGS=-rdynamic
