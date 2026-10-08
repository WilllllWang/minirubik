#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 12,
    TRACKED = 4,
    SUB_POSITIONS = 840,
    SUB_TWISTS = 81,
    SUB_STATES = SUB_POSITIONS * SUB_TWISTS
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

typedef struct {
    uint8_t pos[TRACKED], tw[TRACKED];
} sub_state_t;

static const char *const move_names[MOVES] = {"R", "R2", "R'", "", "B", "B2", "B'", "", "D", "D2", "D'", ""};

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};

static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static const uint8_t tracked[TRACKED] = {3, 4, 5, 6};

static uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
static uint16_t *const perm_row[3] = {permutation[0], permutation[1], permutation[2]};
static uint16_t *const orien_row[3] = {orientation[0], orientation[1], orientation[2]};
static uint8_t perm_dist_table[PERMUTATIONS], orien_dist_table[ORIENTATIONS];
static uint8_t sub_dist_table[SUB_STATES];
static uint16_t sub_pos_table[PERMUTATIONS];
static uint8_t twist_table[ORIENTATIONS][8];
static uint32_t sub_solved;
static uint8_t full_dist_table[STATES];
static uint8_t path[11];
static uint64_t nodes;

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

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

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < (uint32_t) (CUBIES - i); ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
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

static sub_state_t quarter_turn_sub(sub_state_t sub_state, uint8_t face)
{
    for (uint8_t i = 0; i < TRACKED; ++i) {
        for (uint8_t j = 0; j < CUBIES; ++j) {
            if (source[face][j] == sub_state.pos[i]) {
                sub_state.pos[i] = j;
                sub_state.tw[i] = (uint8_t) ((sub_state.tw[i] + twist[face][j]) % 3U);
                break;
            }
        }
    }
    return sub_state;
}

static uint32_t rank_sub_state(const sub_state_t *sub_state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < TRACKED; ++i) {
        uint8_t smaller = sub_state->pos[i];
        for (uint8_t j = 0; j < i; ++j)
            if (sub_state->pos[j] < sub_state->pos[i])
                --smaller;
        p = p * (uint32_t) (CUBIES - i) + smaller;
        o = o * 3U + sub_state->tw[i];
    }
    return p * SUB_TWISTS + o;
}

static void unrank_sub_state(uint32_t rank, sub_state_t *sub_state)
{
    uint32_t p = rank / SUB_TWISTS, o = rank % SUB_TWISTS;
    uint8_t digit[TRACKED], used[CUBIES] = {0};
    for (uint8_t i = TRACKED; i-- > 0;) {
        digit[i] = (uint8_t) (p % (uint32_t) (CUBIES - i));
        p /= (uint32_t) (CUBIES - i);
        sub_state->tw[i] = (uint8_t) (o % 3U);
        o /= 3U;
    }
    for (uint8_t i = 0; i < TRACKED; ++i) {
        uint8_t count = digit[i];
        for (uint8_t j = 0; j < CUBIES; ++j) {
            if (used[j])
                continue;
            if (count == 0) {
                sub_state->pos[i] = j;
                used[j] = 1;
                break;
            }
            --count;
        }
    }
}

static void build_transition_table(void)
{
    state_t state;
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            uint16_t next_p, next_o;
            rank_state(&next, &next_p, &next_o);
            permutation[face][rank] = next_p;
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            uint16_t next_p, next_o;
            rank_state(&next, &next_p, &next_o);
            orientation[face][rank] = next_o;
        }
    }
}

static void build_perm_dist_table(void)
{
    uint8_t distance = 1;
    uint16_t queue[PERMUTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    memset(perm_dist_table, UINT8_MAX, PERMUTATIONS);
    queue[0] = 0;
    perm_dist_table[0] = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++distance;
        }
        uint16_t p = queue[head++];
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                if (perm_dist_table[next_p] == UINT8_MAX) {
                    perm_dist_table[next_p] = distance;
                    queue[tail++] = next_p;
                }
            }
        }
    }
}

static void build_orien_dist_table(void)
{
    uint8_t distance = 1;
    uint16_t queue[ORIENTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    memset(orien_dist_table, UINT8_MAX, ORIENTATIONS);
    queue[0] = 0;
    orien_dist_table[0] = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++distance;
        }
        uint16_t o = queue[head++];
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_o = orientation[face][next_o];
                if (orien_dist_table[next_o] == UINT8_MAX) {
                    orien_dist_table[next_o] = distance;
                    queue[tail++] = next_o;
                }
            }
        }
    }
}

static void build_sub_dist_table(void)
{
    uint8_t distance = 1;
    uint32_t *queue = malloc((size_t) SUB_STATES * sizeof *queue);
    if (!queue)
        return;
    uint32_t head = 0, tail = 1, level_end = 1;
    sub_state_t solved;
    for (uint8_t i = 0; i < TRACKED; ++i) {
        solved.pos[i] = tracked[i];
        solved.tw[i] = 0;
    }
    sub_solved = rank_sub_state(&solved);
    memset(sub_dist_table, UINT8_MAX, SUB_STATES);
    queue[0] = sub_solved;
    sub_dist_table[sub_solved] = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++distance;
        }
        sub_state_t here;
        unrank_sub_state(queue[head++], &here);
        for (uint8_t face = 0; face < 3; ++face) {
            sub_state_t next = here;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next = quarter_turn_sub(next, face);
                uint32_t there = rank_sub_state(&next);
                if (sub_dist_table[there] == UINT8_MAX) {
                    sub_dist_table[there] = distance;
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
}

static void tracked_positions(uint16_t p, uint8_t pos[TRACKED])
{
    state_t state;
    unrank_state((uint32_t) p * ORIENTATIONS, &state);
    for (uint8_t i = 0; i < TRACKED; ++i)
        for (uint8_t j = 0; j < CUBIES; ++j)
            if (state.p[j] == tracked[i])
                pos[i] = j;
}

static void build_sub_index_tables(void)
{
    state_t state;
    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        uint8_t pos[TRACKED];
        tracked_positions(p, pos);
        sub_pos_table[p] = 0;
        for (uint8_t i = 0; i < TRACKED; ++i)
            sub_pos_table[p] |= (uint16_t) (pos[i] << (3 * i));
    }
    for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
        unrank_state(o, &state);
        for (uint8_t j = 0; j < CUBIES; ++j)
            twist_table[o][j] = state.o[j];
    }
}

static void build_full_dist_table(void)
{
    uint8_t distance = 1;
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    if (!queue)
        return;
    uint32_t head = 0, tail = 1, level_end = 1;
    memset(full_dist_table, UINT8_MAX, STATES);
    queue[0] = 0;
    full_dist_table[0] = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++distance;
        }
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (full_dist_table[there] == UINT8_MAX) {
                    full_dist_table[there] = distance;
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
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
    nodes = 0;
    for (int limit = guess(p, o); limit <= 11; ++limit) {
        ++nodes;
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
            ++nodes;

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

static int check_tables(int *perm_max, int *orien_max, int *sub_max, int *full_max)
{
    *perm_max = 0;
    if (perm_dist_table[0] != 0)
        return 1;
    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        if (perm_dist_table[p] == UINT8_MAX)
            return 1;
        if (perm_dist_table[p] > *perm_max)
            *perm_max = perm_dist_table[p];
    }

    *orien_max = 0;
    if (orien_dist_table[0] != 0)
        return 1;
    for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
        if (orien_dist_table[o] == UINT8_MAX)
            return 1;
        if (orien_dist_table[o] > *orien_max)
            *orien_max = orien_dist_table[o];
    }

    *sub_max = 0;
    if (sub_dist_table[sub_solved] != 0)
        return 1;
    for (uint32_t rank = 0; rank < SUB_STATES; ++rank) {
        if (sub_dist_table[rank] == UINT8_MAX)
            return 1;
        if (sub_dist_table[rank] > *sub_max)
            *sub_max = sub_dist_table[rank];
    }

    *full_max = 0;
    if (full_dist_table[0] != 0)
        return 1;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        if (full_dist_table[rank] == UINT8_MAX)
            return 1;
        if (full_dist_table[rank] > *full_max)
            *full_max = full_dist_table[rank];
    }
    return 0;
}

static int check_packed(void)
{
    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        uint8_t pos[TRACKED];
        tracked_positions(p, pos);
        for (uint8_t i = 0; i < TRACKED; ++i)
            if (((sub_pos_table[p] >> (3 * i)) & 7) != pos[i])
                return 1;
    }
    return 0;
}

static int check_admissible(void)
{
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint16_t p = (uint16_t) (rank / ORIENTATIONS);
        uint16_t o = (uint16_t) (rank % ORIENTATIONS);
        if (guess(p, o) > full_dist_table[rank])
            return 1;
    }
    return 0;
}

static int check_optimal(uint64_t *worst_nodes, uint32_t *worst_rank)
{
    *worst_nodes = 0;
    *worst_rank = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint16_t p = (uint16_t) (rank / ORIENTATIONS);
        uint16_t o = (uint16_t) (rank % ORIENTATIONS);
        int length = DFID(p, o);
        if (length != full_dist_table[rank] || !check_solution(p, o, length))
            return 1;
        if (full_dist_table[rank] == 11 && nodes > *worst_nodes) {
            *worst_nodes = nodes;
            *worst_rank = rank;
        }
    }
    return 0;
}

static int run_gates(void)
{
    build_full_dist_table();

    int perm_max, orien_max, sub_max, full_max;
    if (check_tables(&perm_max, &orien_max, &sub_max, &full_max) || full_max != 11) {
        fputs("H2 failed: a table is not valid\n", stderr);
        return 1;
    }
    if (check_admissible()) {
        fputs("H1 failed: guess exceeds true distance\n", stderr);
        return 1;
    }
    if (check_packed()) {
        fputs("H4 failed: a packed position differs\n", stderr);
        return 1;
    }
    uint64_t worst_nodes;
    uint32_t worst_rank;
    if (check_optimal(&worst_nodes, &worst_rank)) {
        fputs("H3 failed: a search result is not the true distance\n", stderr);
        return 1;
    }

    state_t worst;
    unrank_state(worst_rank, &worst);
    printf("H1 passed: guess never exceeds true distance\n");
    printf("H2 passed: tables full, maxima %d, %d, %d, %d\n", perm_max, orien_max, sub_max, full_max);
    printf("H3 passed: every result is the true distance\n");
    printf("H4 passed: packed positions match for all %d permutations\n", PERMUTATIONS);
    printf("Worst distance-11 state: ");
    for (int i = 0; i < CUBIES; ++i)
        putchar('1' + worst.p[i]);
    for (int i = 0; i < CUBIES; ++i)
        putchar('1' + worst.o[i]);
    printf(", %llu nodes\n", (unsigned long long) worst_nodes);
    return 0;
}

static void print_halves(const char *label, const uint16_t *values, uint32_t count)
{
    printf(".balign 2\n%s:", label);
    for (uint32_t i = 0; i < count; ++i)
        printf(i % 16 ? ", %u" : "\n.half %u", values[i]);
    putchar('\n');
}

static void print_bytes(const char *label, const uint8_t *values, uint32_t count)
{
    printf("%s:", label);
    for (uint32_t i = 0; i < count; ++i)
        printf(i % 16 ? ", %u" : "\n.byte %u", values[i]);
    putchar('\n');
}

static int print_tables(void)
{
    print_halves("permutation", &permutation[0][0], 3 * PERMUTATIONS);
    print_halves("orientation", &orientation[0][0], 3 * ORIENTATIONS);
    print_halves("sub_pos_table", sub_pos_table, PERMUTATIONS);
    print_bytes("perm_dist_table", perm_dist_table, PERMUTATIONS);
    print_bytes("orien_dist_table", orien_dist_table, ORIENTATIONS);
    print_bytes("sub_dist_table", sub_dist_table, SUB_STATES);
    print_bytes("twist_table", &twist_table[0][0], ORIENTATIONS * 8);
    return fflush(stdout) != 0;
}

int main(int argc, char **argv)
{
    build_transition_table();
    build_perm_dist_table();
    build_orien_dist_table();
    build_sub_dist_table();
    build_sub_index_tables();

    if (argc == 2 && !strcmp(argv[1], "--gates"))
        return run_gates();
    if (argc == 2 && !strcmp(argv[1], "--tables"))
        return print_tables();

    state_t state;
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO | --gates | --tables\n", argc > 0 && argv[0] ? argv[0] : "search");
        return 2;
    }

    uint16_t p, o;
    rank_state(&state, &p, &o);
    int length = DFID(p, o);
    if (length < 0) {
        fputs("No solution within 11 moves\n", stderr);
        return 1;
    }
    if (!check_solution(p, o, length)) {
        fputs("Solution check failed: moves do not reach solved\n", stderr);
        return 1;
    }
    printf("Solution check complete\n");
    printf("Total nodes: %llu\n", (unsigned long long) nodes);
    printf("Total moves: %d\n", length);
    printf("Moves:");
    for (int i = 0; i < length; ++i)
        printf(" %s", move_names[path[i]]);
    putchar('\n');
    return 0;
}
