#include <stdbool.h>

typedef struct { int count; bool ok; } Pair;

Pair split(const char *text) {
    Pair p = {0, false};
    while (text[p.count]) p.count++;
    p.ok = p.count > 3;
    return p;
}

int total(Pair p) {
    return p.count + p.ok;
}
