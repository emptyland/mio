CC=gcc
CXX=g++
LINKER=ld
CFLAGS=-I../third-party/include -g -O0 -Wall -Wextra
CXXFLAGS=-I../third-party/include -g -O0 -Wall -Wextra

OBJS=object-test.o

all-test: $(OBJS)
	$(CXX) $(OBJS) -lpthread -L../third-party/libs -lglog -lgtest -lgtest_main -o all-test

sexpr_objs: $(OBJS)

.PHONY: clean

clean:
	rm -f $(OBJS) test


