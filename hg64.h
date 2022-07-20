/*
 * hg64 - 64-bit histograms
 *
 * Written by Tony Finch <dot@dotat.at> <fanf@isc.org>
 * You may do anything with this. It has no warranty.
 * <https://creativecommons.org/publicdomain/zero/1.0/>
 * SPDX-License-Identifier: CC0-1.0
 */

typedef struct hg64 hg64;

/*
 * Allocate a new histogram. `sigbits` must be between 1 and 6
 * inclusive; it is the number of significant bits of each value
 * to use when mapping values to buckets.
 */
hg64 *hg64_create(unsigned sigbits);

/*
 * Free the memory used by a histogram
 */
void hg64_destroy(hg64 *hg);

/*
 * Get the histogram's `sigbits` setting
 */
unsigned hg64_sigbits(hg64 *hg);

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
 * If `pmax` is non-NULL it is set to the bucket's maximum exclusive value.
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

/*
 * Serialize the histogram into `buffer`, which has `size` bytes
 * available. Returns the number of bytes required; if the return
 * value is greater than `size` the output has been truncated.
 */
size_t hg64_export(hg64 *hg, uint8_t *buffer, size_t size);
