#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
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

static uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
static uint8_t perm_dist_table[PERMUTATIONS], orien_dist_table[ORIENTATIONS];
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

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
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
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
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
    return sum % 3U == 0;
}


static void build_transition_table(void)
{
    state_t state;
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
}


static void build_perm_dist_table(void)
{
    uint8_t distance;
    uint16_t queue[PERMUTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    memset(perm_dist_table, UINT8_MAX, PERMUTATIONS);
    queue[0] = 0;
    perm_dist_table[0] = 0;
    distance = 1;
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
    uint8_t distance;
    uint16_t queue[ORIENTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    memset(orien_dist_table, UINT8_MAX, ORIENTATIONS);
    queue[0] = 0;
    orien_dist_table[0] = 0;
    distance = 1;
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

static int check_table(int *perm_max, int *orien_max)
{
    *perm_max = 0;
    if (perm_dist_table[0] != 0) return 1;
    for (uint16_t p = 0; p < PERMUTATIONS; p++) {
        if (perm_dist_table[p] == UINT8_MAX) {
            return 1;
        }
        if (perm_dist_table[p] > *perm_max) {
            *perm_max = perm_dist_table[p];
        }
    }

    *orien_max = 0;
    if (orien_dist_table[0] != 0) return 1;
    for (uint16_t o = 0; o < ORIENTATIONS; o++) {
        if (orien_dist_table[o] == UINT8_MAX) {
            return 1;
        }
        if (orien_dist_table[o] > *orien_max) {
            *orien_max = orien_dist_table[o];
        }
    }
    return 0;
}

static uint8_t max(uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

static int recursion(uint16_t p, uint16_t o, int limit, int curr_depth, int prev_face)
{
    nodes++;
    if (p == 0 && o == 0) return 1;
    if (curr_depth + max(perm_dist_table[p], orien_dist_table[o]) > limit) return 0;

    int found = 0;
    for (uint8_t face = 0; face < 3; ++face) {
        if (face == prev_face) continue;
        uint16_t next_p = p;
        uint16_t next_o = o;
        for (uint8_t turn = 0; turn < 3; ++turn) {
            next_p = permutation[face][next_p];
            next_o = orientation[face][next_o];
            path[curr_depth] = face * 3 + turn;
            found = recursion(next_p, next_o, limit, curr_depth + 1, face);
            if (found) return found;
        }
    }

    return found;
}

static int DFID(uint16_t p, uint16_t o)
{
    nodes = 0;
    int found = 0;
    for (int limit = 0); limit <= 11; limit++) {
        found = recursion(p, o, limit, 0, 3);
        if (found) return limit;
    }

    return -1;
}

static int check_solution(uint16_t p, uint16_t o, int length)
{
    for (int i = 0; i < length; ++i) {
        uint8_t face = path[i] / 3, turns = path[i] % 3 + 1;
        for (uint8_t t = 0; t < turns; ++t) {
            p = permutation[face][p];
            o = orientation[face][o];
        }
    }
    return p == 0 && o == 0;
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

int main(int argc, char **argv)
{
    state_t state;
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "search");
        return 2;
    }

    build_transition_table();
    build_perm_dist_table();
    build_orien_dist_table();

    int perm_max, orien_max;
    int result = check_table(&perm_max, &orien_max);
    if (result == 1) {
        fputs("Table not valid\n", stderr);
        return 1;
    }
    printf("Table check complete\n");
    printf("Permutation Maximum = %d\nOrientation Maximum = %d\n", perm_max, orien_max);

    uint32_t rank = rank_state(&state);
    uint16_t p = (uint16_t) (rank / ORIENTATIONS), o = (uint16_t) (rank % ORIENTATIONS);
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
    printf("Moves: ");
    for (int i = 0; i < length; ++i)
        printf(" %s", move_names[path[i]]);
    putchar('\n');
    return 0;
}
