gcc -pthread -L/usr/X11R6/lib -lm                                        \
`pkg-config --libs glib-2.0 gthread-2.0 hildon-1 gupnp-1.0 gupnp-av-1.0` \
-O0 -g -Wall -I/usr/include -I/usr/X11R6/include                         \  
`pkg-config --cflags glib-2.0 gconf-2.0 gthread-2.0 hildon-1 gupnp-1.0 gupnp-av-1.0` \
-o devious devious.c
