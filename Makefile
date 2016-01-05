OBJS=driven.o lex.yy.o grammar.o
CPP=clang++
CC=clang
CPPFLAGS=-c -o

main: $(OBJS)
	$(CPP) $(OBJS) -o main

driven.o: driven.cc parser.h
	$(CPP) driven.cc $(CPPFLAGS) driven.o 

lex.yy.o: lex.yy.c
	$(CC) lex.yy.c $(CPPFLAGS) lex.yy.o 

grammar.o: grammar.cc grammar.hh
	$(CPP) grammar.cc $(CPPFLAGS) grammar.o 

lex.yy.c lex.yy.h: grammar.l grammar.hh
	lex --verbose --header-file=lex.yy.h grammar.l 

grammar.cc grammar.hh: grammar.y
	yacc --verbose --debug -d grammar.y -o grammar.cc

.PHONY : clean
clean:
	rm main $(OBJS) lex.yy.{c,h} grammar.cc grammar.hh grammar.output > /dev/null
