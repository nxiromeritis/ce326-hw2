typedef struct {
  volatile long double reBeg,reInc;  /* beginning and increment for real values */
  volatile long double imBeg,imInc;  /* beginning and increment for imaginary values */
  volatile int reSteps,imSteps;           /* number of steps/points per range */
} mandel_Pars;

extern void mandel_Slice(mandel_Pars *pars, int n, mandel_Pars slices[]);
/* len(slices) == n */

extern void mandel_Calc(mandel_Pars *pars, int maxIterations, volatile int res[]);
/* len(res) = pars->reSteps*pars->imSteps */
