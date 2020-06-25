#ifndef _LIB_MATH_H
#define _LIB_MATH_H

/* max() & min() */
#define	MAX(a,b)	((a) > (b) ? (a) : (b))
#define	MIN(a,b)	((a) < (b) ? (a) : (b))

#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))

#define abs(a)    ((a) > 0 ? (a) : -(a))

/* 除后上入 */
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))

/* 除后下舍 */
#define DIV_ROUND_DOWN(X, STEP) ((X) / (STEP))

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

double sin(double x);
double cos(double x);
double sqrt (double x);
double fabs(double x);

#endif /* _LIB_MATH_H */
