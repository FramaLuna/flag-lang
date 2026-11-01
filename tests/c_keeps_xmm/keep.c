/* Windows has a function keep xmm6 to xmm15 for its caller */
int kept(void (*run)(void));

#ifdef _WIN32
__asm__(
	"\t.text\n"
	"\t.globl kept\n"
	"kept:\n"
	"\tsubq $200, %rsp\n"
	"\tmovaps %xmm6, 32(%rsp)\n"
	"\tmovaps %xmm7, 48(%rsp)\n"
	"\tmovaps %xmm8, 64(%rsp)\n"
	"\tmovaps %xmm9, 80(%rsp)\n"
	"\tmovaps %xmm10, 96(%rsp)\n"
	"\tmovaps %xmm11, 112(%rsp)\n"
	"\tmovaps %xmm12, 128(%rsp)\n"
	"\tmovaps %xmm13, 144(%rsp)\n"
	"\tmovaps %xmm14, 160(%rsp)\n"
	"\tmovaps %xmm15, 176(%rsp)\n"
	"\tmovl $6, %eax\n\tmovq %rax, %xmm6\n"
	"\tmovl $7, %eax\n\tmovq %rax, %xmm7\n"
	"\tmovl $8, %eax\n\tmovq %rax, %xmm8\n"
	"\tmovl $9, %eax\n\tmovq %rax, %xmm9\n"
	"\tmovl $10, %eax\n\tmovq %rax, %xmm10\n"
	"\tmovl $11, %eax\n\tmovq %rax, %xmm11\n"
	"\tmovl $12, %eax\n\tmovq %rax, %xmm12\n"
	"\tmovl $13, %eax\n\tmovq %rax, %xmm13\n"
	"\tmovl $14, %eax\n\tmovq %rax, %xmm14\n"
	"\tmovl $15, %eax\n\tmovq %rax, %xmm15\n"
	"\tcallq *%rcx\n"
	"\txorl %ecx, %ecx\n"
	"\tmovq %xmm6, %rdx\n\txorq $6, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm7, %rdx\n\txorq $7, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm8, %rdx\n\txorq $8, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm9, %rdx\n\txorq $9, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm10, %rdx\n\txorq $10, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm11, %rdx\n\txorq $11, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm12, %rdx\n\txorq $12, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm13, %rdx\n\txorq $13, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm14, %rdx\n\txorq $14, %rdx\n\torq %rdx, %rcx\n"
	"\tmovq %xmm15, %rdx\n\txorq $15, %rdx\n\torq %rdx, %rcx\n"
	"\txorl %eax, %eax\n"
	"\ttestq %rcx, %rcx\n"
	"\tsete %al\n"
	"\tmovaps 32(%rsp), %xmm6\n"
	"\tmovaps 48(%rsp), %xmm7\n"
	"\tmovaps 64(%rsp), %xmm8\n"
	"\tmovaps 80(%rsp), %xmm9\n"
	"\tmovaps 96(%rsp), %xmm10\n"
	"\tmovaps 112(%rsp), %xmm11\n"
	"\tmovaps 128(%rsp), %xmm12\n"
	"\tmovaps 144(%rsp), %xmm13\n"
	"\tmovaps 160(%rsp), %xmm14\n"
	"\tmovaps 176(%rsp), %xmm15\n"
	"\taddq $200, %rsp\n"
	"\tret\n"
);
#else
int kept(void (*run)(void))
{
	run();
	return 1;
}
#endif
