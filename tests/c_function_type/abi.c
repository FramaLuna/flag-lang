typedef struct { float x, y; } V2;
typedef struct { double a, b, c; } Big;
typedef struct { unsigned long long size, align; } SizeAlign;
typedef struct { double x, y, w, h; } Rect;
static float v2len2(V2 v) { return v.x * v.x + v.y * v.y; }
static V2 v2make(float x, float y) { V2 r = {x, y}; return r; }
static double bigsum(Big b) { return b.a + b.b + b.c; }
static Big bigmake(double a) { Big r = {a, a * 2, a * 3}; return r; }
static SizeAlign size_align(unsigned long long n) { SizeAlign r = {n * 2, 256}; return r; }
static Rect frame(Rect r) { Rect o = {r.x, r.y, r.w, r.h - 28}; return o; }
void *lookup(int i) {
    void *all[] = {(void *)v2len2, (void *)v2make, (void *)bigsum, (void *)bigmake, (void *)size_align, (void *)frame};
    return all[i];
}
typedef struct { int (*add)(int, int); int (*twice)(int); } Table;
static int add(int a, int b) { return a + b; }
static int twice(int a) { return 2 * a; }
Table *table(void) { static Table t = {add, twice}; return &t; }
int apply(int (*f)(int), int x) { return f(x) + 1; }
