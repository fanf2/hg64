/*
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 * You may do anything with this. It has no warranty.
 * <https://creativecommons.org/publicdomain/zero/1.0/>
 * SPDX-License-Identifier: CC0-1.0
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

extern void hg64_validate(hg64 *hg);

#define NANOSECS (1000*1000*1000)

static uint64_t
nanotime(void) {
	struct timespec tv;
	assert(clock_gettime(CLOCK_MONOTONIC, &tv) == 0);
	return((uint64_t)tv.tv_sec * NANOSECS + (uint64_t)tv.tv_nsec);
}

#define SAMPLE_COUNT (1000*1000)
#define BUCKET_LIMIT 1000

static uint64_t data[SAMPLE_COUNT];

static int
compare(const void *ap, const void *bp) {
	uint64_t a = *(uint64_t *)ap, b = *(uint64_t *)bp;
	return(a < b ? -1 : a > b ? +1 : 0);
}

static void
summarize(FILE *fp, hg64 *hg) {
	uint64_t count = 0;
	uint64_t max = 0;
	for(size_t i = 0; hg64_get(hg, i, NULL, NULL, &count); i++) {
		max = (max > count) ? max : count;
	}
	fprintf(fp, "%zu bytes\n", hg64_size(hg));
	fprintf(fp, "%zu buckets\n", hg64_buckets(hg));
	fprintf(fp, "%zu largest\n", (size_t)max);
	fprintf(fp, "%zu samples\n", (size_t)hg64_population(hg));
	double mean, var;
	hg64_mean_variance(hg, &mean, &var);
	fprintf(fp, "%f mu\n", mean);
	fprintf(fp, "%f sigma\n", sqrt(var));
}

static void
data_vs_hg64(FILE *fp, hg64 *hg, double q) {
	size_t rank = (size_t)(q * SAMPLE_COUNT);
	uint64_t value = hg64_value_at_quantile(hg, q);
	double p = hg64_quantile_of_value(hg, data[rank]);
	fprintf(fp,
		"data  %5.1f%% %8llu  "
		"hg64 %5.1f%% %8llu  "
		"error value %+f rank %+f\n",
		q * 100, data[rank],
		p * 100, value,
		((double)data[rank] - (double)value) / (double)data[rank],
		(q - p) / (q == 0 ? 1 : q));
}

static void
load_data(FILE *fp, hg64 *hg) {
	uint64_t t0 = nanotime();
	for(size_t i = 0; i < SAMPLE_COUNT; i++) {
		hg64_add(hg, data[i], 1);
	}
	uint64_t t1 = nanotime();
	double nanosecs = t1 - t0;
	fprintf(fp, "%f load time %.2f ns per item\n",
		nanosecs / NANOSECS, nanosecs / SAMPLE_COUNT);
}

int main(void) {

	for(size_t i = 0; i < SAMPLE_COUNT; i++) {
		data[i] = (rand_normal() + 6) * SAMPLE_COUNT;
	}

	hg64 *hg = hg64_create();
	load_data(stderr, hg);
	hg64_validate(hg);
	summarize(stderr, hg);

	qsort(data, sizeof(data)/sizeof(*data), sizeof(*data), compare);

	double q = 0.0;
	for(double expo = -1; expo > -4; expo--) {
		double step = pow(10, expo);
		for(size_t n = 0; n < 9; n++) {
			data_vs_hg64(stderr, hg, q);
			q += step;
		}
	}
	data_vs_hg64(stderr, hg, 0.999);
	data_vs_hg64(stderr, hg, 0.9999);
	data_vs_hg64(stderr, hg, 0.99999);
	data_vs_hg64(stderr, hg, 0.999999);
}
