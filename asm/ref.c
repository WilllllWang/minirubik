/* gcc -O2 reference for main.S: the search functions of search.c, unchanged
 * except that the nodes counter is removed, run on Ripes like main.S.
 */
#include <stdint.h>

#define TESTS 1 /* 1: run the 3 test cases before the input */

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    MOVES = 12,
    SUB_STATES = 840 * 81
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

/* from ref_tables.S, the same tables.s as main.S */
extern const uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
extern const uint16_t sub_pos_table[PERMUTATIONS];
extern const uint8_t perm_dist_table[PERMUTATIONS], orien_dist_table[ORIENTATIONS];
extern const uint8_t sub_dist_table[SUB_STATES];
extern const uint8_t twist_table[ORIENTATIONS][8];

static const uint16_t *const perm_row[3] = {permutation[0], permutation[1], permutation[2]};
static const uint16_t *const orien_row[3] = {orientation[0], orientation[1], orientation[2]};
static uint8_t path[11];

static void rank_state(const state_t *state, uint16_t *p_rank, uint16_t *o_rank)
{
    uint8_t smaller[CUBIES - 1];
    for (uint8_t i = 0; i < CUBIES - 1; ++i) {
        smaller[i] = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller[i];
    }
    uint32_t p = smaller[0];
    p = (p << 3) - (p << 1) + smaller[1];
    p = (p << 2) + p + smaller[2];
    p = (p << 2) + smaller[3];
    p = (p << 1) + p + smaller[4];
    p = (p << 1) + smaller[5];
    *p_rank = (uint16_t) p;

    uint32_t o = 0;
    for (uint8_t i = 0; i < 6; ++i)
        o = (o << 1) + o + state->o[i];
    *o_rank = (uint16_t) o;
}

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    while (sum >= 3)
        sum = (uint8_t) (sum - 3);
    return sum == 0;
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < CUBIES; ++i) {
        if (input[i] < '1' || input[i] > '7')
            return 0;
        state->p[i] = (uint8_t) (input[i] - '1');
    }
    for (int i = 0; i < CUBIES; ++i) {
        if (input[CUBIES + i] < '1' || input[CUBIES + i] > '3')
            return 0;
        state->o[i] = (uint8_t) (input[CUBIES + i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

static uint8_t max(uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

static uint32_t sub_index(uint16_t p, uint16_t o)
{
    uint32_t packed = sub_pos_table[p];
    uint32_t pos0 = packed & 7;
    uint32_t pos1 = (packed >> 3) & 7;
    uint32_t pos2 = (packed >> 6) & 7;
    uint32_t pos3 = (packed >> 9) & 7;

    uint32_t d1 = pos1 - (pos0 < pos1);
    uint32_t d2 = pos2 - (pos0 < pos2) - (pos1 < pos2);
    uint32_t d3 = pos3 - (pos0 < pos3) - (pos1 < pos3) - (pos2 < pos3);
    uint32_t rank = (pos0 << 3) - (pos0 << 1) + d1;
    rank = (rank << 2) + rank + d2;
    rank = (rank << 2) + d3;

    const uint8_t *tw = twist_table[o];
    uint32_t twists = tw[pos0];
    twists = (twists << 1) + twists + tw[pos1];
    twists = (twists << 1) + twists + tw[pos2];
    twists = (twists << 1) + twists + tw[pos3];

    return (rank << 6) + (rank << 4) + rank + twists;
}

static uint8_t guess(uint16_t p, uint16_t o)
{
    uint8_t pt = max(perm_dist_table[p], orien_dist_table[o]);
    return max(pt, sub_dist_table[sub_index(p, o)]);
}

static int DFID(uint16_t p, uint16_t o)
{
    uint16_t stack_p[12], stack_o[12];
    uint8_t stack_move[12];
    for (int limit = guess(p, o); limit <= 11; ++limit) {
        if (p == 0 && o == 0)
            return 0;
        int depth = 0;
        stack_p[0] = p;
        stack_o[0] = o;
        stack_move[0] = 0;

        while (depth >= 0) {
            uint8_t move = stack_move[depth];
            if (move == MOVES) {
                --depth;
                continue;
            }
            uint8_t next_move = (uint8_t) (move + 1);
            if ((next_move & 3) == 3)
                ++next_move;
            stack_move[depth] = next_move;

            uint8_t face = move >> 2, turn = move & 3;
            if (depth > 0 && face == path[depth - 1] >> 2)
                continue;

            uint16_t from_p = turn == 0 ? stack_p[depth] : stack_p[depth + 1];
            uint16_t from_o = turn == 0 ? stack_o[depth] : stack_o[depth + 1];
            uint16_t next_p = perm_row[face][from_p];
            uint16_t next_o = orien_row[face][from_o];
            stack_p[depth + 1] = next_p;
            stack_o[depth + 1] = next_o;
            path[depth] = move;

            if (next_p == 0 && next_o == 0)
                return limit;
            if (depth + 1 + guess(next_p, next_o) > limit)
                continue;
            stack_move[depth + 1] = 0;
            ++depth;
        }
    }
    return -1;
}

static int check_solution(uint16_t p, uint16_t o, int length)
{
    for (int i = 0; i < length; ++i) {
        uint8_t face = path[i] >> 2, turns = (path[i] & 3) + 1;
        for (uint8_t turn = 0; turn < turns; ++turn) {
            p = perm_row[face][p];
            o = orien_row[face][o];
        }
    }
    return p == 0 && o == 0;
}

/* Ripes ecalls: a7 = 1 print_int, 11 print_char, 93 exit */
static void env_call(int a7_value, int a0_value)
{
    register int a0 asm("a0") = a0_value;
    register int a7 asm("a7") = a7_value;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

static const char input[] = "54721631111111";

static const struct {
    const char *state;
    int length; /* -1 = any */
} cases[] = {
#if TESTS
    {"12345671111111", 0},
    {"26547312333212", 3}, /* R B D' from solved */
    {"21345671111111", 11},
#endif
    {input, -1},
};

/* Same work and exit codes as main.S: 0 passed, 1 no solution,
 * 2 invalid input, 3 replay or length check failed.
 */
void _start(void)
{
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        state_t state;
        uint16_t p, o;
        if (!parse_state(cases[i].state, &state))
            env_call(93, 2);
        rank_state(&state, &p, &o);
        int length = DFID(p, o);
        if (length < 0)
            env_call(93, 1);
        if (!check_solution(p, o, length) ||
            (cases[i].length >= 0 && length != cases[i].length))
            env_call(93, 3);
        env_call(1, length);
        for (int j = 0; j < length; ++j) {
            env_call(11, ' ');
            env_call(1, path[j]);
        }
        env_call(11, '\n');
    }
    env_call(93, 0);
}
