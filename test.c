/*
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
#include "random.h"

extern void hg64_validate(void);

#ifndef SIGBITS
#define SIGBITS 5
#endif

#ifndef THREADS
#define THREADS 9
#endif

#ifndef SAMPLES
#define SAMPLES (1000*1000)
#endif

#ifndef RANGE
#define RANGE (1000*1000*1000)
#endif

static uint64_t data[THREADS][SAMPLES];

#define NS_PER_S (1000*1000*1000)
#define NS_PER_MS (1000*1000)

static uint64_t
nanotime(void) {
	struct timespec tv;
	assert(clock_gettime(CLOCK_MONOTONIC, &tv) == 0);
	return((uint64_t)tv.tv_sec * NS_PER_S + (uint64_t)tv.tv_nsec);
}


static int
compare(const void *ap, const void *bp) {
	uint64_t a = *(const uint64_t *)ap;
	uint64_t b = *(const uint64_t *)bp;
	return(a < b ? -1 : a > b ? +1 : 0);
}

static void
summarize(hg64 *hg) {
	uint64_t count = 0;
	uint64_t max = 0;
	uint64_t population = 0;
	for(unsigned key = 0; hg64_get(hg, key, NULL, NULL, &count); key++) {
		max = (max > count) ? max : count;
		population += count;
	}
	printf("%u sigbits\n", hg64_sigbits(hg));
	printf("%zu bytes\n", hg64_size(hg));
	printf("%zu largest\n", (size_t)max);
	printf("%zu samples\n", (size_t)population);
	double mean, var;
	hg64_mean_variance(hg, &mean, &var);
	printf("mean %f +/- %f\n", mean, sqrt(var));
}

struct thread {
	hg64 *hg;
	uint64_t ns;
	uint64_t *data;
	pthread_t tid;
};

static void *
load_data(void *varg) {
	struct thread *arg = varg;
	uint64_t t0 = nanotime();
	for(size_t i = 0; i < SAMPLES; i++) {
		hg64_add(arg->hg, arg->data[i], 1);
	}
	uint64_t t1 = nanotime();
	arg->ns = t1 - t0;
	return(NULL);
}

static void
parallel_load(hg64 *hg, unsigned threads) {
	struct thread thread[THREADS];
	for(unsigned t = 0; t < threads; t++) {
		struct thread *tt = &thread[t];
		*tt = (struct thread){
			.hg = hg,
			.data = data[t],
		};
		assert(pthread_create(&tt->tid, NULL, load_data, tt) == 0);
	}
	double total = 0;
	for(unsigned t = 0; t < threads; t++) {
		assert(pthread_join(thread[t].tid, NULL) == 0);
		double ns = thread[t].ns;
		printf("%u load time %.1f ms %.2f ns per item\n",
		       t, ns / NS_PER_MS, ns / SAMPLES);
		total += ns;
	}
	printf("* load time %.1f ms\n", total / NS_PER_MS);
	summarize(hg);
}

static void
merged_load(hg64 *hg, unsigned threads) {
	struct thread thread[THREADS];
	hg64 *thg[THREADS];
	for(unsigned t = 0; t < threads; t++) {
		struct thread *tt = &thread[t];
		thg[t] = hg64_create(hg64_sigbits(hg));
		*tt = (struct thread){
			.hg = thg[t],
			.data = data[t],
		};
		assert(pthread_create(&tt->tid, NULL, load_data, tt) == 0);
	}
	double total = 0;
	for(unsigned t = 0; t < threads; t++) {
		assert(pthread_join(thread[t].tid, NULL) == 0);
		double ns = thread[t].ns;
		printf("%u load time %.1f ms %.2f ns per item\n",
		       t, ns / NS_PER_MS, ns / SAMPLES);
		total += ns;
	}
	uint64_t t0 = nanotime();
	for(unsigned t = 0; t < threads; t++) {
		hg64_merge(hg, thg[t]);
	}
	uint64_t t1 = nanotime();
	double ns = t1 - t0;
	printf("merged time %.1f ms %.2f ns per item\n",
	       ns / NS_PER_MS, ns / SAMPLES);
	total += ns;
	printf("* load time %.1f ms\n", total / NS_PER_MS);
	summarize(hg);
}

static void
merge(hg64 *hg, unsigned sigbits) {
	hg64 *copy = hg64_create(sigbits);
	uint64_t t0 = nanotime();
	hg64_merge(copy, hg);
	uint64_t t1 = nanotime();
	printf("merge time %.0f μs\n", (double)(t1 - t0) / 1000);
	summarize(copy);
	hg64_destroy(copy);
}

static void
data_vs_hg64(hg64s *hs, double q) {
	size_t rank = (size_t)(q * THREADS * SAMPLES);
	size_t t = rank % THREADS;
	size_t i = rank / THREADS;
	uint64_t value = hg64s_value_at_quantile(hs, q);
	double p = hg64s_quantile_of_value(hs, data[t][i]);
	double div = data[t][i] == 0 ? 1 : (double)data[t][i];
	printf("data  %5.1f%% %8"PRIu64
	       "  hg64 %5.1f%% %8"PRIu64
	       "  error value %+f rank %+f\n",
	       q * 100, data[t][i],
	       p * 100, value,
	       ((double)data[t][i] - (double)value) / div,
	       (q - p) / (q == 0.0 ? 1.0 : q));
}

static void
dump_csv(hg64 *hg) {
	uint64_t value, count;
	printf("value,count\n");
	for(unsigned key = 0;
	    hg64_get(hg, key, &value, NULL, &count);
	    key = hg64_next(hg, key)) {
		if(count != 0) {
			printf("%"PRIu64",%"PRIu64"\n", value, count);
		}
	}
}

int main(void) {

	hg64_validate();

	for(unsigned t = 0; t < THREADS; t++) {
		for(unsigned i = 0; i < SAMPLES; i++) {
			data[t][i] = rand_lemire(RANGE);
		}
	}

	hg64 *hg = NULL;
	for(unsigned t = 1; t < THREADS; t++) {
		if(hg != NULL) {
			hg64_destroy(hg);
		}
		hg = hg64_create(SIGBITS);
		parallel_load(hg, t);

		hg64 *mhg = hg64_create(SIGBITS);
		merged_load(mhg, t);

		uint64_t min, max, count;
		for(unsigned key = 0;
		    hg64_get(hg, key,  &min, &max, &count);
		    key = hg64_next(hg, key)) {
			uint64_t mmin, mmax, mcount;
			assert(hg64_get(mhg, key,  &mmin, &mmax, &mcount));
			assert(min == mmin);
			assert(max == mmax);
			assert(count == mcount);
		}
		hg64_destroy(mhg);
	}

	for(unsigned sigbits = 1; sigbits < 11; sigbits++) {
		printf("MERGE to %u\n", sigbits);
		merge(hg, sigbits);
	}

	for(unsigned t = 0; t < THREADS; t++) {
		qsort(data[t], SAMPLES, sizeof(uint64_t), compare);
	}

	hg64s *hs = hg64_snapshot(hg);

	double q = 0.0;
	for(double expo = -1; expo > -4; expo--) {
		double step = pow(10, expo);
		for(size_t n = 0; n < 9; n++) {
			data_vs_hg64(hs, q);
			q += step;
		}
	}
	data_vs_hg64(hs, 0.999);
	data_vs_hg64(hs, 0.9999);
	data_vs_hg64(hs, 0.99999);
	data_vs_hg64(hs, 0.999999);

	//dump_csv(stdout, hg);
}
