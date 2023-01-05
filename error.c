/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "hg64.h"

static void
dump_csv(hg64 *hg, uint64_t range_min, uint64_t range_max) {
	uint64_t pmin, pmax, err;
	unsigned minerr_key = 0, maxerr_key = 0, maxzeroerr_key = 0, key_count = 0;
	double perc, max_perc = -1, min_perc = 101;
	printf("key,pmin,pmax,error,error_percent\n");
	for(unsigned key = 0;
	    hg64_get(hg, key, &pmin, &pmax, NULL);
	    key++) {
		if (pmin < range_min)
			continue;
		if (pmax > range_max)
			continue;
		key_count++;
		err = pmax - pmin;
		if (err == 0) {
			maxzeroerr_key = key;
			perc = 0;
		} else {
			perc = (double)err * 100 / pmin;
			if (perc > max_perc) {
				max_perc = perc;
				maxerr_key = key;
			}
			if (perc < min_perc) {
				min_perc = perc;
				minerr_key = key;
			}
		}
		printf("%u,%"PRIu64",%"PRIu64",%"PRIu64",%.02f\n", key, pmin, pmax, err, perc);
	}

	printf("%d sigbits: %u keys within range (%"PRIu64" - %"PRIu64")\n", hg64_sigbits(hg), key_count, range_min, range_max);

        hg64_get(hg, maxzeroerr_key, &pmin, NULL, NULL);
	if (pmin >= range_min)
		printf("last value with 0 error: %"PRIu64", key %u\n", pmin, maxzeroerr_key);

        hg64_get(hg, maxerr_key, &pmin, &pmax, NULL);
	printf("min error for non-precise bucket: %0.2f %% (range %"PRIu64" - %"PRIu64", key %u)\n", min_perc, pmin, pmax, minerr_key);

        hg64_get(hg, maxerr_key, &pmin, &pmax, NULL);
	printf("max error: %0.2f %% (range %"PRIu64" - %"PRIu64", key %u)\n", max_perc, pmin, pmax, maxerr_key);
}

void usage(const char *prog) {
	printf("explore bucketization in hg64 for given number of significant"
		" bits, and optional range of expected values\n");
	printf("usage: %s sigbits [min] [max]\n", prog);
	exit(1);
}

int main(int argc, char **argv) {
	if (argc < 2 || argc > 4) {
		usage(argv[0]);
	}

	unsigned sigbits;
	uint64_t pmin = 0, pmax = UINT64_MAX;
	if (sscanf(argv[1], "%u", &sigbits) != 1 || sigbits < 1 || sigbits > 15) {
		usage(argv[0]);
	}
	if (argc >= 3) {
		if (sscanf(argv[2], "%"PRIu64, &pmin) != 1) {
			usage(argv[0]);
		}
	}
	if (argc >= 4) {
		if (sscanf(argv[3], "%"PRIu64, &pmax) != 1) {
			usage(argv[0]);
		}
	}
	if (pmin >= pmax)
		usage(argv[0]);

	struct hg64 *hg;
	hg = hg64_create(sigbits);
	dump_csv(hg, pmin, pmax);
}
