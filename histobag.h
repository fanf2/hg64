typedef struct histobag histobag;

histobag *histobag_create(void);
void histobag_destroy(histobag *h);
void histobag_validate(histobag *h);
bool histobag_get(histobag *h, size_t i, uint64_t *min, uint64_t *max, uint64_t *count);
void histobag_add(histobag *h, uint64_t value, uint64_t count);
uint64_t histobag_population(histobag *h);
size_t histobag_buckets(histobag *h);
size_t histobag_size(histobag *h);
