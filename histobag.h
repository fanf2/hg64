typedef struct histobag histobag;

histobag *histobag_create(void);
void histobag_destroy(histobag *h);

uint64_t histobag_population(histobag *h);
size_t histobag_buckets(histobag *h);
size_t histobag_size(histobag *h);

void histobag_inc(histobag *h, uint64_t value);
void histobag_add(histobag *h, uint64_t value, uint64_t count);

bool histobag_get(histobag *h, size_t i,
		  uint64_t *pmin, uint64_t *pmax, uint64_t *pcount);

void histobag_mean_variance(histobag *h, double *pmean, double *pvar);

uint64_t histobag_value_at_rank(histobag *h, uint64_t rank);
uint64_t histobag_rank_of_value(histobag *h, uint64_t value);
uint64_t histobag_value_at_quantile(histobag *h, double quantile);
double histobag_quantile_of_value(histobag *h, uint64_t value);

void histobag_validate(histobag *h);
