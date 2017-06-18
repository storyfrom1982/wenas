bin_PROGRAMS = pipe malloc memory_leak aligned_malloc


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