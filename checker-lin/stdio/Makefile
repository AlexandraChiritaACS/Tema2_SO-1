build: so_stdio.o
	gcc -Wall -shared so_stdio.o -o libso_stdio.so
so_stdio.o: so_stdio.c
	gcc -fPIC -c so_stdio.c
clean:
	rm -rf *.o
	rm -rf *.so
