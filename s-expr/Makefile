CC=gcc
CXX=g++
LINKER=ld
CFLAGS=-I../third-party/include -g -O0 -Wall -Wextra
CXXFLAGS=-std=c++11 -I../third-party/include -g -O0 -Wall -Wextra

OBJS=object-test.o object_builder.o object.o inline_op.o

all-test: $(OBJS)
	$(CXX) $(OBJS) -lpthread -L../third-party/libs -lglog -lgtest -lgtest_main -o all-test

sexpr_objs: $(OBJS)

.PHONY: clean

clean:
	rm -f $(OBJS) all-test


