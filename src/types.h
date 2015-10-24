/*
  GodWhale, a  USI shogi(japanese-chess) playing engine derived from NanohaMini
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2010 Marco Costalba, Joona Kiiski, Tord Romstad (Stockfish author)
  Copyright (C) 2014 Kazuyuki Kawabata (NanohaMini author)
  Copyright (C) 2015 ebifrier, espelade, kakiage

  NanohaMini is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  GodWhale is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined(TYPES_H_INCLUDED)
#define TYPES_H_INCLUDED

#include <climits>
#include <cstdlib>
#if defined(NANOHA)
#include <cstdio>
#endif


#if defined(_MSC_VER) || defined(_WIN32)

// Disable some silly and noisy warning from MSVC compiler
#pragma warning(disable: 4800) // Forcing value to bool 'true' or 'false'
#pragma warning(disable: 4127) // Conditional expression is constant
#pragma warning(disable: 4146) // Unary minus operator applied to unsigned type

// MSVC does not support <inttypes.h>
typedef   signed __int8    int8_t;
typedef unsigned __int8   uint8_t;
typedef   signed __int16  int16_t;
typedef unsigned __int16 uint16_t;
typedef   signed __int32  int32_t;
typedef unsigned __int32 uint32_t;
typedef   signed __int64  int64_t;
typedef unsigned __int64 uint64_t;

#define snprintf    _snprintf

#else

#include <inttypes.h>

#endif

#ifndef UINT64_C
#if defined(_MSC_VER)
#define UINT64_C(x) (x ## UI64)
#define PRId64      "I64d"
#else
#define UINT64_C(x) (x ## llu)
#define PRId64      "lld"
#endif
#endif

#define Min(x, y) (((x) < (y)) ? (x) : (y))
#define Max(x, y) (((x) < (y)) ? (y) : (x))

////
//// Configuration
////

//// For Linux and OSX configuration is done automatically using Makefile.
//// To get started type "make help".
////
//// For windows part of the configuration is detected automatically, but
//// some switches need to be set manually:
////
//// -DNDEBUG       | Disable debugging mode. Use always.
////
//// -DNO_PREFETCH  | Disable use of prefetch asm-instruction. A must if you want the
////                | executable to run on some very old machines.
////
//// -DUSE_POPCNT   | Add runtime support for use of popcnt asm-instruction.
////                | Works only in 64-bit mode. For compiling requires hardware
////                | with popcnt support. Around 4% speed-up.
////
//// -DOLD_LOCKS    | By default under Windows are used the fast Slim Reader/Writer (SRW)
////                | Locks and Condition Variables: these are not supported by Windows XP
////                | and older, to compile for those platforms you should enable OLD_LOCKS.

// Automatic detection for 64-bit under Windows
#if defined(_WIN64)
#define IS_64BIT
#endif

// Automatic detection for use of bsfq asm-instruction under Windows
#if defined(_WIN64)
#define USE_BSFQ
#endif

// Intel header for _mm_popcnt_u64() intrinsic
#if defined(USE_POPCNT) && defined(_MSC_VER) && defined(__INTEL_COMPILER)
#include <nmmintrin.h>
#endif

// Cache line alignment specification
#if defined(_MSC_VER) || defined(_WIN32) || defined(__INTEL_COMPILER)
#define CACHE_LINE_ALIGNMENT __declspec(align(64))
#else
#define CACHE_LINE_ALIGNMENT  __attribute__ ((aligned(64)))
#endif

// Define a __cpuid() function for gcc compilers, for Intel and MSVC
// is already available as an intrinsic.
#if defined(_MSC_VER) || defined(_WIN32)
#include <intrin.h>
#pragma intrinsic(abs)
#pragma intrinsic(_BitScanForward)
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
inline void __cpuid(int CPUInfo[4], int InfoType)
{
    int* eax = CPUInfo + 0;
    int* ebx = CPUInfo + 1;
    int* ecx = CPUInfo + 2;
    int* edx = CPUInfo + 3;

    *eax = InfoType;
    *ecx = 0;
    __asm__("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                    : "0" (*eax), "2" (*ecx));
}
#else
inline void __cpuid(int CPUInfo[4], int)
{
    CPUInfo[0] = CPUInfo[1] = CPUInfo[2] = CPUInfo[3] = 0;
}
#endif

// Define FORCE_INLINE macro to force inlining overriding compiler choice
#if defined(_MSC_VER) || defined(_WIN32)
#define FORCE_INLINE  __forceinline
#elif defined(__GNUC__)
#define FORCE_INLINE  inline __attribute__((always_inline))
#else
#define FORCE_INLINE  inline
#endif

/// cpu_has_popcnt() detects support for popcnt instruction at runtime
inline bool cpu_has_popcnt() {

    int CPUInfo[4] = {-1};
    __cpuid(CPUInfo, 0x00000001);
    return (CPUInfo[2] >> 23) & 1;
}

/// CpuHasPOPCNT is a global constant initialized at startup that
/// is set to true if CPU on which application runs supports popcnt
/// hardware instruction. Unless USE_POPCNT is not defined.
#if defined(USE_POPCNT)
const bool CpuHasPOPCNT = cpu_has_popcnt();
#else
const bool CpuHasPOPCNT = false;
#endif


/// CpuIs64Bit is a global constant initialized at compile time that
/// is set to true if CPU on which application runs is a 64 bits.
#if defined(IS_64BIT)
const bool CpuIs64Bit = true;
#else
const bool CpuIs64Bit = false;
#endif

#include <string>

typedef uint64_t Key;
#if !defined(NANOHA)
typedef uint64_t Bitboard;
#endif

const int PLY_MAX = 100;
#if !defined(NANOHA)
const int PLY_MAX_PLUS_2 = PLY_MAX + 2;
#else
const int PLY_MAX_PLUS_2 = PLY_MAX + 4;        // 名前と一致しないが、+4にする(+2だと配列オーバーする)。
                                               // あとのVersionでは前に2、後ろに2サイズを拡張しているので妥当な処置かと。
#endif

enum ValueType {
    VALUE_TYPE_NONE  = 0,
    VALUE_TYPE_UPPER = 1,
    VALUE_TYPE_LOWER = 2,
    VALUE_TYPE_EXACT = VALUE_TYPE_UPPER | VALUE_TYPE_LOWER
};

enum Value {
    VALUE_ZERO      = 0,
    VALUE_DRAW      = 0,
    VALUE_KNOWN_WIN = 15000,
    VALUE_MATE      = 30000,
    VALUE_INFINITE  = 30001,
    VALUE_NONE      = 30002,

    VALUE_MATE_IN_PLY_MAX  =  VALUE_MATE - PLY_MAX,
    VALUE_MATED_IN_PLY_MAX = -VALUE_MATE + PLY_MAX,

    VALUE_ENSURE_INTEGER_SIZE_P = INT_MAX,
    VALUE_ENSURE_INTEGER_SIZE_N = INT_MIN
};

#if defined(NANOHA)

enum Player {SENTE = 0, GOTE = 0x10};

// 以下の判定は x が空(EMP)や壁(WALL)でないことを要求する(空や壁の場合は正しく判定できない)
#define IsGote(x)            ((x) & GOTE)
#define IsSente(x)            (!IsGote(x))
#define IsNotOwn(x,SorG)    (((x) & GOTE) != SorG)

enum PieceType {
    PIECE_TYPE_NONE = 0,
    FU = 0x01,
    KY = 0x02,
    KE = 0x03,
    GI = 0x04,
    KI = 0x05,
    KA = 0x06,
    HI = 0x07,
    OU = 0x08,

    PROMOTED = 0x08,
    NAMAMASK = 0x07,

    TO = FU | PROMOTED,
    NY = KY | PROMOTED,
    NK = KE | PROMOTED,
    NG = GI | PROMOTED,
    UM = KA | PROMOTED,
    RY = HI | PROMOTED
};

enum Piece {
    EMP = 0x00,
    WALL= 0x80,

    SFU = FU | SENTE,
    SKY = KY | SENTE,
    SKE = KE | SENTE,
    SGI = GI | SENTE,
    SKI = KI | SENTE,
    SKA = KA | SENTE,
    SHI = HI | SENTE,
    SOU = OU | SENTE,
    STO = TO | SENTE,
    SNY = NY | SENTE,
    SNK = NK | SENTE,
    SNG = NG | SENTE,
    SUM = UM | SENTE,
    SRY = RY | SENTE,

    GFU = FU | GOTE,
    GKY = KY | GOTE,
    GKE = KE | GOTE,
    GGI = GI | GOTE,
    GKI = KI | GOTE,
    GKA = KA | GOTE,
    GHI = HI | GOTE,
    GOU = OU | GOTE,
    GTO = TO | GOTE,
    GNY = NY | GOTE,
    GNK = NK | GOTE,
    GNG = NG | GOTE,
    GUM = UM | GOTE,
    GRY = RY | GOTE,
    PIECE_NONE = 32
};

// 評価関数の38要素化のために使用する駒番号
enum PieceNumber {
    PIECENUMBER_NONE = 0,
    KNS_SOU = 1, // 先手玉
    KNE_SOU = 1,
    KNS_GOU = 2, // 後手玉
    KNE_GOU = 2,
    KNS_HI = 3, // 飛
    KNE_HI = 4,
    KNS_KA = 5, // 角
    KNE_KA = 6,
    KNS_KI = 7, // 金
    KNE_KI = 10,
    KNS_GI = 11, // 銀
    KNE_GI = 14,
    KNS_KE = 15, // 桂
    KNE_KE = 18,
    KNS_KY = 19, // 香車
    KNE_KY = 22,
    KNS_FU = 23, // 歩
    KNE_FU = 40,
    PIECENUMBER_MIN = KNS_HI,
    PIECENUMBER_MAX = KNE_FU
};

enum Color {
    BLACK, WHITE, COLOR_NONE
};

typedef uint8_t PieceKind_t;       // 駒種類
#define NLIST	           38

// 利きの定義
typedef uint32_t effect_t;

#define EFFECT_SHORT_MASK        0x00000FFF
#define EFFECT_LONG_MASK        0x00FF0000
#define EFFECT_LONG_SHIFT        16
#define EXIST_LONG_EFFECT(x)    ((x) & EFFECT_LONG_MASK)
#define EXIST_EFFECT(x)            (x)

#define EFFECT_UP        (1u <<  0)
#define EFFECT_DOWN        (1u <<  1)
#define EFFECT_RIGHT    (1u <<  2)
#define EFFECT_LEFT        (1u <<  3)
#define EFFECT_UR        (1u <<  4)
#define EFFECT_UL        (1u <<  5)
#define EFFECT_DR        (1u <<  6)
#define EFFECT_DL        (1u <<  7)
#define EFFECT_KEUR        (1u <<  8)
#define EFFECT_KEUL        (1u <<  9)
#define EFFECT_KEDR        (1u << 10)
#define EFFECT_KEDL        (1u << 11)

#define EFFECT_KING_S(pos)    relate_pos(pos,kingS)
#define EFFECT_KING_G(pos)    relate_pos(pos,kingG)

// 方向の定義
    //
    //  DIR_KEUL           DIR_KEUR
    //  DIR_UL    DIR_UP   DIR_UR
    //  DIR_LEFT    ●     DIR_RIGHT
    //  DIR_DL   DIR_DOWN  DIR_DR
    //  DIR_KEDL           DIR_KEDR
    //

#define DIR_UP        (-1)
#define DIR_DOWN    (+1)
#define DIR_RIGHT    (-16)
#define DIR_LEFT    (+16)
#define DIR_UR        (DIR_UP   + DIR_RIGHT)
#define DIR_UL        (DIR_UP   + DIR_LEFT)
#define DIR_DR        (DIR_DOWN + DIR_RIGHT)
#define DIR_DL        (DIR_DOWN + DIR_LEFT)
#define DIR_KEUR    (DIR_UP   + DIR_UP   + DIR_RIGHT)
#define DIR_KEUL    (DIR_UP   + DIR_UP   + DIR_LEFT)
#define DIR_KEDR    (DIR_DOWN + DIR_DOWN + DIR_RIGHT)
#define DIR_KEDL    (DIR_DOWN + DIR_DOWN + DIR_LEFT)

#define DIR00        DIR_UP
#define DIR01        DIR_DOWN
#define DIR02        DIR_RIGHT
#define DIR03        DIR_LEFT
#define DIR04        DIR_UR
#define DIR05        DIR_UL
#define DIR06        DIR_DR
#define DIR07        DIR_DL
#define DIR08        DIR_KEUR
#define DIR09        DIR_KEUL
#define DIR10        DIR_KEDR
#define DIR11        DIR_KEDL
#define KIKI00        EFFECT_UP
#define KIKI01        EFFECT_DOWN
#define KIKI02        EFFECT_RIGHT
#define KIKI03        EFFECT_LEFT
#define KIKI04        EFFECT_UR
#define KIKI05        EFFECT_UL
#define KIKI06        EFFECT_DR
#define KIKI07        EFFECT_DL
#define KIKI08        EFFECT_KEUR
#define KIKI09        EFFECT_KEUL
#define KIKI10        EFFECT_KEDR
#define KIKI11        EFFECT_KEDL

// 成れるかどうか？
template<Color color>
inline bool can_promotion(const int pos)
{
    return (color == BLACK) ? ((pos & 0x0F) <= 3) : ((pos & 0x0F) >= 7);
}

// 行きどころのない駒を打ってはいけない
//   歩と香：先手は1段目、後手は9段目がNG
template<Color color>
inline bool is_drop_pawn(const int pos)
{
    if (color == BLACK && (pos & 0x0F) < 2) return false;
    if (color == WHITE && (pos & 0x0F) > 8) return false;
    return true;
}

//   桂：先手は1、2段目、後手は8、9段目がNG
template<Color color>
inline bool is_drop_knight(const int pos)
{
    if (color == BLACK && (pos & 0x0F) < 3) return false;
    if (color == WHITE && (pos & 0x0F) > 7) return false;
    return true;
}

// 持駒
struct Hand {
    uint32_t h;
    static const uint32_t tbl[HI+1];
/*
    xxxxxxxx xxxxxxxx xxxxxxxx xxx11111  歩
    xxxxxxxx xxxxxxxx xxxxx111 xxxxxxxx  香
    xxxxxxxx xxxxxxxx x111xxxx xxxxxxxx  桂
    xxxxxxxx xxxxx111 xxxxxxxx xxxxxxxx  銀
    xxxxxxxx x111xxxx xxxxxxxx xxxxxxxx  金
    xxxxxx11 xxxxxxxx xxxxxxxx xxxxxxxx  角
    xx11xxxx xxxxxxxx xxxxxxxx xxxxxxxx  飛
    このように定義すると16進数で表示したときにどの駒を何枚持っているかわかりやすい。
    例）飛、金、桂を各1枚、歩を8枚持っているときは 0x10101008 となる
 */
#define HAND_FU_SHIFT     0
#define HAND_KY_SHIFT     8
#define HAND_KE_SHIFT    12
#define HAND_GI_SHIFT    16
#define HAND_KI_SHIFT    20
#define HAND_KA_SHIFT    24
#define HAND_HI_SHIFT    28

#define HAND_FU_MASK    (0x1Fu << HAND_FU_SHIFT)
#define HAND_KY_MASK    (0x07u << HAND_KY_SHIFT)
#define HAND_KE_MASK    (0x07u << HAND_KE_SHIFT)
#define HAND_GI_MASK    (0x07u << HAND_GI_SHIFT)
#define HAND_KI_MASK    (0x07u << HAND_KI_SHIFT)
#define HAND_KA_MASK    (0x03u << HAND_KA_SHIFT)
#define HAND_HI_MASK    (0x03u << HAND_HI_SHIFT)
#define HAND_FU_INC        (0x01u << HAND_FU_SHIFT)
#define HAND_KY_INC        (0x01u << HAND_KY_SHIFT)
#define HAND_KE_INC        (0x01u << HAND_KE_SHIFT)
#define HAND_GI_INC        (0x01u << HAND_GI_SHIFT)
#define HAND_KI_INC        (0x01u << HAND_KI_SHIFT)
#define HAND_KA_INC        (0x01u << HAND_KA_SHIFT)
#define HAND_HI_INC        (0x01u << HAND_HI_SHIFT)
#define HAND_DOM_MASK    (~(HAND_FU_MASK | HAND_KY_MASK | HAND_KE_MASK | HAND_GI_MASK | HAND_KI_MASK | HAND_KA_MASK | HAND_HI_MASK))
#define IS_DOM_HAND(h1, h2)    ((((h1) - (h2)) & HAND_DOM_MASK) == 0)
    // 各駒の枚数を取得する
    inline uint32_t getFU() const {return ((h & HAND_FU_MASK) >> HAND_FU_SHIFT);}
    inline uint32_t getKY() const {return ((h & HAND_KY_MASK) >> HAND_KY_SHIFT);}
    inline uint32_t getKE() const {return ((h & HAND_KE_MASK) >> HAND_KE_SHIFT);}
    inline uint32_t getGI() const {return ((h & HAND_GI_MASK) >> HAND_GI_SHIFT);}
    inline uint32_t getKI() const {return ((h & HAND_KI_MASK) >> HAND_KI_SHIFT);}
    inline uint32_t getKA() const {return ((h & HAND_KA_MASK) >> HAND_KA_SHIFT);}
    inline uint32_t getHI() const {return ((h & HAND_HI_MASK) >> HAND_HI_SHIFT);}
    // 駒を持っているかどうか(存在を確認するだけならシフト演算は不要)
    inline uint32_t existFU() const {return (h & HAND_FU_MASK);}
    inline uint32_t existKY() const {return (h & HAND_KY_MASK);}
    inline uint32_t existKE() const {return (h & HAND_KE_MASK);}
    inline uint32_t existGI() const {return (h & HAND_GI_MASK);}
    inline uint32_t existKI() const {return (h & HAND_KI_MASK);}
    inline uint32_t existKA() const {return (h & HAND_KA_MASK);}
    inline uint32_t existHI() const {return (h & HAND_HI_MASK);}
    inline int exist(int kind) const {kind &= ~GOTE; if (IS_DOM_HAND(h, tbl[kind])) return 1; return 0;}
    inline void set(const int hand[]) {
        h = uint32_t((hand[HI] << HAND_HI_SHIFT) + (hand[KA] << HAND_KA_SHIFT)
          + (hand[KI] << HAND_KI_SHIFT) + (hand[GI] << HAND_GI_SHIFT)
          + (hand[KE] << HAND_KE_SHIFT) + (hand[KY] << HAND_KY_SHIFT)
          + (hand[FU] << HAND_FU_SHIFT));
    }
    inline void inc(const int kind) {h += tbl[kind];}
    inline void dec(const int kind) {h -= tbl[kind];}
    Hand& operator = (uint32_t a) {h = a; return *this;}
    bool Empty() const {return h == 0;}
};

#else
enum PieceType {
    PIECE_TYPE_NONE = 0,
    PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6
};

enum Piece {
    PIECE_NONE_DARK_SQ = 0, WP = 1, WN = 2, WB = 3, WR = 4, WQ = 5, WK = 6,
    BP = 9, BN = 10, BB = 11, BR = 12, BQ = 13, BK = 14, PIECE_NONE = 16
};

enum Color {
    WHITE, BLACK, COLOR_NONE
};
#endif

enum Depth {

    ONE_PLY = 2,

    DEPTH_ZERO          =  0 * ONE_PLY,
    DEPTH_QS_CHECKS     = -1 * ONE_PLY,
    DEPTH_QS_NO_CHECKS  = -2 * ONE_PLY,
    DEPTH_QS_RECAPTURES = -4 * ONE_PLY,
    DEPTH_DECISIVE      = 1024 * ONE_PLY,

    DEPTH_NONE = -127 * ONE_PLY
};

#if defined(NANOHA)
enum Square {
    SQ_A1 = 0x11, SQ_A2, SQ_A3, SQ_A4, SQ_A5, SQ_A6, SQ_A7, SQ_A8, SQ_A9,
    SQ_B1 = 0x21, SQ_B2, SQ_B3, SQ_B4, SQ_B5, SQ_B6, SQ_B7, SQ_B8, SQ_B9,
    SQ_C1 = 0x31, SQ_C2, SQ_C3, SQ_C4, SQ_C5, SQ_C6, SQ_C7, SQ_C8, SQ_C9,
    SQ_D1 = 0x41, SQ_D2, SQ_D3, SQ_D4, SQ_D5, SQ_D6, SQ_D7, SQ_D8, SQ_D9,
    SQ_E1 = 0x51, SQ_E2, SQ_E3, SQ_E4, SQ_E5, SQ_E6, SQ_E7, SQ_E8, SQ_E9,
    SQ_F1 = 0x61, SQ_F2, SQ_F3, SQ_F4, SQ_F5, SQ_F6, SQ_F7, SQ_F8, SQ_F9,
    SQ_G1 = 0x71, SQ_G2, SQ_G3, SQ_G4, SQ_G5, SQ_G6, SQ_G7, SQ_G8, SQ_G9,
    SQ_H1 = 0x81, SQ_H2, SQ_H3, SQ_H4, SQ_H5, SQ_H6, SQ_H7, SQ_H8, SQ_H9,
    SQ_I1 = 0x91, SQ_I2, SQ_I3, SQ_I4, SQ_I5, SQ_I6, SQ_I7, SQ_I8, SQ_I9,
    SQ_NONE
};
#else
enum Square {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,

    DELTA_N =  8,
    DELTA_E =  1,
    DELTA_S = -8,
    DELTA_W = -1,

    DELTA_NN = DELTA_N + DELTA_N,
    DELTA_NE = DELTA_N + DELTA_E,
    DELTA_SE = DELTA_S + DELTA_E,
    DELTA_SS = DELTA_S + DELTA_S,
    DELTA_SW = DELTA_S + DELTA_W,
    DELTA_NW = DELTA_N + DELTA_W
};
#endif

enum File {
#if defined(NANOHA)
    FILE_0, FILE_1, FILE_2, FILE_3, FILE_4, FILE_5, FILE_6, FILE_7, FILE_8, FILE_9
#else
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H
#endif
};

enum Rank {
#if defined(NANOHA)
    RANK_0, RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9
#else
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8
#endif
};

#if !defined(NANOHA)
enum SquareColor {
    DARK, LIGHT
};
#endif

enum ScaleFactor {
    SCALE_FACTOR_ZERO   = 0,
    SCALE_FACTOR_NORMAL = 64,
    SCALE_FACTOR_MAX    = 128,
    SCALE_FACTOR_NONE   = 255
};

#if !defined(NANOHA)
enum CastleRight {
    CASTLES_NONE = 0,
    WHITE_OO     = 1,
    BLACK_OO     = 2,
    WHITE_OOO    = 4,
    BLACK_OOO    = 8,
    ALL_CASTLES  = 15
};
#endif


enum { nhand = 7, nfile = 9, nrank = 9, nsquare = 81 };

enum KPP{
    none = 0,
    f_hand_pawn = 0, // 0
    e_hand_pawn = f_hand_pawn + 19,
    f_hand_lance = e_hand_pawn + 19,
    e_hand_lance = f_hand_lance + 5,
    f_hand_knight = e_hand_lance + 5,
    e_hand_knight = f_hand_knight + 5,
    f_hand_silver = e_hand_knight + 5,
    e_hand_silver = f_hand_silver + 5,
    f_hand_gold = e_hand_silver + 5,
    e_hand_gold = f_hand_gold + 5,
    f_hand_bishop = e_hand_gold + 5,
    e_hand_bishop = f_hand_bishop + 3,
    f_hand_rook = e_hand_bishop + 3,
    e_hand_rook = f_hand_rook + 3,
    fe_hand_end = e_hand_rook + 3,

    f_pawn = fe_hand_end,
    e_pawn = f_pawn + 81,
    f_lance = e_pawn + 81,
    e_lance = f_lance + 81,
    f_knight = e_lance + 81,
    e_knight = f_knight + 81,
    f_silver = e_knight + 81,
    e_silver = f_silver + 81,
    f_gold = e_silver + 81,
    e_gold = f_gold + 81,
    f_bishop = e_gold + 81,
    e_bishop = f_bishop + 81,
    f_horse = e_bishop + 81,
    e_horse = f_horse + 81,
    f_rook = e_horse + 81,
    e_rook = f_rook + 81,
    f_dragon = e_rook + 81,
    e_dragon = f_dragon + 81,
    fe_end = e_dragon + 81
};

/// Score enum keeps a midgame and an endgame value in a single
/// integer (enum), first LSB 16 bits are used to store endgame
/// value, while upper bits are used for midgame value. Compiler
/// is free to choose the enum type as long as can keep its data,
/// so ensure Score to be an integer type.
enum Score {
    SCORE_ZERO = 0,
    SCORE_ENSURE_INTEGER_SIZE_P = INT_MAX,
    SCORE_ENSURE_INTEGER_SIZE_N = INT_MIN
};

#define ENABLE_OPERATORS_ON(T) \
inline T operator+ (const T d1, const T d2) { return T(int(d1) + int(d2)); } \
inline T operator- (const T d1, const T d2) { return T(int(d1) - int(d2)); } \
inline T operator* (int i, const T d) {  return T(i * int(d)); } \
inline T operator* (const T d, int i) {  return T(int(d) * i); } \
inline T operator/ (const T d, int i) { return T(int(d) / i); } \
inline T operator+ (const T d) { return T(int(d)); } \
inline T operator- (const T d) { return T(-int(d)); } \
inline T operator++ (T& d) { d = T(int(d) + 1); return d; } \
inline T operator-- (T& d) { d = T(int(d) - 1); return d; } \
inline T operator++ (T& d, int) { T v = d; d = T(int(d) + 1); return v; } \
inline T operator-- (T& d, int) { T v = d; d = T(int(d) - 1); return v; } \
inline void operator+= (T& d1, const T d2) { d1 = d1 + d2; } \
inline void operator-= (T& d1, const T d2) { d1 = d1 - d2; } \
inline void operator*= (T& d, int i) { d = T(int(d) * i); } \
inline void operator/= (T& d, int i) { d = T(int(d) / i); }

ENABLE_OPERATORS_ON(Value)
ENABLE_OPERATORS_ON(PieceType)
ENABLE_OPERATORS_ON(Piece)
ENABLE_OPERATORS_ON(PieceNumber)
ENABLE_OPERATORS_ON(Color)
ENABLE_OPERATORS_ON(Depth)
ENABLE_OPERATORS_ON(Square)
ENABLE_OPERATORS_ON(File)
ENABLE_OPERATORS_ON(Rank)

#undef ENABLE_OPERATORS_ON

// Extra operators for adding integers to a Value
inline Value operator+ (Value v, int i) { return Value(int(v) + i); }
inline Value operator- (Value v, int i) { return Value(int(v) - i); }

// Extracting the _signed_ lower and upper 16 bits it not so trivial
// because according to the standard a simple cast to short is
// implementation defined and so is a right shift of a signed integer.
inline Value mg_value(Score s) { return Value(((s + 32768) & ~0xffff) / 0x10000); }

// Unfortunatly on Intel 64 bit we have a small speed regression, so use a faster code in
// this case, although not 100% standard compliant it seems to work for Intel and MSVC.
#if defined(IS_64BIT) && (!defined(__GNUC__) || defined(__INTEL_COMPILER))
inline Value eg_value(Score s) { return Value(int16_t(s & 0xffff)); }
#else
inline Value eg_value(Score s) { return Value((int)(unsigned(s) & 0x7fffu) - (int)(unsigned(s) & 0x8000u)); }
#endif

inline Score make_score(int mg, int eg) { return Score((mg << 16) + eg); }

// Division must be handled separately for each term
inline Score operator/(Score s, int i) { return make_score(mg_value(s) / i, eg_value(s) / i); }

// Only declared but not defined. We don't want to multiply two scores due to
// a very high risk of overflow. So user should explicitly convert to integer.
inline Score operator*(Score s1, Score s2);

// Remaining Score operators are standard
inline Score operator+ (const Score d1, const Score d2) { return Score(int(d1) + int(d2)); }
inline Score operator- (const Score d1, const Score d2) { return Score(int(d1) - int(d2)); }
inline Score operator* (int i, const Score d) {  return Score(i * int(d)); }
inline Score operator* (const Score d, int i) {  return Score(int(d) * i); }
inline Score operator- (const Score d) { return Score(-int(d)); }
inline void operator+= (Score& d1, const Score d2) { d1 = d1 + d2; }
inline void operator-= (Score& d1, const Score d2) { d1 = d1 - d2; }
inline void operator*= (Score& d, int i) { d = Score(int(d) * i); }
inline void operator/= (Score& d, int i) { d = Score(int(d) / i); }

#if defined(NANOHA)
const Value PawnValueMidgame   = Value(87+87);

extern const Value PieceValueMidgame[32];
extern const Value PieceValueEndgame[32];

#else
const Value PawnValueMidgame   = Value(0x0C6);
const Value PawnValueEndgame   = Value(0x102);
const Value KnightValueMidgame = Value(0x331);
const Value KnightValueEndgame = Value(0x34E);
const Value BishopValueMidgame = Value(0x344);
const Value BishopValueEndgame = Value(0x359);
const Value RookValueMidgame   = Value(0x4F6);
const Value RookValueEndgame   = Value(0x4FE);
const Value QueenValueMidgame  = Value(0x9D9);
const Value QueenValueEndgame  = Value(0x9FE);

extern const Value PieceValueMidgame[17];
extern const Value PieceValueEndgame[17];
extern int SquareDistance[64][64];
#endif

inline Value piece_value_midgame(Piece p) {
    return PieceValueMidgame[p];
}

inline Value piece_value_endgame(Piece p) {
    return PieceValueEndgame[p];
}

inline Value value_mate_in(int ply) {
    return VALUE_MATE - ply;
}

inline Value value_mated_in(int ply) {
    return -VALUE_MATE + ply;
}

inline Piece make_piece(Color c, PieceType pt) {
#if defined(NANOHA)
    return (c == BLACK) ? Piece(int(pt)) : Piece(GOTE | int(pt));
#else
    return Piece((c << 3) | pt);
#endif
}

inline PieceType type_of(Piece p)  {
#if defined(NANOHA)
    return PieceType(int(p) & 0x0F);
#else
    return PieceType(p & 7);
#endif
}

inline Color color_of(Piece p) {
#if defined(NANOHA)
    return (int(p) & GOTE) ? WHITE : BLACK;
#else
    return Color(p >> 3);
#endif
}

inline Color flip(Color c) {
    return Color(c ^ 1);
}

inline Square make_square(File f, Rank r) {
#if defined(NANOHA)
    return Square((int(f) << 4) | int(r));
#else
    return Square((r << 3) | f);
#endif
}

inline Square make_square(int f, int r) {
    return make_square(File(f), Rank(r));
}

inline bool square_is_ok(Square s) {
#if defined(NANOHA)
    return s >= SQ_A1 && s <= SQ_I9;
#else
    return s >= SQ_A1 && s <= SQ_H8;
#endif
}

inline File file_of(Square s) {
#if defined(NANOHA)
    return File(int(s) >> 4);
#else
    return File(s & 7);
#endif
}

inline Rank rank_of(Square s) {
#if defined(NANOHA)
    return Rank(int(s) & 0x0F);
#else
    return Rank(s >> 3);
#endif
}

inline Square flip(Square s) {
#if defined(NANOHA)
    return Square(0xAA - int(s));
#else
    return Square(s ^ 56);
#endif
}

inline Square mirror(Square s) {
#if defined(NANOHA)
    int s2 = int(s);
    int f = s2 & 0x0F;
    int r = s2 & 0xF0;
    return Square(int(0xA0 - r + f));
#else
    return Square(s ^ 7);
#endif
}

#ifndef NANOHA
inline Square relative_square(Color c, Square s) {
    return Square(s ^ (c * 56));
}

inline Rank relative_rank(Color c, Rank r) {
    return Rank(r ^ (c * 7));
}

inline Rank relative_rank(Color c, Square s) {
    return relative_rank(c, rank_of(s));
}
#endif

#if !defined(NANOHA)
inline SquareColor color_of(Square s) {
    return SquareColor(int(rank_of(s) + s) & 1);
}

inline bool opposite_colors(Square s1, Square s2) {
    int s = s1 ^ s2;
    return ((s >> 3) ^ s) & 1;
}

inline int file_distance(Square s1, Square s2) {
    return abs(file_of(s1) - file_of(s2));
}

inline int rank_distance(Square s1, Square s2) {
    return abs(rank_of(s1) - rank_of(s2));
}

inline int square_distance(Square s1, Square s2) {
    return SquareDistance[s1][s2];
}
#endif

#if !defined(NANOHA)
inline char piece_type_to_char(PieceType pt) {
    return " PNBRQK"[pt];
}
#endif


inline char file_to_char(File f) {
#if defined(NANOHA)
    return char(f - FILE_1 + int('a'));
#else
    return char(f - FILE_A + int('a'));
#endif
}

inline char rank_to_char(Rank r) {
    return char(r - RANK_1 + int('1'));
}

inline const std::string square_to_string(Square s) {
    char ch[] = { file_to_char(file_of(s)), rank_to_char(rank_of(s)), 0 };
    return ch;
}

#ifndef NANOHA
inline Square pawn_push(Color c) {
    return c == WHITE ? DELTA_N : DELTA_S;
}
#endif

#if defined(NANOHA)

#define MAX_CHECKS        128        // 王手の最大数
#define MAX_EVASION       128        // 王手回避の最大数

namespace NanohaTbl {
    extern const int Direction[];
    extern const int KomaValue[32];        // 駒の価値
    extern const int KomaValueEx[32];    // 取られたとき(捕獲されたとき)の価値
    extern const int KomaValuePro[32];    // 成る価値
    extern const int Piece2Index[32];    // 駒の種類に変換する({と、杏、圭、全}を金と同一視)

    extern const short z2sq[];
    extern const KPP KppIndex0[32];    // pieceをkppのindexに変換
    extern const KPP KppIndex1[32];    // pieceをkppのindexに変換
    extern const KPP HandIndex0[32];   // kpp_handのindexに変換
    extern const KPP HandIndex1[32];   // kpp_handのindexに変換
}

// なのはの座標系(0x11～0x99)を(0～80)に圧縮する
inline int conv_z2sq(int z){
    return NanohaTbl::z2sq[z];
}
// (0～80)をなのはの座標系(0x11～0x99)にする
inline int conv_sq2z(int sq)
{
    int x, y;
    y = sq / 9;
    x = sq % 9;
    return (x+1)*0x10+(y+1);
}

#if defined(__GNUC__)
inline unsigned char _BitScanForward(unsigned long * Index, unsigned long Mask)
{
    if (Mask == 0) return 0;
    *Index = static_cast<unsigned long>(__builtin_ctz(Mask));
    return 1;
}

// MS のコンパイラのコード生成のヒントだが、gccでは無効なため
#define __assume(x) __builtin_unreachable()
#endif // defined(__GNUC__)

inline unsigned int PopCnt32(unsigned int value)
{
    unsigned int bits = 0;
    while (value) {value &= value-1; bits++;}
    return bits;
}

// 出力系
int output_info(const char *fmt, ...);        // 情報出力(printf()代わり)
int foutput_log(FILE *fp, const char *fmt, ...);        // 情報出力(printf()代わり)
//int output_debug(const char *fmt, ...);        // for デバッグ

#define ENABLE_MYASSERT            // MYASSERT()を有効にする
#if defined(ENABLE_MYASSERT)
extern int debug_level;
// x が真のときに何もしない
// *static_cast<int*>(0)=0でメモリ保護例外が発生し、ただちに止まるためデバッグしやすい.
#define MYASSERT(x)    \
            do {        \
                if (!(x)) {    \
                    fprintf(stderr, "Assert:%s:%d:[%s]\n", __FILE__, __LINE__, #x);    \
                    *static_cast<int*>(0)=0;    \
                }    \
            } while(0)
#define MYABORT()    \
            do {        \
                fprintf(stderr, "Abort:%s:%d\n", __FILE__, __LINE__);    \
                *static_cast<int*>(0)=0;    \
            } while(0)
#define SET_DEBUG(x)    debug_level=(x)
#define DEBUG_LEVEL        debug_level
#else
#define MYASSERT(x)    
#define MYABORT()    
#define SET_DEBUG(x)
#define DEBUG_LEVEL        0
#endif

#if defined(CHK_PERFORM)
#define COUNT_PERFORM(x)    ((x)++)
#else
#define COUNT_PERFORM(x)
#endif // defined(CHK_PERFORM)

#endif

#endif // !defined(TYPES_H_INCLUDED)
