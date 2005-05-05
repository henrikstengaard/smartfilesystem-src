;
; Compile with Devpac's GenAm 3.04
;
	Opt	l+,case

	XDef	_CALCCHECKSUM
	XDef	_MULU64
	XDef	_COMPRESSFROMZERO
	XDef	_COMPRESS
	XDef	_STACKSWAP
	XDef	_TOSERIAL
	XDef	_CHECKSUM_WRITELONG

	XDef	_BFFFO	BitField Find First One
	XDef	_BFFFZ	BitField Find First Zero
	XDef	_BFFLO	BitField Find Last One
	XDef	_BFFLZ	BitField Find Last Zero
	XDef	_BFCNTO	BitField CouNT Ones
	XDef	_BFCNTZ	BitField CouNT Zeroes

	XDef	_BMFFO	BitMap Find First One
	XDef	_BMFFZ	BitMap Find First Zero
	XDef	_BMFLO	BitMap Find Last One
	XDef	_BMFLZ	BitMap Find Last Zero
	XDef	_BMTSTO	BitMap TeST for One
	XDef	_BMTSTZ	BitMap TeST for Zero
	XDef	_BMCNTO	BitMap CouNT Ones
;	XDef	_BMCNTZ	BitMap CouNT Zeroes

	XDef	_SQRT

	Incdir	"INCLUDE:"

	Include	"exec/types.i"
	Include	"exec/nodes.i"
	Include	"exec/tasks.i"
	Include	"exec/libraries.i"
	Include	"dos/dos.i"

	Include	"blockstructure.i"

Start

MuluLWQ	MACRO	;DataReg1,DataReg2
	;Reg1.L x Reg2.W = Reg1.L:Reg2.L
	Move.l	a0,-(sp)
	Move.l	\1,a0
	Mulu.w	\2,\1
	Exg	\1,a0
	Swap	\1
	Mulu.w	\1,\2
	Swap	\2
	Moveq	#0,\1
	Move.w	\2,\1
	Clr.w	\2
	Add.l	a0,\2
	Bcc.s	.ExitRoutine\@
	Addq.l	#1,\1
.ExitRoutine\@	Move.l	(sp)+,a0
	ENDM

MuluLLQ	MACRO	;DataReg1,DataReg2
	;Reg1.L x Reg2.L = Reg1.L:Reg2.L
	Movem.l	a0-a1,-(sp)
	Movem.l	\1/\2,-(sp)
	MuluLWQ	\1,\2
	Move.l	\1,a0
	Move.l	\2,a1
	Movem.l	(sp)+,\1/\2
	Swap	\2
	MuluLWQ	\1,\2
	Swap	\1
	Swap	\2
	Move.w	\2,\1
	Clr.w	\2
	Add.l	a1,\2
	Bcc.s	.NoOverFlow\@
	Addq.l	#1,\1
.NoOverFlow\@	Add.l	a0,\1
	Movem.l	(sp)+,a0-a1
	ENDM


	CNOP	0,4
_MULU64	; IN: d0.l = 32-bit number
	; IN: d1.l = 32-bit number
	; IN: a0.l = Ptr to 4 bytes for low 32 bits
	;OUT: d0.l = High 32 bits

;	Mulu.l	d0,d0:d1	68020+ instruction (not allowed on 060!)

	MuluLLQ	d0,d1
	Move.l	d1,(a0)
	Rts


	CNOP	0,4
_SQRT	; IN: d0.l = 32-bit number
	;OUT: d0.w = Square root of d0.  The result is always accurate for integer
	;            sqrts, but is rounded up/down arbitrarely for non-integer sqrts.
	;            So, 4 -> 2, 9 -> 3, but 3 can give 1 or 2 and 8 can give 2 or 3.
	Move.l	d0,a0
	Moveq	#127,d1
.Loop	Move.l	a0,d0
	Divu.l	d1,d0
	Add.l	d0,d1
	Lsr.l	#1,d1
	Sub.l	d1,d0
	Addq.l	#1,d0
	Cmp.l	#2,d0
	Bhi.s	.Loop

	Move.l	d1,d0
	Rts

;8 / 127 = 0
;(0 + 127) / 2 = 63
;8 / 63 = 0
;(0 + 63) / 2 = 31
;8 / 31 = 0 -> 15
;8 / 15 = 0 -> 7
;8 / 7 = 1
;(1 + 7) / 2 = 4
;8 / 4 = 2
;(2 + 4) / 2 = 3
;8 / 3 = 2
;(2 + 3) / 2 = 2

	CNOP	0,4
_CHECKSUM_WRITELONG	; IN: a0.l - Ptr to BlockHeader
	; IN: a1.l - Ptr to destination long
	; IN: d0.l - Data

	Move.l	(a1),d1
	Move.l	d0,(a1)

	Exg.l	a1,d1
	Sub.l	a0,d1
	And.w	#3,d1
	Exg.l	a1,d1	Does not modify CC
	Beq.s	.LongAligned

	Swap	d0
	Swap	d1

.LongAligned	Sub.l	d1,d0
	Move.l	fsBH_checksum(a0),d1
	Not.l	d1
	Add.l	d0,d1
	Not.l	d1
	Move.l	d1,fsBH_checksum(a0)
	Rts



	CNOP	0,4
_STACKSWAP	; IN: a0.l - Function to call with new stack.
	; IN: d0.l - Minimum stack size.
	Movem.l	d6/a2/a5-a6,-(sp)
	Move.l	a0,a5
	Move.l	d0,d6

	Move.l	4.w,a6
	Cmp.w	#36,LIB_VERSION(a6)
	Blt.s	.NoMemory
	Suba.l	a1,a1
	Jsr	-294(a6)	Exec - FindTask
	Move.l	d0,a0

	Move.l	TC_SPUPPER(a0),d0
	Sub.l	TC_SPLOWER(a0),d0
	Cmp.l	d6,d0
	Bge.s	.KeepStack

	Move.l	#StackSwapStruct_SIZEOF+4096,d0
	Moveq	#0,d1
	Jsr	-198(a6)	Exec - AllocMem
	Tst.l	d0
	Beq.s	.NoMemory
	Move.l	d0,a2

	Lea	StackSwapStruct_SIZEOF(a2),a1
	Move.l	a1,stk_Lower(a2)
	Add.w	#4080,a1
	Move.l	a1,stk_Pointer(a2)
	Addq.l	#8,a1
	Move.l	a1,stk_Upper(a2)

	Move.l	a2,a0
	Jsr	-732(a6)	Exec - StackSwap

	Jsr	(a5)	Run with new stack
	Move.l	d0,d6

	Move.l	4.w,a6
	Move.l	a2,a0
	Jsr	-732(a6)	Exec - StackSwap

	Move.l	a2,a1
	Move.l	#StackSwapStruct_SIZEOF+4096,d0
	Jsr	-210(a6)	Exec - FreeMem
	Move.l	d6,d0
	Bra.s	.Exit

.KeepStack	Jsr	(a5)	Run with original stack
	Bra.s	.Exit

.NoMemory	Moveq	#ERROR_NO_FREE_STORE,d0
.Exit	Movem.l	(sp)+,d6/a2/a5-a6
	Rts



;	CNOP	0,4
;_STACKSWAP	Move.l	(a7),d7
;
;	Move.l	4.w,a6
;	Suba.l	a1,a1
;	Jsr	-294(a6)	Exec - FindTask
;	Move.l	d0,a0
;
;	Move.l	TC_SPUPPER(a0),d0
;	Sub.l	TC_SPLOWER(a0),d0
;	Cmp.l	#4000,d0
;	Bge.s	.KeepStack
;
;	Move.l	#StackSwapStruct_SIZEOF+4096,d0
;	Moveq	#0,d1
;	Jsr	-198(a6)	Exec - AllocMem
;	Tst.l	d0
;	Beq.s	.Exit
;	Move.l	d0,a0
;
;	Lea	StackSwapStruct_SIZEOF(a0),a1
;	Move.l	a1,stk_Lower(a0)
;	Add.w	#4080,a1
;	Move.l	a1,stk_Pointer(a0)
;	Addq.l	#8,a1
;	Move.l	a1,stk_Upper(a0)
;	Jsr	-732(a6)	Exec - StackSwap
;	Move.l	d7,(a7)
;
;.KeepStack	Moveq	#-1,d0
;	Rts
;.Exit	Moveq	#0,d0
;	Rts



	CNOP	0,4
_CALCCHECKSUM	; IN: d0.l = Blocksize in bytes (must be a multiple of 16!)
	; IN: a0.l = ULONG *
	;OUT: d0.l = Checksum

	Move.l	d0,d1
	Lsr.l	#4,d1
	Subq.l	#1,d1
	Moveq	#1,d0

.Loop	Add.l	(a0)+,d0
	Add.l	(a0)+,d0
	Add.l	(a0)+,d0
	Add.l	(a0)+,d0
	Dbra	d1,.Loop

	Rts



	CNOP	0,4
_COMPRESSFROMZERO	; IN: a0.l UWORD *new
	; IN: a1.l UBYTE *dest
	; IN: d0.w bytes_block
	;OUT: d0.l Compressed length

	Movem.l	d2-d3/a2-a4,-(sp)

	Move.l	a1,a4
	Lea	(a0,d0.w),a2
	Moveq	#-1,d2

.Loop	Move.l	a2,d1
	Sub.l	a0,d1	d1 = 2
	Lsr.l	#1,d1	d1 = 1
	Subq.l	#1,d1	d1 = 0

	Cmp.w	#64-1,d1
	Ble.s	.Continue
	Moveq	#64-1,d1

.Continue	Move.w	d1,d3
	Subq.w	#1,d3
	Move.w	(a0),d0
	Bne.s	.NotClear

.LoopClr	Tst.w	(a0)+
	Dbne	d1,.LoopClr

	; Above loop will be left when (a3) wasn't zero or when d1 reaches -1.  If the
	; loop is left because (a3) wasn't zero then d1 won't be decreased by one.

	; d1 = 62 -> 1 zero word found.
	; d1 = 0  -> 63 zero words found.
	; d1 = -1 -> 64 zero words found.

	Tst.b	d1
	Blt.s	.Skip
	Subq.l	#2,a0

.Skip	Sub.b	d1,d3
	Move.b	d3,(a1)+
	Bra.s	.DoLoop

.NotClear	Cmp.w	d2,d0
	Bne.s	.NotSet

.LoopSet	Cmp.w	(a0)+,d2
	Dbne	d1,.LoopSet

	Tst.b	d1
	Blt.s	.Skip2
	Subq.l	#2,a0

.Skip2	Sub.b	d1,d3
	Or.b	#$80,d3
	Move.b	d3,(a1)+
	Bra.s	.DoLoop

.NotSet	Move.l	a1,a3
	Addq.l	#1,a1
;	Move.w	d0,(a1)+

.LoopAny	Move.w	(a0)+,d0
	Beq.s	.Stop
	Cmp.w	d2,d0
	Beq.s	.Stop
	Move.w	d0,(a1)+
	Dbra	d1,.LoopAny

	Bra.s	.Skip3

.Stop	Subq.l	#2,a0
.Skip3	Sub.b	d1,d3
	Or.b	#$C0,d3
	Move.b	d3,(a3)

.DoLoop	Cmp.l	a2,a0
	Blt.s	.Loop

	Move.l	a1,d0
	Sub.l	a4,d0

	Movem.l	(sp)+,d2-d3/a2-a4
	Rts




	CNOP	0,4
_COMPRESS	; IN: a0.l UWORD *new
	; IN: a1.l UWORD *old
	; IN: a6.l UBYTE *dest
	; IN: d0.w bytes_block
	;OUT: d0.l Compressed length

	Movem.l	d2-d4/a2-a4/a6,-(sp)

	Lea	(a0,d0.w),a2
	Moveq	#-1,d2

.Loop	Move.l	a2,d1
	Sub.l	a0,d1	d1 = 2
	Lsr.l	#1,d1	d1 = 1
	Subq.l	#1,d1	d1 = 0

	Cmp.w	#64-1,d1
	Ble.s	.Continue
	Moveq	#64-1,d1

.Continue	Move.w	d1,d3
	Subq.b	#1,d3
	Move.w	d1,d4
	Move.l	a0,a3
	Move.l	a1,a4

.LoopUnchanged	Cmpm.w	(a3)+,(a4)+
	Dbne	d4,.LoopUnchanged

	Move.w	(a0),d0
	Bne.s	.NotClear

.LoopClr	Tst.w	(a0)+
	Dbne	d1,.LoopClr

	; Above loop will be left when (a3) wasn't zero or when d1 reaches -1.  If the
	; loop is left because (a3) wasn't zero then d1 won't be decreased by one.

	; d1 = 62 -> 1 zero word found.
	; d1 = 0  -> 63 zero words found.
	; d1 = -1 -> 64 zero words found.

	Cmp.w	d1,d4	0,3 -> 3-0 = 3
	Blt.s	.Unchanged

	Tst.b	d1
	Blt.s	.Skip
	Subq.l	#2,a0

.Skip	Sub.b	d1,d3
	Lea	2(a1,d3.w*2),a1
	Move.b	d3,(a6)+
	Bra.s	.DoLoop


.NotClear	Cmp.w	d2,d0
	Bne.s	.NotSet

.LoopSet	Cmp.w	(a0)+,d2
	Dbne	d1,.LoopSet

	Cmp.w	d1,d4
	Blt.s	.Unchanged

	Tst.b	d1
	Blt.s	.Skip2
	Subq.l	#2,a0

.Skip2	Sub.b	d1,d3
	Lea	2(a1,d3.w*2),a1
	Or.b	#$80,d3
	Move.b	d3,(a6)+
	Bra.s	.DoLoop


.Unchanged	Tst.b	d4
	Blt.s	.Skip4
	Subq.l	#2,a3
	Subq.l	#2,a4

.Skip4	Sub.b	d4,d3
	Or.b	#$40,d3
	Move.b	d3,(a6)+
	Move.l	a3,a0
	Move.l	a4,a1
	Bra.s	.DoLoop


.NotSet	Cmp.b	d4,d3
	Bge.s	.Unchanged

	Move.l	a6,a3
	Addq.l	#1,a6

.LoopAny	Move.w	(a0)+,d0
	Beq.s	.Stop
	Cmp.w	d2,d0
	Beq.s	.Stop
	Cmp.w	(a1)+,d0
	Beq.s	.Stop2
	Move.w	d0,(a6)+
	Dbra	d1,.LoopAny

	Bra.s	.Skip3

.Stop2	Subq.l	#2,a1
.Stop	Subq.l	#2,a0
.Skip3	Sub.b	d1,d3
	;Lea	2(a1,d3.w*2),a1
	Or.b	#$C0,d3
	Move.b	d3,(a3)

.DoLoop	Cmp.l	a2,a0
	Blt	.Loop

	Move.l	a6,d0

	Movem.l	(sp)+,d2-d4/a2-a4/a6
	Sub.l	a6,d0
	Rts


	CNOP	0,4
_TOSERIAL	; IN: a0.l = Null terminated string.

	Lea	$dff000,a1
	Move.w	#30,$032(a1)	SERPER	30 = 115200 baud
	;Move.w	#185,$032(a1)	SERPER	372 = 9600 baud

.NextByte	Move.w	#$100,d0
	Move.b	(a0)+,d0
	Beq.s	.Exit
	Move.w	d0,$030(a1)	SERDAT

.Loop	Move.w	$018(a1),d1	SERDATR
	Btst	#13,d1	TBE
	Beq.s	.Loop	Wait for TBE to be set.

	Cmp.b	#10,d0
	Bne.s	.NextByte

	; Insert Carriage Return.

	Moveq	#0,d0	Kill d0 :-)
	Move.w	#$10d,$030(a1)	SERDAT
	Bra.s	.Loop

.Exit	Rts



	CNOP	0,4
_BFFFO	; IN: d0.l = 32-bit number
	;OUT: d0.l = First set bit (0 is MSB, 31 is LSB and 32 indicates no bits were set).
	Bfffo	d0{0:32},d0
	Rts



	CNOP	0,4
_BFFFZ	; IN: d0.l = 32-bit number
	;OUT: d0.l = First cleared bit (0 is MSB, 31 is LSB and 32 indicates no bits were set).
	Not.l	d0
	Bfffo	d0{0:32},d0
	Rts



	CNOP	0,4
_BFFLO	; IN: d0.l = 32-bit number
	;OUT: d0.l = Last set bit (0 is MSB, 31 is LSB and -1 indicates no bits were set).

	; Scans the passed in 32-bit value for the first set bit starting
	; from the LSB.

	Move.l	d0,d1
	Moveq	#31,d0

.Loop	Lsr.l	#1,d1
	Dbcs	d0,.Loop

	Ext.l	d0
	Rts



	CNOP	0,4
_BFFLZ	; IN: d0.l = 32-bit number
	;OUT: d0.l = Last cleared bit (0 is MSB, 31 is LSB and -1 indicates no bits were set).

	; Scans the passed in 32-bit value for the first cleared bit starting
	; from the LSB.

	Move.l	d0,d1
	Moveq	#31,d0

.Loop	Lsr.l	#1,d1
	Dbcc	d0,.Loop

	Ext.l	d0
	Rts



	CNOP	0,4
_BFCNTO	; IN: d1.l = 32-bit number
	;OUT: d0.l = Number of set bits
	Move.l	d2,-(sp)
	Moveq	#0,d0
	Moveq	#31,d2

.Loop	Lsl.l	#1,d1
	Bcc.s	.DoLoop
	Addq.l	#1,d0
.DoLoop	Dbra	d2,.Loop

	Move.l	(sp)+,d2
	Rts



	CNOP	0,4
_BFCNTZ	; IN: d1.l = 32-bit number
	;OUT: d0.l = Number of cleared bits
	Move.l	d2,-(sp)
	Moveq	#0,d0
	Moveq	#31,d2

.Loop	Lsl.l	#1,d1
	Bcs.s	.DoLoop
	Addq.l	#1,d0
.DoLoop	Dbra	d2,.Loop

	Move.l	(sp)+,d2
	Rts



_BMCNTO	; IN: a0.l = ULONG * to bitmap
	; IN: d0.l = bit-offset in bitmap
	; IN: d1.l = bits to test
	;OUT: d0.l = Number of set bits
	Movem.l	d2-d5,-(sp)
	Moveq	#0,d4
	Moveq	#0,d5

	Move.w	#32,a1
.Loop	Cmp.l	a1,d1
	Bls.s	.LastOne

	Bfextu	(a0){d0:32},d2

	Moveq	#31,d3
.Loop2	Lsr.l	#1,d2
	Addx.l	d4,d5
	Dbra	d3,.Loop2

	Add.l	a1,d0
	Sub.l	a1,d1
	Bra.s	.Loop

.LastOne	Bfextu	(a0){d0:d1},d2
	Subq.l	#1,d1
.Loop3	Lsr.l	#1,d2
	Addx.l	d4,d5
	Dbra	d1,.Loop3

	Move.l	d5,d0
	Movem.l	(sp)+,d2-d5
	Rts



	CNOP	0,4
_BMTSTZ	; IN: a0.l = ULONG * to bitmap
	; IN: d0.l = bit-offset in bitmap
	; IN: d1.l = bits to test
	;OUT: d0.l = 0 (not all bits were clear) or -1 (all bits were clear)

	Move.w	#32,a1
.Loop	Cmp.l	a1,d1
	Bls.s	.LastOne

	Bftst	(a0){d0:32}
	Bne.s	.NotAllClear
	Add.l	a1,d0
	Sub.l	a1,d1
	Bra.s	.Loop

.LastOne	Bftst	(a0){d0:d1}
	Bne.s	.NotAllClear
	Moveq	#-1,d0
	Rts
.NotAllClear	Moveq	#0,d0
	Rts



	CNOP	0,4
_BMTSTO	; IN: a0.l = ULONG * to bitmap
	; IN: d0.l = bit-offset in bitmap
	; IN: d1.l = bits to test
	;OUT: d0.l = 0 (not all bits were set) or -1 (all bits were set)

	Move.l	d2,-(sp)
	Move.w	#32,a1
.Loop	Cmp.l	a1,d1
	Bls.s	.LastOne

	Bfextu	(a0){d0:32},d2
	Not.l	d2
	Bne.s	.NotAllClear
	Add.l	a1,d0
	Sub.l	a1,d1
	Bra.s	.Loop

.LastOne	Bfexts	(a0){d0:d1},d2
	Not.l	d2
	Bne.s	.NotAllClear
	Moveq	#-1,d0
	Move.l	(sp)+,d2
	Rts
.NotAllClear	Moveq	#0,d0
	Move.l	(sp)+,d2
	Rts




	CNOP	0,4
_BMFLO	; IN: a0.l = ULONG * to bitmap.
	; IN: d0.l = bit-offset in bitmap.
	;OUT: d0.l = First set bit (0 = first bit starting from a0, 'bit-offset' is first bit scanned, -1 = no bits were set).

	; Scans the memory pointed to by a0 for the first set bit starting
	; from the bit-offset in d0.  The scan is backwards -- it scans from
	; bit 'bit-offset' to bit 0.  -1 is returned if no set bits were
	; found.

	Movem.l	d2-d3,-(sp)
	Move.l	d0,d1	0 to 95
	Lsr.l	#5,d1	0 to 2
	Lea	4(a0,d1.l*4),a1

	Move.l	d0,d2
	And.l	#$0000001F,d2	0 to 31
	Eor.l	d2,d0	0, 32, 64
	Move.l	d2,d1
	Sub.w	#31,d1	-31 to 0
	Neg.w	d1	31 to 0

	Move.l	-(a1),d3
	Lsr.l	d1,d3
	Bra.s	.Loop

.Loop2	Move.l	-(a1),d3
	Moveq	#31,d2
	Sub.l	#32,d0

.Loop	Lsr.l	#1,d3
	Dbcs	d2,.Loop

	Ext.l	d2
	Bge.s	.Done
	Cmp.l	a0,a1
	Bne.s	.Loop2

.Done	Add.l	d2,d0
	Movem.l	(sp)+,d2-d3
	Rts



	CNOP	0,4
_BMFLZ	; IN: a0.l = ULONG * to bitmap.
	; IN: d0.l = bit-offset in bitmap.
	;OUT: d0.l = First cleared bit (0 = first bit starting from a0, 'bit-offset' is first bit scanned, -1 = no bits were set).

	; Scans the memory pointed to by a0 for the first cleared bit starting
	; from the bit-offset in d0.  The scan is backwards -- it scans from
	; bit 'bit-offset' to bit 0.  -1 is returned if no cleared bits were
	; found.

	Movem.l	d2-d3,-(sp)
	Move.l	d0,d1
	Lsr.l	#5,d1
	Lea	4(a0,d1.l*4),a1

	Move.l	d0,d2
	And.l	#$0000001F,d2
	Eor.l	d2,d0
	Move.l	d2,d1
	Sub.w	#31,d1
	Neg.w	d1

	Move.l	-(a1),d3
	Lsr.l	d1,d3
	Bra.s	.Loop

.Loop2	Move.l	-(a1),d3
	Moveq	#31,d2
	Sub.l	#32,d0

.Loop	Lsr.l	#1,d3
	Dbcc	d2,.Loop

	Ext.l	d2
	Bge.s	.Done
	Cmp.l	a0,a1
	Bne.s	.Loop2

.Done	Add.l	d2,d0
	Movem.l	(sp)+,d2-d3
	Rts



	CNOP	0,4
_BMFFO	; IN: a0.l = ULONG * to bitmap.
	; IN: d0.l = bit-offset in bitmap.
	; IN: d1.l = size of bitmap in longs.
	;OUT: d0.l = First set bit (0 = first bit starting from a0, 'bit-offset' is first bit scanned, 32*longs-1 = last bit, 32*longs = no bits were set).

	Movem.l	d2-d3,-(sp)
	Move.l	d0,d3
	Lsr.l	#5,d3
	Sub.l	d3,d1
	Subq.l	#1,d1

	And.w	#$001F,d0	0 to 31
	Beq.s	.Enter2

	; Mask: 1 = 0x7FFFFFFF, 31 = 0x0000001
	Sub.w	#32,d0	-31 to -1
	Neg.w	d0	31 to 1

	Moveq	#0,d2
	Bset	d0,d2
	Subq.l	#1,d2
	And.l	(a0,d3.l*4),d2
	Bra.s	.Enter

.Loop	Addq.l	#1,d3
.Enter2	Move.l	(a0,d3.l*4),d2
.Enter	Bfffo	d2{0:32},d0
	Cmp.w	#32,d0
	Dbne	d1,.Loop

	Lsl.l	#5,d3
	Add.l	d3,d0

	Movem.l	(sp)+,d2-d3
	Rts



	CNOP	0,4
_BMFFZ	; IN: a0.l = ULONG * to bitmap.
	; IN: d0.l = bit-offset in bitmap.
	; IN: d1.l = size of bitmap in longs.
	;OUT: d0.l = First cleared bit (0 = first bit starting from a0, 'bit-offset' is first bit scanned, 32*longs-1 = last bit, 32*longs = no bits were set).

	Movem.l	d2-d3,-(sp)
	Move.l	d0,d3
	Lsr.l	#5,d3
	Sub.l	d3,d1
	Subq.l	#1,d1

	And.w	#$001F,d0	0 to 31
	Beq.s	.Enter2

	; Mask: 1 = 0x7FFFFFFF, 31 = 0x0000001
	Sub.w	#32,d0	-31 to -1
	Neg.w	d0	31 to 1

	Moveq	#0,d2
	Bset	d0,d2
	Subq.l	#1,d2
	Not.l	d2
	Or.l	(a0,d3.l*4),d2
	Bra.s	.Enter

.Loop	Addq.l	#1,d3
.Enter2	Move.l	(a0,d3.l*4),d2
.Enter	Not.l	d2
	Bfffo	d2{0:32},d0
	Cmp.w	#32,d0
	Dbne	d1,.Loop

	Lsl.l	#5,d3
	Add.l	d3,d0

	Movem.l	(sp)+,d2-d3
	Rts
