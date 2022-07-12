/*
 * uniform unsigned integers in [0,limit)
 */
uint32_t rand_lemire(uint32_t limit);

/*
 * uniform in (0,1)
 */
double rand_uniform(void);

/*
 * exponential distribution with mean 1
 */
double rand_exponential(void);

/*
 * pareto distribution with mean inf
 */
double rand_pareto(void);

/*
 * gamma distribution with mean 1 shape k scale 1/k
 */
double rand_gamma(unsigned k);

/*
 * normal distribution with mean 0 and sigma 1
 */
double rand_normal(void);

/*
 * log normal distribution with mean exp(0.5)
 */
double rand_lognormal(void);

/*
 * chi squared distribution with k degrees of freedom mean 1
 */
double rand_chisquared(unsigned k);
