/*
 * hg64 - 64-bit histograms
 *
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

typedef struct hg64 hg64;

/*
 * Allocate a new histogram
 */
hg64 *hg64_create(void);

/*
 * Free the memory used by a histogram
 */
void hg64_destroy(hg64 *hg);

/*
 * Calculate the total count of all the buckets in the histogram
 */
uint64_t hg64_population(hg64 *hg);

/*
 * Calculate the number of buckets
 */
size_t hg64_buckets(hg64 *hg);

/*
 * Calculate the memory used in bytes
 */
size_t hg64_size(hg64 *hg);

/*
 * Get the compile-time KEYBITS setting
 */
unsigned hg64_keybits(void);

/*
 * Add 1 to the value's bucket
 */
void hg64_inc(hg64 *hg, uint64_t value);

/*
 * Add an arbitrary count to the value's bucket
 */
void hg64_add(hg64 *hg, uint64_t value, uint64_t count);

/*
 * Get information about a bucket. This can be used as an iterator, by
 * initializing `key` to zero and incrementing by one until `hg64_get()`
 * returns `false`.
 *
 * If `pmin` is non-NULL it is set to the bucket's minimum inclusive value.
 *
 * If `pmax` is non-NULL it is set to the bucket's maximum inclusive value.
 *
 * If `pcount` is non-NULL it is set to the bucket's counter, which
 * can be zero. (Empty buckets are included in the iterator.)
 */
bool hg64_get(hg64 *hg, unsigned key,
		  uint64_t *pmin, uint64_t *pmax, uint64_t *pcount);

/*
 * Increase the counts in `target` by the counts recorded in `source`
 */
void hg64_merge(hg64 *target, hg64 *source);

/*
 * Get summary statistics about the histogram.
 *
 * If `pmean` is non-NULL it is set to the mean of the recorded data.
 *
 * If `pvar` is non-NULL it is set to the variance of the recorded
 * data. The standard deviation is the square root of the variance.
 */
void hg64_mean_variance(hg64 *hg, double *pmean, double *pvar);

/*
 * Get the approximate value at a given rank in the recorded data.
 * The rank must be less than the histogram's population.
 */
uint64_t hg64_value_at_rank(hg64 *hg, uint64_t rank);

/*
 * Get the approximate value at a given quantile in the recorded data.
 * The quantile must be >= 0.0 and < 1.0
 *
 * Quantiles are percentiles divided by 100. The median is quantile 1/2.
 */
uint64_t hg64_value_at_quantile(hg64 *hg, double quantile);

/*
 * Get the approximate rank of a value in the recorded data.
 * You can query the rank of any value.
 */
uint64_t hg64_rank_of_value(hg64 *hg, uint64_t value);

/*
 * Get the approximate quantile of a value in the recorded data.
 */
double hg64_quantile_of_value(hg64 *hg, uint64_t value);
