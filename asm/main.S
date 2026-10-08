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
    slli t5, s1, 1
    add  s1, s1, t5
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

    la   s2, perm_dist_table            # Load tables
    la   s3, orien_dist_table
    la   s4, sub_dist_table
    la   s5, sub_pos_table
    la   s6, twist_table

    mv   a0, s0                         # guess(p, o)
    mv   a1, s1
    jal  ra, guess

    li   a7, 1
    ecall
    li   a7, 10                         # exit(0)
    ecall

guess:
    add  t0, s2, a0
    lbu  t0, 0(t0)                      # P
    add  t1, s3, a1
    lbu  t1, 0(t1)                      # T
    bgeu t0, t1, guess_pt
    mv   t0, t1                         # keep the larger
guess_pt:
    slli t2, a0, 1                      # .half = 2 bytes per entry
    add  t2, s5, t2
    lhu  t2, 0(t2)                      # packed
    andi t3, t2, 7                      # pos0
    srli t4, t2, 3
    andi t4, t4, 7                      # pos1
    srli t5, t2, 6
    andi t5, t5, 7                      # pos2
    srli t6, t2, 9                      # pos3, packed is only 12 bits
    slli a2, t3, 1
    add  a2, a2, t3
    slli a2, a2, 1                      # pos0 * 6
    add  a2, a2, t4
    sltu a3, t3, t4
    sub  a2, a2, a3                     # + d1
    slli a3, a2, 2
    add  a2, a2, a3                     # * 5
    add  a2, a2, t5
    sltu a3, t3, t5
    sub  a2, a2, a3
    sltu a3, t4, t5
    sub  a2, a2, a3                     # + d2
    slli a2, a2, 2                      # * 4
    add  a2, a2, t6
    sltu a3, t3, t6
    sub  a2, a2, a3
    sltu a3, t4, t6
    sub  a2, a2, a3
    sltu a3, t5, t6
    sub  a2, a2, a3                     # + d3, a2 = rank
    slli t2, a1, 3                      # 8 bytes per twist row
    add  t2, s6, t2                     # twist row
    add  a4, t2, t3
    lbu  a4, 0(a4)                      # tw[pos0]
    add  a3, t2, t4
    lbu  a3, 0(a3)                      # tw[pos1]
    add  a3, a3, a4
    slli a4, a4, 1
    add  a4, a4, a3                     # a4 = a4 * 3 + tw
    add  a3, t2, t5
    lbu  a3, 0(a3)                      # tw[pos2]
    add  a3, a3, a4
    slli a4, a4, 1
    add  a4, a4, a3
    add  a3, t2, t6
    lbu  a3, 0(a3)                      # tw[pos3]
    add  a3, a3, a4
    slli a4, a4, 1
    add  a4, a4, a3                     # a4 = twists
    slli a3, a2, 4                      # rank << 4
    add  a4, a4, a2
    add  a4, a4, a3
    slli a3, a3, 2                      # rank << 6
    add  a4, a4, a3                     # index = rank * 81 + twists
    add  a4, s4, a4
    lbu  a4, 0(a4)                      # Q
    bgeu t0, a4, guess_done
    mv   t0, a4
guess_done:
    mv   a0, t0
    ret

invalid:
    li   a0, 2
    li   a7, 93
    ecall
