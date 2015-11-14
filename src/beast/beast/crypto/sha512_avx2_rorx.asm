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
; windows:  yasm -f x64 -d winabi sha512_rorx.asm
; linux:    yasm -f elf64 sha512_rorx.asm
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; this code schedules 1 blocks at a time, with 4 lanes per block
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

bits 64
section .text

; virtual registers
%define y_0 ymm4
%define y_1 ymm5
%define y_2 ymm6
%define y_3 ymm7

%define ytmp0 ymm0
%define ytmp1 ymm1
%define ytmp2 ymm2
%define ytmp3 ymm3
%define ytmp4 ymm8
%define xfer  ytmp0

%define byte_flip_mask  ymm9

%ifdef winabi
	%define inp         rcx ; 1st arg
	%define ctx         rdx ; 2nd arg
	%define num_blks    r8  ; 3rd arg
	%define c           rdi 
	%define d           rsi 
	%define e           r8
	%define y3          rcx
%else
	%define inp         rdi ; 1st arg
	%define ctx         rsi ; 2nd arg
	%define num_blks    rdx ; 3rd arg
	%define c           rcx
	%define d           r8
	%define e           rdx
	%define y3          rdi
%endif

%define tbl   rbp
	      
%define a     rax
%define b     rbx
	      
%define f     r9
%define g     r10
%define h     r11
%define old_h r11

%define t1    r12
%define y0    r13
%define y1    r14
%define y2    r15

%define y4    r12

; local variables (stack frame)
struc frame
	.xfer:    resq  4
	.srnd:    resq  1
	.inp:     resq  1
	.inpend:  resq  1
	.rspsave: resq  1

%ifdef winabi
	.xmmsave: resdq 4
	.gprsave: resq  8
%else
	.gprsave: resq  6
%endif
endstruc

%define	vmovdq vmovdqu ;; assume buffers not aligned 

; addm [mem], reg
; add reg to mem using reg-mem add and store
%macro addm 2
	add	%2, %1
	mov	%1, %2
%endm


; copy_ymm_and_bswap ymm, [mem], byte_flip_mask
; load ymm with mem and byte swap each dword
%macro copy_ymm_and_bswap 3
	vmovdq %1, %2
	vpshufb %1, %1 ,%3
%endmacro
; rotate_ys
; rotate values of symbols y0...y3
%macro rotate_ys 0
	%xdefine %%y_ y_0
	%xdefine y_0 y_1
	%xdefine y_1 y_2
	%xdefine y_2 y_3
	%xdefine y_3 %%y_
%endm

; rotatestate
%macro rotatestate 0
	; rotate symbles a..h right
	%xdefine old_h  h
	%xdefine %%tmp_ h
	%xdefine h      g
	%xdefine g      f
	%xdefine f      e
	%xdefine e      d
	%xdefine d      c
	%xdefine c      b
	%xdefine b      a
	%xdefine a      %%tmp_
%endm

; %macro my_vpalignr	ydst, ysrc1, ysrc2, rval
; ydst = {ysrc1, ysrc2} >> rval*8
%macro my_vpalignr 4
%define %%ydst 	%1
%define %%ysrc1 %2
%define %%ysrc2	%3
%define %%rval 	%4
	vperm2f128 	%%ydst, %%ysrc1, %%ysrc2, 0x3	; ydst = {ys1_lo, ys2_hi}
	vpalignr 	%%ydst, %%ydst, %%ysrc2, %%rval	; ydst = {yds1, ys2} >> rval*8
%endm

%macro four_rounds_and_sched 0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 0 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

		; extract w[t-7]
		my_vpalignr	ytmp0, y_3, y_2, 8		; ytmp0 = w[-7]
		; calculate w[t-16] + w[t-7]
		vpaddq		ytmp0, ytmp0, y_0		; ytmp0 = w[-7] + w[-16]
		; extract w[t-15]
		my_vpalignr	ytmp1, y_1, y_0, 8		; ytmp1 = w[-15]

		; calculate sigma0

		; calculate w[t-15] ror 1
		vpsrlq		ytmp2, ytmp1, 1
		vpsllq		ytmp3, ytmp1, (64-1)
		vpor		ytmp3, ytmp3, ytmp2		; ytmp3 = w[-15] ror 1
		; calculate w[t-15] shr 7
		vpsrlq		ytmp4, ytmp1, 7			; ytmp4 = w[-15] >> 7

	mov	y3, a		; y3 = a                                       ; maja	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b

	add	h, [rsp+frame.xfer+0*8]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	
	mov	y2, f		; y2 = f                                       ; ch	
	rorx	t1, a, 34	; t1 = a >> 34					; s0b

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	xor	y2, g		; y2 = f^g                                     ; ch	
	rorx	y1, e, 14	; y1 = (e >> 14)					; s1

	and	y2, e		; y2 = (f^g)&e                                 ; ch	
	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	add	d, h		; d = k + w + h + d                            ; --	

	and	y3, b		; y3 = (a|c)&b                                 ; maja	
	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	rorx	t1, a, 28	; t1 = (a >> 28)					; s0

	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	
	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	t1, c		; t1 = a&c                                     ; majb	

	add	y2, y0		; y2 = s1 + ch                                 ; --	
	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	

	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	

	add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	
	add	h, y3		; h = t1 + s0 + maj                            ; --	

rotatestate

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 1 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;

		; calculate w[t-15] ror 8
		vpsrlq		ytmp2, ytmp1, 8
		vpsllq		ytmp1, ytmp1, (64-8)
		vpor		ytmp1, ytmp1, ytmp2		; ytmp1 = w[-15] ror 8
		; xor the three components
		vpxor		ytmp3, ytmp3, ytmp4		; ytmp3 = w[-15] ror 1 ^ w[-15] >> 7
		vpxor		ytmp1, ytmp3, ytmp1		; ytmp1 = s0


		; add three components, w[t-16], w[t-7] and sigma0
		vpaddq		ytmp0, ytmp0, ytmp1		; ytmp0 = w[-16] + w[-7] + s0
		; move to appropriate lanes for calculating w[16] and w[17]
		vperm2f128	y_0, ytmp0, ytmp0, 0x0		; y_0 = w[-16] + w[-7] + s0 {baba}
		; move to appropriate lanes for calculating w[18] and w[19]
		vpand		ytmp0, ytmp0, [mask_ymm_lo wrt rip]	; ytmp0 = w[-16] + w[-7] + s0 {dc00}

		; calculate w[16] and w[17] in both 128 bit lanes

		; calculate sigma1 for w[16] and w[17] on both 128 bit lanes
		vperm2f128	ytmp2, y_3, y_3, 0x11		; ytmp2 = w[-2] {baba}
		vpsrlq		ytmp4, ytmp2, 6			; ytmp4 = w[-2] >> 6 {baba}


	mov	y3, a		; y3 = a                                       ; maja	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	add	h, [rsp+frame.xfer+1*8]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	


	mov	y2, f		; y2 = f                                       ; ch	
	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	xor	y2, g		; y2 = f^g                                     ; ch	


	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	and	y2, e		; y2 = (f^g)&e                                 ; ch	
	add	d, h		; d = k + w + h + d                            ; --	

	and	y3, b		; y3 = (a|c)&b                                 ; maja	
	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0

	rorx	t1, a, 28	; t1 = (a >> 28)					; s0
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	t1, c		; t1 = a&c                                     ; majb	
	add	y2, y0		; y2 = s1 + ch                                 ; --	

	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	

	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	
	add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	
	add	h, y3		; h = t1 + s0 + maj                            ; --	

rotatestate




;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 2 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;


		vpsrlq		ytmp3, ytmp2, 19		; ytmp3 = w[-2] >> 19 {baba}
		vpsllq		ytmp1, ytmp2, (64-19)		; ytmp1 = w[-2] << 19 {baba}
		vpor		ytmp3, ytmp3, ytmp1		; ytmp3 = w[-2] ror 19 {baba}
		vpxor		ytmp4, ytmp4, ytmp3		; ytmp4 = w[-2] ror 19 ^ w[-2] >> 6 {baba}
		vpsrlq		ytmp3, ytmp2, 61		; ytmp3 = w[-2] >> 61 {baba}
		vpsllq		ytmp1, ytmp2, (64-61)		; ytmp1 = w[-2] << 61 {baba}
		vpor		ytmp3, ytmp3, ytmp1		; ytmp3 = w[-2] ror 61 {baba}
		vpxor		ytmp4, ytmp4, ytmp3		; ytmp4 = s1 = (w[-2] ror 19) ^ (w[-2] ror 61) ^ (w[-2] >> 6) {baba}

		; add sigma1 to the other compunents to get w[16] and w[17]
		vpaddq		y_0, y_0, ytmp4			; y_0 = {w[1], w[0], w[1], w[0]}

		; calculate sigma1 for w[18] and w[19] for upper 128 bit lane
		vpsrlq		ytmp4, y_0, 6			; ytmp4 = w[-2] >> 6 {dc--}

	mov	y3, a		; y3 = a                                       ; maja	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	add	h, [rsp+frame.xfer+2*8]		; h = k + w + h                                ; --	

	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	or	y3, c		; y3 = a|c                                     ; maja	
	mov	y2, f		; y2 = f                                       ; ch	
	xor	y2, g		; y2 = f^g                                     ; ch	

	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	and	y2, e		; y2 = (f^g)&e                                 ; ch	

	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	add	d, h		; d = k + w + h + d                            ; --	
	and	y3, b		; y3 = (a|c)&b                                 ; maja	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	rorx	t1, a, 28	; t1 = (a >> 28)					; s0

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	t1, c		; t1 = a&c                                     ; majb	
	add	y2, y0		; y2 = s1 + ch                                 ; --	

	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	
	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	
	add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	

	add	h, y3		; h = t1 + s0 + maj                            ; --	

rotatestate

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 3 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;

		vpsrlq		ytmp3, y_0, 19			; ytmp3 = w[-2] >> 19 {dc--}
		vpsllq		ytmp1, y_0, (64-19)		; ytmp1 = w[-2] << 19 {dc--}
		vpor		ytmp3, ytmp3, ytmp1		; ytmp3 = w[-2] ror 19 {dc--}
		vpxor		ytmp4, ytmp4, ytmp3		; ytmp4 = w[-2] ror 19 ^ w[-2] >> 6 {dc--}
		vpsrlq		ytmp3, y_0, 61			; ytmp3 = w[-2] >> 61 {dc--}
		vpsllq		ytmp1, y_0, (64-61)		; ytmp1 = w[-2] << 61 {dc--}
		vpor		ytmp3, ytmp3, ytmp1		; ytmp3 = w[-2] ror 61 {dc--}
		vpxor		ytmp4, ytmp4, ytmp3		; ytmp4 = s1 = (w[-2] ror 19) ^ (w[-2] ror 61) ^ (w[-2] >> 6) {dc--}

		; add the sigma0 + w[t-7] + w[t-16] for w[18] and w[19] to newly calculated sigma1 to get w[18] and w[19]
		vpaddq		ytmp2, ytmp0, ytmp4		; ytmp2 = {w[3], w[2], --, --}

		; form w[19, w[18], w17], w[16]
		vpblendd		y_0, y_0, ytmp2, 0xf0		; y_0 = {w[3], w[2], w[1], w[0]}
;		vperm2f128		y_0, y_0, ytmp2, 0x30

	mov	y3, a		; y3 = a                                       ; maja	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	add	h, [rsp+frame.xfer+3*8]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	


	mov	y2, f		; y2 = f                                       ; ch	
	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	xor	y2, g		; y2 = f^g                                     ; ch	


	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	and	y2, e		; y2 = (f^g)&e                                 ; ch	
	add	d, h		; d = k + w + h + d                            ; --	
	and	y3, b		; y3 = (a|c)&b                                 ; maja	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	

	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	add	y2, y0		; y2 = s1 + ch                                 ; --	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	

	rorx	t1, a, 28	; t1 = (a >> 28)					; s0

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	t1, c		; t1 = a&c                                     ; majb	
	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	

	add	h, y1		; h = k + w + h + s0                           ; --	
	add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	
	add	h, y3		; h = t1 + s0 + maj                            ; --	

rotatestate

rotate_ys
%endm

%macro do_4rounds 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 0 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	mov	y2, f		; y2 = f                                       ; ch	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	xor	y2, g		; y2 = f^g                                     ; ch	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	and	y2, e		; y2 = (f^g)&e                                 ; ch	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	mov	y3, a		; y3 = a                                       ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	rorx	t1, a, 28	; t1 = (a >> 28)					; s0
	add	h, [rsp + frame.xfer + 8*0]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	y3, b		; y3 = (a|c)&b                                 ; maja	
	and	t1, c		; t1 = a&c                                     ; majb	
	add	y2, y0		; y2 = s1 + ch                                 ; --	


	add	d, h		; d = k + w + h + d                            ; --	
	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	

	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	


	;add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	

	;add	h, y3		; h = t1 + s0 + maj                            ; --	

	rotatestate

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 1 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	add	old_h, y2	; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	
	mov	y2, f		; y2 = f                                       ; ch	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	xor	y2, g		; y2 = f^g                                     ; ch	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	and	y2, e		; y2 = (f^g)&e                                 ; ch	
	add	old_h, y3	; h = t1 + s0 + maj                            ; --	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	mov	y3, a		; y3 = a                                       ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	rorx	t1, a, 28	; t1 = (a >> 28)					; s0
	add	h, [rsp + frame.xfer + 8*1]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	y3, b		; y3 = (a|c)&b                                 ; maja	
	and	t1, c		; t1 = a&c                                     ; majb	
	add	y2, y0		; y2 = s1 + ch                                 ; --	


	add	d, h		; d = k + w + h + d                            ; --	
	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	

	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	


	;add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	

	;add	h, y3		; h = t1 + s0 + maj                            ; --	

	rotatestate

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 2 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	add	old_h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	
	mov	y2, f		; y2 = f                                       ; ch	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	xor	y2, g		; y2 = f^g                                     ; ch	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	and	y2, e		; y2 = (f^g)&e                                 ; ch	
	add	old_h, y3	; h = t1 + s0 + maj                            ; --	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	mov	y3, a		; y3 = a                                       ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	rorx	t1, a, 28	; t1 = (a >> 28)					; s0
	add	h, [rsp + frame.xfer + 8*2]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	y3, b		; y3 = (a|c)&b                                 ; maja	
	and	t1, c		; t1 = a&c                                     ; majb	
	add	y2, y0		; y2 = s1 + ch                                 ; --	


	add	d, h		; d = k + w + h + d                            ; --	
	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	

	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	


	;add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	

	;add	h, y3		; h = t1 + s0 + maj                            ; --	

	rotatestate

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; rnd n + 3 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	add	old_h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	
	mov	y2, f		; y2 = f                                       ; ch	
	rorx	y0, e, 41	; y0 = e >> 41					; s1a
	rorx	y1, e, 18	; y1 = e >> 18					; s1b
	xor	y2, g		; y2 = f^g                                     ; ch	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18)			; s1
	rorx	y1, e, 14	; y1 = (e >> 14)					; s1
	and	y2, e		; y2 = (f^g)&e                                 ; ch	
	add	old_h, y3	; h = t1 + s0 + maj                            ; --	

	xor	y0, y1		; y0 = (e>>41) ^ (e>>18) ^ (e>>14)		; s1
	rorx	t1, a, 34	; t1 = a >> 34					; s0b
	xor	y2, g		; y2 = ch = ((f^g)&e)^g                        ; ch	
	rorx	y1, a, 39	; y1 = a >> 39					; s0a
	mov	y3, a		; y3 = a                                       ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34)			; s0
	rorx	t1, a, 28	; t1 = (a >> 28)					; s0
	add	h, [rsp + frame.xfer + 8*3]		; h = k + w + h                                ; --	
	or	y3, c		; y3 = a|c                                     ; maja	

	xor	y1, t1		; y1 = (a>>39) ^ (a>>34) ^ (a>>28)		; s0
	mov	t1, a		; t1 = a                                       ; majb	
	and	y3, b		; y3 = (a|c)&b                                 ; maja	
	and	t1, c		; t1 = a&c                                     ; majb	
	add	y2, y0		; y2 = s1 + ch                                 ; --	


	add	d, h		; d = k + w + h + d                            ; --	
	or	y3, t1		; y3 = maj = (a|c)&b)|(a&c)                    ; maj	
	add	h, y1		; h = k + w + h + s0                           ; --	

	add	d, y2		; d = k + w + h + d + s1 + ch = d + t1         ; --	


	add	h, y2		; h = k + w + h + s0 + s1 + ch = t1 + s0       ; --	

	add	h, y3		; h = t1 + s0 + maj                            ; --	

	rotatestate

%endm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; void sha512_rorx(const void* m, void* d, uint64_t l);
; purpose: updates the sha512 digest stored at d with the message stored in m.
; the size of the message pointed to by m must be an integer multiple of sha512
;   message blocks.
; l is the message length in sha512 blocks
global sha512_rorx:function
sha512_rorx:

	; allocate stack space
	mov	rax, rsp
	sub	rsp, frame_size
	and	rsp, ~(0x20 - 1)
	mov	[rsp + frame.rspsave], rax

	; save gprs
	mov	[rsp + frame.gprsave + 8 * 0], rbp
	mov	[rsp + frame.gprsave + 8 * 1], rbx
	mov	[rsp + frame.gprsave + 8 * 2], r12
	mov	[rsp + frame.gprsave + 8 * 3], r13
	mov	[rsp + frame.gprsave + 8 * 4], r14
	mov	[rsp + frame.gprsave + 8 * 5], r15
%ifdef winabi
	mov	[rsp + frame.gprsave + 8 * 6], rsi
	mov	[rsp + frame.gprsave + 8 * 7], rdi
%endif

%ifdef winabi
	vmovdqa	[rsp + frame.xmmsave + 0*16], xmm6
	vmovdqa	[rsp + frame.xmmsave + 1*16], xmm7
	vmovdqa	[rsp + frame.xmmsave + 2*16], xmm8	
	vmovdqa	[rsp + frame.xmmsave + 3*16], xmm9	
%endif

	vpblendd	xmm0, xmm0, xmm1, 0xf0
	vpblendd	ymm0, ymm0, ymm1, 0xf0

	shl	num_blks, 7	; convert to bytes
	jz	done_hash
	add	num_blks, inp	; pointer to end of data
	mov	[rsp + frame.inpend], num_blks

	;; load initial digest
	mov	a,[8*0 + ctx]
	mov	b,[8*1 + ctx]
	mov	c,[8*2 + ctx]
	mov	d,[8*3 + ctx]
	mov	e,[8*4 + ctx]
	mov	f,[8*5 + ctx]
	mov	g,[8*6 + ctx]
	mov	h,[8*7 + ctx]

	vmovdqa	byte_flip_mask, [pshuffle_byte_flip_mask wrt rip]

loop0:
	lea	tbl,[k512 wrt rip]

	;; byte swap first 16 dwords
	copy_ymm_and_bswap	y_0, [inp + 0*32], byte_flip_mask
	copy_ymm_and_bswap	y_1, [inp + 1*32], byte_flip_mask
	copy_ymm_and_bswap	y_2, [inp + 2*32], byte_flip_mask
	copy_ymm_and_bswap	y_3, [inp + 3*32], byte_flip_mask
	
	mov	[rsp + frame.inp], inp

	;; schedule 64 input dwords, by doing 12 rounds of 4 each
	mov	qword[rsp + frame.srnd],4

align 16
loop1:
	vpaddq	xfer, y_0, [tbl + 0*32]
	vmovdqa [rsp + frame.xfer], xfer
	four_rounds_and_sched

	vpaddq	xfer, y_0, [tbl + 1*32]
	vmovdqa [rsp + frame.xfer], xfer
	four_rounds_and_sched

	vpaddq	xfer, y_0, [tbl + 2*32]
	vmovdqa [rsp + frame.xfer], xfer
	four_rounds_and_sched

	vpaddq	xfer, y_0, [tbl + 3*32]
	vmovdqa [rsp + frame.xfer], xfer
	add	tbl, 4*32
	four_rounds_and_sched

	sub	qword[rsp + frame.srnd], 1
	jne	loop1

	mov	qword[rsp + frame.srnd], 2
loop2:
	vpaddq	xfer, y_0, [tbl + 0*32]
	vmovdqa [rsp + frame.xfer], xfer
	do_4rounds
	vpaddq	xfer, y_1, [tbl + 1*32]
	vmovdqa [rsp + frame.xfer], xfer
	add	tbl, 2*32
	do_4rounds

	vmovdqa	y_0, y_2
	vmovdqa	y_1, y_3

	sub	qword[rsp + frame.srnd], 1
	jne	loop2

	addm	[8*0 + ctx],a
	addm	[8*1 + ctx],b
	addm	[8*2 + ctx],c
	addm	[8*3 + ctx],d
	addm	[8*4 + ctx],e
	addm	[8*5 + ctx],f
	addm	[8*6 + ctx],g
	addm	[8*7 + ctx],h

	mov	inp, [rsp + frame.inp]
	add	inp, 128
	cmp	inp, [rsp + frame.inpend]
	jne	loop0

    done_hash:
%ifdef winabi
	vmovdqa	xmm6, [rsp + frame.xmmsave + 0*16]
	vmovdqa	xmm7, [rsp + frame.xmmsave + 1*16]
	vmovdqa	xmm8, [rsp + frame.xmmsave + 2*16]
	vmovdqa	xmm9, [rsp + frame.xmmsave + 3*16]
%endif

; restore gprs
	mov	rbp, [rsp + frame.gprsave + 8 * 0]
	mov	rbx, [rsp + frame.gprsave + 8 * 1]
	mov	r12, [rsp + frame.gprsave + 8 * 2]
	mov	r13, [rsp + frame.gprsave + 8 * 3]
	mov	r14, [rsp + frame.gprsave + 8 * 4]
	mov	r15, [rsp + frame.gprsave + 8 * 5]
%ifdef winabi
	mov	rsi, [rsp + frame.gprsave + 8 * 6]
	mov	rdi, [rsp + frame.gprsave + 8 * 7]
%endif
	; restore stack pointer
	mov	rsp, [rsp + frame.rspsave]

	ret	
	

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; binary data

section .data

align 64
; k[t] used in sha512 hashing
k512:
	dq	0x428a2f98d728ae22,0x7137449123ef65cd 
	dq	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	dq	0x3956c25bf348b538,0x59f111f1b605d019 
	dq	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	dq	0xd807aa98a3030242,0x12835b0145706fbe 
	dq	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	dq	0x72be5d74f27b896f,0x80deb1fe3b1696b1 
	dq	0x9bdc06a725c71235,0xc19bf174cf692694
	dq	0xe49b69c19ef14ad2,0xefbe4786384f25e3 
	dq	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	dq	0x2de92c6f592b0275,0x4a7484aa6ea6e483 
	dq	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	dq	0x983e5152ee66dfab,0xa831c66d2db43210 
	dq	0xb00327c898fb213f,0xbf597fc7beef0ee4
	dq	0xc6e00bf33da88fc2,0xd5a79147930aa725 
	dq	0x06ca6351e003826f,0x142929670a0e6e70
	dq	0x27b70a8546d22ffc,0x2e1b21385c26c926 
	dq	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	dq	0x650a73548baf63de,0x766a0abb3c77b2a8 
	dq	0x81c2c92e47edaee6,0x92722c851482353b
	dq	0xa2bfe8a14cf10364,0xa81a664bbc423001 
	dq	0xc24b8b70d0f89791,0xc76c51a30654be30
	dq	0xd192e819d6ef5218,0xd69906245565a910 
	dq	0xf40e35855771202a,0x106aa07032bbd1b8
	dq	0x19a4c116b8d2d0c8,0x1e376c085141ab53 
	dq	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	dq	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb 
	dq	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	dq	0x748f82ee5defb2fc,0x78a5636f43172f60 
	dq	0x84c87814a1f0ab72,0x8cc702081a6439ec
	dq	0x90befffa23631e28,0xa4506cebde82bde9 
	dq	0xbef9a3f7b2c67915,0xc67178f2e372532b
	dq	0xca273eceea26619c,0xd186b8c721c0c207 
	dq	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	dq	0x06f067aa72176fba,0x0a637dc5a2c898a6 
	dq	0x113f9804bef90dae,0x1b710b35131c471b
	dq	0x28db77f523047d84,0x32caab7b40c72493 
	dq	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	dq	0x4cc5d4becb3e42b6,0x597f299cfc657e2a 
	dq	0x5fcb6fab3ad6faec,0x6c44198c4a475817

align 32

; mask for byte-swapping a couple of qwords in an xmm register using (v)pshufb.
pshuffle_byte_flip_mask: ddq 0x08090a0b0c0d0e0f0001020304050607
                         ddq 0x18191a1b1c1d1e1f1011121314151617

mask_ymm_lo: 		 ddq 0x00000000000000000000000000000000
             		 ddq 0xffffffffffffffffffffffffffffffff
