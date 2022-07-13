typedef struct hg64 hg64;

hg64 *hg64_create(void);
void hg64_destroy(hg64 *hg);

uint64_t hg64_population(hg64 *hg);
size_t hg64_buckets(hg64 *hg);
size_t hg64_size(hg64 *hg);

void hg64_inc(hg64 *hg, uint64_t value);
void hg64_add(hg64 *hg, uint64_t value, uint64_t count);

bool hg64_get(hg64 *hg, size_t i,
		  uint64_t *pmin, uint64_t *pmax, uint64_t *pcount);

void hg64_mean_variance(hg64 *hg, double *pmean, double *pvar);

uint64_t hg64_value_at_rank(hg64 *hg, uint64_t rank);
uint64_t hg64_rank_of_value(hg64 *hg, uint64_t value);
uint64_t hg64_value_at_quantile(hg64 *hg, double quantile);
double hg64_quantile_of_value(hg64 *hg, uint64_t value);

void hg64_validate(hg64 *hg);
