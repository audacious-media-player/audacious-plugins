
all : libcdaudio-ng.so

libcdaudio-ng.so :
	gcc -shared -fPIC -o libcdaudio-ng.so cdaudio-ng.c \
		$(shell pkg-config --libs --cflags libcdio) \
		$(shell pkg-config --libs --cflags libcdio_paranoia) \
		$(shell pkg-config --libs --cflags libcdio_cdda) \
		$(shell pkg-config --libs --cflags glib-2.0) \
		$(shell pkg-config --libs --cflags gtk+-2.0)

clean :
	rm *.so
