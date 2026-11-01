#include "all.h"

static void
blit(Ref sd[2], int sz, Fn *fn)
{
	struct { int st, ld, cls, size; } *p, tbl[] = {
		{ Ostorel, Oload,   Kl, 8 },
		{ Ostorew, Oload,   Kw, 4 },
		{ Ostoreh, Oloaduh, Kw, 2 },
		{ Ostoreb, Oloadub, Kw, 1 }
	};
	Ref r, r1, ro;
	int off, fwd, n;

	fwd = sz >= 0;
	sz = abs(sz);
	off = fwd ? sz : 0;
	for (p=tbl; sz; p++)
		for (n=p->size; sz>=n; sz-=n) {
			off -= fwd ? n : 0;
			r = newtmp("blt", Kl, fn);
			r1 = newtmp("blt", Kl, fn);
			ro = getcon(off, fn);
			emit(p->st, 0, R, r, r1);
			emit(Oadd, Kl, r1, sd[1], ro);
			r1 = newtmp("blt", Kl, fn);
			emit(p->ld, p->cls, r, r1, R);
			emit(Oadd, Kl, r1, sd[0], ro);
			off += fwd ? 0 : n;
		}
}

static int
ulog2_tab64[64] = {
	63,  0,  1, 41, 37,  2, 16, 42,
	38, 29, 32,  3, 12, 17, 43, 55,
	39, 35, 30, 53, 33, 21,  4, 23,
	13,  9, 18,  6, 25, 44, 48, 56,
	62, 40, 36, 15, 28, 31, 11, 54,
	34, 52, 20, 22,  8,  5, 24, 47,
	61, 14, 27, 10, 51, 19,  7, 46,
	60, 26, 50, 45, 59, 49, 58, 57,
};

static int
ulog2(uint64_t pow2)
{
	return ulog2_tab64[(pow2 * 0x5b31ab928877a7e) >> 58];
}

static int
ispow2(uint64_t v)
{
	return v && (v & (v - 1)) == 0;
}

static void
ins(Ins **pi, int *new, Blk *b, Fn *fn)
{
	ulong ni;
	Con *c;
	Ins *i;
	Ref r;
	int n;

	i = *pi;
	/* simplify more instructions here;
	 * copy 0 into xor, bit rotations,
	 * etc. */
	switch (i->op) {
	case Oblit1:
		assert(i > b->ins);
		assert((i-1)->op == Oblit0);
		if (!*new) {
			curi = &insb[NIns];
			ni = &b->ins[b->nins] - (i+1);
			curi -= ni;
			icpy(curi, i+1, ni);
			*new = 1;
		}
		blit((i-1)->arg, rsval(i->arg[0]), fn);
		*pi = i-1;
		return;
	case Oudiv:
	case Ourem:
		r = i->arg[1];
		if (KBASE(i->cls) == 0)
		if (rtype(r) == RCon) {
			c = &fn->con[r.val];
			if (c->type == CBits)
			if (ispow2(c->bits.i)) {
				n = ulog2(c->bits.i);
				if (i->op == Ourem) {
					i->op = Oand;
					i->arg[1] = getcon((1ull<<n) - 1, fn);
				} else {
					i->op = Oshr;
					i->arg[1] = getcon(n, fn);
				}
			}
		}
		break;
	}
	if (*new)
		emiti(*i);
}

void
simpl(Fn *fn)
{
	Blk *b;
	Ins *i;
	int new;

	for (b=fn->start; b; b=b->link) {
		new = 0;
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			ins(&i, &new, b, fn);
		}
		if (new)
			idup(b, curi, &insb[NIns]-curi);
	}
}

/* where reading the address of thread data
 * costs a call (apple) or a lookup in a table
 * (windows), read it once per function, after
 * the parameters, and keep it in a temporary
 */

static Sym *thrsym;
static Ref *thrtmp;
static uint nthr;
static Ins *thrins;
static uint nthrins;

static Ref
thrbase(Ref r, Ref *off, Fn *fn)
{
	Con *c;
	uint n;

	*off = R;
	if (rtype(r) != RCon)
		return R;
	c = &fn->con[r.val];
	if (c->type != CAddr || c->sym.type != SThr)
		return R;
	for (n=0; n<nthr; n++)
		if (thrsym[n].id == c->sym.id)
			break;
	if (n == nthr) {
		vgrow(&thrsym, ++nthr);
		vgrow(&thrtmp, nthr);
		thrsym[n] = c->sym;
		thrtmp[n] = newtmp("thr", Kl, fn);
	}
	if (c->bits.i)
		*off = getcon(c->bits.i, fn);
	return thrtmp[n];
}

static Ref
thrstart(Ref r, Fn *fn)
{
	Ref b, off, t;

	b = thrbase(r, &off, fn);
	if (req(b, R) || req(off, R))
		return b;
	t = newtmp("thr", Kl, fn);
	addins(&thrins, &nthrins, &(Ins){.op = Oadd, .cls = Kl, .to = t, .arg = {b, off}});
	return t;
}

void
thronce(Fn *fn)
{
	Blk *b;
	Ins *i, in, *first;
	Phi *p;
	Ref r, off[2], t[2];
	Con c;
	uint n, x;

	if (!T.apple && !T.windows)
		return;
	thrsym = vnew(0, sizeof thrsym[0], PFn);
	thrtmp = vnew(0, sizeof thrtmp[0], PFn);
	thrins = vnew(0, sizeof thrins[0], PFn);
	nthr = 0;
	nthrins = 0;
	for (b=fn->start; b; b=b->link) {
		for (p=b->phi; p; p=p->link)
			for (n=0; n<p->narg; n++) {
				r = thrstart(p->arg[n], fn);
				if (!req(r, R))
					p->arg[n] = r;
			}
		r = thrstart(b->jmp.arg, fn);
		if (!req(r, R))
			b->jmp.arg = r;
		curi = &insb[NIns];
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			in = *--i;
			for (x=0; x<2; x++) {
				t[x] = R;
				if (isarg(in.op) || in.op == Osel1) {
					/* the arguments of a call and the sels stay together */
					r = thrstart(in.arg[x], fn);
					if (!req(r, R))
						in.arg[x] = r;
					continue;
				}
				r = thrbase(in.arg[x], &off[x], fn);
				if (req(r, R))
					continue;
				in.arg[x] = r;
				if (!req(off[x], R)) {
					t[x] = r;
					in.arg[x] = newtmp("thr", Kl, fn);
				}
			}
			emiti(in);
			for (x=0; x<2; x++)
				if (!req(t[x], R))
					emit(Oadd, Kl, in.arg[x], t[x], off[x]);
		}
		idup(b, curi, &insb[NIns]-curi);
	}
	if (nthr == 0)
		return;
	b = fn->start;
	for (first=b->ins; first<&b->ins[b->nins] && ispar(first->op); first++)
		;
	curi = &insb[NIns];
	for (i=&b->ins[b->nins]; i!=first;)
		emiti(*--i);
	for (n=nthrins; n>0; n--)
		emiti(thrins[n-1]);
	for (n=nthr; n>0; n--) {
		c = (Con){.type = CAddr, .sym = thrsym[n-1]};
		emit(Ocopy, Kl, thrtmp[n-1], newcon(&c, fn), R);
	}
	for (i=first; i!=b->ins;)
		emiti(*--i);
	idup(b, curi, &insb[NIns]-curi);
	filluse(fn);
}
