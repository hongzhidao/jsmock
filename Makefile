CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2 -D_GNU_SOURCE -Ideps/quickjs -Isrc
LDFLAGS = -Ldeps/quickjs -lpthread -lm
LIBS    = deps/quickjs/libquickjs.a

SRCDIR  = src
BUILDDIR = build

SRCS    = js_main.c js_epoll.c js_timer.c js_engine.c js_thread.c \
          js_conn.c js_http.c js_route.c js_store.c js_qjs.c js_web.c \
          js_tls.c js_runtime.c
OBJS    = $(patsubst %.c,$(BUILDDIR)/%.o,$(SRCS))
TARGET  = jsmock

all: $(LIBS) $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/js_main.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(LIBS):
	$(MAKE) -C deps/quickjs libquickjs.a

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
