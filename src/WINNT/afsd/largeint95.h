#ifndef LARGEINT_H
#define LARGEINT_H

typedef struct {
  unsigned long LowPart;
  unsigned long HighPart;
} LARGE_INTEGER;

LARGE_INTEGER LargeIntegerAdd(LARGE_INTEGER a, LARGE_INTEGER b);
LARGE_INTEGER LargeIntegerSubtract(LARGE_INTEGER a, LARGE_INTEGER b);
/*int LargeIntegerGreaterThan(LARGE_INTEGER a, LARGE_INTEGER b);
int LargeIntegerGreaterThanOrEqualTo(LARGE_INTEGER a, LARGE_INTEGER b);
int LargeIntegerEqualTo(LARGE_INTEGER a, LARGE_INTEGER b);
int LargeIntegerGreaterOrEqualToZero(LARGE_INTEGER a);
int LargeIntegerLessThanZero(LARGE_INTEGER a);*/
LARGE_INTEGER ConvertLongToLargeInteger(unsigned long a);
LARGE_INTEGER LargeIntegerMultiplyByLong(LARGE_INTEGER a, unsigned long b);
unsigned long LargeIntegerDivideByLong(LARGE_INTEGER a, unsigned long b);

#define LargeIntegerGreaterThan(a, b) \
 ((a).HighPart > (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart > (b).LowPart))

#define LargeIntegerGreaterThanOrEqualTo(a, b) \
 ((a).HighPart > (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart >= (b).LowPart))
  
#define LargeIntegerLessThan(a, b) \
 ((a).HighPart < (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart < (b).LowPart))

#define LargeIntegerLessThanOrEqualTo(a, b) \
 ((a).HighPart < (b).HighPart || \
  ((a).HighPart == (b).HighPart && (a).LowPart <= (b).LowPart))

#define LargeIntegerEqualTo(a, b) \
  ((a).HighPart == (b).HighPart && (a).LowPart == (b).LowPart)
  
#define LargeIntegerGreaterOrEqualToZero(a) ((a).HighPart >= 0)
  
#define LargeIntegerLessThanZero(a) ((a).HighPart < 0)

#define LargeIntegerNotEqualToZero(a) ((a).HighPart || (a).LowPart)

#endif
