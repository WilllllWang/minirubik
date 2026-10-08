.section .rodata
.include "tables.s"
.balign 2                               # lhu needs an even address
factorials: .half 720, 120, 24, 6, 2, 1 # Lehmer rank values to avoid multiply 6! .. 1!
.data
input: .string "54721631111111"
.text
.globl _start
_start:
    la   a0, input                      # a0 = address of the input
    mv   t0, a0                         # ptr = input
    addi t1, a0, 7                      # end = input + 7
    li   t2, 7                          # cubie digit limit
    li   t3, 1                          # the 1 to shift
    li   s1, 0                          # seen = 0
pos_valid_check:
    lbu  t4, 0(t0)                      # *ptr
    addi t4, t4, -49                    # '1'..'7' -> 0..6
    bgeu t4, t2, invalid                # check if in range < 7
    sll  t5, t3, t4                     # bit = 1 << d
    or   s1, s1, t5                     # seen |= bit
    addi t0, t0, 1                      # ++ptr
    bne  t0, t1, pos_valid_check

    li   t6, 127                        # 0b1111111 mask
    bne  s1, t6, invalid                # check for duplicates

    addi t1, t0, 6                      # end = ptr + 6, ptr is at first twist
    li   t2, 3                          # twist digit limit
    li   t3, 0                          # twist sum = 0
    li   s1, 0                          # orientation = 0
twist_valid_check:
    lbu  t4, 0(t0)                      # *ptr
    addi t4, t4, -49                    # '1'..'3' -> 0..2
    bgeu t4, t2, invalid                # t >= 3
    add  t3, t3, t4                     # sum += t
    slli t5, s1, 1                      # o * 2
    add  s1, s1, t5                     # o * 3
    add  s1, s1, t4                     # o = o * 3 + t, to avoid multiply
    addi t0, t0, 1                      # ++ptr
    bne  t0, t1, twist_valid_check

    lbu  t4, 0(t0)                      # 7th twist
    addi t4, t4, -49
    bgeu t4, t2, invalid
    add  t3, t3, t4
    lbu  t4, 1(t0)                      # byte 14 must be 0
    bnez t4, invalid
    li   t5, 0x1249                     # bits 0, 3, 6, 9, 12 set
    srl  t5, t5, t3                     # bit number sum to the bottom
    andi t5, t5, 1
    beqz t5, invalid                    # twist sum % 3 == 0

    mv   t0, a0                         # i = input
    addi t1, a0, 6                      # i stops after position 5
    addi t2, a0, 7                      # j stops after position 6
    la   t3, factorials                 # f = factorials
    li   s0, 0                          # p = 0
rank_outer:
    lbu  t4, 0(t0)                      # cubie at i
    lhu  t5, 0(t3)                      # its place value
    addi t6, t0, 1                      # j = i + 1
rank_inner:
    lbu  a1, 0(t6)                      # cubie at j
    bgeu a1, t4, rank_skip              # not smaller
    add  s0, s0, t5                     # p += place value
rank_skip:
    addi t6, t6, 1                      # ++j
    bne  t6, t2, rank_inner
    addi t0, t0, 1                      # ++i
    addi t3, t3, 2
    bne  t0, t1, rank_outer

    li   a7, 10                         # exit(0)
    ecall
invalid:
    li   a0, 2                          # exit(2) error
    li   a7, 93
    ecall
