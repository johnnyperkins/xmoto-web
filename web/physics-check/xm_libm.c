/* SPDX-License-Identifier: GPL-2.0-or-later */
/* sin, cos and atan2 built only from IEEE add/multiply and floor, so it gives the same bits
 * on every platform. Used to test whether libm is the ONLY native vs wasm difference
 * in the solver. Accuracy is good but this is not meant for the game itself. */
#include <math.h>

static const double PI2 = 6.283185307179586476925287;

static double reduce(double x) {
  return x - PI2 * floor(x / PI2 + 0.5); /* to [-pi, pi] */
}

/* These replace libm's symbols at link time in both builds. */
double sin(double x) {
  double r = reduce(x), r2 = r * r, term = r, sum = r;
  for (int i = 1; i <= 12; i++) {
    term = -term * r2 / ((2 * i) * (2 * i + 1));
    sum += term;
  }
  return sum;
}

double cos(double x) {
  double r = reduce(x), r2 = r * r, term = 1.0, sum = 1.0;
  for (int i = 1; i <= 12; i++) {
    term = -term * r2 / ((2 * i - 1) * (2 * i));
    sum += term;
  }
  return sum;
}

static double xm_atan_kernel(double t) { /* |t| <= tan(pi/12) */
  double t2 = t * t, term = t, sum = t;
  for (int i = 1; i <= 16; i++) {
    term = -term * t2;
    sum += term / (2 * i + 1);
  }
  return sum;
}

static double xm_atan(double x) {
  static const double PI_2 = 1.570796326794896619231322;
  static const double PI_6 = 0.523598775598298873077107;
  static const double TAN_PI_6 = 0.577350269189625764509149;
  int neg = x < 0;
  double a = neg ? -x : x, r;
  if (a > 1.0) {
    r = PI_2 - xm_atan(1.0 / a);
  } else if (a > 0.267949192431122706472554) { /* tan(pi/12) */
    r = PI_6 + xm_atan_kernel((a - TAN_PI_6) / (1.0 + a * TAN_PI_6));
  } else {
    r = xm_atan_kernel(a);
  }
  return neg ? -r : r;
}

double atan2(double y, double x) {
  static const double PI = 3.141592653589793238462643;
  if (x > 0) return xm_atan(y / x);
  if (x < 0) return y >= 0 ? xm_atan(y / x) + PI : xm_atan(y / x) - PI;
  if (y > 0) return PI / 2;
  if (y < 0) return -PI / 2;
  return 0.0;
}
