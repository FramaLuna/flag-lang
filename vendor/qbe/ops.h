#ifndef X /* amd64 */
	#define X(NMemArgs, SetsZeroFlag, LeavesFlags)
#endif

#ifndef V /* riscv64 */
	#define V(Imm)
#endif

#ifndef F
#define F(a,b,c,d,e,f,g,h,i,j)
#endif

#define T5(a,b,c,d,v,w,x,y,z,u) {                                 \
	{[Kw]=K##a, [Kl]=K##b, [Ks]=K##c, [Kd]=K##d, [Kv]=K##v}, \
	{[Kw]=K##w, [Kl]=K##x, [Ks]=K##y, [Kd]=K##z, [Kv]=K##u}  \
}
#define T(a,b,c,d,w,x,y,z) T5(a,b,c,d,e,w,x,y,z,e)
#define TV(a,b) T5(e,e,e,e,a,e,e,e,e,b) /* lanes: v */

/*********************/
/* PUBLIC OPERATIONS */
/*********************/

/*                                can fold                        */
/*                                | has identity                  */
/*                                | | identity value for arg[1]   */
/*                                | | | commutative               */
/*                                | | | | associative             */
/*                                | | | | | idempotent            */
/*                                | | | | | | c{eq,ne}[wl]        */
/*                                | | | | | | | c[us][gl][et][wl] */
/*                                | | | | | | | | value if = args */
/*                                | | | | | | | | | pinned        */
/* Arithmetic and Bits            v v v v v v v v v v             */
O(add,     T(w,l,s,d, w,l,s,d), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sub,     T(w,l,s,d, w,l,s,d), F(1,1,0,0,0,0,0,0,0,0)) X(2,1,0) V(0)
O(neg,     T(w,l,s,d, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(div,     T(w,l,s,d, w,l,s,d), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rem,     T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(udiv,    T(w,l,e,e, w,l,e,e), F(1,1,1,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(urem,    T(w,l,e,e, w,l,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(mul,     T(w,l,s,d, w,l,s,d), F(1,1,1,1,0,0,0,0,0,0)) X(2,0,0) V(0)
O(and,     T(w,l,e,e, w,l,e,e), F(1,0,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(or,      T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,1,0,0,0,0)) X(2,1,0) V(1)
O(xor,     T(w,l,e,e, w,l,e,e), F(1,1,0,1,1,0,0,0,0,0)) X(2,1,0) V(1)
O(sar,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shr,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)
O(shl,     T(w,l,e,e, w,w,e,e), F(1,1,0,0,0,0,0,0,0,0)) X(1,1,0) V(1)

/* Comparisons */
O(ceqw,    T(w,w,e,e, w,w,e,e), F(1,1,1,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnew,    T(w,w,e,e, w,w,e,e), F(1,1,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culew,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultw,   T(w,w,e,e, w,w,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceql,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,1,0)) X(0,1,0) V(0)
O(cnel,    T(l,l,e,e, l,l,e,e), F(1,0,0,1,0,0,1,0,0,0)) X(0,1,0) V(0)
O(csgel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csgtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(cslel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(csltl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)
O(cugel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cugtl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(0)
O(culel,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,1,0)) X(0,1,0) V(0)
O(cultl,   T(l,l,e,e, l,l,e,e), F(1,0,0,0,0,0,0,1,0,0)) X(0,1,0) V(1)

O(ceqs,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cges,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cles,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(clts,    T(s,s,e,e, s,s,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cnes,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cos,     T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuos,    T(s,s,e,e, s,s,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

O(ceqd,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cged,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cgtd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cled,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cltd,    T(d,d,e,e, d,d,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cned,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cod,     T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)
O(cuod,    T(d,d,e,e, d,d,e,e), F(1,0,0,1,0,0,0,0,0,0)) X(0,1,0) V(0)

/* Memory */
O(storeb,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storeh,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storew,  T(w,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storel,  T(l,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stores,  T(s,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(stored,  T(d,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(storev,  T(v,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

O(loadsb,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadub,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduh,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loadsw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(loaduw,  T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(load,    T5(m,m,m,m,m, x,x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* Extensions and Truncations */
O(extsb,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extub,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuh,   T(w,w,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extsw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(extuw,   T(e,w,e,e, e,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

O(exts,    T(e,e,e,s, e,e,e,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(truncd,  T(e,e,d,e, e,e,x,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stosi,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(stoui,   T(s,s,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtosi,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(dtoui,   T(d,d,e,e, x,x,e,e), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(swtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(uwtof,   T(e,e,w,w, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(sltof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(ultof,   T(e,e,l,l, e,e,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(cast,    T(s,d,w,l, x,x,x,x), F(1,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Stack Allocation */
O(alloc4,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc8,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(alloc16, T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Variadic Function Helpers */
O(vaarg,   T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(vastart, T(m,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

O(copy,    T5(w,l,s,d,v, x,x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Lanes: 16 bytes in a register, the suffix is the lane */
/* (b h w l: integers of 1 2 4 8 bytes, s d: floats)     */
O(vaddb,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vaddh,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vaddw,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vaddl,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vadds,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vaddd,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsubb,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsubh,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsubw,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsubl,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsubs,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsubd,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmulh,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmulw,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmuls,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmuld,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vdivs,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vdivd,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vand,    TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vor,     TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vxor,    TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
/* the amount is one number for every lane, past the lane it gives 0, or the sign */
O(vshlh,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vshlw,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vshll,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vshrh,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vshrw,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vshrl,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsarh,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsarw,   TV(v,w), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
/* each lane all ones where it holds, zero where not; gt is signed */
O(vceqb,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vceqh,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vceqw,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vceql,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vceqs,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vceqd,   TV(v,v), F(0,0,0,1,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcgtb,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcgth,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcgtw,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcgtl,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vclts,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcltd,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcles,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vcled,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
/* min a, b is b < a ? b : a, and max a, b is a < b ? b : a */
O(vmins,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmind,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmaxs,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vmaxd,   TV(v,v), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsqrts,  TV(v,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsqrtd,  TV(v,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
/* one number in every lane */
O(vsplatb, TV(w,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsplath, TV(w,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsplatw, TV(w,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsplatl, TV(l,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsplats, TV(s,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vsplatd, TV(d,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
/* lanes of w to lanes of s */
O(vitos,   TV(v,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Debug */
O(dbgloc,  T(w,e,e,e, w,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(dbgslot, T(m,e,e,e, w,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(dbgmark, T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(frame,   T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* amd64: ecx of cpuid with the leaf in eax */
O(cpuid,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/****************************************/
/* INTERNAL OPERATIONS (keep nop first) */
/****************************************/

/* Miscellaneous and Architecture-Specific Operations */
O(nop,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(addr,    T(m,m,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(blit0,   T(m,e,e,e, m,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(blit1,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,1,0) V(0)
O(sel0,    T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(sel1,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(swap,    T5(w,l,s,d,v, w,l,s,d,v), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(sign,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(salloc,  T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xidiv,   T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xdiv,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,0,0) V(0)
O(xcpuid,  T(x,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(xcmp,    T(w,l,s,d, w,l,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(xtest,   T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(1,1,0) V(0)
O(acmp,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(acmn,    T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(afcmp,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(reqz,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(rnez,    T(w,l,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

/* Lanes of a v, in the order of the loads and the stores: */
/* get has the lane in arg[1], set the lane of the vlane before it */
O(vgetsb,  T(v,v,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vgetub,  T(v,v,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vgetsh,  T(v,v,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vgetuh,  T(v,v,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vgetsw,  T(v,v,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vgetuw,  T(v,v,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vget,    T(v,v,v,v, x,x,x,x), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(vlane,   T(w,e,e,e, x,e,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(vsetb,   TV(v,w), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(vseth,   TV(v,w), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(vsetw,   TV(v,w), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(vsetl,   TV(v,l), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(vsets,   TV(v,s), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)
O(vsetd,   TV(v,d), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,1) V(0)

/* Arguments, Parameters, and Calls */
O(par,     T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsb,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parub,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parsh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(paruh,   T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(parc,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(pare,    T(e,x,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arg,     T(w,l,s,d, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsb,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argub,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argsh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arguh,   T(w,e,e,e, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argc,    T(e,x,e,e, e,l,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(arge,    T(e,l,e,e, e,x,e,e), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(argv,    T(x,x,x,x, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)
O(call,    T(m,m,m,m, x,x,x,x), F(0,0,0,0,0,0,0,0,0,1)) X(0,0,0) V(0)

/* Flags Setting */
O(flagieq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagine,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisgt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagisle, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagislt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiuge, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiugt, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiule, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagiult, T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfeq,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfge,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfgt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfle,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagflt,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfne,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfo,   T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)
O(flagfuo,  T(x,x,e,e, x,x,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,1) V(0)

/* Backend Flag Select (Condition Move) */
O(xselieq,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseline,  T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisgt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselisle, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselislt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliuge, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliugt, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliule, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xseliult, T(w,l,e,e, w,l,e,e), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfeq,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfge,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfgt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfle,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselflt,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfne,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfo,   T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)
O(xselfuo,  T(e,e,s,d, e,e,s,d), F(0,0,0,0,0,0,0,0,0,0)) X(0,0,0) V(0)

#undef T
#undef T5
#undef TV
#undef X
#undef V
#undef O

/*
| column -t -o ' '
*/
