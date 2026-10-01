#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#define min(a, b) ((a) < (b) ? (a) : (b))
static clock_t wall_clock(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (clock_t)ts.tv_sec * CLOCKS_PER_SEC + (clock_t)ts.tv_nsec / (1000000000 / CLOCKS_PER_SEC);
}
#define clock wall_clock
#endif

#define Bitboard uint64_t
#define var uint8_t

#define TOL 0.24
#define UCI_MODE 1
#define EVAL_MODE 0
#define E 2.718281828459
#define PI 3.14159265358979

Bitboard trackers[4] = {0, 0, 0, 0};

static int16_t PST_WHITE[6][64];
static int16_t PST_BLACK[6][64];
static int16_t PST_CONDITIONAL[2][6][64];
static double historyTable[2][2][64][256];
static int moveTable[64][256];
static int gamePhase[33];

int movesPlayed[1] = {0};

static var g_ttFrom = 255, g_ttTo = 255;

static Bitboard repStack[4096];
static int repPlies = 0;

double tuners[9] = {0.231293, 0.864824, 0.276000, 0.752314, -0.779402, 2.297329, 0.163243, -0.451373, 0.664241};
int constants[80] = {0};

extern const Bitboard knightBitboards[64];
extern const Bitboard kingBitboards[64];
extern const Bitboard fileBitboards[8];
extern const Bitboard isolatedBitboards[8];
extern const Bitboard passedBitboards[64];
extern const Bitboard ROOK_MASKS[64];
extern const Bitboard ROOK_MAGICS[64];
extern const Bitboard ROOK_ATTACK_TABLE_FLAT[262144];
extern const Bitboard BISHOP_MASKS[64];
extern const Bitboard BISHOP_MAGICS[64];
extern const Bitboard BISHOP_ATTACK_TABLE_FLAT[32768];

double LOG_CONSTANT = 3.3939956795872539;
double LMRTable[218];

static const int16_t PST_INIT[7][64] = {
    // Pawn
    {2, -3, -5, -1, -1, -5, -3, 2,
     498, 494, 491, 495, 495, 491, 494, 498,
     112, 108, 205, 309, 309, 205, 108, 112,
     56, 52, 99, 253, 253, 99, 52, 56,
     7, 3, 0, 204, 204, 0, 3, 7,
     60, -45, -97, 7, 7, -97, -45, 60,
     48, 94, 91, -205, -205, 91, 94, 48,
     2, -3, -5, -1, -1, -5, -3, 2},
    // Knight
    {-269, -161, -60, -61, -61, -60, -161, -269,
     -265, -57, 144, 144, 144, 144, -57, -265,
     -195, 113, 214, 264, 264, 214, 113, -195,
     -221, 137, 238, 288, 288, 238, 137, -221,
     -251, 57, 208, 258, 258, 208, 57, -251,
     -243, 115, 166, 216, 216, 166, 115, -243,
     -338, -130, 71, 121, 121, 71, -130, -338,
     -267, -159, -58, -59, -59, -58, -159, -267},
    // Bishop
    {-32, 69, 68, 69, 69, 68, 69, -32,
     4, 105, 104, 105, 105, 104, 105, 4,
     -9, 92, 141, 192, 192, 141, 92, -9,
     -21, 130, 129, 180, 180, 129, 130, -21,
     -27, 74, 173, 174, 174, 173, 74, -27,
     -44, 157, 156, 157, 157, 156, 157, -44,
     -26, 125, 74, 75, 75, 74, 125, -26,
     -108, -8, -9, -7, -7, -9, -8, -108},
    // Rook
    {154, 156, 155, 156, 156, 155, 156, 154,
     151, 203, 202, 203, 203, 202, 203, 151,
     94, 146, 145, 146, 146, 145, 146, 94,
     82, 134, 133, 134, 134, 133, 134, 82,
     64, 116, 115, 116, 116, 115, 116, 64,
     47, 99, 98, 99, 99, 98, 99, 47,
     46, 98, 97, 98, 98, 97, 98, 46,
     90, 92, 91, 142, 142, 91, 92, 90},
    // Queen
    {122, 222, 221, 270, 270, 221, 222, 122,
     146, 246, 245, 244, 244, 245, 246, 146,
     122, 222, 271, 270, 270, 271, 222, 122,
     163, 213, 262, 261, 261, 262, 213, 163,
     173, 198, 247, 246, 246, 247, 198, 173,
     87, 212, 236, 235, 235, 236, 212, 87,
     94, 194, 243, 192, 192, 243, 194, 94,
     8, 108, 107, 156, 156, 107, 108, 8},
    // King early-game
    {-300, -400, -400, -500, -500, -400, -400, -300,
     -300, -400, -400, -500, -500, -400, -400, -300,
     -300, -400, -400, -500, -500, -400, -400, -300,
     -300, -400, -400, -500, -500, -400, -400, -300,
     -200, -300, -300, -400, -400, -300, -300, -200,
     -100, -200, -200, -200, -200, -200, -200, -100,
     200, 200, 0, 0, 0, 0, 200, 200,
     200, 300, 100, 0, 0, 100, 300, 200},
    // King endgame
    {-200, -131, -83, -65, -65, -83, -131, -200,
     -131, -46, 18, 44, 44, 18, -46, -131,
     -83, 18, 108, 153, 153, 108, 18, -83,
     -65, 44, 153, 262, 262, 153, 44, -65,
     -65, 44, 153, 262, 262, 153, 44, -65,
     -83, 18, 108, 153, 153, 108, 18, -83,
     -131, -46, 18, 44, 44, 18, -46, -131,
     -200, -131, -83, -65, -65, -83, -131, -200}};

Bitboard board_global[15];

#define CAST_WK 1ULL
#define CAST_WQ 2ULL
#define CAST_BK 4ULL
#define CAST_BQ 8ULL
#define EP_SHIFT 4
#define EP_MASK ((Bitboard)0x3FULL << EP_SHIFT)

static inline int get_ep_square_from_flags(Bitboard flags) {
    Bitboard v = (flags & EP_MASK) >> EP_SHIFT;
    if (!v) return -1;
    return (int)(v - 1);
}
static inline void set_ep_square_in_flags(Bitboard *flags_ptr, int sq) {
    Bitboard flags = *flags_ptr;
    flags &= ~EP_MASK;
    if (sq >= 0 && sq < 64) flags |= ((Bitboard)(sq + 1) << EP_SHIFT);
    *flags_ptr = flags;
}

double normal_random(double mean, double stddev) {
    return mean + stddev * sqrt(-2.0 * log((rand() + 1.0) / (RAND_MAX + 2.0))) * cos((rand() + 1.0) / (RAND_MAX + 2.0) * 2.0 * PI);
}

static bool is_attacked(Bitboard b[], Bitboard sq, bool byWhite) {
    Bitboard occupied_all = b[12] | b[13];
    if (byWhite) {
        if ((b[1] & knightBitboards[sq]) || (b[5] & kingBitboards[sq]) || ((((b[0] & 0xFEFEFEFEFEFEFEFEULL) << 7) | ((b[0] & 0x7F7F7F7F7F7F7F7FULL) << 9)) & (1ULL << sq))) return true;
        if ((b[2] | b[4]) & BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)]) {
            return true;
        }
        if ((b[3] | b[4]) & ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)]) {
            return true;
        }
    } else {
        if ((b[7] & knightBitboards[sq]) || (b[11] & kingBitboards[sq]) || ((((b[6] & 0x7F7F7F7F7F7F7F7FULL) >> 7) | ((b[6] & 0xFEFEFEFEFEFEFEFEULL) >> 9)) & (1ULL << sq))) return true;
        if ((b[8] | b[10]) & BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)]) {
            return true;
        }
        if ((b[9] | b[10]) & ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)]) {
            return true;
        }
    }
    return false;
}

static bool isLegalMove(var from, var to, Bitboard b[], bool isWhite) {
    int capturedPieceID = 255;
    int capturedSq = -1;

    if (isWhite) {
        for (Bitboard i = 6 + 6 * ((b[13] & (1ULL << to)) == 0); i < 12; ++i) {
            if (b[i] & (1ULL << to)) {
                b[i] ^= (1ULL << to);
                b[13] ^= (1ULL << to);
                capturedPieceID = i;
                capturedSq = to;
                break;
            }
        }

        if (capturedPieceID == 255) {
            if ((get_ep_square_from_flags(b[14]) == to) && (b[0] & (1ULL << from))) {
                int cap_pawn_sq = to - 8;
                if (cap_pawn_sq >= 0 && (b[6] & (1ULL << cap_pawn_sq))) {
                    b[6] ^= (1ULL << cap_pawn_sq);
                    b[13] ^= (1ULL << cap_pawn_sq);
                    capturedPieceID = 6;
                    capturedSq = cap_pawn_sq;
                }
            }
        }

        for (int i = 0; i < 6; ++i) {
            if (b[i] & (1ULL << from)) {
                b[i] ^= (1ULL << from) | (1ULL << to);
                b[12] ^= (1ULL << from) | (1ULL << to);
                bool inCheck = is_attacked(b, __builtin_ctzll(b[5]), false);
                b[i] ^= (1ULL << from) | (1ULL << to);
                b[12] ^= (1ULL << from) | (1ULL << to);
                if (capturedPieceID != 255) {
                    b[capturedPieceID] |= (1ULL << capturedSq);
                    b[13] |= (1ULL << capturedSq);
                }
                return !inCheck;
            }
        }
    } else {
        for (Bitboard i = 0 + 6 * ((b[12] & (1ULL << to)) == 0); i < 6; ++i) {
            if (b[i] & (1ULL << to)) {
                b[i] ^= (1ULL << to);
                b[12] ^= (1ULL << to);
                capturedPieceID = i;
                capturedSq = to;
                break;
            }
        }

        if (capturedPieceID == 255) {
            if ((get_ep_square_from_flags(b[14]) == to) && (b[6] & (1ULL << from))) {
                int cap_pawn_sq = to + 8;
                if (cap_pawn_sq < 64 && (b[0] & (1ULL << cap_pawn_sq))) {
                    b[0] ^= (1ULL << cap_pawn_sq);
                    b[12] ^= (1ULL << cap_pawn_sq);
                    capturedPieceID = 0;
                    capturedSq = cap_pawn_sq;
                }
            }
        }

        for (int i = 6; i < 12; ++i) {
            if (b[i] & (1ULL << from)) {
                b[i] ^= (1ULL << from) | (1ULL << to);
                b[13] ^= (1ULL << from) | (1ULL << to);
                bool inCheck = is_attacked(b, __builtin_ctzll(b[11]), true);
                b[i] ^= (1ULL << from) | (1ULL << to);
                b[13] ^= (1ULL << from) | (1ULL << to);
                if (capturedPieceID != 255) {
                    b[capturedPieceID] |= (1ULL << capturedSq);
                    b[12] |= (1ULL << capturedSq);
                }
                return !inCheck;
            }
        }
    }
    return false;
}

static void init_board_start(Bitboard b[]) {
    b[0] = 0x000000000000FF00ULL;
    b[1] = 0x0000000000000042ULL;
    b[2] = 0x0000000000000024ULL;
    b[3] = 0x0000000000000081ULL;
    b[4] = 0x0000000000000008ULL;
    b[5] = 0x0000000000000010ULL;
    b[6] = 0x00FF000000000000ULL;
    b[7] = 0x4200000000000000ULL;
    b[8] = 0x2400000000000000ULL;
    b[9] = 0x8100000000000000ULL;
    b[10] = 0x0800000000000000ULL;
    b[11] = 0x1000000000000000ULL;
    b[12] = 0x000000000000FFFFULL;
    b[13] = 0xFFFF000000000000ULL;
    b[14] = 15ULL;
    movesPlayed[0] = 0;
    repPlies = 0;
    set_ep_square_in_flags(&b[14], -1);
}

void findLegalMoves(Bitboard b[], var moves[218][2], bool white) {
    int idx = 0;
    Bitboard piecesLeft;
    Bitboard own_bb;
    Bitboard opp_bb;
    Bitboard occupied_all = b[12] | b[13];

#define ADD_MOVE(FROM, TO)                                         \
    {                                                              \
        moves[idx][0] = FROM;                                      \
        moves[idx][1] = TO;                                        \
        if (isLegalMove(moves[idx][0], moves[idx][1], b, white)) { \
            idx++;                                                 \
        }                                                          \
    }
// One legality check, then all four pieces, queen first
#define ADD_PROMOS(FROM, TO)                             \
    {                                                    \
        const int pf = (FROM), pt = (TO);                \
        if (isLegalMove(pf, pt, b, white)) {             \
            for (int pr = 3; pr >= 0; --pr) {            \
                moves[idx][0] = (var)pf;                 \
                moves[idx++][1] = (var)(pt | (pr << 6)); \
            }                                            \
        }                                                \
    }

    Bitboard sq;
    Bitboard moveBitboard;
    Bitboard twoSquarePawnMoves;
    Bitboard promotions;

    int ep_sq = get_ep_square_from_flags(b[14]);

    if (white) {
        piecesLeft = b[12] & (~b[0]);
        own_bb = b[12];
        opp_bb = b[13];

        // pawn single pushes
        moveBitboard = (b[0] << 8) & (~occupied_all);
        twoSquarePawnMoves = ((moveBitboard & 0x0000000000FF0000ULL) << 8) & (~occupied_all);
        promotions = moveBitboard & 0xFF00000000000000ULL;
        moveBitboard &= 0x00FFFFFFFFFFFFFFULL;
        while (moveBitboard) {
            const int to = __builtin_ctzll(moveBitboard);
            const int from = to - 8;
            moveBitboard &= moveBitboard - 1;

            if (!isLegalMove(from, to, b, white))
                continue;

            moves[idx][0] = (var)from;
            moves[idx++][1] = (var)to;
        }
        while (promotions) {
            ADD_PROMOS(__builtin_ctzll(promotions) - 8, __builtin_ctzll(promotions));
            promotions &= promotions - 1;
        }
        while (twoSquarePawnMoves) {
            ADD_MOVE(__builtin_ctzll(twoSquarePawnMoves) - 16, __builtin_ctzll(twoSquarePawnMoves));
            twoSquarePawnMoves &= twoSquarePawnMoves - 1;
        }
        // pawn captures
        moveBitboard = ((b[0] & 0xFEFEFEFEFEFEFEFEULL) << 7) & opp_bb;
        promotions = moveBitboard & 0xFF00000000000000ULL;
        moveBitboard &= 0x00FFFFFFFFFFFFFFULL;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) - 7, __builtin_ctzll(moveBitboard));
            moveBitboard &= moveBitboard - 1;
        }
        while (promotions) {
            ADD_PROMOS(__builtin_ctzll(promotions) - 7, __builtin_ctzll(promotions));
            promotions &= promotions - 1;
        }
        moveBitboard = ((b[0] & 0x7F7F7F7F7F7F7F7FULL) << 9) & opp_bb;
        promotions = moveBitboard & 0xFF00000000000000ULL;
        moveBitboard &= 0x00FFFFFFFFFFFFFFULL;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) - 9, __builtin_ctzll(moveBitboard));
            moveBitboard &= moveBitboard - 1;
        }
        while (promotions) {
            ADD_PROMOS(__builtin_ctzll(promotions) - 9, __builtin_ctzll(promotions));
            promotions &= promotions - 1;
        }

        /* en passant captures for white: destination is ep_sq (must be empty), pawn source is ep_sq - 7 or ep_sq - 9,
           the pawn being captured should be at ep_sq - 8 (a black pawn) */
        if (ep_sq != -1) {
            /* source candidate ep_sq - 7 */
            int src = ep_sq - 7;
            if ((src >> 3) == (ep_sq >> 3) - 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[0] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq - 8;
                if (cap_pawn_sq >= 0 && (b[6] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
            /* source candidate ep_sq - 9 */
            src = ep_sq - 9;
            if ((src >> 3) == (ep_sq >> 3) - 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[0] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq - 8;
                if (cap_pawn_sq >= 0 && (b[6] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
        }

    } else {
        piecesLeft = b[13] & (~b[6]);
        own_bb = b[13];
        opp_bb = b[12];

        // pawn single pushes
        moveBitboard = (b[6] >> 8) & (~occupied_all);
        twoSquarePawnMoves = ((moveBitboard & 0x0000FF0000000000ULL) >> 8) & (~occupied_all);
        promotions = moveBitboard & 0xFFULL;
        moveBitboard &= ~0xFFULL;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) + 8, __builtin_ctzll(moveBitboard));
            moveBitboard &= moveBitboard - 1;
        }
        while (promotions) {
            ADD_PROMOS(__builtin_ctzll(promotions) + 8, __builtin_ctzll(promotions));
            promotions &= promotions - 1;
        }
        // pawn double pushes
        while (twoSquarePawnMoves) {
            ADD_MOVE(__builtin_ctzll(twoSquarePawnMoves) + 16, __builtin_ctzll(twoSquarePawnMoves));
            twoSquarePawnMoves &= twoSquarePawnMoves - 1;
        }
        // pawn captures
        moveBitboard = ((b[6] & 0x7F7F7F7F7F7F7F7FULL) >> 7) & opp_bb;
        promotions = moveBitboard & 0xFFULL;
        moveBitboard &= ~0xFFULL;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) + 7, __builtin_ctzll(moveBitboard));
            moveBitboard &= moveBitboard - 1;
        }
        while (promotions) {
            ADD_PROMOS(__builtin_ctzll(promotions) + 7, __builtin_ctzll(promotions));
            promotions &= promotions - 1;
        }
        moveBitboard = ((b[6] & 0xFEFEFEFEFEFEFEFEULL) >> 9) & opp_bb;
        promotions = moveBitboard & 0xFFULL;
        moveBitboard &= ~0xFFULL;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) + 9, __builtin_ctzll(moveBitboard));
            moveBitboard &= moveBitboard - 1;
        }
        while (promotions) {
            ADD_PROMOS(__builtin_ctzll(promotions) + 9, __builtin_ctzll(promotions));
            promotions &= promotions - 1;
        }

        /* en passant captures for black: destination is ep_sq (must be empty), pawn source is ep_sq + 7 or ep_sq + 9,
           the pawn being captured should be at ep_sq + 8 (a white pawn) */
        if (ep_sq != -1) {
            /* source candidate ep_sq + 7 */
            int src = ep_sq + 7;
            if ((src >> 3) == (ep_sq >> 3) + 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[6] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq + 8;
                if (cap_pawn_sq < 64 && (b[0] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
            /* source candidate ep_sq + 9 */
            src = ep_sq + 9;
            if ((src >> 3) == (ep_sq >> 3) + 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[6] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq + 8;
                if (cap_pawn_sq < 64 && (b[0] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
        }
    }

    while (piecesLeft) {
        sq = __builtin_ctzll(piecesLeft);
        piecesLeft &= piecesLeft - 1;
        int pieceType = (((b[2] | b[8]) >> sq) & 1) + 2 * (((b[3] | b[9]) >> sq) & 1) + 3 * (((b[4] | b[10]) >> sq) & 1) + 4 * (((b[5] | b[11]) >> sq) & 1);
        /*
        0: knight
        1: bishop
        2: rook
        3: queen
        4: king
        */
        if (pieceType == 1) {
            moveBitboard = BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] & ~own_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 2) {
            moveBitboard = ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)] & ~own_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 3) {
            moveBitboard = (BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] | ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)]) & ~own_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 0) {
            moveBitboard = knightBitboards[sq] & (~own_bb);
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else {
            moveBitboard = kingBitboards[sq] & (~own_bb);
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
            Bitboard cast = b[14];
            if (white) {
                if (sq == 4) {
                    if ((cast & CAST_WK) && (b[3] & (1ULL << 7)) && !(occupied_all & ((1ULL << 5) | (1ULL << 6)))) {
                        if (!is_attacked(b, 4, false) && !is_attacked(b, 5, false)) ADD_MOVE(4, 6);
                    }
                    if ((cast & CAST_WQ) && (b[3] & (1ULL << 0)) && !(occupied_all & ((1ULL << 3) | (1ULL << 2) | (1ULL << 1)))) {
                        if (!is_attacked(b, 4, false) && !is_attacked(b, 3, false)) ADD_MOVE(4, 2);
                    }
                }
            } else {
                if (sq == 60) {
                    if ((cast & CAST_BK) && (b[9] & (1ULL << 63)) && !(occupied_all & ((1ULL << 61) | (1ULL << 62)))) {
                        if (!is_attacked(b, 60, true) && !is_attacked(b, 61, true)) ADD_MOVE(60, 62);
                    }
                    if ((cast & CAST_BQ) && (b[9] & (1ULL << 56)) && !(occupied_all & ((1ULL << 59) | (1ULL << 58) | (1ULL << 57)))) {
                        if (!is_attacked(b, 60, true) && !is_attacked(b, 59, true)) ADD_MOVE(60, 58);
                    }
                }
            }
        }
    }
    for (int i = idx; i < 218; ++i) {
        moves[i][0] = 255;
        moves[i][1] = 255;
    }
#undef ADD_MOVE
#undef ADD_PROMOS
}

bool legalQuietMoveExists(Bitboard b[], bool white) {
    int idx = 0;
    Bitboard piecesLeft;
    Bitboard occupied_all = b[12] | b[13];

    Bitboard sq;
    Bitboard moveBitboard;
    Bitboard twoSquarePawnMoves;

    if (white) {
        piecesLeft = b[12] & (~b[0]);
        moveBitboard = (b[0] << 8) & (~occupied_all);
        twoSquarePawnMoves = ((moveBitboard & 0x0000000000FF0000ULL) << 8) & (~occupied_all);
        while (moveBitboard) {
            if (isLegalMove(__builtin_ctzll(moveBitboard) - 8, __builtin_ctzll(moveBitboard), b, white)) {
                return true;
            }
            moveBitboard &= moveBitboard - 1;
        }
        while (twoSquarePawnMoves) {
            if (isLegalMove(__builtin_ctzll(twoSquarePawnMoves) - 16, __builtin_ctzll(twoSquarePawnMoves), b, white)) {
                return true;
            }
            twoSquarePawnMoves &= twoSquarePawnMoves - 1;
        }
    } else {
        piecesLeft = b[13] & (~b[6]);
        moveBitboard = (b[6] >> 8) & (~occupied_all);
        twoSquarePawnMoves = ((moveBitboard & 0x0000FF0000000000ULL) >> 8) & (~occupied_all);
        while (moveBitboard) {
            if (isLegalMove(__builtin_ctzll(moveBitboard) + 8, __builtin_ctzll(moveBitboard), b, white)) {
                return true;
            }
            moveBitboard &= moveBitboard - 1;
        }
        while (twoSquarePawnMoves) {
            if (isLegalMove(__builtin_ctzll(twoSquarePawnMoves) + 16, __builtin_ctzll(twoSquarePawnMoves), b, white)) {
                return true;
            }
            twoSquarePawnMoves &= twoSquarePawnMoves - 1;
        }
    }

    while (piecesLeft) {
        sq = __builtin_ctzll(piecesLeft);
        piecesLeft &= piecesLeft - 1;
        int pieceType = (((b[2] | b[8]) >> sq) & 1) + 2 * (((b[3] | b[9]) >> sq) & 1) + 3 * (((b[4] | b[10]) >> sq) & 1) + 4 * (((b[5] | b[11]) >> sq) & 1);
        /*
        0: knight
        1: bishop
        2: rook
        3: queen
        4: king
        */
        if (pieceType == 1) {
            moveBitboard = BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] & ~occupied_all;
            while (moveBitboard) {
                if (isLegalMove(sq, __builtin_ctzll(moveBitboard), b, white)) {
                    return true;
                }
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 2) {
            moveBitboard = ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)] & ~occupied_all;
            while (moveBitboard) {
                if (isLegalMove(sq, __builtin_ctzll(moveBitboard), b, white)) {
                    return true;
                }
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 3) {
            moveBitboard = (BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] | ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)]) & ~occupied_all;
            while (moveBitboard) {
                if (isLegalMove(sq, __builtin_ctzll(moveBitboard), b, white)) {
                    return true;
                }
                moveBitboard &= moveBitboard - 1;
            }
        } else {
            moveBitboard = ((pieceType == 0) ? knightBitboards[sq] : kingBitboards[sq]) & (~occupied_all);
            while (moveBitboard) {
                if (isLegalMove(sq, __builtin_ctzll(moveBitboard), b, white)) {
                    return true;
                }
                moveBitboard &= moveBitboard - 1;
            }
        }
    }
    return false;
}

void findLegalCaptures(Bitboard b[], var moves[218][2], bool white) {
    int idx = 0;
    Bitboard piecesLeft;
    Bitboard own_bb;
    Bitboard opp_bb;

    Bitboard occupied_all = b[12] | b[13];

#define ADD_MOVE(FROM, TO)                                              \
    {                                                                   \
        moves[idx][0] = FROM;                                           \
        moves[idx][1] = TO;                                             \
        if (isLegalMove(moves[idx][0], moves[idx][1] & 63, b, white)) { \
            idx++;                                                      \
        }                                                               \
    }

    Bitboard sq;
    Bitboard moveBitboard;

    int ep_sq = get_ep_square_from_flags(b[14]);

    if (white) {
        piecesLeft = b[12] & (~b[0]);
        own_bb = b[12];
        opp_bb = b[13];

        /* pawn captures (normal); promotions are searched as queens only */
        moveBitboard = ((b[0] & 0xFEFEFEFEFEFEFEFEULL) << 7) & opp_bb;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) - 7, __builtin_ctzll(moveBitboard) | (moveBitboard & 0xFF00000000000000ULL & -moveBitboard ? 192 : 0));
            moveBitboard &= moveBitboard - 1;
        }
        moveBitboard = ((b[0] & 0x7F7F7F7F7F7F7F7FULL) << 9) & opp_bb;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) - 9, __builtin_ctzll(moveBitboard) | (moveBitboard & 0xFF00000000000000ULL & -moveBitboard ? 192 : 0));
            moveBitboard &= moveBitboard - 1;
        }

        /* en passant captures for white: destination is ep_sq (must be empty), pawn source is ep_sq - 7 or ep_sq - 9,
           the pawn being captured should be at ep_sq - 8 (a black pawn) */
        if (ep_sq != -1) {
            /* source candidate ep_sq - 7 */
            int src = ep_sq - 7;
            if ((src >> 3) == (ep_sq >> 3) - 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[0] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq - 8;
                if (cap_pawn_sq >= 0 && (b[6] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
            /* source candidate ep_sq - 9 */
            src = ep_sq - 9;
            if ((src >> 3) == (ep_sq >> 3) - 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[0] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq - 8;
                if (cap_pawn_sq >= 0 && (b[6] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
        }

    } else {
        piecesLeft = b[13] & (~b[6]);
        own_bb = b[13];
        opp_bb = b[12];

        /* pawn captures (normal); promotions are searched as queens only */
        moveBitboard = ((b[6] & 0x7F7F7F7F7F7F7F7FULL) >> 7) & opp_bb;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) + 7, __builtin_ctzll(moveBitboard) | (moveBitboard & 0xFFULL & -moveBitboard ? 192 : 0));
            moveBitboard &= moveBitboard - 1;
        }
        moveBitboard = ((b[6] & 0xFEFEFEFEFEFEFEFEULL) >> 9) & opp_bb;
        while (moveBitboard) {
            ADD_MOVE(__builtin_ctzll(moveBitboard) + 9, __builtin_ctzll(moveBitboard) | (moveBitboard & 0xFFULL & -moveBitboard ? 192 : 0));
            moveBitboard &= moveBitboard - 1;
        }

        /* en passant captures for black: destination is ep_sq (must be empty), pawn source is ep_sq + 7 or ep_sq + 9,
           the pawn being captured should be at ep_sq + 8 (a white pawn) */
        if (ep_sq != -1) {
            /* source candidate ep_sq + 7 */
            int src = ep_sq + 7;
            if ((src >> 3) == (ep_sq >> 3) + 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[6] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq + 8;
                if (cap_pawn_sq < 64 && (b[0] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
            /* source candidate ep_sq + 9 */
            src = ep_sq + 9;
            if ((src >> 3) == (ep_sq >> 3) + 1 && (abs((int)(src & 7) - (int)(ep_sq & 7)) == 1) && (b[6] & (1ULL << src))) {
                int cap_pawn_sq = ep_sq + 8;
                if (cap_pawn_sq < 64 && (b[0] & (1ULL << cap_pawn_sq))) {
                    ADD_MOVE(src, ep_sq);
                }
            }
        }
    }

    while (piecesLeft) {
        sq = __builtin_ctzll(piecesLeft);
        piecesLeft &= piecesLeft - 1;
        int pieceType = (((b[2] | b[8]) >> sq) & 1) + 2 * (((b[3] | b[9]) >> sq) & 1) + 3 * (((b[4] | b[10]) >> sq) & 1) + 4 * (((b[5] | b[11]) >> sq) & 1);
        /*
        0: knight
        1: bishop
        2: rook
        3: queen
        4: king
        */
        if (pieceType == 1) {
            moveBitboard = BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] & opp_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 2) {
            moveBitboard = ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)] & opp_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else if (pieceType == 3) {
            moveBitboard = (BISHOP_ATTACK_TABLE_FLAT[(sq << 9) + ((((occupied_all & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] | ROOK_ATTACK_TABLE_FLAT[(sq << 12) + ((((occupied_all & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)]) & opp_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        } else {
            moveBitboard = ((pieceType == 0) ? knightBitboards[sq] : kingBitboards[sq]) & opp_bb;
            while (moveBitboard) {
                ADD_MOVE(sq, __builtin_ctzll(moveBitboard));
                moveBitboard &= moveBitboard - 1;
            }
        }
    }
    for (int i = idx; i < 218; ++i) {
        moves[i][0] = 255;
        moves[i][1] = 255;
    }
#undef ADD_MOVE
}

void playMoveInto(Bitboard *restrict dst, const Bitboard *restrict src, var move[]) {
    const int from = move[0];
    const int to = move[1] & 0b00111111;
    const Bitboard fromBB = 1ULL << from, toBB = 1ULL << to;
    const bool isWhite = ((src[13] >> from) & 1) == 0;
    const int me = 6 - 6 * isWhite, myOcc = 13 - isWhite, oppOcc = 12 + isWhite;
    const int pieceType = ((src[me + 1] >> from) & 1) + 2 * ((src[me + 2] >> from) & 1) + 3 * ((src[me + 3] >> from) & 1) + 4 * ((src[me + 4] >> from) & 1) + 5 * ((src[me + 5] >> from) & 1);
    const int i = me + pieceType;
    const bool captured = (src[oppOcc] & toBB) != 0;

    memcpy(dst, src, 15 * sizeof(Bitboard));
    dst[myOcc] = src[myOcc] ^ (fromBB | toBB);
    if (captured) {
        const int opp = 6 - me;
        const int v = opp + ((src[opp + 1] >> to) & 1) + 2 * ((src[opp + 2] >> to) & 1) + 3 * ((src[opp + 3] >> to) & 1) + 4 * ((src[opp + 4] >> to) & 1) + 5 * ((src[opp + 5] >> to) & 1);
        dst[v] = src[v] & ~toBB;
        dst[oppOcc] = src[oppOcc] & ~toBB;
    }

    Bitboard flags = src[14];
    int newEp = -1;
    if (pieceType == 0) {
        if (toBB & 0xFF000000000000FFULL) {
            int ptype = 7 + (move[1] >> 6);
            dst[i] = src[i] & ~fromBB;
            dst[ptype - 6 * isWhite] = src[ptype - 6 * isWhite] | toBB;
        } else {
            dst[i] = src[i] ^ (fromBB | toBB);
            int diff = to - from;
            if (diff == 16 || diff == -16)
                newEp = (from + to) >> 1;
            /* en-passant capture: destination empty (victimIndex == -1) but destination equals ep square */
            else if (!captured && to == get_ep_square_from_flags(flags)) {
                const Bitboard capBB = isWhite ? toBB >> 8 : toBB << 8;
                dst[6 - me] = src[6 - me] & ~capBB;
                dst[oppOcc] = src[oppOcc] & ~capBB;
            }
        }
    } else {
        dst[i] = src[i] ^ (fromBB | toBB);
        if (pieceType == 5) {
            flags &= isWhite ? ~0x3ULL : ~0xCULL;
            int diff = to - from;
            if ((diff == 2 || diff == -2) && from == (isWhite ? 4 : 60)) {
                const Bitboard rookMove = (diff == 2) ? (isWhite ? (1ULL << 7) | (1ULL << 5) : (1ULL << 63) | (1ULL << 61))
                                                      : (isWhite ? (1ULL << 0) | (1ULL << 3) : (1ULL << 56) | (1ULL << 59));
                dst[me + 3] = src[me + 3] ^ rookMove;
                dst[myOcc] ^= rookMove;
            }
        }
    }
    if ((fromBB | toBB) & 0x8100000000000081ULL)
        flags &= ~((CAST_WK * (((dst[3] >> 7) & 1) == 0)) | (CAST_WQ * ((dst[3] & 1) == 0)) | (CAST_BK * (((dst[9] >> 63) & 1) == 0)) | (CAST_BQ * (((dst[9] >> 56) & 1) == 0)));
    dst[14] = (flags & ~EP_MASK) | ((Bitboard)(newEp + 1) << EP_SHIFT);
}

void playMove(Bitboard board[], var move[]) {
    Bitboard src[15];
    memcpy(src, board, sizeof src);
    playMoveInto(board, src, move);
}

int evaluate(Bitboard b[]) {
    const Bitboard wp = b[0], bp = b[6];
    Bitboard pieceBoard = b[12] | b[13];
    // {204, 9, 19, 105, 189, 39, 19, -17};

    // Bishop pairs
    int score = 204 * ((__builtin_popcountll(b[2]) > 1) - (__builtin_popcountll(b[8]) > 1));

    // King pawn shield
    const int wk = __builtin_ctzll(b[5]), bk = __builtin_ctzll(b[11]);
    score += 9 * (__builtin_popcountll(kingBitboards[wk] & wp) - __builtin_popcountll(kingBitboards[bk] & bp));

    // Pawn strength
    score += 19 * (__builtin_popcountll(pieceBoard & (((wp & 0xFEFEFEFEFEFEFEFEULL) << 7) | ((wp & 0x7F7F7F7F7F7F7F7FULL) << 9))) - __builtin_popcountll(pieceBoard & (((bp & 0xFEFEFEFEFEFEFEULL) >> 9) | ((bp & 0x7F7F7F7F7F7F7F7FULL) >> 7))));

    // King placement
    int gameLeft = gamePhase[__builtin_popcountll(pieceBoard)];
    const int endLeft = 65536 - gameLeft;
    score += (gameLeft * (PST_WHITE[5][wk] - PST_BLACK[5][bk])) >> 16; // Early game
    score += (endLeft * (PST_INIT[6][wk] - PST_INIT[6][bk])) >> 16;    // Endgame

    // Rook placement
    Bitboard curBoard = b[3];
    while (curBoard) {
        int sq = __builtin_ctzll(curBoard);
        // Open files
        int cnt = __builtin_popcountll(pieceBoard & fileBitboards[sq & 7]);
        score += (cnt == 1) * 39 + (cnt == 2) * 19;
        // Connected rooks
        score += -17 * ((curBoard & (curBoard - 1) & ((0xFFULL << (sq & 0xF8)) | fileBitboards[sq & 7])) != 0);
        curBoard &= curBoard - 1;
    }
    curBoard = b[9];
    while (curBoard) {
        int sq = __builtin_ctzll(curBoard);
        // Open files
        int cnt = __builtin_popcountll(pieceBoard & fileBitboards[sq & 7]);
        score -= (cnt == 1) * 39 + (cnt == 2) * 19;
        // Connected rooks
        score -= -17 * ((curBoard & (curBoard - 1) & ((0xFFULL << (sq & 0xF8)) | fileBitboards[sq & 7])) != 0);
        curBoard &= curBoard - 1;
    }

    for (int side = 0; side < 2; ++side) {
        const Bitboard pawns = side ? bp : wp;
        Bitboard f = pawns;
        f |= f >> 32;
        f |= f >> 16;
        f |= f >> 8;
        const unsigned files = (unsigned)(f & 0xFF);
        const unsigned adjacent = ((files << 1) | (files >> 1)) & 0xFF;
        int penalty = 189 * __builtin_popcountll(pawns & ((Bitboard)(files & ~adjacent) * 0x0101010101010101ULL));

        Bitboard fill = pawns | (pawns << 8);
        fill |= fill << 16;
        fill |= fill << 32;
        const Bitboard behind = pawns & (fill << 8);
        Bitboard deep = behind | (behind << 8);
        deep |= deep << 16;
        deep |= deep << 32;
        if (__builtin_expect((behind & (deep << 8)) == 0, 1)) {
            penalty += 210 * __builtin_popcountll(behind);
        } else {
            Bitboard rest = pawns;
            while (rest) {
                penalty += 105 * (__builtin_popcountll(pawns & fileBitboards[__builtin_ctzll(rest) & 7]) - 1);
                rest &= rest - 1;
            }
        }
        score += side ? penalty : -penalty;
    }

    // Pawns
    const int passedWeight = 75 * endLeft;
    curBoard = wp;
    while (curBoard) {
        int sq = __builtin_ctzll(curBoard);
        score += PST_WHITE[0][sq] + (int)(((sq >> 3) * passedWeight >> 16) * ((bp & passedBitboards[sq]) == 0)); // Account for position, doubling, isolation, passed
        curBoard &= curBoard - 1;
    }
    curBoard = bp;
    while (curBoard) {
        int sq = __builtin_ctzll(curBoard);
        score -= PST_BLACK[0][sq] + (int)((((7 - (sq >> 3)) * passedWeight >> 16)) * ((wp & __builtin_bswap64(passedBitboards[sq ^ 56])) == 0)); // Account for position, doubling, isolation, passed
        curBoard &= curBoard - 1;
    }

    // Non-pawn/king pieces
    for (int p = 1; p < 5; ++p) {
        curBoard = b[p];
        while (curBoard) {
            score += PST_WHITE[p][__builtin_ctzll(curBoard)];
            curBoard &= curBoard - 1;
        }
    }
    for (int p = 7; p < 11; ++p) {
        curBoard = b[p];
        while (curBoard) {
            score -= PST_BLACK[p - 6][__builtin_ctzll(curBoard)];
            curBoard &= curBoard - 1;
        }
    }

    return score;
}

int evaluateTest(Bitboard b[], int constants[]) {
    Bitboard pieceBoard = b[12] | b[13];
    // {204, 9, 19, 105, 189, 39, 19, -17};

    // Bishop pairs
    int score = 204 * ((__builtin_popcountll(b[2]) > 1) - (__builtin_popcountll(b[8]) > 1));

    // King pawn shield
    score += 9 * (__builtin_popcountll(kingBitboards[__builtin_ctzll(b[5])] & b[0]) - __builtin_popcountll(kingBitboards[__builtin_ctzll(b[11])] & b[6]));

    // Pawn strength
    score += 19 * (__builtin_popcountll(pieceBoard & (((b[0] & 0xFEFEFEFEFEFEFEFEULL) << 7) | ((b[0] & 0x7F7F7F7F7F7F7F7FULL) << 9))) - __builtin_popcountll(pieceBoard & (((b[6] & 0xFEFEFEFEFEFEFEULL) >> 9) | ((b[6] & 0x7F7F7F7F7F7F7F7FULL) >> 7))));

    // King placement
    int gameLeft = gamePhase[__builtin_popcountll(pieceBoard)];
    score += (gameLeft * (PST_WHITE[5][__builtin_ctzll(b[5])] - PST_BLACK[5][__builtin_ctzll(b[11])])) >> 16;         // Early game
    score += ((65536 - gameLeft) * (PST_INIT[6][__builtin_ctzll(b[5])] - PST_INIT[6][__builtin_ctzll(b[11])])) >> 16; // Endgame

    // Rook placement
    Bitboard curBoard = b[3];
    while (curBoard) {
        // Open files
        int cnt = __builtin_popcountll(pieceBoard & fileBitboards[__builtin_ctzll(curBoard) & 7]);
        score += (cnt == 1) * 39 + (cnt == 2) * 19;
        // Connected rooks
        score += -17 * ((__builtin_popcountll(curBoard & (0xFFULL << (__builtin_ctzll(curBoard) & 0xF8ULL))) + __builtin_popcountll(curBoard & fileBitboards[__builtin_ctzll(curBoard) & 7])) > 2);
        curBoard &= curBoard - 1;
    }
    curBoard = b[9];
    while (curBoard) {
        // Open files
        int cnt = __builtin_popcountll(pieceBoard & fileBitboards[__builtin_ctzll(curBoard) & 7]);
        score -= (cnt == 1) * 39 + (cnt == 2) * 19;
        // Connected rooks
        score -= -17 * ((__builtin_popcountll(curBoard & (0xFFULL << (__builtin_ctzll(curBoard) & 0xF8ULL))) + __builtin_popcountll(curBoard & fileBitboards[__builtin_ctzll(curBoard) & 7])) > 2);
        curBoard &= curBoard - 1;
    }

    // Pawns
    curBoard = b[0];
    while (curBoard) {
        score += PST_WHITE[0][__builtin_ctzll(curBoard)] - 105 * (__builtin_popcountll(b[0] & fileBitboards[__builtin_ctzll(curBoard) & 0b111ULL]) - 1) - (__builtin_popcountll(b[0] & isolatedBitboards[__builtin_ctzll(curBoard) & 0b111]) == 0) * 189 + (int)(75 * (__builtin_ctzll(curBoard) >> 3) * (((65536 - gameLeft) * ((b[6] & passedBitboards[__builtin_ctzll(curBoard)]) == 0))) >> 16); // Account for position, doubling, isolation, passed
        score += constants[7 - (__builtin_ctzll(curBoard) >> 3)];
        score += constants[40 + (__builtin_ctzll(curBoard) & 0b111ULL)];
        curBoard &= curBoard - 1;
    }
    curBoard = b[6];
    while (curBoard) {
        score -= PST_BLACK[0][__builtin_ctzll(curBoard)] - 105 * (__builtin_popcountll(b[6] & fileBitboards[__builtin_ctzll(curBoard) & 0b111ULL]) - 1) - (__builtin_popcountll(b[6] & isolatedBitboards[__builtin_ctzll(curBoard) & 0b111]) == 0) * 189 + (int)(75 * (7 - (__builtin_ctzll(curBoard) >> 3)) * (((65536 - gameLeft) * ((b[0] & __builtin_bswap64(passedBitboards[__builtin_ctzll(curBoard) ^ 56])) == 0))) >> 16); // Account for position, doubling, isolation, passed
        score -= constants[__builtin_ctzll(curBoard) >> 3];
        score -= constants[40 + (__builtin_ctzll(curBoard) & 0b111ULL)];
        curBoard &= curBoard - 1;
    }

    // Non-pawn/king pieces
    for (int p = 1; p < 5; ++p) {
        curBoard = b[p];
        while (curBoard) {
            score += PST_WHITE[p][__builtin_ctzll(curBoard)];
            score += constants[(p * 8) + 7 - (__builtin_ctzll(curBoard) >> 3)];
            score += constants[40 + (p * 8) + (__builtin_ctzll(curBoard) & 0b111ULL)];
            curBoard &= curBoard - 1;
        }
    }
    for (int p = 7; p < 11; ++p) {
        curBoard = b[p];
        while (curBoard) {
            score -= PST_BLACK[p - 6][__builtin_ctzll(curBoard)];
            score -= constants[((p - 6) * 8) + (__builtin_ctzll(curBoard) >> 3)];
            score -= constants[40 + ((p - 6) * 8) + (__builtin_ctzll(curBoard) & 0b111ULL)];
            curBoard &= curBoard - 1;
        }
    }

    return score;
}

void eval_init(void) {
    for (int n = 0; n <= 32; ++n) {
        double x = (n >= 2) ? pow(n - 2, 0.70710678118) / 11.0785381058 : 0.0;
        gamePhase[n] = (int)(min(x, 1.0) * 65536);
    }
}

void orderMoves(var moves[218][2], Bitboard b[], bool white) {
    int count = 0;
    while (moves[count][0] != 255)
        ++count; // Count moves
    static const int vals[6] = {1000, 3050, 3330, 5630, 9500, 50000};
    int32_t scores[218];
    for (int i = 0; i < count; ++i) {
        var from = moves[i][0];
        var mv = moves[i][1]; // square plus promotion bits; history and the TT move keep them, board lookups drop them
        var to = mv & 63;
        if (from == g_ttFrom && mv == g_ttTo) {
            scores[i] = INT32_MAX;
            continue;
        }
        int attackerIdx = (((b[1] | b[7]) >> from) & 1) + 2 * (((b[2] | b[8]) >> from) & 1) + 3 * (((b[3] | b[9]) >> from) & 1) + 4 * (((b[4] | b[10]) >> from) & 1) + 5 * (((b[5] | b[11]) >> from) & 1);
        int victimVal = 0;
        if ((b[12 + white] >> to) & 1) {
            if (((b[5] | b[11]) >> to) & 1) {
                moves[0][0] = from;
                moves[0][1] = mv;
                return;
            }
            int victimIdx = (((b[1] | b[7]) >> to) & 1) + 2 * (((b[2] | b[8]) >> to) & 1) + 3 * (((b[3] | b[9]) >> to) & 1) + 4 * (((b[4] | b[10]) >> to) & 1);
            victimVal = vals[victimIdx];
            scores[i] = (1 << 30) + vals[victimIdx] * 100 - vals[attackerIdx];
        } else {
            double h = ((524288 * historyTable[white][0][from][mv]) / (1 + historyTable[white][1][from][mv])) + PST_CONDITIONAL[white][attackerIdx][to] - PST_CONDITIONAL[white][attackerIdx][from];
            scores[i] = h > (1 << 29) - 1 ? (1 << 29) - 1 : h < -(1 << 29) ? -(1 << 29)
                                                                           : h;
        }
        // Queen promotions rank with the best captures; underpromotions go after every quiet move, knight first
        if (attackerIdx == 0 && ((1ULL << to) & 0xFF000000000000FFULL))
            scores[i] = (mv >> 6) == 3 ? (1 << 30) + (vals[4] - vals[0] + victimVal) * 100 : -(1 << 30) - (mv >> 6);
    }

    for (int i = 1; i < count; ++i) {
        int keyScore = scores[i];
        var keyFrom = moves[i][0], keyTo = moves[i][1];
        int j = i - 1;
        while (j >= 0 && scores[j] < keyScore) {
            scores[j + 1] = scores[j];
            moves[j + 1][0] = moves[j][0];
            moves[j + 1][1] = moves[j][1];
            --j;
        }
        int insert = j + 1;
        scores[insert] = keyScore;
        moves[insert][0] = keyFrom;
        moves[insert][1] = keyTo;
    }
}

void orderMovesEval(var moves[218][2], unsigned long long b[], bool white) {
    int count = 0;
    while (moves[count][0] != 255)
        ++count;
    int32_t scores[218];
    for (int i = 0; i < count; ++i) {
        var from = moves[i][0];
        var to = moves[i][1];
        int attackerIdx = (((b[1] | b[7]) >> from) & 1) + 2 * (((b[2] | b[8]) >> from) & 1) + 3 * (((b[3] | b[9]) >> from) & 1) + 4 * (((b[4] | b[10]) >> from) & 1) + 5 * (((b[5] | b[11]) >> from) & 1);

        Bitboard tmp[15];
        playMoveInto(tmp, b, moves[i]);
        scores[i] = white ? evaluate(tmp) : -evaluate(tmp);

        // scores[i] += evalTuners[attackerIdx] * ((to>>3)-(from>>3));
    }

    for (int i = 1; i < count; ++i) {
        int32_t keyScore = scores[i];
        var keyFrom = moves[i][0], keyTo = moves[i][1];
        int j = i - 1;
        while (j >= 0 && scores[j] < keyScore) {
            scores[j + 1] = scores[j];
            moves[j + 1][0] = moves[j][0];
            moves[j + 1][1] = moves[j][1];
            --j;
        }
        int insert = j + 1;
        scores[insert] = keyScore;
        moves[insert][0] = keyFrom;
        moves[insert][1] = keyTo;
    }
}

static const int PIECE_VAL[6] = {1000, 3050, 3330, 5630, 9500, 50000};

static inline int victimType(Bitboard b[], var to) {
    return (((b[1] | b[7]) >> to) & 1) + 2 * (((b[2] | b[8]) >> to) & 1) + 3 * (((b[3] | b[9]) >> to) & 1) + 4 * (((b[4] | b[10]) >> to) & 1);
}

static inline int attackerType(Bitboard b[], var from) {
    return (((b[1] | b[7]) >> from) & 1) + 2 * (((b[2] | b[8]) >> from) & 1) + 3 * (((b[3] | b[9]) >> from) & 1) + 4 * (((b[4] | b[10]) >> from) & 1) + 5 * (((b[5] | b[11]) >> from) & 1);
}

static Bitboard attackersTo(Bitboard b[], int sq, Bitboard occ) {
    Bitboard s = 1ULL << sq;
    Bitboard att = (((s & 0xFEFEFEFEFEFEFEFEULL) >> 9) | ((s & 0x7F7F7F7F7F7F7F7FULL) >> 7)) & b[0];
    att |= (((s & 0xFEFEFEFEFEFEFEFEULL) << 7) | ((s & 0x7F7F7F7F7F7F7F7FULL) << 9)) & b[6];
    att |= knightBitboards[sq] & (b[1] | b[7]);
    att |= kingBitboards[sq] & (b[5] | b[11]);
    att |= BISHOP_ATTACK_TABLE_FLAT[((Bitboard)sq << 9) + ((((occ & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq]) >> 55) & 0x1FF)] & (b[2] | b[8] | b[4] | b[10]);
    att |= ROOK_ATTACK_TABLE_FLAT[((Bitboard)sq << 12) + ((((occ & ROOK_MASKS[sq]) * ROOK_MAGICS[sq]) >> 52) & 0xFFF)] & (b[3] | b[9] | b[4] | b[10]);
    return att & occ;
}

// Static exchange evaluation: material won by capturing on `to`, after both sides swap off
static int seeCapture(Bitboard b[], var from, var to, bool white) {
    int gain[32];
    int d = 0;
    Bitboard occ = b[12] | b[13];
    Bitboard fromBB = 1ULL << from;
    int attacker = attackerType(b, from);
    bool side = white;
    gain[0] = PIECE_VAL[victimType(b, to)];
    if (attacker == 0 && ((1ULL << to) & 0xFF000000000000FFULL)) { // promotes (to a queen, as quiescence generates it), and a queen is what can be recaptured
        gain[0] += PIECE_VAL[4] - PIECE_VAL[0];
        attacker = 4;
    }
    while (d < 30) {
        ++d;
        gain[d] = PIECE_VAL[attacker] - gain[d - 1];
        if (-gain[d - 1] < 0 && gain[d] < 0) break;
        occ ^= fromBB;
        side = !side;
        Bitboard att = attackersTo(b, to, occ);
        fromBB = 0;
        for (int pt = 0; pt < 6; ++pt) {
            Bitboard cand = att & b[pt + (side ? 0 : 6)];
            if (cand) {
                fromBB = cand & (~cand + 1);
                attacker = pt;
                break;
            }
        }
        if (!fromBB) break;
    }
    while (--d > 0)
        if (-gain[d] < gain[d - 1]) gain[d - 1] = -gain[d];
    return gain[0];
}

int quiescence(Bitboard b[], int alpha, int beta, bool maxP) {
    int stand = evaluate(b);
    if (maxP) {
        if (stand >= beta) return stand;
        if (alpha < stand) alpha = stand;
    } else {
        if (stand <= alpha) return stand;
        if (beta > stand) beta = stand;
    }
    var moves[218][2];
    findLegalCaptures(b, moves, maxP);
    if (moves[0][0] == 255) {
        if (!legalQuietMoveExists(b, maxP)) {
            if (is_attacked(b, __builtin_ctzll(b[11 - 6 * maxP]), !maxP)) {
                return maxP ? -100000000 : 100000000; // Checkmate
            } else {
                return 0; // Stalemate
            }
        } else {
            return stand;
        }
    }
    bool saw = false;
    for (int i = 0; i < 218 && moves[i][0] != 255; ++i) {
        // Richest victim first, so a cutoff costs one scan; then discard captures that lose the exchange
        int bestVal = PIECE_VAL[victimType(b, moves[i][1] & 63)];
        for (int j = i + 1; j < 218 && moves[j][0] != 255; ++j) {
            int val = PIECE_VAL[victimType(b, moves[j][1] & 63)];
            if (val > bestVal) {
                bestVal = val;
                var swapFrom = moves[i][0], swapTo = moves[i][1];
                moves[i][0] = moves[j][0];
                moves[i][1] = moves[j][1];
                moves[j][0] = swapFrom;
                moves[j][1] = swapTo;
            }
        }
        if (seeCapture(b, moves[i][0], moves[i][1] & 63, maxP) < 0) continue;
        saw = true;
        Bitboard tmp[15];
        playMoveInto(tmp, b, moves[i]);
        int sc = quiescence(tmp, alpha, beta, !maxP);
        if (maxP) {
            if (sc > alpha) alpha = sc;
            if (alpha >= beta) return alpha;
        } else {
            if (sc < beta) beta = sc;
            if (alpha >= beta) return beta;
        }
    }
    return saw ? (maxP ? alpha : beta) : stand;
}

// Zobrist and simple transposition table
static Bitboard zob_piece[12][64], zob_castle[4], zob_ep[64], zob_side;
static Bitboard sm64_state;
static inline Bitboard sm64(void) {
    Bitboard z = (sm64_state += 0xb10a6a740cf4e7f0ULL);
    z = (z ^ (z >> 30)) * 0x17eb6c5a4a17ce03ULL;
    z = (z ^ (z >> 27)) * 0x76bd3de5b18e2a36ULL;
    return z ^ (z >> 31);
}
static void init_zobrist(void) {
    sm64_state = 0xf6fa3c183a806c5cULL;
    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            zob_piece[p][s] = sm64();
    for (int i = 0; i < 4; ++i)
        zob_castle[i] = sm64();
    for (int s = 0; s < 64; ++s)
        zob_ep[s] = sm64();
    zob_side = sm64();
}
static Bitboard hash64(Bitboard b[], bool whiteToMove) {
    Bitboard k = 0x4b73b864f1e5da7dULL;
    for (int p = 0; p < 12; ++p) {
        Bitboard bb = b[p];
        while (bb) {
            k ^= zob_piece[p][__builtin_ctzll(bb)];
            bb &= bb - 1;
        }
    }
    return k ^ (((b[14] & CAST_WK) > 0) * zob_castle[0]) ^ (((b[14] & CAST_WQ) > 0) * zob_castle[1]) ^ (((b[14] & CAST_BK) > 0) * zob_castle[2]) ^ (((b[14] & CAST_BQ) > 0) * zob_castle[3]) ^ (zob_ep[(b[14] & EP_MASK) >> EP_SHIFT]) ^ ((whiteToMove == 0) * zob_side);
}

#define TT_BITS 25U
#define TT_SIZE (1U << TT_BITS)
#define TT_MASK (TT_SIZE - 1)
enum { TT_EXACT = 0,
       TT_ALPHA = 1,
       TT_BETA = 2 };
#pragma pack(push, 1)
typedef struct {
    uint64_t key;
    int32_t value;
    double depth;
    var flag;
    var from, to;
    var pad[3];
} TTEntry;
#pragma pack(pop)
static TTEntry tt[TT_SIZE];
static inline bool tt_probe(uint64_t key, double depth, int alpha, int beta, int *outVal) {
    TTEntry *e = &tt[key & TT_MASK];
    *outVal = e->value;
    return ((e->key == key && e->depth >= depth) && ((e->flag == TT_ALPHA && e->value <= alpha) || (e->flag == TT_BETA && e->value >= beta) || (e->flag == TT_EXACT)));
}

static inline void ttLookupMove(uint64_t key) {
    TTEntry *e = &tt[key & TT_MASK];
    if (e->key == key) {
        g_ttFrom = e->from;
        g_ttTo = e->to;
    } else {
        g_ttFrom = 255;
        g_ttTo = 255;
    }
}

static inline void tt_store(uint64_t key, double depth, int value, var flag, var from, var to) {
    TTEntry *e = &tt[key & TT_MASK];
    if (e->key == key) {
        if (depth < e->depth) return;
    }
    e->key = key;
    e->value = value;
    e->depth = depth;
    e->flag = flag;
    e->from = from;
    e->to = to;
}

static bool g_afterNull = false;

static clock_t g_hardDeadline = 0; // pickBestMove's stop time; 0 = no limit
static bool g_stopSearch = false;  // once set, every node returns at once without storing anything
static int g_stopCountdown = 512;

int minimax(Bitboard curBoard[], double depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth <= 0) return quiescence(curBoard, alpha, beta, maximizingPlayer);
    if (__builtin_expect(--g_stopCountdown <= 0, 0)) {
        g_stopCountdown = 512;
        if (g_hardDeadline && clock() >= g_hardDeadline) g_stopSearch = true;
    }
    if (__builtin_expect(g_stopSearch, 0)) return 0;
    bool afterNull = g_afterNull;
    g_afterNull = false;

    uint64_t key = hash64(curBoard, maximizingPlayer);
    int reps = 0;
    for (int i = 0; i < repPlies; ++i)
        reps += (repStack[i] == key);
    if (reps >= 2) return 0; // Threefold repetition (this node would be the third occurrence)
    int tt_value;
    int orig_alpha = alpha, orig_beta = beta;
    if (tt_probe(key, depth + movesPlayed[0], alpha, beta, &tt_value)) return tt_value;

    if (depth <= 4.37 && !is_attacked(curBoard, __builtin_ctzll(curBoard[11 - 6 * maximizingPlayer]), !maximizingPlayer)) {
        int margin = 91.9 * depth, staticEval = evaluate(curBoard);
        if (maximizingPlayer && staticEval - margin >= beta) return staticEval;
        if (!maximizingPlayer && staticEval + margin <= alpha) return staticEval;
    }

    // Null move: if passing still clears the bound on a search shallower by 3 + depth/4 + up to 3 more (a ply per 2 pawns of eval margin), prune. Never twice in a row, in check, or with only pawns to move
    if (!afterNull && depth >= 3 && !is_attacked(curBoard, __builtin_ctzll(curBoard[11 - 6 * maximizingPlayer]), !maximizingPlayer) && ((curBoard[6 - 6 * maximizingPlayer] | curBoard[11 - 6 * maximizingPlayer]) ^ curBoard[13 - maximizingPlayer])) {
        int staticEval = evaluate(curBoard);
        double edge = maximizingPlayer ? (double)staticEval - beta : (double)alpha - staticEval;
        if (edge >= 0) {
            double R = 3 + depth / 4 + (edge / 2000 < 3 ? edge / 2000 : 3);
            Bitboard tmp[15];
            memcpy(tmp, curBoard, sizeof tmp);
            set_ep_square_in_flags(&tmp[14], -1);
            g_afterNull = true;
            repStack[repPlies++] = key;
            int val = maximizingPlayer ? minimax(tmp, depth - 1 - R, beta - 1, beta, false) : minimax(tmp, depth - 1 - R, alpha, alpha + 1, true);
            --repPlies;
            g_afterNull = false;
            if (__builtin_expect(g_stopSearch, 0)) return 0;
            if (maximizingPlayer && val >= beta) return beta;
            if (!maximizingPlayer && val <= alpha) return alpha;
        }
    }

    var moves_list[218][2];
    findLegalMoves(curBoard, moves_list, maximizingPlayer);
    if (moves_list[0][0] == 255) {
        if (is_attacked(curBoard, __builtin_ctzll(curBoard[11 - 6 * maximizingPlayer]), !maximizingPlayer)) {
            return maximizingPlayer ? -100000000 - (int)(depth) : 100000000 + (int)(depth); // Checkmate
        } else {
            return 0; // Stalemate
        }
    }
    ttLookupMove(key);
    if (EVAL_MODE == 0) {
        orderMoves(moves_list, curBoard, maximizingPlayer);
    } else {
        orderMovesEval(moves_list, curBoard, maximizingPlayer);
    }

    var bestMoveAt = 0;

    if (maximizingPlayer) {
        int best = -100000000;
        var bf = 255, bt = 255;
        for (int i = 0; i < 218 && moves_list[i][0] != 255; ++i) {
            Bitboard tmp[15];
            playMoveInto(tmp, curBoard, moves_list[i]);
            repStack[repPlies++] = key;
            int val = minimax(tmp, depth - LMRTable[i], alpha, beta, false);
            --repPlies;
            if (__builtin_expect(g_stopSearch, 0)) return 0;
            if (val > best) {
                best = val;
                bf = moves_list[i][0];
                bt = moves_list[i][1];
                historyTable[true][0][bf][bt] += tuners[4] + tuners[5] * ((double)i) + tuners[6] * depth + tuners[7] * (double)i * pow(depth, tuners[8]);
                bestMoveAt = i;
            }
            if (val > alpha) alpha = val;
            historyTable[true][1][moves_list[i][0]][moves_list[i][1]] += 1;
            if (alpha >= beta) {
                historyTable[true][0][moves_list[i][0]][moves_list[i][1]] += tuners[0] + ((double)i) * pow(depth, tuners[1]) + tuners[2] * ((double)i) + tuners[3] * depth;
                tt_store(key, depth + TOL + movesPlayed[0], best, TT_BETA, moves_list[i][0], moves_list[i][1]);

                trackers[0] += bestMoveAt;
                trackers[1] += 1;

                return best;
            }
        }
        var store_flag = TT_EXACT;
        if (best <= orig_alpha)
            store_flag = TT_ALPHA;
        else if (best >= orig_beta)
            store_flag = TT_BETA;
        tt_store(key, depth + TOL + movesPlayed[0], best, store_flag, bf, bt);

        trackers[0] += bestMoveAt;
        trackers[1] += 1;

        return best;
    } else {
        int best = 100000000;
        var bf = 255, bt = 255;
        for (int i = 0; i < 218 && moves_list[i][0] != 255; ++i) {
            Bitboard tmp[15];
            playMoveInto(tmp, curBoard, moves_list[i]);
            repStack[repPlies++] = key;
            int val = minimax(tmp, depth - LMRTable[i], alpha, beta, true);
            --repPlies;
            if (__builtin_expect(g_stopSearch, 0)) return 0;
            if (val < best) {
                best = val;
                bf = moves_list[i][0];
                bt = moves_list[i][1];
                historyTable[false][0][bf][bt] += tuners[4] + tuners[5] * ((double)i) + tuners[6] * depth + tuners[7] * (double)i * pow(depth, tuners[8]);
                bestMoveAt = i;
            }
            if (val < beta) beta = val;
            historyTable[false][1][moves_list[i][0]][moves_list[i][1]] += 1;
            if (alpha >= beta) {
                historyTable[false][0][moves_list[i][0]][moves_list[i][1]] += tuners[0] + ((double)i) * pow(depth, tuners[1]) + tuners[2] * ((double)i) + tuners[3] * depth;
                tt_store(key, depth + TOL + movesPlayed[0], best, TT_ALPHA, moves_list[i][0], moves_list[i][1]);

                trackers[0] += bestMoveAt;
                trackers[1] += 1;

                return best;
            }
        }
        var store_flag = TT_EXACT;
        if (best <= orig_alpha)
            store_flag = TT_ALPHA;
        else if (best >= orig_beta)
            store_flag = TT_BETA;
        tt_store(key, depth + TOL + movesPlayed[0], best, store_flag, bf, bt);

        trackers[0] += bestMoveAt;
        trackers[1] += 1;

        return best;
    }
}

void reorderMoves(var moves[218][2], unsigned long long b[]) {
    int count = 0;
    while (count < 218 && moves[count][0] != 255)
        ++count;
    if (!count) return;

    int32_t scores[218];

    for (int i = 0; i < count; ++i) {
        var from = moves[i][0], to = moves[i][1];
        scores[i] = moveTable[from][to];
    }

    for (int i = 1; i < count; ++i) {
        int32_t keyScore = scores[i];
        var keyFrom = moves[i][0], keyTo = moves[i][1];
        int j = i - 1;
        while (j >= 0 && scores[j] < keyScore) {
            scores[j + 1] = scores[j];
            moves[j + 1][0] = moves[j][0];
            moves[j + 1][1] = moves[j][1];
            --j;
        }
        int insert = j + 1;
        scores[insert] = keyScore;
        moves[insert][0] = keyFrom;
        moves[insert][1] = keyTo;
    }
}

int pickBestMove(Bitboard curBoard[], bool whiteToMove, var outMove[2], int *outScore, int moveTime) {
    if (UCI_MODE != 1) {
        memset(tt, 0, sizeof(tt));
    }
    memset(moveTable, 0, sizeof(moveTable));
    trackers[0] = 0;
    trackers[1] = 0;
    var moves_list[218][2];
    findLegalMoves(curBoard, moves_list, whiteToMove);
    if (moves_list[0][0] == 255) return 0;
    if (moves_list[1][0] == 255) {
        outMove[0] = moves_list[0][0];
        outMove[1] = moves_list[0][1];
        return 1;
    }
    ttLookupMove(hash64(curBoard, whiteToMove));
    if (EVAL_MODE == 0) {
        orderMoves(moves_list, curBoard, whiteToMove);
    } else {
        orderMovesEval(moves_list, curBoard, whiteToMove);
    }
    memset(historyTable, 0, sizeof(historyTable));
    int bestScore = whiteToMove ? INT32_MIN : INT32_MAX;
    var bestMove[2] = {255, 255};
    var finalMove[2] = {255, 255};
    int finalScore = 0;

    clock_t t0 = clock();
    // The budget is only checked between root moves, and one root move can take far longer than the whole budget
    // (Kxf8 at depth 32 once took 90 s on a 1.8 s budget), so the search also stops itself at 3x the budget.
    g_stopSearch = false;
    g_stopCountdown = 512;
    g_hardDeadline = ((UCI_MODE == 1 || UCI_MODE == 2) && moveTime < 100000000) ? t0 + (clock_t)(3.0 * moveTime * CLOCKS_PER_SEC / 1000.0) + 1 : 0;
    var lastMove[2] = {moves_list[0][0], moves_list[0][1]};
    int lastScore = 0;

    double depth = 3;

    while (true) {
        bestScore = whiteToMove ? INT32_MIN : INT32_MAX;
        bestMove[0] = 255;
        bestMove[1] = 255;

        for (int i = 0; i < 218 && moves_list[i][0] != 255; ++i) {
            Bitboard tmp[15];
            var mv[2] = {moves_list[i][0], moves_list[i][1]};
            playMoveInto(tmp, curBoard, mv);
            int val = minimax(tmp, depth - LMRTable[i], whiteToMove ? bestScore : -100000000, whiteToMove ? 100000000 : bestScore, !whiteToMove);
            if (__builtin_expect(g_stopSearch, 0)) break; // this root move's value is unfinished; drop it
            if (whiteToMove) {
                moveTable[mv[0]][mv[1]] = val;
                historyTable[true][1][moves_list[i][0]][moves_list[i][1]] += 1;
                if (val > bestScore) {
                    bestScore = val;
                    bestMove[0] = mv[0];
                    bestMove[1] = mv[1];
                    historyTable[true][0][moves_list[i][0]][moves_list[i][1]] += tuners[4] + tuners[5] * ((double)i) + tuners[6] * depth + tuners[7] * (double)i * pow(depth, tuners[8]);
                }
            } else {
                moveTable[mv[0]][mv[1]] = -val;
                historyTable[false][1][moves_list[i][0]][moves_list[i][1]] += 1;
                if (val < bestScore) {
                    bestScore = val;
                    bestMove[0] = mv[0];
                    bestMove[1] = mv[1];
                    historyTable[false][0][moves_list[i][0]][moves_list[i][1]] += tuners[4] + tuners[5] * ((double)i) + tuners[6] * depth + tuners[7] * (double)i * pow(depth, tuners[8]);
                }
            }

            if (UCI_MODE == 1) {
                clock_t t1 = clock();
                int elapsed_ms = (int)((double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC);
                if (elapsed_ms > moveTime) {
                    break;
                }
            }
        }

        if (bestMove[0] != 255) { // a finished iteration, or a cut-off one that at least re-searched the previous best move
            lastMove[0] = bestMove[0];
            lastMove[1] = bestMove[1];
            lastScore = bestScore;
        }

        if (g_stopSearch) break;
        if (depth >= 100) break;

        if (UCI_MODE == 1 || UCI_MODE == 2) {
            clock_t t1 = clock();
            int elapsed_ms = (int)((double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC);
            if (elapsed_ms > moveTime) break;
        }

        if (UCI_MODE == 0) {
            if (depth > 10) {
                if (((double)trackers[0] / (double)trackers[1]) > 0.6) {
                    return 0;
                }
            }
            if (depth == 13) {
                return 0;
            }
        }

        /*if ((bestScore < -99999999) && whiteToMove) {
            outMove[0] = 0;
            outMove[1] = 0;
            return (int)depth;
        }
        if ((bestScore > 99999999) && (!whiteToMove)) {
            outMove[0] = 0;
            outMove[1] = 0;
            return (int)depth;
        }*/

        // good for bot
        if (bestScore > 99999999 || bestScore < -99999999) {
            outMove[0] = bestMove[0];
            outMove[1] = bestMove[1];
            if (outScore) *outScore = bestScore;
            return (int)depth;
        }

        reorderMoves(moves_list, curBoard);

        if (depth >= 100) break;
        ++depth;
    }
    g_hardDeadline = 0;
    outMove[0] = lastMove[0];
    outMove[1] = lastMove[1];
    if (outScore) *outScore = lastScore;
    return (int)(depth);
}

void pickRandomMove(var moves[218][2], unsigned long long b[], var move[2]) {
    // count number of moves
    int count = 0;
    while (count < 218 && moves[count][0] != 255)
        ++count;
    if (!count) return;

    // sort best moves
    int scores[218];

    for (int i = 0; i < count; ++i) {
        var from = moves[i][0], to = moves[i][1];
        scores[i] = moveTable[from][to];
    }

    for (int i = 1; i < count; ++i) {
        int keyScore = scores[i];
        var keyFrom = moves[i][0], keyTo = moves[i][1];
        int j = i - 1;
        while (j >= 0 && scores[j] < keyScore) {
            scores[j + 1] = scores[j];
            moves[j + 1][0] = moves[j][0];
            moves[j + 1][1] = moves[j][1];
            --j;
        }
        int insert = j + 1;
        scores[insert] = keyScore;
        moves[insert][0] = keyFrom;
        moves[insert][1] = keyTo;
    }

    // pick random one
    int viableMoves = 0;
    for (int i = 0; i < count; i++) {
        if (scores[i] + 185 > scores[0]) {
            viableMoves += 1;
        }
    }

    int moveID = (double)viableMoves * ((double)rand() / (double)RAND_MAX);
    move[0] = moves[moveID][0];
    move[1] = moves[moveID][1];
}

#if UCI_MODE == 3
typedef struct {
    int score;
    Bitboard board[15];
} TunePos;

static int64_t range_err(TunePos *ps, size_t a, size_t b, const int *cns, int nc) {
    int saved[nc];
    memcpy(saved, constants, nc * sizeof *saved);
    memcpy(constants, cns, nc * sizeof *cns);
    int64_t s = 0;
    for (size_t i = a; i < b; ++i)
        s += llabs((int64_t)ps[i].score - (int64_t)evaluateTest(ps[i].board, constants));
    memcpy(constants, saved, nc * sizeof *saved);
    return s;
}

static int tune_coords(int *c, int nc, TunePos *ps, size_t lo, size_t hi) {
    int passes = 0;
    int64_t cur_err = range_err(ps, lo, hi, c, nc);
    bool improved;
    do {
        improved = false;
        ++passes;
        for (int i = 0; i < nc; ++i) {
            int64_t e0 = cur_err;
            c[i] += 1;
            int64_t ep = range_err(ps, lo, hi, c, nc);
            c[i] -= 2;
            int64_t em = range_err(ps, lo, hi, c, nc);
            c[i] += 1;

            int dir = 0;
            int64_t best = e0;
            if (ep < best) {
                dir = +1;
                best = ep;
            }
            if (em < best) {
                dir = -1;
                best = em;
            }
            if (dir) {
                c[i] += dir;
                improved = true;
                cur_err = best;
            }
        }
    } while (improved);
    return passes;
}
#endif

int parseSquare(const char *s) {
    if (!s || !isalpha((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) return -1;
    char file = tolower((unsigned char)s[0]), rank = s[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return -1;
    return (rank - '1') * 8 + (file - 'a');
}

void algebraic_from_move(Bitboard b[], var mv[2], char out[6]) {
    int f = mv[0], t = mv[1] & 63;
    out[0] = 'a' + (f % 8);
    out[1] = '1' + (f / 8);
    out[2] = 'a' + (t % 8);
    out[3] = '1' + (t / 8);
    out[4] = '\0';
    int pidx = (((b[0] | b[6]) >> f) & 1) + 2 * (((b[1] | b[7]) >> f) & 1) + 3 * (((b[2] | b[8]) >> f) & 1) + 4 * (((b[3] | b[9]) >> f) & 1) + 5 * (((b[4] | b[10]) >> f) & 1) + 6 * (((b[5] | b[11]) >> f) & 1) + 6 * ((b[13] >> f) & 1) - 1;
    if (pidx >= 0 && (pidx % 6) == 0) {
        int toRank = t >> 3;
        if ((pidx < 6 && toRank == 7) || (pidx >= 6 && toRank == 0)) {
            out[4] = "nbrq"[mv[1] >> 6];
            out[5] = '\0';
        }
    }
}

void apply_uci_move_to_board(Bitboard b[], const char *mvstr) {
    if (!mvstr || strlen(mvstr) < 4) return;
    char s1[3] = {mvstr[0], mvstr[1], 0}, s2[3] = {mvstr[2], mvstr[3], 0};
    int from = parseSquare(s1);
    int to = parseSquare(s2);
    if (from < 0 || to < 0) return;
    // Promotion piece goes in bits 6-7 of the destination (n, b, r, q = 0-3); queen if the suffix is missing or unknown
    // playMoveInto only reads those bits when a pawn reaches the last rank, so setting them on other moves is harmless
    int promo = 3;
    size_t len = strlen(mvstr);
    if (len >= 5) {
        char c = (char)tolower((unsigned char)(mvstr[4] == '=' && len >= 6 ? mvstr[5] : mvstr[4]));
        const char *p = c ? strchr("nbrq", c) : NULL;
        if (p) promo = (int)(p - "nbrq");
    }
    var m[2] = {(var)from, (var)(to | (promo << 6))};
    playMove(b, m);
}

// Loads the board part of a FEN (placement, side, castling, en passant); returns true when white is to move
static bool load_fen(Bitboard b[], const char *fen) {
    memset(b, 0, 15 * sizeof(Bitboard));
    movesPlayed[0] = 0;
    repPlies = 0;
    while (*fen == ' ')
        ++fen;
    for (int rank = 7, file = 0; *fen && *fen != ' '; ++fen) {
        if (*fen == '/') {
            --rank;
            file = 0;
        } else if (isdigit((unsigned char)*fen)) {
            file += *fen - '0';
        } else {
            const char *p = strchr("PNBRQKpnbrqk", *fen);
            if (p && rank >= 0 && file < 8) b[p - "PNBRQKpnbrqk"] |= 1ULL << (rank * 8 + file);
            ++file;
        }
    }
    for (int i = 0; i < 6; ++i) {
        b[12] |= b[i];
        b[13] |= b[i + 6];
    }
    while (*fen == ' ')
        ++fen;
    bool white = *fen != 'b';
    while (*fen && *fen != ' ')
        ++fen;
    while (*fen == ' ')
        ++fen;
    for (; *fen && *fen != ' '; ++fen)
        b[14] |= *fen == 'K' ? CAST_WK : *fen == 'Q' ? CAST_WQ
                                     : *fen == 'k'   ? CAST_BK
                                     : *fen == 'q'   ? CAST_BQ
                                                     : 0;
    while (*fen == ' ')
        ++fen;
    set_ep_square_in_flags(&b[14], *fen != '-' ? parseSquare(fen) : -1);
    return white;
}

// Leaf nodes at `depth`, counted in bulk at the last ply
static uint64_t perft(Bitboard b[], int depth, bool white) {
    var moves[218][2];
    findLegalMoves(b, moves, white);
    int count = 0;
    while (count < 218 && moves[count][0] != 255)
        ++count;
    if (depth <= 1) return depth == 1 ? (uint64_t)count : 1;
    uint64_t nodes = 0;
    for (int i = 0; i < count; ++i) {
        Bitboard tmp[15];
        playMoveInto(tmp, b, moves[i]);
        nodes += perft(tmp, depth - 1, !white);
    }
    return nodes;
}

static void perft_divide(Bitboard b[], int depth, bool white) {
    var moves[218][2];
    findLegalMoves(b, moves, white);
    uint64_t total = 0;
    clock_t t0 = clock();
    for (int i = 0; i < 218 && moves[i][0] != 255; ++i) {
        Bitboard tmp[15];
        playMoveInto(tmp, b, moves[i]);
        uint64_t n = depth > 1 ? perft(tmp, depth - 1, !white) : 1;
        char alg[6];
        algebraic_from_move(b, moves[i], alg);
        printf("%s: %llu\n", alg, (unsigned long long)n);
        total += n;
    }
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("\nNodes searched: %llu\ntime %.3f s, %.0f nodes/s\n", (unsigned long long)total, secs, secs > 0 ? total / secs : 0.0);
}

/* --------------------------- UCI loop --------------------------- */
void uci_loop(void) {
    char line[8192];
    Bitboard board[15];
    init_board_start(board);
    bool color = true;
    for (;;) {
        if (!fgets(line, sizeof line, stdin)) break;
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) {
            line[--L] = 0;
        }
        if ((strncmp(line, "uci", 3) == 0) && (!(strncmp(line, "ucinewgame", 10) == 0))) {
            printf("id name wonderful-chess\nid author You\nuciok\n");
        } else if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        } else if (strncmp(line, "ucinewgame", 10) == 0) {
            init_board_start(board);
        } else if (strncmp(line, "position", 8) == 0) {
            char *fen = strstr(line, "fen");
            if (strstr(line, "startpos") || fen) {
                char *mv = strstr(line, "moves");
                if (fen) {
                    if (mv) mv[-1] = 0; // End the FEN before the move list
                    color = load_fen(board, fen + 3);
                    if (mv) mv[-1] = ' ';
                } else {
                    init_board_start(board);
                    color = true;
                }
                movesPlayed[0] = 0;
                repStack[repPlies++] = hash64(board, color);
                if (mv) {
                    mv += 5;
                    while (*mv && isspace((unsigned char)*mv))
                        ++mv;
                    char token[8];
                    int consumed;
                    while (sscanf(mv, "%7s%n", token, &consumed) == 1) {
                        apply_uci_move_to_board(board, token);
                        movesPlayed[0] += 1;
                        color = !color;
                        repStack[repPlies++] = hash64(board, color);
                        mv += consumed;
                    }
                }
            }
        } else if (strncmp(line, "go perft", 8) == 0) {
            perft_divide(board, atoi(line + 8), color);
        } else if (strncmp(line, "go", 2) == 0) {
            double depth = 15;
            int wtime = 125000, btime = 125000, winc = 0, binc = 0;
            char *p = strstr(line, "depth");
            if (p) sscanf(p, "depth %d", (int *)(&depth));
            p = strstr(line, "wtime");
            if (p) sscanf(p, "wtime %d", &wtime);
            p = strstr(line, "btime");
            if (p) sscanf(p, "btime %d", &btime);
            p = strstr(line, "winc");
            if (p) sscanf(p, "winc %d", &winc);
            p = strstr(line, "binc");
            if (p) sscanf(p, "binc %d", &binc);
            int remaining = color ? wtime : btime;
            int increment = color ? winc : binc;
            int moveTime = min((int)(remaining * 0.024 + increment * 0.67), 10000);
            if (moveTime < 1) moveTime = 1;
            clock_t t0 = clock();
            var best[2];
            int score = 0;
            depth = pickBestMove(board, color, best, &score, moveTime);
            // Movegen debug
            // var moves[218][2];
            // findLegalMoves(board,moves,true);
            // for (var i = 0; moves[i][0] != 255; ++i){
            //     printf("%d %d\n",moves[i][0],moves[i][1]);
            // }
            int cp = score / 10;
            if (!color) cp = -cp;
            clock_t t1 = clock();
            int elapsed_ms = (int)((double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC);
            printf("info depth %d score cp %d time %d\n", (int)depth, cp, elapsed_ms);
            char alg[6];
            algebraic_from_move(board, best, alg);
            printf("bestmove %s\n", alg);
            // Metric tracking
            // printf("Tracker 0: %llu\n", trackers[0]);
            // printf("Tracker 1: %llu\n", trackers[1]);
            // printf("ordering score: %lf\n", ((double)trackers[0]) / ((double)trackers[1]));
        }
        fflush(stdout);
    }
}

int main(void) {
    for (int i = 0; i < 218; i++)
        LMRTable[i] = 1 + log(i + 1) / log(LOG_CONSTANT);

    int fundamentalValues[6] = {1000, 3050, 3240, 5630, 9500, 0};
    for (int p = 0; p < 6; ++p)
        for (int s = 0; s < 64; ++s) {
            PST_WHITE[p][s] = PST_INIT[p][63 - s] + fundamentalValues[p];
            PST_BLACK[p][s] = PST_INIT[p][s] + fundamentalValues[p];
            PST_CONDITIONAL[0][p][s] = PST_INIT[p][s];
            PST_CONDITIONAL[1][p][s] = PST_INIT[p][63 - s];
        }

    eval_init();
    srand((unsigned int)time(NULL));
    memset(historyTable, 0, sizeof(historyTable));
    init_zobrist();
    memset(tt, 0, sizeof(tt));
#if UCI_MODE == 0
    init_board_start(board_global);
    bool side = true;
    var chosen[2] = {255, 255};
    int eval = 0;
    pickBestMove(board_global, true, chosen, &eval, INT_MAX);

    int constantsToTune = 9;

    double ogTuners[9] = {0.18703825, 0.5890885, 0.5481135, 0.6710855, -1.104099, 2.3307195, 0.235299, -0.277866295, 1.0950241475};
    double devs[9] = {0.073855, 0.591282, 0.01445, 0.09797, 0.1179, 0.095066, 0.192608, 0.05124518, 0.69065259};
    while (true) {
        for (int i = 0; i < constantsToTune; ++i) {
            tuners[i] = ogTuners[i] + (4 * devs[i] + 0.8) * (2 * ((double)rand() / RAND_MAX) - 1);
        }

        trackers[0] = 0;
        trackers[1] = 0;
        for (int i = 0; i < 100; ++i) {
            movesPlayed[0] += 50;
            pickBestMove(board_global, side, chosen, &eval, INT_MAX);
        }
        double score = (double)trackers[0] / (double)trackers[1];

        if (score < 0.6) {
            BOOL written = FALSE;
            while (!written) {
                HANDLE hFile = CreateFile("param_log.txt", FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    char line[1024];
                    int len = snprintf(line, sizeof(line), "Score: %lf", score);
                    for (int k = 0; k < constantsToTune; ++k)
                        len += snprintf(line + len, sizeof(line) - len, ", #%d: %lf", k, tuners[k]);
                    len += snprintf(line + len, sizeof(line) - len, "\r\n");
                    DWORD bytesWritten;
                    WriteFile(hFile, line, len, &bytesWritten, NULL);
                    CloseHandle(hFile);
                    written = TRUE;
                } else {
                    Sleep(30);
                }
            }
        }
    }
    return 0;
#elif UCI_MODE == 1
    uci_loop();
    return 0;
#elif UCI_MODE == 2
    HANDLE hFile = CreateFile(
        "positions.txt",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    while (true) {
        Bitboard board[15];
        init_board_start(board);
        bool color = true;
        int result = 0;
        uint64_t seen[202];
        int seen_count = 0;
        seen[seen_count++] = hash64(board, color);
        repStack[repPlies++] = seen[0];

        for (int ply = 0; ply < 200; ++ply) {
            var best[2];
            int score = 0;
            pickBestMove(board, color, best, &score, 235);

            if (ply < 10) {
                var moves[218][2];
                findLegalMoves(board, moves, color);
                pickRandomMove(moves, board, best);
            }
            if (score >= 123000) {
                result = 2 * color - 1;
                break;
            }
            if (score <= -123000) {
                result = 1 - 2 * color;
                break;
            }

            playMove(board, best);

            var oppMoves[218][2];
            findLegalMoves(board, oppMoves, !color);
            if (oppMoves[0][0] == 255) {
                if (is_attacked(board, __builtin_ctzll(board[5 + 6 * color]), color)) {
                    result = 2 * color - 1;
                    break; // Checkmate
                } else {
                    result = 0;
                    break; // Stalemate
                }
            }

            uint64_t h = hash64(board, !color);
            int dup = 0;
            for (int i = 0; i < seen_count; ++i)
                dup |= (seen[i] == h);
            if (dup) break; // One-fold repetition (draw)
            seen[seen_count++] = h;
            repStack[repPlies++] = h;

            if ((ply >= 10) && abs(evaluate(board) - score) <= 650 && (score != 0)) {
                char line[2048];
                int len = snprintf(line, sizeof(line), "%d", score);
                for (int k = 0; k < 15; ++k)
                    len += snprintf(line + len, sizeof(line) - len, " %llu", (Bitboard)board[k]);
                len += snprintf(line + len, sizeof(line) - len, "\r\n");

                DWORD bytesWritten;
                WriteFile(hFile, line, (DWORD)len, &bytesWritten, NULL);
            }

            color = !color;
        }
    }
#elif UCI_MODE == 3
    // WORK IN PROGRESS

    Bitboard board[15];
    bool color = true;

    double defaultConstants = {0.24};

    for (int iteration = 0; true; ++iteration) { // tuning loop
        // start the game, play the opening
        init_board_start(board);
        for (var moveNumber = 0; moveNumber < 20; ++moveNumber) {
            var moves[218][2];
            var bestMove[2];
            findLegalMoves(board, moves, (moveNumber + 1) % 2);
            pickRandomMove(moves, board, bestMove);
            playMove(board, bestMove);
        }
        for (var moveNumber = 20; moveNumber < 200; ++moveNumber) { // game loop, adjucate after 200 moves
        }
    }
#endif
}