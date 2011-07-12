all: tcapi fastbit

# bmgen
tcapi: 
	gcc -g -O2 -c -o tcapi.o tcapi.c -L/usr/local/lib/ /usr/local/lib/libfastbit.la
	libtool --tag=CC --mode=link gcc -R/usr/local/lib/ -g -O2  -o tcapi tcapi.o /usr/local/lib/libfastbit.la 

fastbit: fastbit.c
	gcc -g -O2 -c -o fastbit.o fastbit.c -L/usr/local/lib/ /usr/local/lib/libfastbit.la -g `xml2-config --cflags`
	libtool --tag=CC --mode=link gcc -R/usr/local/lib/ -g -O2  -o fastbit fastbit.o /usr/local/lib/libfastbit.la -lcommlbr `xml2-config --libs`

clean:
	rm -f fastbit *.o
