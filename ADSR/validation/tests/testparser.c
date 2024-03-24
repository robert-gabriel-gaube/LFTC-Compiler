struct Pt {
  int x;
  int y;
};
struct Pt points[10];

double max(double a, double b) {
  if (a > b)
    return a;
  else
    return b;
}

int len(char s[]) {
  int i;
  i = 0;
  while (s[i])
    i = i + 1;
  return i;
}

double function_test1(int a[], double b, double c, char d) {
  double x;
  x = (double)a[0];
  x = (1 + 2) / (2 + 3) * (5 + 6);
  if(((int)x + 1 == 1 && (int)x - 2 == 1) || ((int)x + 1 == 1 && (int)x - 2 == 1)) {
    x = x + 1;
  }
  return -1.10;
}

void main() {
  int i;
  i = 10;
  while (i != 0) {
    puti(i);
    i = i / 2;
  }
}
