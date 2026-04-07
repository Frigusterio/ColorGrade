all:cg

cg: cg.c
	gcc -o cg cg.c -lz -lm