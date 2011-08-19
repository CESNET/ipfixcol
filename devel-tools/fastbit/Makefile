all: tcapi fastbit.so

# bmgen
fastbit.so: fastbit.c
	gcc -g -O2 -c -o fastbit.o fastbit.c -L/usr/local/lib/ /usr/local/lib/libfastbit.la -g `xml2-config --cflags` -fPIC
	libtool --tag=CC --mode=link gcc -shared  -R/usr/local/lib/ -g -O2 fastbit.o /usr/local/lib/libfastbit.la -lcommlbr `xml2-config --libs` -Wl,-soname -Wl,fastbit.so.1 -o $@ 

clean:
	rm -f fastbit *.o
