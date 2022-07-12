typedef struct histobag histobag;

histobag *histobag_create(double alpha);
void histobag_destroy(histobag *h);
void histobag_validate(histobag *h);
bool histobag_next(histobag *h, double *value, size_t *count);
void histobag_add(histobag *h, double value, size_t count);
size_t histobag_population(histobag *h);
size_t histobag_buckets(histobag *h);
size_t histobag_size(histobag *h);
