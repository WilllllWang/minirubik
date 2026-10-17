# The tables of main.S, made visible to ref.c
.section .rodata
.globl permutation, orientation, sub_pos_table
.globl perm_dist_table, orien_dist_table, sub_dist_table, twist_table
.include "tables.s"
