llvm	= /opt/homebrew/opt/llvm
CC	= $(llvm)/bin/clang
CFLAGS	= -g -O2 -std=c18 -Wall -Wextra -Werror -fsanitize=undefined,address
LDFLAGS	= -g -L$(llvm)/lib -Wl,-rpath,$(llvm)/lib -fsanitize=undefined,address

test: test.o histobag.o random.o
test.o: test.c histobag.h random.h
histobag.o: histobag.c histobag.h
random.o: random.c random.h

clean:
	rm -f *.o test
