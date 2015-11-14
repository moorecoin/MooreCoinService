;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; copyright (c) 2012, intel corporation 
; 
; all rights reserved. 
; 
; redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are
; met: 
; 
; * redistributions of source code must retain the above copyright
;   notice, this list of conditions and the following disclaimer.  
; 
; * redistributions in binary form must reproduce the above copyright
;   notice, this list of conditions and the following disclaimer in the
;   documentation and/or other materials provided with the
;   distribution. 
; 
; * neither the name of the intel corporation nor the names of its
;   contributors may be used to endorse or promote products derived from
;   this software without specific prior written permission. 
; 
; 
; this software is provided by intel corporation "as is" and any
; express or implied warranties, including, but not limited to, the
; implied warranties of merchantability and fitness for a particular
; purpose are disclaimed. in no event shall intel corporation or
; contributors be liable for any direct, indirect, incidental, special,
; exemplary, or consequential damages (including, but not limited to,
; procurement of substitute goods or services; loss of use, data, or
; profits; or business interruption) however caused and on any theory of
; liability, whether in contract, strict liability, or tort (including
; negligence or otherwise) arising in any way out of the use of this
; software, even if advised of the possibility of such damage.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; example yasm command lines:
; windows:  yasm -f x64 -d winabi sha512_avx.asm
; linux:    yasm -f elf64 sha512_avx.asm
;

bits 64
section .text

; virtual registers
%ifdef winabi
	%define msg	rcx ; arg1
	%define digest	rdx ; arg2
	%define msglen	r8  ; arg3
	%define t1	rsi
	%define t2	rdi
%else
	%define msg	rdi ; arg1
	%define digest	rsi ; arg2
	%define msglen	rdx ; arg3
	%define t1	rcx
	%define t2	r8
%endif
%define a_64	r9
%define b_64	r10
%define c_64	r11
%define d_64	r12
%define e_64	r13
%define f_64	r14
%define g_64	r15
%define h_64	rbx
%define tmp0	rax

; local variables (stack frame)
; note: frame_size must be an odd multiple of 8 bytes to xmm align rsp
struc frame
	.w:       resq 80 ; message schedule
	.wk:      resq  2 ; w[t] + k[t] | w[t+1] + k[t+1]

%ifdef winabi
	.xmmsave: resdq 4
	.gprsave: resq  7
%else
	.gprsave: resq  5
%endif
endstruc

; useful qword "arrays" for simpler memory references
%define msg(i)    msg    + 8*(i) ; input message (arg1)
%define digest(i) digest + 8*(i) ; output digest (arg2)
%define k_t(i)    k512   + 8*(i) wrt rip ; sha constants (static mem)
%define w_t(i)    rsp + frame.w  + 8*(i) ; message schedule (stack frame)
%define wk_2(i)   rsp + frame.wk + 8*((i) % 2) ; w[t]+k[t] (stack frame)
; msg, digest, k_t, w_t are arrays
; wk_2(t) points to 1 of 2 qwords at frame.wk depdending on t being odd/even

%macro rotatestate 0
	; rotate symbles a..h right
	%xdefine	%%tmp h_64
	%xdefine	h_64 g_64
	%xdefine	g_64 f_64
	%xdefine	f_64 e_64
	%xdefine	e_64 d_64
	%xdefine	d_64 c_64
	%xdefine	c_64 b_64
	%xdefine	b_64 a_64
	%xdefine	a_64 %%tmp
%endmacro

%macro rorq 2
	; shld is faster than ror on sandybridge
	shld	%1, %1, (64 - %2)
%endmacro

%macro sha512_round 1
%assign %%t   (%1)

	; compute round %%t
	mov	t1,   f_64        ; t1 = f
	mov	tmp0, e_64        ; tmp = e
	xor	t1,   g_64        ; t1 = f ^ g
	rorq	tmp0, 23 ; 41     ; tmp = e ror 23
	and	t1,   e_64        ; t1 = (f ^ g) & e
	xor	tmp0, e_64        ; tmp = (e ror 23) ^ e
	xor	t1,   g_64        ; t1 = ((f ^ g) & e) ^ g = ch(e,f,g)
	add	t1,   [wk_2(%%t)] ; w[t] + k[t] from message scheduler
	rorq	tmp0, 4 ; 18      ; tmp = ((e ror 23) ^ e) ror 4
	xor	tmp0, e_64        ; tmp = (((e ror 23) ^ e) ror 4) ^ e
	mov	t2,   a_64        ; t2 = a
	add	t1,   h_64        ; t1 = ch(e,f,g) + w[t] + k[t] + h
	rorq	tmp0, 14 ; 14     ; tmp = ((((e ror23)^e)ror4)^e)ror14 = s1(e)
	add	t1,   tmp0        ; t1 = ch(e,f,g) + w[t] + k[t] + s1(e)
	mov	tmp0, a_64        ; tmp = a
	xor	t2,   c_64        ; t2 = a ^ c
	and	tmp0, c_64        ; tmp = a & c
	and	t2,   b_64        ; t2 = (a ^ c) & b
	xor	t2,   tmp0        ; t2 = ((a ^ c) & b) ^ (a & c) = maj(a,b,c)
	mov	tmp0, a_64        ; tmp = a
	rorq	tmp0, 5 ; 39      ; tmp = a ror 5
	xor	tmp0, a_64        ; tmp = (a ror 5) ^ a
	add	d_64, t1          ; e(next_state) = d + t1 
	rorq	tmp0, 6 ; 34      ; tmp = ((a ror 5) ^ a) ror 6
	xor	tmp0, a_64        ; tmp = (((a ror 5) ^ a) ror 6) ^ a
	lea	h_64, [t1 + t2]   ; a(next_state) = t1 + maj(a,b,c)
	rorq	tmp0, 28 ; 28     ; tmp = ((((a ror5)^a)ror6)^a)ror28 = s0(a)
	add	h_64, tmp0        ; a(next_state) = t1 + maj(a,b,c) s0(a)
	rotatestate
%endmacro

%macro sha512_2sched_2round_avx 1
%assign %%t %1
	; compute rounds %%t-2 and %%t-1
	; compute message schedule qwords %%t and %%t+1

	;   two rounds are computed based on the values for k[t-2]+w[t-2] and 
	; k[t-1]+w[t-1] which were previously stored at wk_2 by the message
	; scheduler.
	;   the two new schedule qwords are stored at [w_t(%%t)] and [w_t(%%t+1)].
	; they are then added to their respective sha512 constants at
	; [k_t(%%t)] and [k_t(%%t+1)] and stored at dqword [wk_2(%%t)]
	;   for brievity, the comments following vectored instructions only refer to
	; the first of a pair of qwords.
	; eg. xmm4=w[t-2] really means xmm4={w[t-2]|w[t-1]}
	;   the computation of the message schedule and the rounds are tightly
	; stitched to take advantage of instruction-level parallelism.
	; for clarity, integer instructions (for the rounds calculation) are indented
	; by one tab. vectored instructions (for the message scheduler) are indented
	; by two tabs.

		vmovdqa	xmm4, [w_t(%%t-2)]   ; xmm4 = w[t-2]
		vmovdqu	xmm5, [w_t(%%t-15)]  ; xmm5 = w[t-15]
	mov	t1,   f_64
		vpsrlq	xmm0, xmm4, 61       ; xmm0 = w[t-2]>>61
	mov	tmp0, e_64
		vpsrlq	xmm6, xmm5, 1        ; xmm6 = w[t-15]>>1
	xor	t1,   g_64
	rorq	tmp0, 23 ; 41
		vpsrlq	xmm1, xmm4, 19       ; xmm1 = w[t-2]>>19
	and	t1,   e_64
	xor	tmp0, e_64
		vpxor	xmm0, xmm1           ; xmm0 = w[t-2]>>61 ^ w[t-2]>>19
	xor	t1,   g_64
	add	t1,   [wk_2(%%t)];
		vpsrlq	xmm7, xmm5, 8        ; xmm7 = w[t-15]>>8
	rorq	tmp0, 4 ; 18
		vpsrlq	xmm2, xmm4, 6        ; xmm2 = w[t-2]>>6
	xor	tmp0, e_64
	mov	t2,   a_64
	add	t1,   h_64
		vpxor	xmm6, xmm7           ; xmm6 = w[t-15]>>1 ^ w[t-15]>>8
	rorq	tmp0, 14 ; 14
	add	t1,   tmp0
		vpsrlq	xmm8, xmm5, 7        ; xmm8 = w[t-15]>>7
	mov 	tmp0, a_64
	xor	t2,   c_64
		vpsllq	xmm3, xmm4, (64-61)  ; xmm3 = w[t-2]<<3
	and	tmp0, c_64
	and	t2,   b_64
		vpxor	xmm2, xmm3           ; xmm2 = w[t-2]>>6 ^ w[t-2]<<3
	xor	t2,   tmp0
	mov	tmp0, a_64
		vpsllq	xmm9, xmm5, (64-1)   ; xmm9 = w[t-15]<<63
	rorq	tmp0, 5 ; 39
		vpxor	xmm8, xmm9           ; xmm8 = w[t-15]>>7 ^ w[t-15]<<63
	xor	tmp0, a_64
	add	d_64, t1
	rorq	tmp0, 6 ; 34
	xor	tmp0, a_64
		vpxor	xmm6, xmm8           ; xmm6 = w[t-15]>>1 ^ w[t-15]>>8 ^ w[t-15]>>7 ^ w[t-15]<<63
	lea	h_64, [t1 + t2]
	rorq 	tmp0, 28 ; 28
		vpsllq	xmm4, (64-19)        ; xmm4 = w[t-2]<<25
	add	h_64, tmp0
	rotatestate
		vpxor	xmm0, xmm4           ; xmm0 = w[t-2]>>61 ^ w[t-2]>>19 ^ w[t-2]<<25
	mov	t1, f_64
		vpxor	xmm0, xmm2           ; xmm0 = s1(w[t-2])
	mov	tmp0, e_64
	xor	t1,   g_64
		vpaddq	xmm0, [w_t(%%t-16)]  ; xmm0 = s1(w[t-2]) + w[t-16]
		vmovdqu	xmm1, [w_t(%%t- 7)]  ; xmm1 = w[t-7]
	rorq	tmp0, 23 ; 41
	and	t1,   e_64
	xor	tmp0, e_64
	xor	t1,   g_64
		vpsllq	xmm5, (64-8)         ; xmm5 = w[t-15]<<56
	add	t1,   [wk_2(%%t+1)]
		vpxor	xmm6, xmm5           ; xmm6 = s0(w[t-15])
	rorq	tmp0, 4 ; 18
		vpaddq	xmm0, xmm6           ; xmm0 = s1(w[t-2]) + w[t-16] + s0(w[t-15])
	xor	tmp0, e_64
		vpaddq	xmm0, xmm1           ; xmm0 = w[t] = s1(w[t-2]) + w[t-7] + s0(w[t-15]) + w[t-16]
	mov	t2,   a_64
	add	t1,   h_64
	rorq	tmp0, 14 ; 14
	add	t1,   tmp0
		vmovdqa	[w_t(%%t)], xmm0      ; store w[t]
		vpaddq	xmm0, [k_t(t)]        ; compute w[t]+k[t]
		vmovdqa	[wk_2(t)], xmm0       ; store w[t]+k[t] for next rounds
	mov	tmp0, a_64
	xor	t2,   c_64
	and	tmp0, c_64
	and	t2,   b_64
	xor	t2,   tmp0
	mov	tmp0, a_64
	rorq	tmp0, 5 ; 39
	xor	tmp0, a_64
	add	d_64, t1
	rorq	tmp0, 6 ; 34
	xor	tmp0, a_64
	lea	h_64, [t1 + t2]
	rorq	tmp0, 28 ; 28
	add	h_64, tmp0
	rotatestate
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; void sha512_avx(const void* m, void* d, uint64_t l);
; purpose: updates the sha512 digest stored at d with the message stored in m.
; the size of the message pointed to by m must be an integer multiple of sha512
;   message blocks.
; l is the message length in sha512 blocks
global sha512_avx:function
sha512_avx:
	cmp	msglen, 0
	je	.nowork
	
	; allocate stack space
	sub	rsp, frame_size

	; save gprs
	mov	[rsp + frame.gprsave + 8 * 0], rbx
	mov	[rsp + frame.gprsave + 8 * 1], r12
	mov	[rsp + frame.gprsave + 8 * 2], r13
	mov	[rsp + frame.gprsave + 8 * 3], r14
	mov	[rsp + frame.gprsave + 8 * 4], r15
%ifdef winabi
	mov	[rsp + frame.gprsave + 8 * 5], rsi
	mov	[rsp + frame.gprsave + 8 * 6], rdi
%endif
	; save xmms
%ifdef winabi
	vmovdqa	[rsp + frame.xmmsave + 16 * 0], xmm6
	vmovdqa	[rsp + frame.xmmsave + 16 * 1], xmm7
	vmovdqa	[rsp + frame.xmmsave + 16 * 2], xmm8
	vmovdqa	[rsp + frame.xmmsave + 16 * 3], xmm9
%endif	

.updateblock:

	; load state variables
	mov	a_64, [digest(0)]
	mov	b_64, [digest(1)]
	mov	c_64, [digest(2)]
	mov	d_64, [digest(3)]
	mov	e_64, [digest(4)]
	mov	f_64, [digest(5)]
	mov	g_64, [digest(6)]
	mov	h_64, [digest(7)]

	%assign t 0
	%rep 80/2 + 1
	; (80 rounds) / (2 rounds/iteration) + (1 iteration)
	; +1 iteration because the scheduler leads hashing by 1 iteration
		%if t < 2
			; bswap 2 qwords
			vmovdqa	xmm1, [xmm_qword_bswap wrt rip]
			vmovdqu	xmm0, [msg(t)]
			vpshufb	xmm0, xmm0, xmm1     ; bswap
			vmovdqa	[w_t(t)], xmm0       ; store scheduled pair
			vpaddq	xmm0, xmm0, [k_t(t)] ; compute w[t]+k[t]
			vmovdqa	[wk_2(t)], xmm0      ; store into wk for rounds
		%elif t < 16
			; bswap 2 qwords, compute 2 rounds
			vmovdqu	xmm0, [msg(t)]
			vpshufb	xmm0, xmm0, xmm1     ; bswap
			sha512_round t - 2           ; round t-2
			vmovdqa	[w_t(t)], xmm0       ; store scheduled pair
			vpaddq	xmm0, xmm0, [k_t(t)] ; compute w[t]+k[t]
			sha512_round t - 1           ; round t-1
			vmovdqa	[wk_2(t)], xmm0      ; w[t]+k[t] into wk
		%elif t < 79
			; schedule 2 qwords; compute 2 rounds
			sha512_2sched_2round_avx t
		%else
			; compute 2 rounds
			sha512_round t - 2
			sha512_round t - 1
		%endif
	%assign t t+2
	%endrep

	; update digest
	add	[digest(0)], a_64
	add	[digest(1)], b_64
	add	[digest(2)], c_64
	add	[digest(3)], d_64
	add	[digest(4)], e_64
	add	[digest(5)], f_64
	add	[digest(6)], g_64
	add	[digest(7)], h_64

	; advance to next message block
	add	msg, 16*8
	dec	msglen
	jnz	.updateblock

	; restore xmms
%ifdef winabi
	vmovdqa	xmm6, [rsp + frame.xmmsave + 16 * 0]
	vmovdqa	xmm7, [rsp + frame.xmmsave + 16 * 1]
	vmovdqa	xmm8, [rsp + frame.xmmsave + 16 * 2]
	vmovdqa	xmm9, [rsp + frame.xmmsave + 16 * 3]
%endif
	; restore gprs
	mov	rbx, [rsp + frame.gprsave + 8 * 0]
	mov	r12, [rsp + frame.gprsave + 8 * 1]
	mov	r13, [rsp + frame.gprsave + 8 * 2]
	mov	r14, [rsp + frame.gprsave + 8 * 3]
	mov	r15, [rsp + frame.gprsave + 8 * 4]
%ifdef winabi
	mov	rsi, [rsp + frame.gprsave + 8 * 5]
	mov	rdi, [rsp + frame.gprsave + 8 * 6]
%endif
	; restore stack pointer
	add	rsp, frame_size

.nowork:
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; binary data

section .data

align 16

; mask for byte-swapping a couple of qwords in an xmm register using (v)pshufb.
xmm_qword_bswap: 
	ddq 0x08090a0b0c0d0e0f0001020304050607

; k[t] used in sha512 hashing
k512:
	dq 0x428a2f98d728ae22,0x7137449123ef65cd 
	dq 0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	dq 0x3956c25bf348b538,0x59f111f1b605d019 
	dq 0x923f82a4af194f9b,0xab1c5ed5da6d8118
	dq 0xd807aa98a3030242,0x12835b0145706fbe 
	dq 0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	dq 0x72be5d74f27b896f,0x80deb1fe3b1696b1 
	dq 0x9bdc06a725c71235,0xc19bf174cf692694
	dq 0xe49b69c19ef14ad2,0xefbe4786384f25e3 
	dq 0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	dq 0x2de92c6f592b0275,0x4a7484aa6ea6e483 
	dq 0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	dq 0x983e5152ee66dfab,0xa831c66d2db43210 
	dq 0xb00327c898fb213f,0xbf597fc7beef0ee4
	dq 0xc6e00bf33da88fc2,0xd5a79147930aa725 
	dq 0x06ca6351e003826f,0x142929670a0e6e70
	dq 0x27b70a8546d22ffc,0x2e1b21385c26c926 
	dq 0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	dq 0x650a73548baf63de,0x766a0abb3c77b2a8 
	dq 0x81c2c92e47edaee6,0x92722c851482353b
	dq 0xa2bfe8a14cf10364,0xa81a664bbc423001 
	dq 0xc24b8b70d0f89791,0xc76c51a30654be30
	dq 0xd192e819d6ef5218,0xd69906245565a910 
	dq 0xf40e35855771202a,0x106aa07032bbd1b8
	dq 0x19a4c116b8d2d0c8,0x1e376c085141ab53 
	dq 0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	dq 0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb 
	dq 0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	dq 0x748f82ee5defb2fc,0x78a5636f43172f60 
	dq 0x84c87814a1f0ab72,0x8cc702081a6439ec
	dq 0x90befffa23631e28,0xa4506cebde82bde9 
	dq 0xbef9a3f7b2c67915,0xc67178f2e372532b
	dq 0xca273eceea26619c,0xd186b8c721c0c207 
	dq 0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	dq 0x06f067aa72176fba,0x0a637dc5a2c898a6 
	dq 0x113f9804bef90dae,0x1b710b35131c471b
	dq 0x28db77f523047d84,0x32caab7b40c72493 
	dq 0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	dq 0x4cc5d4becb3e42b6,0x597f299cfc657e2a 
	dq 0x5fcb6fab3ad6faec,0x6c44198c4a475817

