#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "histobag.h"
#include "random.h"

#define NANOSECS (1000*1000*1000)

static uint64_t
nanotime(void) {
	struct timespec tv;
	assert(clock_gettime(CLOCK_MONOTONIC, &tv) == 0);
	return((uint64_t)tv.tv_sec * NANOSECS + (uint64_t)tv.tv_nsec);
}

#define SAMPLE_COUNT (1000*1000)
#define BUCKET_LIMIT 1000

static double data[SAMPLE_COUNT];

static int
compare(const void *ap, const void *bp) {
	double a = *(double *)ap, b = *(double *)bp;
	return(a < b ? -1 : a > b ? +1 : 0);
}

static void
summarize(FILE *fp, histobag *h) {
	uint64_t count = 0;
	uint64_t max = 0;
	for(size_t i = 0; histobag_get(h, i, NULL, NULL, &count); i++) {
		if(max < count) {
			max = count;
		}
	}
	fprintf(fp, "%zu bytes\n", histobag_size(h));
	fprintf(fp, "%zu buckets\n", histobag_buckets(h));
	fprintf(fp, "%zu largest\n", (size_t)max);
	fprintf(fp, "%zu samples\n", (size_t)histobag_population(h));
	/* double mu, sd; */
	/* histobag_mean_sd(h, &mu, &sd); */
	/* fprintf(fp, "%f mu\n", mu); */
	/* fprintf(fp, "%f sigma\n", sd); */
}

/* static void */
/* data_vs_histo(FILE *fp, histobag *h, double q) { */
/* 	size_t rank = (size_t)(q * SAMPLE_COUNT); */
/* 	double value = udds_value_at_quantile(h, q); */
/* 	double p = udds_rank_of_value(h, data[rank]); */
/* 	fprintf(fp, */
/* 		"data  %5.1f%% %-10.7g  " */
/* 		"histo %5.1f%% %-10.7g  " */
/* 		"error %+f\n", */
/* 		q * 100, data[rank], */
/* 		p * 100, value, */
/* 		(data[rank] - value) / data[rank]); */
/* } */

static void
load_data(FILE *fp, histobag *h) {
	uint64_t t0 = nanotime();
	for(size_t i = 0; i < SAMPLE_COUNT; i++) {
		histobag_add(h, data[i], 1);
	}
	uint64_t t1 = nanotime();
	double nanosecs = t1 - t0;
	fprintf(fp, "%f load time %.2f ns per item\n",
		nanosecs / NANOSECS, nanosecs / SAMPLE_COUNT);
}

int main(void) {

	for(size_t i = 0; i < SAMPLE_COUNT; i++) {
		data[i] = rand_lemire(SAMPLE_COUNT);
	}

	histobag *h = histobag_create();
	load_data(stderr, h);
	histobag_validate(h);
	summarize(stderr, h);

	qsort(data, sizeof(data)/sizeof(*data), sizeof(*data), compare);

	/* double q = 0.0; */
	/* for(double expo = -1; expo > -4; expo--) { */
	/* 	double step = pow(10, expo); */
	/* 	for(size_t n = 0; n < 9; n++) { */
	/* 		data_vs_histo(stderr, h, q); */
	/* 		q += step; */
	/* 	} */
	/* } */
	/* data_vs_histo(stderr, h, 0.999); */
	/* data_vs_histo(stderr, h, 0.9999); */
	/* data_vs_histo(stderr, h, 0.99999); */
}
