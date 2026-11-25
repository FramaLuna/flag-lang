typedef struct { float x; } One;
typedef struct { double a; int b; } Mixed;
double one_last(double a0, double a1, double a2, double a3, double a4, double a5, double a6, One one, float after) { return one.x * 10 + after; }
double mixed_last(long a0, long a1, long a2, long a3, long a4, long a5, long a6, Mixed mixed, double after) { return mixed.a * 100 + mixed.b * 10 + after; }
double flag_one_last(double, double, double, double, double, double, double, One, float);
double flag_mixed_last(long, long, long, long, long, long, long, Mixed, double);
double call_back(void) {
    One one = {2};
    Mixed mixed = {3, 4};
    return flag_one_last(1, 1, 1, 1, 1, 1, 1, one, 5) * 1000 + flag_mixed_last(1, 1, 1, 1, 1, 1, 1, mixed, 6);
}
