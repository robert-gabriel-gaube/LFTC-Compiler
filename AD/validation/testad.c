int x;
char y;
double z;
double p[100];

struct S1 {
  int i;
  double d[2];
  char x;
};
struct S1 p1;
struct S1 vp[10];

double sum(double x[5], int n) {
  double r;
  int i;
  r = 0;
  i = 0;
  while (i < n) {
    double n;
    n = x[i];
    r = r + n;
    i = i + 1;
  }
  return r;
}

void f(struct S1 p) { puti(p.i); }
