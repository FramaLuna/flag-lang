typedef union { float f; int i; } Num;
typedef struct { float x, y; } Pair;
typedef union { Pair p; double d; } PairOrDouble;

Num twice(Num n, int is_float) {
    if (is_float) n.f *= 2; else n.i *= 2;
    return n;
}

double sum(PairOrDouble u, int is_pair) {
    return is_pair ? u.p.x + u.p.y : u.d;
}
