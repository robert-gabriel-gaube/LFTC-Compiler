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

void main() {
  int i;
  i = 10;
  while (i != 0) {
    puti(i);
    i = i / 2;
  }
}
