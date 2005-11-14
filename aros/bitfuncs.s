	.file	"bitfuncs.c"
	.text
	.align 4
.globl bmffo
	.type	bmffo, @function
bmffo:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	movl	16(%ebp), %esi
	movl	%esi, %eax
	sarl	$5, %eax
	movl	8(%ebp), %edi
	subl	%eax, 12(%ebp)
	andl	$31, %esi
	pushl	%ebx
	leal	(%edi,%eax,4), %ebx
	je	.L2
	movl	(%ebx), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	sall	$16, %edx
	movzwl	%ax, %eax
	orl	%eax, %edx
	movl	%esi, %ecx
	movl	$-1, %eax
	shrl	%cl, %eax
	andl	%edx, %eax
	movl	$-1, %esi
	je	.L8
#APP
	bsr %eax, %eax
#NO_APP
	movl	$31, %esi
	subl	%eax, %esi
.L8:
	testl	%esi, %esi
	js	.L3
	subl	%edi, %ebx
	sarl	$2, %ebx
	sall	$5, %ebx
	leal	(%ebx,%esi), %eax
.L1:
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
.L3:
	addl	$4, %ebx
	decl	12(%ebp)
	.align 4
.L2:
	movl	12(%ebp), %eax
	decl	12(%ebp)
	testl	%eax, %eax
	jle	.L20
	.align 4
.L18:
	movl	(%ebx), %eax
	addl	$4, %ebx
	testl	%eax, %eax
	jne	.L21
	movl	12(%ebp), %eax
	decl	12(%ebp)
	testl	%eax, %eax
	jg	.L18
.L20:
	movl	$-1, %eax
	jmp	.L1
.L21:
	subl	$4, %ebx
	movl	(%ebx), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	sall	$16, %edx
	movzwl	%ax, %eax
	orl	%edx, %eax
	movl	$-1, %ecx
	je	.L17
#APP
	bsr %eax, %eax
#NO_APP
	movl	$31, %ecx
	subl	%eax, %ecx
.L17:
	subl	%edi, %ebx
	sarl	$2, %ebx
	sall	$5, %ebx
	leal	(%ebx,%ecx), %eax
	jmp	.L1
	.size	bmffo, .-bmffo
	.align 4
.globl bmffz
	.type	bmffz, @function
bmffz:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	movl	16(%ebp), %esi
	movl	%esi, %eax
	sarl	$5, %eax
	movl	8(%ebp), %edi
	subl	%eax, 12(%ebp)
	andl	$31, %esi
	pushl	%ebx
	leal	(%edi,%eax,4), %ebx
	je	.L23
	movl	(%ebx), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	sall	$16, %edx
	movzwl	%ax, %eax
	orl	%eax, %edx
	movl	%esi, %ecx
	movl	$-1, %eax
	notl	%edx
	shrl	%cl, %eax
	andl	%eax, %edx
	je	.L25
#APP
	bsr %edx, %eax
#NO_APP
	movl	$31, %edx
	subl	%eax, %edx
.L29:
	testl	%edx, %edx
	js	.L24
	subl	%edi, %ebx
	sarl	$2, %ebx
	sall	$5, %ebx
	leal	(%ebx,%edx), %eax
.L22:
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
.L24:
	addl	$4, %ebx
	decl	12(%ebp)
	.align 4
.L23:
	movl	12(%ebp), %eax
	decl	12(%ebp)
	testl	%eax, %eax
	jle	.L43
	.align 4
.L41:
	movl	(%ebx), %eax
	addl	$4, %ebx
	incl	%eax
	jne	.L44
	movl	12(%ebp), %eax
	decl	12(%ebp)
	testl	%eax, %eax
	jg	.L41
.L43:
	movl	$-1, %eax
	jmp	.L22
.L44:
	subl	$4, %ebx
	movl	(%ebx), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	sall	$16, %edx
	movzwl	%ax, %eax
	orl	%eax, %edx
	movl	%edx, %eax
	xorl	$-1, %eax
	movl	$-1, %ecx
	je	.L39
#APP
	bsr %eax, %eax
#NO_APP
	movl	$31, %ecx
	subl	%eax, %ecx
.L39:
	subl	%edi, %ebx
	sarl	$2, %ebx
	sall	$5, %ebx
	leal	(%ebx,%ecx), %eax
	jmp	.L22
.L25:
	movl	$-1, %edx
	jmp	.L29
	.size	bmffz, .-bmffz
	.align 4
.globl bmclr
	.type	bmclr, @function
bmclr:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ecx
	movl	16(%ebp), %eax
	sarl	$5, %eax
	movl	12(%ebp), %esi
	leal	0(,%eax,4), %edi
	movl	20(%ebp), %ebx
	subl	%eax, %esi
	addl	8(%ebp), %edi
	andl	$31, 16(%ebp)
	movl	%ebx, -16(%ebp)
	je	.L46
	cmpl	$31, %ebx
	jg	.L47
	movl	(%edi), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	movzwl	%ax, %eax
	sall	$16, %edx
	movl	$32, %ecx
	orl	%eax, %edx
	subl	%ebx, %ecx
	movl	$-1, %eax
	sall	%cl, %eax
.L62:
	movb	16(%ebp), %cl
	shrl	%cl, %eax
	notl	%eax
	andl	%eax, %edx
	movl	%edx, %eax
	shrl	$16, %edx
	rolw	$8, %ax
	rolw	$8, %dx
	sall	$16, %eax
	movzwl	%dx, %edx
	orl	%edx, %eax
	movl	%eax, (%edi)
	movl	16(%ebp), %eax
	addl	$4, %edi
	decl	%esi
	leal	-32(%eax,%ebx), %ebx
.L46:
	testl	%ebx, %ebx
	jle	.L61
	movl	%esi, %eax
	decl	%esi
	testl	%eax, %eax
	jle	.L52
	.align 4
.L58:
	cmpl	$31, %ebx
	jle	.L55
	movl	$0, (%edi)
	addl	$4, %edi
.L56:
	subl	$32, %ebx
	testl	%ebx, %ebx
	jle	.L61
	movl	%esi, %eax
	decl	%esi
	testl	%eax, %eax
	jg	.L58
.L52:
	testl	%ebx, %ebx
	jle	.L61
	movl	-16(%ebp), %eax
	subl	%ebx, %eax
.L45:
	popl	%edx
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
	.align 4
.L61:
	movl	-16(%ebp), %eax
	jmp	.L45
	.align 4
.L55:
	movl	(%edi), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	movzwl	%ax, %eax
	sall	$16, %edx
	movl	$32, %ecx
	orl	%eax, %edx
	subl	%ebx, %ecx
	movl	$1, %eax
	sall	%cl, %eax
	decl	%eax
	andl	%eax, %edx
	movl	%edx, %eax
	shrl	$16, %edx
	rolw	$8, %ax
	rolw	$8, %dx
	sall	$16, %eax
	movzwl	%dx, %edx
	orl	%edx, %eax
	movl	%eax, (%edi)
	jmp	.L56
.L47:
	movl	(%edi), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	movzwl	%ax, %eax
	sall	$16, %edx
	orl	%eax, %edx
	movl	$-1, %eax
	jmp	.L62
	.size	bmclr, .-bmclr
	.align 4
.globl bmset
	.type	bmset, @function
bmset:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%esi
	movl	16(%ebp), %eax
	sarl	$5, %eax
	movl	12(%ebp), %esi
	leal	0(,%eax,4), %edi
	movl	20(%ebp), %ebx
	subl	%eax, %esi
	addl	8(%ebp), %edi
	andl	$31, 16(%ebp)
	movl	%ebx, -16(%ebp)
	je	.L64
	cmpl	$31, %ebx
	jg	.L65
	movl	(%edi), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	movzwl	%ax, %eax
	sall	$16, %edx
	movl	$32, %ecx
	orl	%eax, %edx
	subl	%ebx, %ecx
	movl	$-1, %eax
	sall	%cl, %eax
.L80:
	movb	16(%ebp), %cl
	shrl	%cl, %eax
	orl	%eax, %edx
	movl	%edx, %eax
	shrl	$16, %edx
	rolw	$8, %ax
	rolw	$8, %dx
	sall	$16, %eax
	movzwl	%dx, %edx
	orl	%edx, %eax
	movl	%eax, (%edi)
	movl	16(%ebp), %eax
	addl	$4, %edi
	decl	%esi
	leal	-32(%eax,%ebx), %ebx
.L64:
	testl	%ebx, %ebx
	jle	.L79
	movl	%esi, %eax
	decl	%esi
	testl	%eax, %eax
	jle	.L70
	.align 4
.L76:
	cmpl	$31, %ebx
	jle	.L73
	movl	$-1, (%edi)
	addl	$4, %edi
.L74:
	subl	$32, %ebx
	testl	%ebx, %ebx
	jle	.L79
	movl	%esi, %eax
	decl	%esi
	testl	%eax, %eax
	jg	.L76
.L70:
	testl	%ebx, %ebx
	jle	.L79
	movl	-16(%ebp), %eax
	subl	%ebx, %eax
.L63:
	popl	%ebx
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
	.align 4
.L79:
	movl	-16(%ebp), %eax
	jmp	.L63
	.align 4
.L73:
	movl	(%edi), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	movzwl	%ax, %eax
	sall	$16, %edx
	movl	$32, %ecx
	orl	%eax, %edx
	subl	%ebx, %ecx
	movl	$-1, %eax
	sall	%cl, %eax
	orl	%eax, %edx
	movl	%edx, %eax
	shrl	$16, %edx
	rolw	$8, %ax
	rolw	$8, %dx
	sall	$16, %eax
	movzwl	%dx, %edx
	orl	%edx, %eax
	movl	%eax, (%edi)
	jmp	.L74
.L65:
	movl	(%edi), %eax
	movl	%eax, %edx
	shrl	$16, %eax
	rolw	$8, %dx
	rolw	$8, %ax
	movzwl	%ax, %eax
	sall	$16, %edx
	orl	%eax, %edx
	movl	$-1, %eax
	jmp	.L80
	.size	bmset, .-bmset
	.ident	"GCC: (GNU) 3.3.1"
