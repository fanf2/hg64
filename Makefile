# Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
# You may do anything with this. It has no warranty.
# <https://creativecommons.org/publicdomain/zero/1.0/>
# SPDX-License-Identifier: CC0-1.0

KEYBITS = 12

#llvm	= /opt/homebrew/opt/llvm
#CC	= $(llvm)/bin/clang
#CFLAGS	= -g -O3 -std=c18 -Wall -Wextra -Werror -DKEYBITS=$(KEYBITS)
#LDFLAGS = -g -L$(llvm)/lib -Wl,-rpath,$(llvm)/lib -fsanitize=undefined,address

all: test

clean:
	rm -f *.o test

test: test.o hg64.o random.o
test.o: test.c hg64.h random.h
hg64.o: hg64.c hg64.h
random.o: random.c random.h
