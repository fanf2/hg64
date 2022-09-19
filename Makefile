# Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
#
# Permission is hereby granted to use, copy, modify, and/or
# distribute this software for any purpose with or without fee.
#
# This software is provided 'as is', without warranty of any kind.
# In no event shall the authors be liable for any damages arising
# from the use of this software.
#
# SPDX-License-Identifier: 0BSD OR MIT-0

KEYBITS = 12

#llvm	= /opt/homebrew/opt/llvm
#CC	= $(llvm)/bin/clang
#CFLAGS	= -g -O3 -std=c18 -Wall -Wextra -DKEYBITS=$(KEYBITS) #-fsanitize=undefined,address
#LDFLAGS = -g -L$(llvm)/lib -Wl,-rpath,$(llvm)/lib -fsanitize=undefined,address

all: test

clean:
	rm -f *.o test

test: test.o hg64.o random.o
test.o: test.c hg64.h random.h
hg64.o: hg64.c hg64.h
random.o: random.c random.h
