OBJS=driven.o lex.yy.o grammar.o
CPP=clang++
CPPFLAGS=-c -o

main: $(OBJS)
	$(CPP) $(OBJS) -o main

driven.o: driven.cc
	$(CPP) driven.cc $(CPPFLAGS) driven.o 

lex.yy.o: lex.yy.cc
	$(CPP) lex.yy.cc $(CPPFLAGS) lex.yy.o 

grammar.o: grammar.cc grammar.hh
	$(CPP) grammar.cc $(CPPFLAGS) grammar.o 

lex.yy.cc: grammar.l grammar.hh
	lex --c++ grammar.l 

grammar.cc grammar.hh: grammar.y
	yacc --verbose --debug -d grammar.y -o grammar.cc

.PHONY : clean
clean:
	rm main $(OBJS) lex.yy.cc grammar.cc grammar.hh grammar.output > /dev/null
