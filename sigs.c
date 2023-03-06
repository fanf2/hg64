/*
 * hg64 - 64-bit histograms
 *
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 *
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>

static double
convert_sigs(double sigs, uint from_base, uint to_base) {
	if(from_base < 2 || to_base < 2 || sigs < 1.0) {
		return(nan(""));
	} else {
		double factor = log(from_base) / log(to_base);
		return(1.0 - (1.0 - sigs) * factor);
	}
}

int
main(void) {
	printf("conversion tables between significant digits and bits\n");
	printf("\n%8s%8s%8s%8s\n", "digits", "bits", "floor", "ceil");
	for(uint sigs = 1; sigs < 8; sigs++) {
		double exact = convert_sigs(sigs, 10, 2);
		printf("%8u%8.2f%8u%8u\n",
		       sigs, exact, (uint)floor(exact), (uint)ceil(exact));
	}
	printf("\n%8s%8s%8s%8s\n", "bits", "digits", "floor", "ceil");
	for(uint sigs = 1; sigs < 20; sigs++) {
		double exact = convert_sigs(sigs, 2, 10);
		printf("%8u%8.2f%8u%8u\n",
		       sigs, exact, (uint)floor(exact), (uint)ceil(exact));
	}
}
