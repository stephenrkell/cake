default: cli-test.so

cli-test.so: cli-test.c
	gcc -shared -o cli-test.so cli-test.c -ldl -lc

concrete: concrete-component.o concrete-main.o concrete-helper.o
	cc -o "$@" $+

abstracted-component.o: concrete-component.o lens-link.o
	ld -r -o "$@" $+ --wrap concrete_outcall

abstract: abstracted-component.o abstract-main.o abstract-helper.o
	cc -o "$@" $+

clean:
	rm -f *.o concrete abstract 
