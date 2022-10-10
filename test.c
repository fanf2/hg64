/*
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 *
 * Permission is hereby granted to use, copy, modify, and/or
 * distribute this software for any purpose with or without fee.
 *
 * This software is provided 'as is', without warranty of any kind.
 * In no event shall the authors be liable for any damages arising
 * from the use of this software.
 *
 * SPDX-License-Identifier: 0BSD OR MIT-0
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "hg64.h"
#include "random.h"

#ifndef KEYBITS
#define KEYBITS 12
#endif

extern void hg64_validate(hg64 *hg);

#define NANOSECS (1000*1000*1000)

static uint64_t
nanotime(void) {
	struct timespec tv;
	assert(clock_gettime(CLOCK_MONOTONIC, &tv) == 0);
	return((uint64_t)tv.tv_sec * NANOSECS + (uint64_t)tv.tv_nsec);
}

#define SAMPLE_COUNT (1000*1000)

static uint64_t data[SAMPLE_COUNT];

static int
compare(const void *ap, const void *bp) {
	uint64_t a = *(const uint64_t *)ap;
	uint64_t b = *(const uint64_t *)bp;
	return(a < b ? -1 : a > b ? +1 : 0);
}

static void
summarize(FILE *fp, hg64 *hg) {
	uint64_t count = 0;
	uint64_t max = 0;
	uint64_t population = 0;
	for(unsigned key = 0; hg64_get(hg, key, NULL, NULL, &count); key++) {
		max = (max > count) ? max : count;
		population += count;
	}
	fprintf(fp, "%u sigbits\n", hg64_sigbits(hg));
	fprintf(fp, "%zu bytes\n", hg64_size(hg));
	fprintf(fp, "%zu largest\n", (size_t)max);
	fprintf(fp, "%zu samples\n", (size_t)population);
	double mean, var;
	hg64_mean_variance(hg, &mean, &var);
	fprintf(fp, "mean %f +/- %f\n", mean, sqrt(var));
}

static void
data_vs_hg64(FILE *fp, hg64s *hs, double q) {
	size_t rank = (size_t)(q * SAMPLE_COUNT);
	uint64_t value = hg64s_value_at_quantile(hs, q);
	double p = hg64s_quantile_of_value(hs, data[rank]);
	double div = data[rank] == 0 ? 1 : (double)data[rank];
	fprintf(fp,
		"data  %5.1f%% %8llu  "
		"hg64 %5.1f%% %8llu  "
		"error value %+f rank %+f\n",
		q * 100, data[rank],
		p * 100, value,
		((double)data[rank] - (double)value) / div,
		(q - p) / (q == 0.0 ? 1.0 : q));
}

static void
load_data(FILE *fp, hg64 *hg) {
	uint64_t t0 = nanotime();
	for(size_t i = 0; i < SAMPLE_COUNT; i++) {
		hg64_add(hg, data[i], 1);
	}
	uint64_t t1 = nanotime();
	double nanosecs = (double)(t1 - t0);
	fprintf(fp, "%f load time %.2f ns per item\n",
		nanosecs / NANOSECS, nanosecs / SAMPLE_COUNT);
}

static void
dump_csv(FILE *fp, hg64 *hg) {
	uint64_t value, count;
	fprintf(fp, "value,count\n");
	for(unsigned key = 0; hg64_get(hg, key, &value, NULL, &count); key++) {
		if(count != 0) {
			fprintf(fp, "%llu,%llu\n", value, count);
		}
	}
}

int main(void) {

	for(size_t i = 0; i < SAMPLE_COUNT; i++) {
		data[i] = rand_lemire(SAMPLE_COUNT);
	}

	hg64 *hg = hg64_create(KEYBITS - 6);
	load_data(stderr, hg);
	hg64_validate(hg);
	summarize(stderr, hg);

	hg64s *hs = hg64_snapshot(hg);
	qsort(data, sizeof(data)/sizeof(*data), sizeof(*data), compare);

	double q = 0.0;
	for(double expo = -1; expo > -4; expo--) {
		double step = pow(10, expo);
		for(size_t n = 0; n < 9; n++) {
			data_vs_hg64(stderr, hs, q);
			q += step;
		}
	}
	data_vs_hg64(stderr, hs, 0.999);
	data_vs_hg64(stderr, hs, 0.9999);
	data_vs_hg64(stderr, hs, 0.99999);
	data_vs_hg64(stderr, hs, 0.999999);

	//dump_csv(stdout, hg);
}
