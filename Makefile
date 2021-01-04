all: ssu_shell ttop pps

ssu_shell: ssu_shell.o
	gcc ssu_shell.o -o ssu_shell
ssu_shell.o: ssu_shell.c
	gcc -c ssu_shell.c -o ssu_shell.o


pps: pps.o
	gcc pps.o -o pps
pps.o: pps.c
	gcc -c pps.c -o pps.o

ttop: ttop.o
	gcc ttop.o -o ttop
ttop.o: ttop.c
	gcc -c ttop.c -o ttop.o

clean:
	rm *.o core ssu_shell pps ttop
