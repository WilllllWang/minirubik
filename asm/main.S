.option norelax                           # keep la as auipc + addi, nothing sets gp
.equ TESTS, 1                             # 1: run the 3 test cases before the input
.equ RENDER, 0                            # 1: GUI build that draws on the LED matrix
.equ DELAY, 3000000

.section .rodata
.include "tables.s"                       # 7 tables from ./search --tables
.balign 2                                 # lhu needs an even address
factorials:  .half 720, 120, 24, 6, 2, 1  # Lehmer place values 6! .. 1!, no multiply
.balign 4
move_rows:   .word permutation, orientation, permutation, orientation, permutation, orientation, 0, 0
             .word permutation + 10080, orientation + 1458, permutation + 10080, orientation + 1458, permutation + 10080, orientation + 1458, 0, 0
             .word permutation + 20160, orientation + 2916, permutation + 20160, orientation + 2916, permutation + 20160, orientation + 2916, 0, 0
next_move:   .byte 1, 2, 4, 0, 5, 6, 8, 0, 9, 10, 12, 0
.if RENDER
.balign 4                                 # lw needs a multiple of 4
colors:      .word 0xFFFFFF, 0xFFA500, 0x00FF00, 0xFF0000, 0x0000FF, 0xFFFF00   # U L F R B D
cycles:      .byte 1,18,21,9,   3,16,23,11,  12,13,15,14,  0,0,0,0    # R: U1 B2 D1 F1, U3 B0 D3 F3, R0 R1 R3 R2
             .byte 0,6,23,13,   1,4,22,15,   16,17,19,18,  0,0,0,0    # B: U0 L2 D3 R1, U1 L0 D2 R3, B0 B1 B3 B2
             .byte 6,10,14,18,  7,11,15,19,  20,21,23,22,  0,0,0,0    # D: L2 F2 R2 B2, L3 F3 R3 B3, D0 D1 D3 D2

# Ripes defines the LED_MATRIX_0 names in ripes_system.h, which only the GUI build reads
.equ LED, LED_MATRIX_0_BASE
.equ W, LED_MATRIX_0_WIDTH
pixels:      .word LED + (0*W + 9)*4,  LED + (0*W + 13)*4,  LED + (3*W + 9)*4,   LED + (3*W + 13)*4     # U
             .word LED + (7*W + 0)*4,  LED + (7*W + 4)*4,   LED + (10*W + 0)*4,  LED + (10*W + 4)*4     # L
             .word LED + (7*W + 9)*4,  LED + (7*W + 13)*4,  LED + (10*W + 9)*4,  LED + (10*W + 13)*4    # F
             .word LED + (7*W + 18)*4, LED + (7*W + 22)*4,  LED + (10*W + 18)*4, LED + (10*W + 22)*4    # R
             .word LED + (7*W + 27)*4, LED + (7*W + 31)*4,  LED + (10*W + 27)*4, LED + (10*W + 31)*4    # B
             .word LED + (14*W + 9)*4, LED + (14*W + 13)*4, LED + (17*W + 9)*4,  LED + (17*W + 13)*4    # D
.endif

.data
frames:      .byte 0, 0, 0, 0, 0, 255, 0, 0     # frame -1, only its move byte (255) is used
             .zero 96                           # frames 0..11, 8 bytes each
input:       .string "54721631111111"

test_solved: .string "12345671111111"
test_short:  .string "26547312333212"           # R B D' from solved
test_far:    .string "21345671111111"
.balign 4
cases:                                          # (string, expected length), -1 = any
.if TESTS
             .word test_solved, 0
             .word test_short, 3
             .word test_far, 11
.endif
             .word input, -1
cases_end:
.if RENDER
stickers:    .byte 0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5   # solved, U L F R B D
.endif

.text
.globl _start
_start:
    la   a5, cases                      # a5 = current case, a6 = end of the table
    la   a6, cases_end
next_case:
    lw   a0, 0(a5)                      # a0 = address of the input

    # cubie digits '1'..'7': range check and seen mask
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

    # twist digits '1'..'3': range check, twist sum, o from the first 6
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

    # permutation rank p: add the place value for each smaller cubie to the right
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

    # table addresses for guess, loaded once
    la   s2, perm_dist_table
    la   s3, orien_dist_table
    la   s4, sub_dist_table
    la   s5, sub_pos_table
    la   s6, twist_table

    # search setup: root in frame 0, first limit = guess(root)
    la   s7, frames + 8                 # frame 0
    sh   s0, 0(s7)                      # root p
    sh   s1, 2(s7)                      # root o
    la   s8, move_rows
    li   s10, 12                        # all 12 moves tried
    la   s11, next_move
    li   s9, 0
    or   t0, s0, s1
    beqz t0, found                      # already solved, 0 moves
    mv   a0, s0
    mv   a1, s1
    jal  ra, guess
    mv   s9, a0                         # limit = guess(root)

    # one round per limit, depth-first over the frames
round:
    li   t0, 12
    beq  s9, t0, no_solution
    mv   s0, s7                         # back to frame 0
    addi s1, s9, -1                     # budget at depth 0
    sb   zero, 4(s7)                    # frame 0 starts at move 0
search_loop:
    lbu  t0, 4(s0)                      # move to try
    beq  t0, s10, go_up                 # all 12 tried
    add  t1, s11, t0
    lbu  t1, 0(t1)                      # next move
    sb   t1, 4(s0)
    srli t2, t0, 2                      # face
    lbu  t3, -3(s0)                     # byte +5 of the frame above
    srli t3, t3, 2                      # face of the move that led here
    beq  t2, t3, search_loop            # same face, skip
    andi t3, t0, 3                      # turn
    lhu  a0, 8(s0)                      # child slot (previous turn)
    lhu  a1, 10(s0)
    bnez t3, have_src
    lhu  a0, 0(s0)                      # turn 0 starts from this frame
    lhu  a1, 2(s0)
have_src:
    slli t3, t0, 3
    add  t3, s8, t3
    lw   t4, 0(t3)                      # perm row
    lw   t5, 4(t3)                      # orien row
    slli a0, a0, 1
    add  a0, t4, a0
    lhu  a0, 0(a0)                      # next p
    slli a1, a1, 1
    add  a1, t5, a1
    lhu  a1, 0(a1)                      # next o
    sh   a0, 8(s0)                      # child
    sh   a1, 10(s0)
    sb   t0, 5(s0)                      # path
    or   t1, a0, a1
    beqz t1, found
    jal  ra, guess                      # a0, a1 are already the child
    bltu s1, a0, search_loop            # cut
    sb   zero, 12(s0)                   # child starts at move 0
    addi s0, s0, 8                      # go down
    addi s1, s1, -1
    j    search_loop

go_up:
    addi s0, s0, -8
    addi s1, s1, 1
    bgeu s0, s7, search_loop            # still inside the round
    addi s9, s9, 1                      # next limit
    j    round

no_solution:
    li   a0, 1
    li   a7, 93                         # exit(1)
    ecall

    # T5: replay the path from the root, it must reach solved
found:
    lhu  a0, 0(s7)                      # root p
    lhu  a1, 2(s7)                      # root o
    mv   t0, s7
    slli t1, s9, 3
    add  t1, s7, t1
replay:
    beq  t0, t1, replay_done
    lbu  t2, 5(t0)                      # move
    slli t3, t2, 3
    add  t3, s8, t3
    lw   t4, 0(t3)                      # rows of its face
    lw   t5, 4(t3)
    andi t6, t2, 3
    addi t6, t6, 1                      # quarter turns
replay_turn:
    slli a0, a0, 1
    add  a0, t4, a0
    lhu  a0, 0(a0)
    slli a1, a1, 1
    add  a1, t5, a1
    lhu  a1, 0(a1)
    addi t6, t6, -1
    bnez t6, replay_turn
    addi t0, t0, 8
    j    replay
replay_done:
    or   t2, a0, a1
    bnez t2, check_failed               # path must reach solved
    lw   t2, 4(a5)                      # expected length
    bltz t2, print_result               # -1 = any
    bne  t2, s9, check_failed

    # print the length and the move numbers
print_result:
    mv   a0, s9
    li   a7, 1
    ecall
    mv   t0, s7                         # frame 0
    slli t1, s9, 3
    add  t1, s7, t1                     # end = frame s9
print_path:
    beq  t0, t1, print_done
    li   a0, 32
    li   a7, 11
    ecall
    lbu  a0, 5(t0)
    li   a7, 1
    ecall
    addi t0, t0, 8
    j    print_path
print_done:
    li   a0, 10
    li   a7, 11
    ecall
.if RENDER
    # animation: rebuild the input from solved, then play the path
    slli t0, s9, 3
    add  s1, s7, t0                     # s1 = frame after the last move
    mv   s0, s1
rebuild:
    beq  s0, s7, rebuild_done           # back at frame 0
    addi s0, s0, -8                     # previous frame
    lbu  t0, 5(s0)                      # move
    srli a0, t0, 2                      # face
    andi t0, t0, 3                      # turn
    li   a1, 3
    sub  a1, a1, t0                     # inverse = 3 - turn quarter turns
    jal  ra, turn_face
    j    rebuild
rebuild_done:
    jal  ra, draw                       # the input state
play:
    beq  s0, s1, play_done              # all moves shown
    lbu  t0, 5(s0)                      # move
    srli a0, t0, 2                      # face
    andi a1, t0, 3
    addi a1, a1, 1                      # turn + 1 quarter turns
    jal  ra, turn_face
    jal  ra, draw
    addi s0, s0, 8                      # next frame
    j    play
play_done:
.endif
    addi a5, a5, 8
    bne  a5, a6, next_case
    li   a7, 10                         # exit(0)
    ecall


check_failed:
    li   a0, 3
    li   a7, 93                         # exit(3)
    ecall

    # guess(a0 = p, a1 = o) -> a0 = max(P, T, Q)
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

.if RENDER
    # turn_face(a0 = face, a1 = quarter turns)
turn_face:
    la   t2, stickers
    la   t0, cycles
    slli a0, a0, 4                      # 16 bytes per face
    add  a0, t0, a0                     # row of this face
turn_once:
    mv   t0, a0                         # first cycle
    addi t1, a0, 12                     # end of the 3 cycles
turn_cycle:
    lbu  t3, 0(t0)
    add  t3, t2, t3                     # &s[a]
    lbu  t4, 1(t0)
    add  t4, t2, t4                     # &s[b]
    lbu  t5, 2(t0)
    add  t5, t2, t5                     # &s[c]
    lbu  t6, 3(t0)
    add  t6, t2, t6                     # &s[d]

    lbu  a2, 0(t6)                      # keep s[d]
    lbu  a3, 0(t5)
    sb   a3, 0(t6)                      # s[d] = s[c]
    lbu  a3, 0(t4)
    sb   a3, 0(t5)                      # s[c] = s[b]
    lbu  a3, 0(t3)
    sb   a3, 0(t4)                      # s[b] = s[a]
    sb   a2, 0(t3)                      # s[a] = old s[d]

    addi t0, t0, 4                      # next cycle
    bne  t0, t1, turn_cycle
    addi a1, a1, -1                     # one quarter turn done
    bnez a1, turn_once
    ret

    # draw(): paint all 24 stickers
draw:
    la   t0, stickers
    addi t1, t0, 24                     # end of stickers
    la   t2, pixels
    la   t3, colors
draw_loop:
    lbu  t4, 0(t0)                      # color number
    slli t4, t4, 2
    add  t4, t3, t4
    lw   t4, 0(t4)                      # 0x00RRGGBB

    lw   t5, 0(t2)                      # LED address of the top-left pixel

    sw   t4, 0(t5)                      # row 0
    sw   t4, 4(t5)
    sw   t4, 8(t5)
    sw   t4, 12(t5)
    sw   t4, W*4(t5)                    # row 1
    sw   t4, W*4+4(t5)
    sw   t4, W*4+8(t5)
    sw   t4, W*4+12(t5)
    sw   t4, 2*W*4(t5)                  # row 2
    sw   t4, 2*W*4+4(t5)
    sw   t4, 2*W*4+8(t5)
    sw   t4, 2*W*4+12(t5)

    addi t0, t0, 1                      # next sticker
    addi t2, t2, 4                      # next pixels entry
    bne  t0, t1, draw_loop
    li   t0, DELAY
draw_wait:
    addi t0, t0, -1                     # busy-wait so the picture stays visible
    bnez t0, draw_wait
    ret
.endif

invalid:
    li   a0, 2
    li   a7, 93                         # exit(2)
    ecall
