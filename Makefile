all:
	g++ -I./include/ main.cpp IQ.cpp ISU.cpp -g

run: all
	./a.out

gdb: all
	gdb ./a.out

clean:
	rm -rf a.out log

.PHONY:all run gdb

