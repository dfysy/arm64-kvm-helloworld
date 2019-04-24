__head:
	mov x3, 0x1100
	
	mov w4, 'h'
	strb w4, [x3]

	mov w4, 'e'
	strb w4, [x3]

	mov w4, 'l'
	strb w4, [x3]

	mov w4, 'l'
	strb w4, [x3]

	mov w4, 'o'
	strb w4, [x3]
exit:
	b exit


