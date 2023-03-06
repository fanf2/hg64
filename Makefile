# Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
#
# Copyright (C) Internet Systems Consortium, Inc. ("ISC")
#
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0.  If a copy of the MPL was not distributed with this
# file, you can obtain one at https://mozilla.org/MPL/2.0/.

#llvm	= /opt/homebrew/opt/llvm
#CC	= $(llvm)/bin/clang
#LDFLAGS = -L$(llvm)/lib -Wl,-rpath,$(llvm)/lib
CFLAGS	= -g -O2 -Wall -Wextra #-fsanitize=undefined,address

LIBS = -lm -lpthread
OBJS = test.o hg64.o random.o

all: test sigs

clean:
	rm -f $(OBJS) test

test: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

sigs: sigs.c
test.o: test.c hg64.h random.h
hg64.o: hg64.c hg64.h
random.o: random.c random.h
