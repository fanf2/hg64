llvm	= /opt/homebrew/opt/llvm
CC	= $(llvm)/bin/clang
CFLAGS	= -g -O2 -std=c18 -Wall -Wextra -Werror -fsanitize=undefined,address
LDFLAGS	= -g -L$(llvm)/lib -Wl,-rpath,$(llvm)/lib -fsanitize=undefined,address

test: test.o hg64.o random.o
test.o: test.c hg64.h random.h
hg64.o: hg64.c hg64.h
random.o: random.c random.h

clean:
	rm -f *.o test
