#include "all.h"

typedef struct Range Range;
typedef struct Store Store;
typedef struct Slot Slot;

/* require use, maintains use counts */
void
promote(Fn *fn)
{
	Blk *b;
	Ins *i, *l;
	Tmp *t;
	Use *u, *ue;
	int s, k;

	/* promote uniform stack slots to temporaries */
	b = fn->start;
	for (i=b->ins; i<&b->ins[b->nins]; i++) {
		if (Oalloc > i->op || i->op > Oalloc1)
			continue;
		/* specific to NAlign == 3 */
		assert(rtype(i->to) == RTmp);
		t = &fn->tmp[i->to.val];
		if (t->ndef != 1)
			goto Skip;
		k = -1;
		s = -1;
		for (u=t->use; u<&t->use[t->nuse]; u++) {
			if (u->type != UIns)
				goto Skip;
			l = u->u.ins;
			if (isload(l->op))
			if (s == -1 || s == loadsz(l)) {
				s = loadsz(l);
				continue;
			}
			if (isstore(l->op))
			if (req(i->to, l->arg[1]) && !req(i->to, l->arg[0]))
			if (s == -1 || s == storesz(l))
			if (k == -1 || k == optab[l->op].argcls[0][0]) {
				s = storesz(l);
				k = optab[l->op].argcls[0][0];
				continue;
			}
			goto Skip;
		}
		/* get rid of the alloc and replace uses */
		*i = (Ins){.op = Onop};
		t->ndef--;
		ue = &t->use[t->nuse];
		for (u=t->use; u!=ue; u++) {
			l = u->u.ins;
			if (isstore(l->op)) {
				l->cls = k;
				l->op = Ocopy;
				l->to = l->arg[1];
				l->arg[1] = R;
				t->nuse--;
				t->ndef++;
			} else {
				if (k == -1)
					err("slot %%%s is read but never stored to",
						fn->tmp[l->arg[0].val].name);
				/* try to turn loads into copies so we
				 * can eliminate them later */
				switch(l->op) {
				case Oloadsw:
				case Oloaduw:
					if (k == Kl)
						goto Extend;
					/* fall through */
				case Oload:
					if (KBASE(k) != KBASE(l->cls))
						l->op = Ocast;
					else
						l->op = Ocopy;
					break;
				default:
				Extend:
					l->op = Oextsb + (l->op - Oloadsb);
					break;
				}
			}
		}
	Skip:;
	}
	if (debug['M']) {
		fprintf(stderr, "\n> After slot promotion:\n");
		printfn(fn, stderr);
	}
}

/* [a, b) with 0 <= a */
struct Range {
	int a, b;
};

struct Store {
	int ip;
	Ins *i;
};

struct Slot {
	int t;
	int sz;
	bits m;
	bits l;
	Range r;
	Slot *s;
	Store *st;
	int nst;
};

static inline int
rin(Range r, int n)
{
	return r.a <= n && n < r.b;
}

static inline int
rovlap(Range r0, Range r1)
{
	return r0.b && r1.b && r0.a < r1.b && r1.a < r0.b;
}

static void
radd(Range *r, int n)
{
	if (!r->b)
		*r = (Range){n, n+1};
	else if (n < r->a)
		r->a = n;
	else if (n >= r->b)
		r->b = n+1;
}

static int
slot(Slot **ps, int64_t *off, Ref r, Fn *fn, Slot *sl)
{
	Alias a;
	Tmp *t;

	getalias(&a, r, fn);
	if (a.type != ALoc)
		return 0;
	t = &fn->tmp[a.base];
	if (t->visit < 0)
		return 0;
	*off = a.offset;
	*ps = &sl[t->visit];
	return 1;
}

static void
load(Ref r, bits x, int ip, Fn *fn, Slot *sl)
{
	int64_t off;
	Slot *s;

	if (slot(&s, &off, r, fn, sl)) {
		s->l |= x << off;
		s->l &= s->m;
		if (s->l)
			radd(&s->r, ip);
	}
}

static void
store(Ref r, bits x, int ip, Ins *i, Fn *fn, Slot *sl)
{
	int64_t off;
	Slot *s;

	if (slot(&s, &off, r, fn, sl)) {
		if (s->l) {
			radd(&s->r, ip);
			s->l &= ~(x << off);
		} else {
			vgrow(&s->st, ++s->nst);
			s->st[s->nst-1].ip = ip;
			s->st[s->nst-1].i = i;
		}
	}
}

static int
scmp(const void *pa, const void *pb)
{
	Slot *a, *b;

	a = (Slot *)pa, b = (Slot *)pb;
	if (a->sz != b->sz)
		return b->sz - a->sz;
	return a->r.a - b->r.a;
}

static void
maxrpo(Blk *hd, Blk *b)
{
	if (hd->loop < (int)b->id)
		hd->loop = b->id;
}

void
coalesce(Fn *fn)
{
	Range r, *br;
	Slot *s, *s0, *sl;
	Blk *b, **ps, *succ[3];
	Ins *i, **bl;
	Use *u;
	Tmp *t, *ts;
	Ref *arg;
	bits x;
	int64_t off0, off1;
	int n, m, ip, sz, nsl, nbl, *stk;
	uint total, freed, fused;

	/* minimize the stack usage
	 * by coalescing slots
	 */
	nsl = 0;
	sl = vnew(0, sizeof sl[0], PHeap);
	for (n=Tmp0; n<fn->ntmp; n++) {
		t = &fn->tmp[n];
		t->visit = -1;
		if (t->alias.type == ALoc)
		if (t->alias.slot == &t->alias)
		if (t->bid == fn->start->id)
		if (t->alias.u.loc.sz != -1) {
			t->visit = nsl;
			vgrow(&sl, ++nsl);
			s = &sl[nsl-1];
			s->t = n;
			s->sz = t->alias.u.loc.sz;
			s->m = t->alias.u.loc.m;
			s->s = 0;
			s->st = vnew(0, sizeof s->st[0], PHeap);
			s->nst = 0;
		}
	}

	/* one-pass liveness analysis */
	for (b=fn->start; b; b=b->link)
		b->loop = -1;
	loopiter(fn, maxrpo);
	nbl = 0;
	bl = vnew(0, sizeof bl[0], PHeap);
	br = emalloc(fn->nblk * sizeof br[0]);
	ip = INT_MAX - 1;
	for (n=fn->nblk-1; n>=0; n--) {
		b = fn->rpo[n];
		succ[0] = b->s1;
		succ[1] = b->s2;
		succ[2] = 0;
		br[n].b = ip--;
		for (s=sl; s<&sl[nsl]; s++) {
			s->l = 0;
			for (ps=succ; *ps; ps++) {
				m = (*ps)->id;
				if (m > n && rin(s->r, br[m].a)) {
					s->l = s->m;
					radd(&s->r, ip);
				}
			}
		}
		if (b->jmp.type == Jretc)
			load(b->jmp.arg, -1, --ip, fn, sl);
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			arg = i->arg;
			if (i->op == Oargc) {
				load(arg[1], -1, --ip, fn, sl);
			}
			if (isload(i->op)) {
				x = BIT(loadsz(i)) - 1;
				load(arg[0], x, --ip, fn, sl);
			}
			if (isstore(i->op)) {
				x = BIT(storesz(i)) - 1;
				store(arg[1], x, ip--, i, fn, sl);
			}
			if (i->op == Oblit0) {
				assert((i+1)->op == Oblit1);
				assert(rtype((i+1)->arg[0]) == RInt);
				sz = abs(rsval((i+1)->arg[0]));
				x = sz >= NBit ? (bits)-1 : BIT(sz) - 1;
				store(arg[1], x, ip--, i, fn, sl);
				load(arg[0], x, ip, fn, sl);
				vgrow(&bl, ++nbl);
				bl[nbl-1] = i;
			}
		}
		for (s=sl; s<&sl[nsl]; s++)
			if (s->l) {
				radd(&s->r, ip);
				if (b->loop != -1) {
					assert(b->loop >= n);
					radd(&s->r, br[b->loop].b - 1);
				}
			}
		br[n].a = ip;
	}
	free(br);

	/* kill dead stores */
	for (s=sl; s<&sl[nsl]; s++)
		for (n=0; n<s->nst; n++)
			if (!rin(s->r, s->st[n].ip)) {
				i = s->st[n].i;
				if (i->op == Oblit0)
					*(i+1) = (Ins){.op = Onop};
				*i = (Ins){.op = Onop};
			}

	/* kill slots with an empty live range */
	total = 0;
	freed = 0;
	stk = vnew(0, sizeof stk[0], PHeap);
	n = 0;
	for (s=s0=sl; s<&sl[nsl]; s++) {
		total += s->sz;
		if (!s->r.b) {
			vfree(s->st);
			vgrow(&stk, ++n);
			stk[n-1] = s->t;
			freed += s->sz;
		} else
			*s0++ = *s;
	}
	nsl = s0-sl;
	if (debug['M']) {
		fputs("\n> Slot coalescing:\n", stderr);
		if (n) {
			fputs("\tkill [", stderr);
			for (m=0; m<n; m++)
				fprintf(stderr, " %%%s",
					fn->tmp[stk[m]].name);
			fputs(" ]\n", stderr);
		}
	}
	while (n--) {
		t = &fn->tmp[stk[n]];
		assert(t->ndef == 1 && t->def);
		i = t->def;
		if (isload(i->op)) {
			i->op = Ocopy;
			i->arg[0] = UNDEF;
			continue;
		}
		*i = (Ins){.op = Onop};
		for (u=t->use; u<&t->use[t->nuse]; u++) {
			if (u->type == UJmp) {
				b = fn->rpo[u->bid];
				assert(isret(b->jmp.type));
				b->jmp.type = Jret0;
				b->jmp.arg = R;
				continue;
			}
			assert(u->type == UIns);
			i = u->u.ins;
			if (!req(i->to, R)) {
				assert(rtype(i->to) == RTmp);
				vgrow(&stk, ++n);
				stk[n-1] = i->to.val;
			} else if (isarg(i->op)) {
				assert(i->op == Oargc);
				i->arg[1] = CON_Z;  /* crash */
			} else {
				if (i->op == Oblit0)
					*(i+1) = (Ins){.op = Onop};
				*i = (Ins){.op = Onop};
			}
		}
	}
	vfree(stk);

	/* fuse slots by decreasing size */
	qsort(sl, nsl, sizeof *sl, scmp);
	fused = 0;
	for (n=0; n<nsl; n++) {
		s0 = &sl[n];
		if (s0->s)
			continue;
		s0->s = s0;
		r = s0->r;
		for (s=s0+1; s<&sl[nsl]; s++) {
			if (s->s || !s->r.b)
				goto Skip;
			if (rovlap(r, s->r))
				/* O(n); can be approximated
				 * by 'goto Skip;' if need be
				 */
				for (m=n; &sl[m]<s; m++)
					if (sl[m].s == s0)
					if (rovlap(sl[m].r, s->r))
						goto Skip;
			radd(&r, s->r.a);
			radd(&r, s->r.b - 1);
			s->s = s0;
			fused += s->sz;
		Skip:;
		}
	}

	/* substitute fused slots */
	for (s=sl; s<&sl[nsl]; s++) {
		t = &fn->tmp[s->t];
		/* the visit link is stale,
		 * reset it before the slot()
		 * calls below
		 */
		t->visit = s-sl;
		assert(t->ndef == 1 && t->def);
		if (s->s == s)
			continue;
		*t->def = (Ins){.op = Onop};
		ts = &fn->tmp[s->s->t];
		assert(t->bid == ts->bid);
		if (t->def < ts->def) {
			/* make sure the slot we
			 * selected has a def that
			 * dominates its new uses
			 */
			*t->def = *ts->def;
			*ts->def = (Ins){.op = Onop};
			ts->def = t->def;
		}
		for (u=t->use; u<&t->use[t->nuse]; u++) {
			if (u->type == UJmp) {
				b = fn->rpo[u->bid];
				b->jmp.arg = TMP(s->s->t);
				continue;
			}
			assert(u->type == UIns);
			arg = u->u.ins->arg;
			for (n=0; n<2; n++)
				if (req(arg[n], TMP(s->t)))
					arg[n] = TMP(s->s->t);
		}
	}

	/* fix newly overlapping blits */
	for (n=0; n<nbl; n++) {
		i = bl[n];
		if (i->op == Oblit0)
		if (slot(&s, &off0, i->arg[0], fn, sl))
		if (slot(&s0, &off1, i->arg[1], fn, sl))
		if (s->s == s0->s) {
			if (off0 < off1) {
				sz = rsval((i+1)->arg[0]);
				assert(sz >= 0);
				(i+1)->arg[0] = INT(-sz);
			} else if (off0 == off1) {
				*i = (Ins){.op = Onop};
				*(i+1) = (Ins){.op = Onop};
			}
		}
	}
	vfree(bl);

	if (debug['M']) {
		for (s0=sl; s0<&sl[nsl]; s0++) {
			if (s0->s != s0)
				continue;
			fprintf(stderr, "\tfuse (% 3db) [", s0->sz);
			for (s=s0; s<&sl[nsl]; s++) {
				if (s->s != s0)
					continue;
				fprintf(stderr, " %%%s", fn->tmp[s->t].name);
				if (s->r.b)
					fprintf(stderr, "[%d,%d)",
						s->r.a-ip, s->r.b-ip);
				else
					fputs("{}", stderr);
			}
			fputs(" ]\n", stderr);
		}
		fprintf(stderr, "\tsums %u/%u/%u (killed/fused/total)\n\n",
			freed, fused, total);
		printfn(fn, stderr);
	}

	for (s=sl; s<&sl[nsl]; s++)
		vfree(s->st);
	vfree(sl);
}

/* cut a small slot whose address only reaches
 * loads, stores, blits and constant offsets
 * into one slot per piece, so promote() can
 * turn the pieces into temporaries
 */

enum { ChopMax = 32 };

typedef struct Chop Chop;
typedef struct Blit Blit;

struct Chop {
	int sz;
	int bad;
	char *cut; /* a piece starts or ends here */
	char *rd;
	char *wr;
	int *cls;  /* -1 if unknown, 4 marks a class only read */
	Ref *to;
};

struct Blit {
	int k, m;
	int64_t off[2];
	int64_t sz;
};

static Chop *chops;
static int *chopof;
static int64_t *offof;
static int nofs;

static int
reach(int t, int c, int64_t off, Fn *fn)
{
	Use *u;
	Ins *i;
	Ref r;

	chopof[t] = c + 1;
	offof[t] = off;
	for (u=fn->tmp[t].use; u<&fn->tmp[t].use[fn->tmp[t].nuse]; u++) {
		if (u->type != UIns)
			return 0;
		i = u->u.ins;
		if (isload(i->op) && req(i->arg[0], TMP(t)))
			continue;
		if (isstore(i->op) && req(i->arg[1], TMP(t)) && !req(i->arg[0], TMP(t)))
			continue;
		if (i->op == Oblit0 && !req(i->arg[0], i->arg[1]))
			continue;
		if (i->op != Oadd || rtype(i->to) != RTmp || fn->tmp[i->to.val].ndef != 1)
			return 0;
		r = req(i->arg[0], TMP(t)) ? i->arg[1] : i->arg[0];
		if (rtype(r) != RCon || fn->con[r.val].type != CBits)
			return 0;
		if (!reach(i->to.val, c, off + fn->con[r.val].bits.i, fn))
			return 0;
	}
	return 1;
}

static int
chopat(Ref r, int64_t *off)
{
	int c;

	if (rtype(r) != RTmp || r.val >= nofs || !chopof[r.val])
		return -1;
	c = chopof[r.val] - 1;
	if (chops[c].bad)
		return -1;
	*off = offof[r.val];
	return c;
}

static int
chopnext(Chop *c, int p)
{
	while (!c->cut[++p])
		;
	return p;
}

static int
chopcls(int k, int sz)
{
	if (sz < 8)
		return k == Ks && sz == 4 ? Ks : Kw;
	return k == Kd ? Kd : Kl;
}

static int
piececls(Chop *c, int p)
{
	if (c->cls[p] >= 0)
		return c->cls[p] & 3;
	return chopnext(c, p) - p < 8 ? Kw : Kl;
}

static int
chopst(int k, int sz)
{
	switch (sz) {
	case 1: return Ostoreb;
	case 2: return Ostoreh;
	case 4: return k == Ks ? Ostores : Ostorew;
	default: return k == Kd ? Ostored : Ostorel;
	}
}

/* the accesses of i, a blit reads and writes */
static int
chopuse(Ins *i, Ref r[2], int64_t sz[2], int wr[2])
{
	if (isload(i->op)) {
		r[0] = i->arg[0];
		sz[0] = loadsz(i);
		wr[0] = 0;
		return 1;
	}
	if (isstore(i->op)) {
		r[0] = i->arg[1];
		sz[0] = storesz(i);
		wr[0] = 1;
		return 1;
	}
	if (i->op == Oblit0) {
		r[0] = i->arg[0];
		r[1] = i->arg[1];
		sz[0] = sz[1] = rsval((i+1)->arg[0]);
		wr[0] = 0;
		wr[1] = 1;
		return 2;
	}
	return 0;
}

/* where a blit reads or writes the piece at rel */
static Ref
chopref(int k, int64_t off, Ref r, int64_t rel, Fn *fn)
{
	if (k >= 0)
		return chops[k].to[off + rel];
	if (rel == 0)
		return r;
	return newtmp("chop", Kl, fn);
}

void
chop(Fn *fn)
{
	Blk *b;
	Ins *i;
	Chop *c, *c1;
	Blit *bl, *blits;
	Ref r[2], src, dst, v;
	int64_t sz[2], off[2], o, s, bits;
	int wr[2], k, m, n, p, q, t, cl, hard, grew, good, nchop, nblit, piece[ChopMax];

	nofs = fn->ntmp;
	chopof = vnew(nofs, sizeof chopof[0], PFn);
	offof = vnew(nofs, sizeof offof[0], PFn);
	memset(chopof, 0, nofs * sizeof chopof[0]);
	chops = vnew(0, sizeof chops[0], PFn);
	nchop = 0;
	for (i=fn->start->ins; i<&fn->start->ins[fn->start->nins]; i++) {
		if (!isalloc(i->op) || rtype(i->arg[0]) != RCon)
			continue;
		s = fn->con[i->arg[0].val].bits.i;
		t = i->to.val;
		if (s <= 0 || s > ChopMax || fn->tmp[t].ndef != 1)
			continue;
		vgrow(&chops, ++nchop);
		c = &chops[nchop-1];
		c->sz = s;
		c->cut = vnew(s+1, 1, PFn);
		c->rd = vnew(s, 1, PFn);
		c->wr = vnew(s, 1, PFn);
		c->cls = vnew(s, sizeof c->cls[0], PFn);
		c->to = vnew(s, sizeof c->to[0], PFn);
		memset(c->cut, 0, s + 1);
		memset(c->rd, 0, s);
		memset(c->wr, 0, s);
		for (p=0; p<s; p++)
			c->cls[p] = -1;
		c->bad = !reach(t, nchop-1, 0, fn);
	}
	if (!nchop)
		return;
	blits = vnew(0, sizeof blits[0], PFn);
	nblit = 0;
	for (b=fn->start; b; b=b->link)
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			n = chopuse(i, r, sz, wr);
			for (m=0; m<n; m++) {
				k = chopat(r[m], &off[m]);
				if (k < 0)
					continue;
				c = &chops[k];
				if (off[m] < 0 || sz[m] <= 0 || off[m] + sz[m] > c->sz)
					c->bad = 1;
				else
					c->cut[off[m]] = c->cut[off[m] + sz[m]] = 1;
			}
			if (n == 2 && (k = chopat(r[0], &off[0])) >= 0 && (m = chopat(r[1], &off[1])) >= 0) {
				if (k == m) {
					chops[k].bad = 1;
					continue;
				}
				vgrow(&blits, ++nblit);
				blits[nblit-1] = (Blit){k, m, {off[0], off[1]}, sz[0]};
			}
		}
	do {
		grew = 0;
		for (bl=blits; bl<&blits[nblit]; bl++) {
			c = &chops[bl->k];
			c1 = &chops[bl->m];
			if (c->bad || c1->bad)
				continue;
			for (o=0; o<=bl->sz; o++)
				if (c->cut[bl->off[0]+o] != c1->cut[bl->off[1]+o]) {
					c->cut[bl->off[0]+o] = c1->cut[bl->off[1]+o] = 1;
					grew = 1;
				}
		}
	} while (grew);
	for (c=chops; c<&chops[nchop]; c++)
		if (!c->bad)
			for (p=0; p<c->sz; p=q) {
				if (!c->cut[p]) {
					q = p + 1;
					continue;
				}
				q = chopnext(c, p);
				for (s=8; s>q-p; s/=2)
					;
				if (s < q - p)
					c->cut[q = p + s] = 1;
			}
	for (b=fn->start; b; b=b->link)
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			n = chopuse(i, r, sz, wr);
			hard = n == 1 && !(wr[0] && rtype(i->arg[0]) == RCon && fn->con[i->arg[0].val].type == CBits);
			for (m=0; m<n; m++) {
				k = chopat(r[m], &o);
				if (k < 0)
					continue;
				c = &chops[k];
				for (p=o; p<o+sz[m]; p++) {
					if (!c->cut[p])
						continue;
					if (hard && p != o)
						c->bad = 1;
					if (wr[m])
						c->wr[p] = 1;
					else
						c->rd[p] = 1;
				}
				if (!hard || c->bad)
					continue;
				/* stores choose the class, loads
				 * only when no store does */
				if (isstore(i->op))
					cl = chopcls(optab[i->op].argcls[0][0], sz[m]);
				else
					cl = chopcls(i->cls, sz[m]) | 4;
				if (c->cls[o] < 0 || (c->cls[o] & 4) && !(cl & 4))
					c->cls[o] = cl;
				else if (!(cl & 4) && c->cls[o] != cl)
					c->bad = 1;
			}
		}
	do {
		grew = 0;
		for (bl=blits; bl<&blits[nblit]; bl++) {
			c = &chops[bl->k];
			c1 = &chops[bl->m];
			if (c->bad || c1->bad)
				continue;
			for (o=0; o<bl->sz; o++)
				if (c->cut[bl->off[0]+o] && (c->cls[bl->off[0]+o] < 0) != (c1->cls[bl->off[1]+o] < 0)) {
					if (c->cls[bl->off[0]+o] < 0)
						c->cls[bl->off[0]+o] = c1->cls[bl->off[1]+o] | 4;
					else
						c1->cls[bl->off[1]+o] = c->cls[bl->off[0]+o] | 4;
					grew = 1;
				}
		}
	} while (grew);
	good = 0;
	for (c=chops; c<&chops[nchop]; c++)
		if (!c->bad)
			for (p=0; p<c->sz; p++)
				if (c->cut[p] && (c->rd[p] || c->wr[p])) {
					c->to[p] = newtmp("chop", Kl, fn);
					good = 1;
				}
	if (!good)
		return;
	for (b=fn->start; b; b=b->link) {
		curi = &insb[NIns];
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			if (i->op == Oblit1) {
				i--;
				chopuse(i, r, sz, wr);
				k = chopat(r[0], &off[0]);
				m = chopat(r[1], &off[1]);
				if (k < 0 && m < 0) {
					emiti(*(i+1));
					emiti(*i);
					continue;
				}
				c = &chops[m >= 0 ? m : k];
				o = off[m >= 0];
				for (n=0, p=o; p<o+sz[0]; p=chopnext(c, p))
					piece[n++] = p;
				while (n--) {
					p = piece[n];
					s = chopnext(c, p) - p;
					cl = piececls(c, p);
					src = chopref(k, off[0], r[0], p - o, fn);
					dst = chopref(m, off[1], r[1], p - o, fn);
					v = newtmp("chop", cl, fn);
					emit(chopst(cl, s), 0, R, v, dst);
					if (m < 0 && p != o)
						emit(Oadd, Kl, dst, r[1], getcon(p - o, fn));
					emit(s == 1 ? Oloadub : s == 2 ? Oloaduh : Oload, cl, v, src, R);
					if (k < 0 && p != o)
						emit(Oadd, Kl, src, r[0], getcon(p - o, fn));
				}
				continue;
			}
			if ((isalloc(i->op) || i->op == Oadd) && chopat(i->to, &o) >= 0)
				continue;
			if (isload(i->op) && (k = chopat(i->arg[0], &o)) >= 0) {
				emit(i->op, i->cls, i->to, chops[k].to[o], R);
				continue;
			}
			if (isstore(i->op) && (k = chopat(i->arg[1], &o)) >= 0) {
				c = &chops[k];
				s = storesz(i);
				if (chopnext(c, o) - o == s) {
					emit(chopst(piececls(c, o), s), 0, R, i->arg[0], c->to[o]);
					continue;
				}
				bits = fn->con[i->arg[0].val].bits.i;
				for (p=o; p<o+s; p=q) {
					q = chopnext(c, p);
					v = getcon(q - p == 8 ? bits >> 8*(p-o) : bits >> 8*(p-o) & ((1ll << 8*(q-p)) - 1), fn);
					emit(chopst(piececls(c, p), q - p), 0, R, v, c->to[p]);
				}
				continue;
			}
			emiti(*i);
		}
		/* a piece read before a path writes it reads zero */
		if (b == fn->start)
			for (c=chops; c<&chops[nchop]; c++)
				for (p=0; p<c->sz; p++)
					if (!c->bad && c->cut[p] && (c->rd[p] || c->wr[p])) {
						s = chopnext(c, p) - p;
						if (c->rd[p])
							emit(chopst(piececls(c, p), s), 0, R, getcon(0, fn), c->to[p]);
						emit(s <= 4 ? Oalloc4 : Oalloc8, Kl, c->to[p], getcon(s, fn), R);
					}
		idup(b, curi, &insb[NIns]-curi);
	}
}
