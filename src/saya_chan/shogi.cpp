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

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include "position.h"
#include "tt.h"
#include "book.h"
#include "ucioption.h"
#if defined(EVAL_MICRO)
#include "param_micro.h"
#elif defined(EVAL_OLD)
#include "param_old.h"
#else
#include "param_new.h"
#endif

const uint32_t Hand::tbl[HI+1] = {
    0, HAND_FU_INC, HAND_KY_INC, HAND_KE_INC, HAND_GI_INC, HAND_KI_INC, HAND_KA_INC, HAND_HI_INC, 
};
#if defined(ENABLE_MYASSERT)
int debug_level;
#endif

#if !defined(NDEBUG)
#define MOVE_TRACE        // 局面に至った指し手を保持する
#endif
#if defined(MOVE_TRACE)
Move m_trace[PLY_MAX_PLUS_2];
void disp_trace(int n)
{
    for (int i = 0; i < n; i++) {
        std::cerr << i << ":" << move_to_csa(m_trace[i]) << " ";
    }
}
#endif

namespace NanohaTbl {
    // 方向の定義
    const int Direction[32] = {
        DIR00, DIR01, DIR02, DIR03, DIR04, DIR05, DIR06, DIR07,
        DIR08, DIR09, DIR10, DIR11, 0,     0,     0,     0,
        DIR00, DIR01, DIR02, DIR03, DIR04, DIR05, DIR06, DIR07,
        DIR00, DIR01, DIR02, DIR03, DIR04, DIR05, DIR06, DIR07,
    };

#if !defined(TSUMESOLVER)
    // 駒の価値
    const int KomaValue[32] = {
         0,
         DPawn,
         DLance,
         DKnight,
         DSilver,
         DGold,
         DBishop,
         DRook,
         DKing,
         DProPawn,
         DProLance,
         DProKnight,
         DProSilver,
         0,
         DHorse,
         DDragon,
         0,
        -DPawn,
        -DLance,
        -DKnight,
        -DSilver,
        -DGold,
        -DBishop,
        -DRook,
        -DKing,
        -DProPawn,
        -DProLance,
        -DProKnight,
        -DProSilver,
        -0,
        -DHorse,
        -DDragon,
    };

    // 取られたとき(捕獲されたとき)の価値
    const int KomaValueEx[32] = {
         0 + 0,
         DPawn + DPawn,
         DLance + DLance,
         DKnight + DKnight,
         DSilver + DSilver,
         DGold + DGold,
         DBishop + DBishop,
         DRook + DRook,
         DKing + DKing,
         DProPawn + DPawn,
         DProLance + DLance,
         DProKnight + DKnight,
         DProSilver + DSilver,
         0 + 0,
         DHorse + DBishop,
         DDragon + DRook,
         0 + 0,
        -DPawn    -DPawn,
        -DLance    -DLance,
        -DKnight    -DKnight,
        -DSilver    -DSilver,
        -DGold    -DGold,
        -DBishop    -DBishop,
        -DRook    -DRook,
        -DKing    -DKing,
        -DProPawn    -DPawn,
        -DProLance    -DLance,
        -DProKnight    -DKnight,
        -DProSilver    -DSilver,
        -0    -0,
        -DHorse    -DBishop,
        -DDragon    -DRook,
    };

    // 成る価値
    const int KomaValuePro[32] = {
         0,
         DProPawn - DPawn,
         DProLance - DLance,
         DProKnight - DKnight,
         DProSilver - DSilver,
         0,
         DHorse - DBishop,
         DDragon - DRook,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
        -(DProPawn - DPawn),
        -(DProLance - DLance),
        -(DProKnight - DKnight),
        -(DProSilver - DSilver),
         0,
        -(DHorse - DBishop),
        -(DDragon - DRook),
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
    };
#endif//#if !defined(TSUMESOLVER)

    // Historyのindex変換用
    const int Piece2Index[32] = {    // 駒の種類に変換する({と、杏、圭、全}を金と同一視)
        EMP, SFU, SKY, SKE, SGI, SKI, SKA, SHI,
        SOU, SKI, SKI, SKI, SKI, EMP, SUM, SRY,
        EMP, GFU, GKY, GKE, GGI, GKI, GKA, GHI,
        GOU, GKI, GKI, GKI, GKI, EMP, GUM, GRY,
    };
}

static FILE *fp_info = stdout;
static FILE *fp_log = NULL;

int output_info(const char *fmt, ...)
{
    int ret = 0;

    va_list argp;
    va_start(argp, fmt);
    if (fp_log) vfprintf(fp_log, fmt, argp);
#if !defined(USE_USI)
    if (fp_info) {
        ret = vfprintf(fp_info, fmt, argp);
    }
#endif
    va_end(argp);

    return ret;
}
int foutput_log(FILE *fp, const char *fmt, ...)
{
    int ret = 0;

    va_list argp;
    va_start(argp, fmt);
    if (fp == stdout) {
        if (fp_log) vfprintf(fp_log, fmt, argp);
    }
    if (fp) ret = vfprintf(fp, fmt, argp);
    va_end(argp);

    return ret;
}

// 実行ファイル起動時に行う初期化.
void init_application_once()
{
    Position::init_evaluate();    // 評価ベクトルの読み込み
    Position::initMate1ply();

// 定跡ファイルの読み込み
    if (book == NULL) {
        book = new Book();
        book->open(Options["BookFile"].value<std::string>());
    }

    int from;
    int to;
    int i;
    memset(Position::DirTbl, 0, sizeof(Position::DirTbl));
    for (from = 0x11; from <= 0x99; from++) {
        if ((from & 0x0F) == 0 || (from & 0x0F) > 9) continue;
        for (i = 0; i < 8; i++) {
            int dir = NanohaTbl::Direction[i];
            to = from;
            while (1) {
                to += dir;
                if ((to & 0x0F) == 0 || (to & 0x0F) >    9) break;
                if ((to & 0xF0) == 0 || (to & 0xF0) > 0x90) break;
                Position::DirTbl[from][to] = (1 << i);
            }
        }
    }
}

// 初期化関係
void Position::init_position(const unsigned char board_ori[9][9], const int Mochigoma_ori[])
{
//    Tesu = 0;

    size_t i;
    unsigned char board[9][9];
    int Mochigoma[GOTE+HI+1] = {0};
    {
        int x, y;
        for (y = 0; y < 9; y++) {
            for (x = 0; x < 9; x++) {
                board[y][x] = board_ori[y][x];
            }
        }
        memcpy(Mochigoma, Mochigoma_ori, sizeof(Mochigoma));

        // 持ち駒設定。
        handS.set(&Mochigoma[SENTE]);
        handG.set(&Mochigoma[GOTE]);
    }

    // 盤面をWALL（壁）で埋めておきます。
    for (i = 0; i < sizeof(banpadding)/sizeof(banpadding[0]); i++) {
        banpadding[i] = WALL;
    }
    for (i = 0; i < sizeof(ban)/sizeof(ban[0]); i++) {
        ban[i] = WALL;
    }

    // 初期化
    memset(komano, 0, sizeof(komano));
    memset(knkind, 0, sizeof(knkind));
    memset(knpos,  0, sizeof(knpos));

    // boardで与えられた局面を設定します。
    int z;
    PieceNumber kn;
    for(int dan = 1; dan <= 9; dan++) {
        for(int suji = 0x10; suji <= 0x90; suji += 0x10) {
            // 将棋の筋は左から右なので、配列の宣言と逆になるため、筋はひっくり返さないとなりません。
            z = suji + dan;
            ban[z] = Piece(board[dan-1][9 - suji/0x10]);

            // 駒番号系のデータ設定
#define KNABORT()    fprintf(stderr, "Error!:%s:%d:ban[0x%X] == 0x%X\n", __FILE__, __LINE__, z, ban[z]);exit(-1)
#define KNSET(kind) for (kn = KNS_##kind; kn <= KNE_##kind; kn++) {        \
                        if (knkind[kn] == 0) break;        \
                    }        \
                    if (kn > KNE_##kind) {KNABORT();}        \
                    knkind[kn] = ban[z];            \
                    knpos[kn] = z;                    \
                    komano[z] = kn;

            switch (ban[z]) {
            case EMP:
                break;
            case SFU:
            case STO:
            case GFU:
            case GTO:
                KNSET(FU);
                break;
            case SKY:
            case SNY:
            case GKY:
            case GNY:
                KNSET(KY);
                break;
            case SKE:
            case SNK:
            case GKE:
            case GNK:
                KNSET(KE);
                break;
            case SGI:
            case SNG:
            case GGI:
            case GNG:
                KNSET(GI);
                break;
            case SKI:
            case GKI:
                KNSET(KI);
                break;
            case SKA:
            case SUM:
            case GKA:
            case GUM:
                KNSET(KA);
                break;
            case SHI:
            case SRY:
            case GHI:
            case GRY:
                KNSET(HI);
                break;
            case SOU:
                KNSET(SOU);
                break;
            case GOU:
                KNSET(GOU);
                break;
            case WALL:
            case PIECE_NONE:
            default:
                KNABORT();
                break;
            }
#undef KNABORT
#undef KNSET

        }
    }

#define KNABORT(kind)    fprintf(stderr, "Error!:%s:%d:kind=%d\n", __FILE__, __LINE__, kind);exit(-1)
#define KNHANDSET(SorG, kind) \
        for (kn = KNS_##kind; kn <= KNE_##kind; kn++) {        \
            if (knkind[kn] == EMP) break;        \
        }        \
        if (kn > KNE_##kind) {KNABORT(kind);}          \
        knkind[kn] = static_cast<Piece>(SorG | kind);  \
        knpos[kn] = (SorG == SENTE) ? 1 : 2;

    int n;
    n = Mochigoma[SENTE+FU];    while (n--) {KNHANDSET(SENTE, FU)}
    n = Mochigoma[SENTE+KY];    while (n--) {KNHANDSET(SENTE, KY)}
    n = Mochigoma[SENTE+KE];    while (n--) {KNHANDSET(SENTE, KE)}
    n = Mochigoma[SENTE+GI];    while (n--) {KNHANDSET(SENTE, GI)}
    n = Mochigoma[SENTE+KI];    while (n--) {KNHANDSET(SENTE, KI)}
    n = Mochigoma[SENTE+KA];    while (n--) {KNHANDSET(SENTE, KA)}
    n = Mochigoma[SENTE+HI];    while (n--) {KNHANDSET(SENTE, HI)}

    n = Mochigoma[GOTE+FU];    while (n--) {KNHANDSET(GOTE, FU)}
    n = Mochigoma[GOTE+KY];    while (n--) {KNHANDSET(GOTE, KY)}
    n = Mochigoma[GOTE+KE];    while (n--) {KNHANDSET(GOTE, KE)}
    n = Mochigoma[GOTE+GI];    while (n--) {KNHANDSET(GOTE, GI)}
    n = Mochigoma[GOTE+KI];    while (n--) {KNHANDSET(GOTE, KI)}
    n = Mochigoma[GOTE+KA];    while (n--) {KNHANDSET(GOTE, KA)}
    n = Mochigoma[GOTE+HI];    while (n--) {KNHANDSET(GOTE, HI)}
#undef KNABORT
#undef KNHANDSET

    // effectB/effectWの初期化
    init_effect();

    // ピン情報の初期化
    make_pin_info();

#if defined(MAKELIST_DIFF)
    // listの初期化
    init_make_list();
#endif
}

// ピンの状態を設定する
void Position::make_pin_info()
{
    memset(pin+0x11, 0, (0x99-0x11+1)*sizeof(pin[0]));
    int p;
#define ADDKING1(SG, dir) \
    do {                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        if (ban[p -= DIR_ ## dir] != EMP) break;                \
        p -= DIR_ ## dir;                                        \
    } while (0)
#define ADDKING2(SG, dir) 

    if (kingS) {    //自玉が盤面にある時のみ有効
#define SetPinS(dir)    \
        p = kingS;        \
        ADDKING1(S, dir);    \
        if (ban[p] != WALL) {    \
            if ((ban[p] & GOTE) == 0) {        \
                if (effectW[p] & (EFFECT_ ## dir << EFFECT_LONG_SHIFT)) pin[p] = DIR_ ## dir;        \
            }        \
            ADDKING2(S, dir);    \
        }

        SetPinS(UP);
        SetPinS(UL);
        SetPinS(UR);
        SetPinS(LEFT);
        SetPinS(RIGHT);
        SetPinS(DL);
        SetPinS(DR);
        SetPinS(DOWN);
#undef SetPinS
    }
    if (kingG) {    //敵玉が盤面にある時のみ有効
#define SetPinG(dir)    \
        p = kingG;        \
        ADDKING1(G, dir);    \
        if (ban[p] != WALL) {        \
            if ((ban[p] & GOTE) != 0) {        \
                if (effectB[p] & (EFFECT_ ## dir << EFFECT_LONG_SHIFT)) pin[p] = DIR_ ## dir;        \
            }    \
            ADDKING2(G, dir);    \
        }

        SetPinG(DOWN);
        SetPinG(DL);
        SetPinG(DR);
        SetPinG(RIGHT);
        SetPinG(LEFT);
        SetPinG(UL);
        SetPinG(UR);
        SetPinG(UP);
#undef SetPinG
    }
#undef ADDKING1
#undef ADDKING2
}

// 利き関係
// effectB/effectWの初期化
void Position::init_effect()
{
    int dan, suji;

    memset(effect, 0, sizeof(effect));

    for (suji = 0x10; suji <= 0x90; suji += 0x10) {
        for (dan = 1 ; dan <= 9 ; dan++) {
            add_effect(suji + dan);
        }
    }
}

void Position::add_effect(const int z)
{
#define ADD_EFFECT(turn,dir) zz = z + DIR_ ## dir; effect[turn][zz] |= EFFECT_ ## dir;

    int zz;

    switch (ban[z]) {

    case EMP:    break;
    case SFU:
        ADD_EFFECT(BLACK, UP);
        break;
    case SKY:
        AddKikiDirS(z, DIR_UP, EFFECT_UP << EFFECT_LONG_SHIFT);
        break;
    case SKE:
        ADD_EFFECT(BLACK, KEUR);
        ADD_EFFECT(BLACK, KEUL);
        break;
    case SGI:
        ADD_EFFECT(BLACK, UP);
        ADD_EFFECT(BLACK, UR);
        ADD_EFFECT(BLACK, UL);
        ADD_EFFECT(BLACK, DR);
        ADD_EFFECT(BLACK, DL);
        break;
    case SKI:
    case STO:
    case SNY:
    case SNK:
    case SNG:
        ADD_EFFECT(BLACK, UP);
        ADD_EFFECT(BLACK, UR);
        ADD_EFFECT(BLACK, UL);
        ADD_EFFECT(BLACK, RIGHT);
        ADD_EFFECT(BLACK, LEFT);
        ADD_EFFECT(BLACK, DOWN);
        break;
    case SUM:
        ADD_EFFECT(BLACK, UP);
        ADD_EFFECT(BLACK, RIGHT);
        ADD_EFFECT(BLACK, LEFT);
        ADD_EFFECT(BLACK, DOWN);
        // 角と同じ利きを追加するため break しない
    case SKA:
        AddKikiDirS(z, DIR_UR, EFFECT_UR << EFFECT_LONG_SHIFT);
        AddKikiDirS(z, DIR_UL, EFFECT_UL << EFFECT_LONG_SHIFT);
        AddKikiDirS(z, DIR_DR, EFFECT_DR << EFFECT_LONG_SHIFT);
        AddKikiDirS(z, DIR_DL, EFFECT_DL << EFFECT_LONG_SHIFT);
        break;
    case SRY:
        ADD_EFFECT(BLACK, UR);
        ADD_EFFECT(BLACK, UL);
        ADD_EFFECT(BLACK, DR);
        ADD_EFFECT(BLACK, DL);
        // 飛と同じ利きを追加するため break しない
    case SHI:
        AddKikiDirS(z, DIR_UP,    EFFECT_UP    << EFFECT_LONG_SHIFT);
        AddKikiDirS(z, DIR_DOWN,  EFFECT_DOWN  << EFFECT_LONG_SHIFT);
        AddKikiDirS(z, DIR_LEFT,  EFFECT_LEFT  << EFFECT_LONG_SHIFT);
        AddKikiDirS(z, DIR_RIGHT, EFFECT_RIGHT << EFFECT_LONG_SHIFT);
        break;
    case SOU:
        ADD_EFFECT(BLACK, UP);
        ADD_EFFECT(BLACK, UR);
        ADD_EFFECT(BLACK, UL);
        ADD_EFFECT(BLACK, RIGHT);
        ADD_EFFECT(BLACK, LEFT);
        ADD_EFFECT(BLACK, DOWN);
        ADD_EFFECT(BLACK, DR);
        ADD_EFFECT(BLACK, DL);
        break;

    case GFU:
        ADD_EFFECT(WHITE, DOWN);
        break;
    case GKY:
        AddKikiDirG(z, DIR_DOWN, EFFECT_DOWN << EFFECT_LONG_SHIFT);
        break;
    case GKE:
        ADD_EFFECT(WHITE, KEDR);
        ADD_EFFECT(WHITE, KEDL);
        break;
    case GGI:
        ADD_EFFECT(WHITE, DOWN);
        ADD_EFFECT(WHITE, DR);
        ADD_EFFECT(WHITE, DL);
        ADD_EFFECT(WHITE, UR);
        ADD_EFFECT(WHITE, UL);
        break;
    case GKI:
    case GTO:
    case GNY:
    case GNK:
    case GNG:
        ADD_EFFECT(WHITE, DOWN);
        ADD_EFFECT(WHITE, DR);
        ADD_EFFECT(WHITE, DL);
        ADD_EFFECT(WHITE, RIGHT);
        ADD_EFFECT(WHITE, LEFT);
        ADD_EFFECT(WHITE, UP);
        break;
    case GUM:
        ADD_EFFECT(WHITE, DOWN);
        ADD_EFFECT(WHITE, RIGHT);
        ADD_EFFECT(WHITE, LEFT);
        ADD_EFFECT(WHITE, UP);
        // 角と同じ利きを追加するため break しない
    case GKA:
        AddKikiDirG(z, DIR_DR, EFFECT_DR << EFFECT_LONG_SHIFT);
        AddKikiDirG(z, DIR_DL, EFFECT_DL << EFFECT_LONG_SHIFT);
        AddKikiDirG(z, DIR_UR, EFFECT_UR << EFFECT_LONG_SHIFT);
        AddKikiDirG(z, DIR_UL, EFFECT_UL << EFFECT_LONG_SHIFT);
        break;
    case GRY:
        ADD_EFFECT(WHITE, DR);
        ADD_EFFECT(WHITE, DL);
        ADD_EFFECT(WHITE, UR);
        ADD_EFFECT(WHITE, UL);
        // 飛と同じ利きを追加するため break しない
    case GHI:
        AddKikiDirG(z, DIR_DOWN,  EFFECT_DOWN  << EFFECT_LONG_SHIFT);
        AddKikiDirG(z, DIR_UP,    EFFECT_UP    << EFFECT_LONG_SHIFT);
        AddKikiDirG(z, DIR_RIGHT, EFFECT_RIGHT << EFFECT_LONG_SHIFT);
        AddKikiDirG(z, DIR_LEFT,  EFFECT_LEFT  << EFFECT_LONG_SHIFT);
        break;
    case GOU:
        ADD_EFFECT(WHITE, DOWN);
        ADD_EFFECT(WHITE, DR);
        ADD_EFFECT(WHITE, DL);
        ADD_EFFECT(WHITE, RIGHT);
        ADD_EFFECT(WHITE, LEFT);
        ADD_EFFECT(WHITE, UP);
        ADD_EFFECT(WHITE, UR);
        ADD_EFFECT(WHITE, UL);
        break;

    case WALL:
    case PIECE_NONE:
    default:__assume(0);
        break;
    }
}

void Position::del_effect(const int z, const Piece kind)
{
#define DEL_EFFECT(turn,dir) zz = z + DIR_ ## dir; effect[turn][zz] &= ~(EFFECT_ ## dir);

    int zz;
    switch (kind) {

    case EMP: break;

    case SFU:
        DEL_EFFECT(BLACK, UP);
        break;
    case SKY:
        DelKikiDirS(z, DIR_UP, ~(EFFECT_UP << EFFECT_LONG_SHIFT));
        break;
    case SKE:
        DEL_EFFECT(BLACK, KEUR);
        DEL_EFFECT(BLACK, KEUL);
        break;
    case SGI:
        DEL_EFFECT(BLACK, UP);
        DEL_EFFECT(BLACK, UR);
        DEL_EFFECT(BLACK, UL);
        DEL_EFFECT(BLACK, DR);
        DEL_EFFECT(BLACK, DL);
        break;
    case SKI:
    case STO:
    case SNY:
    case SNK:
    case SNG:
        DEL_EFFECT(BLACK, UP);
        DEL_EFFECT(BLACK, UR);
        DEL_EFFECT(BLACK, UL);
        DEL_EFFECT(BLACK, RIGHT);
        DEL_EFFECT(BLACK, LEFT);
        DEL_EFFECT(BLACK, DOWN);
        break;
    case SUM:
        DEL_EFFECT(BLACK, UP);
        DEL_EFFECT(BLACK, RIGHT);
        DEL_EFFECT(BLACK, LEFT);
        DEL_EFFECT(BLACK, DOWN);
        // 角と同じ利きを削除するため break しない
    case SKA:
        DelKikiDirS(z, DIR_UR, ~(EFFECT_UR << EFFECT_LONG_SHIFT));
        DelKikiDirS(z, DIR_UL, ~(EFFECT_UL << EFFECT_LONG_SHIFT));
        DelKikiDirS(z, DIR_DR, ~(EFFECT_DR << EFFECT_LONG_SHIFT));
        DelKikiDirS(z, DIR_DL, ~(EFFECT_DL << EFFECT_LONG_SHIFT));
        break;
    case SRY:
        DEL_EFFECT(BLACK, UR);
        DEL_EFFECT(BLACK, UL);
        DEL_EFFECT(BLACK, DR);
        DEL_EFFECT(BLACK, DL);
        // 飛と同じ利きを削除するため break しない
    case SHI:
        DelKikiDirS(z, DIR_UP,    ~(EFFECT_UP    << EFFECT_LONG_SHIFT));
        DelKikiDirS(z, DIR_DOWN,  ~(EFFECT_DOWN  << EFFECT_LONG_SHIFT));
        DelKikiDirS(z, DIR_LEFT,  ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT));
        DelKikiDirS(z, DIR_RIGHT, ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT));
        break;
    case SOU:
        DEL_EFFECT(BLACK, UP);
        DEL_EFFECT(BLACK, UR);
        DEL_EFFECT(BLACK, UL);
        DEL_EFFECT(BLACK, RIGHT);
        DEL_EFFECT(BLACK, LEFT);
        DEL_EFFECT(BLACK, DOWN);
        DEL_EFFECT(BLACK, DR);
        DEL_EFFECT(BLACK, DL);
        break;

    case GFU:
        DEL_EFFECT(WHITE, DOWN);
        break;
    case GKY:
        DelKikiDirG(z, DIR_DOWN, ~(EFFECT_DOWN << EFFECT_LONG_SHIFT));
        break;
    case GKE:
        DEL_EFFECT(WHITE, KEDR);
        DEL_EFFECT(WHITE, KEDL);
        break;
    case GGI:
        DEL_EFFECT(WHITE, DOWN);
        DEL_EFFECT(WHITE, DR);
        DEL_EFFECT(WHITE, DL);
        DEL_EFFECT(WHITE, UR);
        DEL_EFFECT(WHITE, UL);
        break;
    case GKI:
    case GTO:
    case GNY:
    case GNK:
    case GNG:
        DEL_EFFECT(WHITE, DOWN);
        DEL_EFFECT(WHITE, DR);
        DEL_EFFECT(WHITE, DL);
        DEL_EFFECT(WHITE, RIGHT);
        DEL_EFFECT(WHITE, LEFT);
        DEL_EFFECT(WHITE, UP);
        break;
    case GUM:
        DEL_EFFECT(WHITE, UP);
        DEL_EFFECT(WHITE, RIGHT);
        DEL_EFFECT(WHITE, LEFT);
        DEL_EFFECT(WHITE, DOWN);
        // 角と同じ利きを削除するため break しない
    case GKA:
        DelKikiDirG(z, DIR_UR, ~(EFFECT_UR << EFFECT_LONG_SHIFT));
        DelKikiDirG(z, DIR_UL, ~(EFFECT_UL << EFFECT_LONG_SHIFT));
        DelKikiDirG(z, DIR_DR, ~(EFFECT_DR << EFFECT_LONG_SHIFT));
        DelKikiDirG(z, DIR_DL, ~(EFFECT_DL << EFFECT_LONG_SHIFT));
        break;
    case GRY:
        DEL_EFFECT(WHITE, UR);
        DEL_EFFECT(WHITE, UL);
        DEL_EFFECT(WHITE, DR);
        DEL_EFFECT(WHITE, DL);
        // 飛と同じ利きを削除するため break しない
    case GHI:
        DelKikiDirG(z, DIR_UP,    ~(EFFECT_UP    << EFFECT_LONG_SHIFT));
        DelKikiDirG(z, DIR_DOWN,  ~(EFFECT_DOWN  << EFFECT_LONG_SHIFT));
        DelKikiDirG(z, DIR_LEFT,  ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT));
        DelKikiDirG(z, DIR_RIGHT, ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT));
        break;
    case GOU:
        DEL_EFFECT(WHITE, DOWN);
        DEL_EFFECT(WHITE, DR);
        DEL_EFFECT(WHITE, DL);
        DEL_EFFECT(WHITE, RIGHT);
        DEL_EFFECT(WHITE, LEFT);
        DEL_EFFECT(WHITE, UP);
        DEL_EFFECT(WHITE, UR);
        DEL_EFFECT(WHITE, UL);
        break;

    case WALL:
    case PIECE_NONE:
    default:__assume(0);
        break;
    }
}


/// Position::do_move() は手を進める。そして必要なすべての情報を StateInfo オブジェクトに保存する。
/// 手は合法であることを前提としている。Pseudo-legalな手はこの関数を呼ぶ前に取り除く必要がある。

/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.

void Position::do_move(Move m, StateInfo& newSt)
{
    assert(is_ok());
    assert(&newSt != st);
    assert(!at_checking());
#if defined(MOVE_TRACE)
    m_trace[st->gamePly] = m;
    assert(m != MOVE_NULL);    // NullMoveはdo_null_move()で処理する
#endif

    nodes++;
    Key key = st->key;

    // Copy some fields of old state to our new StateInfo object except the
    // ones which are recalculated from scratch anyway, then switch our state
    // pointer to point to the new, ready to be updated, state.
    /// ※ position.cpp の struct StateInfo と定義を合わせる
    struct ReducedStateInfo {
        int gamePly;
        int pliesFromNull;
        Piece captured;
        uint32_t hand;
        uint32_t effect;
        Key key;
        PieceNumber kncap;  // 捕った持駒の駒番号

#if defined(MAKELIST_DIFF)
        Piece cap_hand;     // 捕獲した持駒のPiece
        Piece drop_hand;    // 打った持駒のPiece
        PieceNumber kndrop; // 打った持駒の駒番号
        int caplist[2];     // 捕獲される駒のlist
        int droplist[2];    // 打つ持駒のlist
        int fromlist[2];    // 動かす駒のlist
#endif
    };

    memcpy(&newSt, st, sizeof(ReducedStateInfo));

    newSt.previous = st;
    st = &newSt;

    // Update side to move
    key ^= zobSideToMove;

    // Increment the 50 moves rule draw counter. Resetting it to zero in the
    // case of non-reversible moves is taken care of later.
    st->pliesFromNull++;

    const Color us = side_to_move();
    if (move_is_drop(m))
    {
        st->key = key;
        do_drop(m);
        st->hand = hand[us].h;
        st->effect = (us == BLACK) ? effectB[kingG] : effectW[kingS];
        assert(!at_checking());
        assert(get_key() == compute_key());
        return;
    }

    const Square from = move_from(m);
    const Square to = move_to(m);
    bool pm = is_promotion(m);

    Piece piece = piece_on(from);
    Piece capture = piece_on(to);
    PieceNumber kn;
    unsigned long id;
    unsigned long tkiki;

    assert(color_of(piece_on(from)) == us);
    assert(color_of(piece_on(to)) == flip(us) || square_is_empty(to));

    // ピン情報のクリア
    if (piece == SOU) {
        // 先手玉を動かす
        DelPinInfS(DIR_UP);
        DelPinInfS(DIR_DOWN);
        DelPinInfS(DIR_RIGHT);
        DelPinInfS(DIR_LEFT);
        DelPinInfS(DIR_UR);
        DelPinInfS(DIR_UL);
        DelPinInfS(DIR_DR);
        DelPinInfS(DIR_DL);
        if (EFFECT_KING_G(to) /*&& EFFECT_KING_G(to) == ((effectB[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
            _BitScanForward(&id, EFFECT_KING_G(to));
            DelPinInfG(NanohaTbl::Direction[id]);
        }
    } else if (piece == GOU) {
        // 後手玉を動かす
        DelPinInfG(DIR_UP);
        DelPinInfG(DIR_DOWN);
        DelPinInfG(DIR_RIGHT);
        DelPinInfG(DIR_LEFT);
        DelPinInfG(DIR_UR);
        DelPinInfG(DIR_UL);
        DelPinInfG(DIR_DR);
        DelPinInfG(DIR_DL);
        if (EFFECT_KING_S(to) /*&& EFFECT_KING_S(to) == ((effectW[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
            _BitScanForward(&id, EFFECT_KING_S(to));
            DelPinInfS(NanohaTbl::Direction[id]);
        }
    } else {
        if (us == BLACK) {
            // 先手番
            if (EFFECT_KING_S(from)) {
///                _BitScanForward(&id, EFFECT_KING_S(from));
///                DelPinInfS(NanohaTbl::Direction[id]);
                pin[from] = 0;
            }
            if (EFFECT_KING_S(to)/* && (effectW[to] & EFFECT_LONG_MASK)*/) {
                _BitScanForward(&id, EFFECT_KING_S(to));
                DelPinInfS(NanohaTbl::Direction[id]);
            }
///            if (DirTbl[kingG][from] == DirTbl[kingG][to]) {
///                pin[to] = 0;
///            } else 
            {
                if (EFFECT_KING_G(from)) {
                    _BitScanForward(&id, EFFECT_KING_G(from));
                    DelPinInfG(NanohaTbl::Direction[id]);
                }
                if (EFFECT_KING_G(to)) {
                    _BitScanForward(&id, EFFECT_KING_G(to));
                    DelPinInfG(NanohaTbl::Direction[id]);
                }
            }
        } else {
            // 後手番
///            if (DirTbl[kingS][from] == DirTbl[kingS][to]) {
///                pin[to] = 0;
///            } else 
            {
                if (EFFECT_KING_S(from)) {
                    _BitScanForward(&id, EFFECT_KING_S(from));
                    DelPinInfS(NanohaTbl::Direction[id]);
                }
                if (EFFECT_KING_S(to)) {
                    _BitScanForward(&id, EFFECT_KING_S(to));
                    DelPinInfS(NanohaTbl::Direction[id]);
                }
            }
            if (EFFECT_KING_G(from)) {
//                _BitScanForward(&id, EFFECT_KING_G(from));
//                DelPinInfG(NanohaTbl::Direction[id]);
                pin[from] = 0;
            }
            if (EFFECT_KING_G(to)/* && (effectB[to] & EFFECT_LONG_MASK)*/) {
                _BitScanForward(&id, EFFECT_KING_G(to));
                DelPinInfG(NanohaTbl::Direction[id]);
            }
        }
    }

    del_effect(from, piece);                    // 動かす駒の利きを消す
    if (capture) {
        del_effect(to, capture);    // 取る駒の利きを消す
        kn = komano[to];
        knkind[kn] = static_cast<Piece>((capture ^ GOTE) & ~PROMOTED);
        knpos[kn] = (us == BLACK) ? 1 : 2;
        if (us == BLACK) handS.inc(capture & ~(GOTE | PROMOTED));
        else             handG.inc(capture & ~(GOTE | PROMOTED));

        st->kncap = PieceNumber(kn); // 捕獲された駒の番号をセーブ

#if defined(MAKELIST_DIFF)
        make_list_capture(kn, Piece(knkind[kn]));
#endif

#if !defined(TSUMESOLVER)
        // material 更新
        material -= NanohaTbl::KomaValueEx[capture];
#endif

        // ハッシュ更新
        key ^= zobrist[capture][to];
    } else {
        // 移動先は空→移動先の長い利きを消す
        // 後手の利きを消す
        if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
            while (tkiki) {
                _BitScanForward(&id, tkiki);
                tkiki &= tkiki-1;
                DelKikiDirG(to, NanohaTbl::Direction[id], ~(1u << id));
            }
        }
        // 先手の利きを消す
        if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
            while (tkiki) {
                _BitScanForward(&id, tkiki);
                tkiki &= tkiki-1;
                DelKikiDirS(to, NanohaTbl::Direction[id], ~(1u << id));
            }
        }
    }

    kn = komano[from];
    if (pm) {
#if !defined(TSUMESOLVER)
        // material 更新
        material += NanohaTbl::KomaValuePro[piece];
#endif

        piece = Piece(int(piece)|PROMOTED);
    }
    knkind[kn] = piece;
    knpos[kn] = to;

    // ハッシュ更新
    key ^= zobrist[ban[from]][from] ^ zobrist[piece][to];

    // Prefetch TT access as soon as we know key is updated
    prefetch(reinterpret_cast<char*>(TT.first_entry(key)));

    // Move the piece
    ban[to]   = piece;
    ban[from] = EMP;
    komano[to] = kn;
    komano[from] = PIECENUMBER_NONE;
    add_effect(to); // 利きを更新

#if defined(MAKELIST_DIFF)
    make_list_move(kn, piece, to);
#endif

    // 移動元の長い利きを伸ばす
    // 後手の利きを書く
    if ((tkiki = effectW[from] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            AddKikiDirG(from, NanohaTbl::Direction[id], 1u << id);
        }
    }
    // 先手の利きを書く
    if ((tkiki = effectB[from] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            AddKikiDirS(from, NanohaTbl::Direction[id], 1u << id);
        }
    }

    // ピン情報の付加
    if (piece == SOU) {
        AddPinInfS(DIR_UP);
        AddPinInfS(DIR_DOWN);
        AddPinInfS(DIR_RIGHT);
        AddPinInfS(DIR_LEFT);
        AddPinInfS(DIR_UR);
        AddPinInfS(DIR_UL);
        AddPinInfS(DIR_DR);
        AddPinInfS(DIR_DL);
        if (EFFECT_KING_G(from) /*&& (effectB[from] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_G(from));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(to) /*&& (effectB[from] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_G(to));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
    } else if (piece == GOU) {
        AddPinInfG(DIR_UP);
        AddPinInfG(DIR_DOWN);
        AddPinInfG(DIR_RIGHT);
        AddPinInfG(DIR_LEFT);
        AddPinInfG(DIR_UR);
        AddPinInfG(DIR_UL);
        AddPinInfG(DIR_DR);
        AddPinInfG(DIR_DL);
        if (EFFECT_KING_S(from) /*&& (effectW[from] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_S(from));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_S(to) /*&& (effectW[from] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_S(to));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
    } else {
        if (EFFECT_KING_S(from)) {
            _BitScanForward(&id, EFFECT_KING_S(from));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_S(to)) {
            _BitScanForward(&id, EFFECT_KING_S(to));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(from)) {
            _BitScanForward(&id, EFFECT_KING_G(from));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(to)) {
            _BitScanForward(&id, EFFECT_KING_G(to));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
    }

    // Set capture piece
    st->captured = capture;

    // Update the key with the final value
    st->key = key;
    st->hand = hand[us].h;
    st->effect = (us == BLACK) ? effectB[kingG] : effectW[kingS];

#if !defined(NDEBUG)
    // 手を指したあとに、王手になっている⇒自殺手になっている
    if (in_check()) {
        print_csa(m);
        disp_trace(st->gamePly + 1);
        MYABORT();
    }
#endif

    // Finish
    sideToMove = flip(sideToMove);

#if defined(MOVE_TRACE)
    int fail;
    if (is_ok(&fail) == false) {
        std::cerr << "Error!:is_ok() is false. Reason code = " << fail << std::endl;
        print_csa(m);
    }
#else
    assert(is_ok());
#endif
    assert(get_key() == compute_key());
}

void Position::do_drop(Move m)
{
    const Color us = side_to_move();
    const Square to = move_to(m);

    assert(square_is_empty(to));

    Piece piece = move_piece(m);
    //int kne = 0;
    unsigned long id;
    unsigned long tkiki;

    // ピン情報のクリア
    if (EFFECT_KING_S(to)/* && EFFECT_KING_S(to) == ((effectW[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
        _BitScanForward(&id, EFFECT_KING_S(to));
        DelPinInfS(NanohaTbl::Direction[id]);
    }
    if (EFFECT_KING_G(to)/* && EFFECT_KING_G(to) == ((effectB[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
        _BitScanForward(&id, EFFECT_KING_G(to));
        DelPinInfG(NanohaTbl::Direction[id]);
    }

    // 移動先は空→移動先の長い利きを消す
    // 後手の利きを消す
    if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            DelKikiDirG(to, NanohaTbl::Direction[id], ~(1u << id));
        }
    }
    // 先手の利きを消す
    if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            DelKikiDirS(to, NanohaTbl::Direction[id], ~(1u << id));
        }
    }

    unsigned int diff = 0;
    switch (piece & ~GOTE) {
    case EMP:
        break;
    case FU:
        //kn  = KNS_FU;
        //kne = KNE_FU;
        diff = HAND_FU_INC;
        break;
    case KY:
        //kn  = KNS_KY;
        //kne = KNE_KY;
        diff = HAND_KY_INC;
        break;
    case KE:
        //kn  = KNS_KE;
        //kne = KNE_KE;
        diff = HAND_KE_INC;
        break;
    case GI:
        //kn  = KNS_GI;
        //kne = KNE_GI;
        diff = HAND_GI_INC;
        break;
    case KI:
        //kn  = KNS_KI;
        //kne = KNE_KI;
        diff = HAND_KI_INC;
        break;
    case KA:
        //kn  = KNS_KA;
        //kne = KNE_KA;
        diff = HAND_KA_INC;
        break;
    case HI:
        //kn  = KNS_HI;
        //kne = KNE_HI;
        diff = HAND_HI_INC;
        break;
    default:
        break;
    }

    if (us == BLACK) {
        handS.h -= diff;
        /*while (kn <= kne) {
            if (knpos[kn] == 1) break;
            kn++;
        }*/
    } else {
        handG.h -= diff;
        /*while (kn <= kne) {
            if (knpos[kn] == 2) break;
            kn++;
        }*/
    }

#if !defined(NDEBUG)
    // エラーのときに Die!
    if (kn > kne) {
        print_csa(m);
        MYABORT();
    }
#endif

    assert(color_of(piece) == us);

#if defined(MAKELIST_DIFF)
    PieceNumber kn = make_list_drop(piece, to);
#endif
    knkind[kn] = piece;
    knpos[kn] = to;
    ban[to] = piece;
    komano[to] = kn;

    // 利きを更新
    add_effect(to);

    // 移動元、移動先が玉の延長線上にあったときにそこのピン情報を追加する
    if (EFFECT_KING_S(to)) {
        _BitScanForward(&id, EFFECT_KING_S(to));
        AddPinInfS(NanohaTbl::Direction[id]);
    }
    if (EFFECT_KING_G(to)) {
        _BitScanForward(&id, EFFECT_KING_G(to));
        AddPinInfG(NanohaTbl::Direction[id]);
    }

    // Set capture piece
    st->captured = EMP;

    // Update the key with the final value
    st->key ^= zobrist[piece][to];

    // Prefetch TT access as soon as we know key is updated
    prefetch(reinterpret_cast<char*>(TT.first_entry(st->key)));

    // Finish
    sideToMove = flip(sideToMove);

    assert(is_ok());
}

/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.

void Position::undo_move(Move m) {

#if defined(MOVE_TRACE)
    assert(m != MOVE_NULL);    // NullMoveはundo_null_move()で処理する
    int fail;
    if (is_ok(&fail) == false) {
        disp_trace(st->gamePly+1);
        MYABORT();
    }
#else
    assert(is_ok());
#endif
    assert(::is_ok(m));

    sideToMove = flip(sideToMove);

    if (move_is_drop(m))
    {
        undo_drop(m);
        return;
    }

    Color us = side_to_move();
    Square from = move_from(m);
    Square to = move_to(m);
    bool pm = is_promotion(m);
    Piece piece = move_piece(m);
    Piece captured = st->captured;
    PieceNumber kn;
    unsigned long id;
    unsigned long tkiki;

    assert(square_is_empty(from));
    assert(color_of(piece_on(to)) == us);

    // ピン情報のクリア
    if (piece == SOU) {
        DelPinInfS(DIR_UP);
        DelPinInfS(DIR_DOWN);
        DelPinInfS(DIR_RIGHT);
        DelPinInfS(DIR_LEFT);
        DelPinInfS(DIR_UR);
        DelPinInfS(DIR_UL);
        DelPinInfS(DIR_DR);
        DelPinInfS(DIR_DL);
        if (EFFECT_KING_G(from) /*&& (effectB[from] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_G(from));
            DelPinInfG(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(to)) {
            _BitScanForward(&id, EFFECT_KING_G(to));
            DelPinInfG(NanohaTbl::Direction[id]);
        }
    } else if (piece == GOU) {
        DelPinInfG(DIR_UP);
        DelPinInfG(DIR_DOWN);
        DelPinInfG(DIR_RIGHT);
        DelPinInfG(DIR_LEFT);
        DelPinInfG(DIR_UR);
        DelPinInfG(DIR_UL);
        DelPinInfG(DIR_DR);
        DelPinInfG(DIR_DL);
        if (EFFECT_KING_S(from) /*&& (effectW[from] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_S(from));
            DelPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_S(to)) {
            _BitScanForward(&id, EFFECT_KING_S(to));
            DelPinInfS(NanohaTbl::Direction[id]);
        }
    } else {
        if (us == BLACK) {
            if (EFFECT_KING_S(from)) {
                _BitScanForward(&id, EFFECT_KING_S(from));
                DelPinInfS(NanohaTbl::Direction[id]);
            }
            if (EFFECT_KING_S(to)) {
                _BitScanForward(&id, EFFECT_KING_S(to));
                DelPinInfS(NanohaTbl::Direction[id]);
///                pin[to] = 0;
            }
            if (EFFECT_KING_G(from)) {
                _BitScanForward(&id, EFFECT_KING_G(from));
                DelPinInfG(NanohaTbl::Direction[id]);
            }
            if (EFFECT_KING_G(to)) {
                _BitScanForward(&id, EFFECT_KING_G(to));
                DelPinInfG(NanohaTbl::Direction[id]);
            }
        } else {
            if (EFFECT_KING_S(from)) {
                _BitScanForward(&id, EFFECT_KING_S(from));
                DelPinInfS(NanohaTbl::Direction[id]);
            }
            if (EFFECT_KING_S(to)) {
                _BitScanForward(&id, EFFECT_KING_S(to));
                DelPinInfS(NanohaTbl::Direction[id]);
            }
            if (EFFECT_KING_G(from)) {
                _BitScanForward(&id, EFFECT_KING_G(from));
                DelPinInfG(NanohaTbl::Direction[id]);
            }
            if (EFFECT_KING_G(to)) {
                _BitScanForward(&id, EFFECT_KING_G(to));
                DelPinInfG(NanohaTbl::Direction[id]);
///                pin[to] = 0;
            }
        }
    }

    del_effect(to, ban[to]);                    // 動かした駒の利きを消す

    kn = komano[to];
    if (pm) {
#if !defined(TSUMESOLVER)
        // material 更新
        material -= NanohaTbl::KomaValuePro[piece];
#endif
    }
    knkind[kn] = piece;
    knpos[kn] = from;

    ban[to] = captured;
    komano[from] = komano[to];
    ban[from] = piece;

#if defined(MAKELIST_DIFF)
    make_list_undo_move(kn);
#endif

    if (captured) {
#if !defined(TSUMESOLVER)
        // material 更新
        material += NanohaTbl::KomaValueEx[captured];
#endif

        // 捕獲された駒の番号をロード
        kn = st->kncap;
        knkind[kn] = captured;
        knpos[kn] = to;
        ban[to] = captured;
        komano[to] = kn;
        add_effect(to);    // 取った駒の利きを追加

#if defined(MAKELIST_DIFF)
        make_list_undo_capture(kn);
#endif

        if (us == BLACK) handS.dec(captured & ~(GOTE | PROMOTED));
        else             handG.dec(captured & ~(GOTE | PROMOTED));
    } else {
        // 移動先は空→移動先の長い利きを通す
        // 後手の利きを消す
        if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
            while (tkiki) {
                _BitScanForward(&id, tkiki);
                tkiki &= tkiki-1;
                AddKikiDirG(to, NanohaTbl::Direction[id], 1u << id);
            }
        }
        // 先手の利きを消す
        if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
            while (tkiki) {
                _BitScanForward(&id, tkiki);
                tkiki &= tkiki-1;
                AddKikiDirS(to, NanohaTbl::Direction[id], 1u << id);
            }
        }
        ban[to] = EMP;
        komano[to] = PIECENUMBER_NONE;
    }

    // 移動元の長い利きをさえぎる
    // 後手の利きを消す
    if ((tkiki = effectW[from] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            DelKikiDirG(from, NanohaTbl::Direction[id], ~(1u << id));
            if (piece == SOU) {
                // 長い利きは玉を一つだけ貫く
                if (ban[from + NanohaTbl::Direction[id]] != WALL) effectW[from + NanohaTbl::Direction[id]] |= (1u << id);
            }
        }
    }
    // 先手の利きを消す
    if ((tkiki = effectB[from] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            DelKikiDirS(from, NanohaTbl::Direction[id], ~(1u << id));
            if (piece == GOU) {
                // 長い利きは玉を一つだけ貫く
                if (ban[from + NanohaTbl::Direction[id]] != WALL) effectB[from + NanohaTbl::Direction[id]] |= (1u << id);
            }
        }
    }

    // 利きを更新
    add_effect(from);

    // ピン情報付加
    if (piece == SOU) {
        AddPinInfS(DIR_UP);
        AddPinInfS(DIR_DOWN);
        AddPinInfS(DIR_RIGHT);
        AddPinInfS(DIR_LEFT);
        AddPinInfS(DIR_UR);
        AddPinInfS(DIR_UL);
        AddPinInfS(DIR_DR);
        AddPinInfS(DIR_DL);
        if (EFFECT_KING_G(from)) {
            _BitScanForward(&id, EFFECT_KING_G(from));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(to) /*&& (effectB[to] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_G(to));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
    } else if (piece == GOU) {
        AddPinInfG(DIR_UP);
        AddPinInfG(DIR_DOWN);
        AddPinInfG(DIR_RIGHT);
        AddPinInfG(DIR_LEFT);
        AddPinInfG(DIR_UR);
        AddPinInfG(DIR_UL);
        AddPinInfG(DIR_DR);
        AddPinInfG(DIR_DL);
        if (EFFECT_KING_S(from)) {
            _BitScanForward(&id, EFFECT_KING_S(from));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_S(to) /*&& (effectW[to] & EFFECT_LONG_MASK)*/) {
            _BitScanForward(&id, EFFECT_KING_S(to));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
    } else {
        if (EFFECT_KING_S(from)) {
            _BitScanForward(&id, EFFECT_KING_S(from));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_S(to)) {
            _BitScanForward(&id, EFFECT_KING_S(to));
            AddPinInfS(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(from)) {
            _BitScanForward(&id, EFFECT_KING_G(from));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
        if (EFFECT_KING_G(to)) {
            _BitScanForward(&id, EFFECT_KING_G(to));
            AddPinInfG(NanohaTbl::Direction[id]);
        }
    }

    // Finally point our state pointer back to the previous state
    st = st->previous;

    assert(is_ok());
}

void Position::undo_drop(Move m)
{
    Color us = side_to_move();
    Square to = move_to(m);
    Piece piece = move_piece(m);
    unsigned long id;
    unsigned long tkiki;

    assert(color_of(piece_on(to)) == us);

    // 移動元、移動先が玉の延長線上にあったときにそこのピン情報を削除する
    if (EFFECT_KING_S(to)) {
        _BitScanForward(&id, EFFECT_KING_S(to));
        DelPinInfS(NanohaTbl::Direction[id]);
    }
    if (EFFECT_KING_G(to)) {
        _BitScanForward(&id, EFFECT_KING_G(to));
        DelPinInfG(NanohaTbl::Direction[id]);
    }

    unsigned int diff = 0;
    switch (type_of(piece)) {
    case PIECE_TYPE_NONE:
        break;
    case FU:
        diff = HAND_FU_INC;
        break;
    case KY:
        diff = HAND_KY_INC;
        break;
    case KE:
        diff = HAND_KE_INC;
        break;
    case GI:
        diff = HAND_GI_INC;
        break;
    case KI:
        diff = HAND_KI_INC;
        break;
    case KA:
        diff = HAND_KA_INC;
        break;
    case HI:
        diff = HAND_HI_INC;
        break;
    default:
        __assume(false);
        break;
    }

    PieceNumber kn = komano[to];
    knkind[kn] = piece;
    knpos[kn] = (us == BLACK) ? 1 : 2;
    ban[to] = EMP;
    komano[to] = PIECENUMBER_NONE;
    del_effect(to, piece);  // 動かした駒の利きを消す

#if defined(MAKELIST_DIFF)
    make_list_undo_drop(kn, piece);
#endif

    // 打った位置の長い利きを通す
    // 後手の利きを追加
    if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            AddKikiDirG(to, NanohaTbl::Direction[id], 1u << id);
        }
    }
    // 先手の利きを追加
    if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
        while (tkiki) {
            _BitScanForward(&id, tkiki);
            tkiki &= tkiki-1;
            AddKikiDirS(to, NanohaTbl::Direction[id], 1u << id);
        }
    }

    // 移動元、移動先が玉の延長線上にあったときにそこのピン情報を追加する
    if (EFFECT_KING_S(to) /*&& EFFECT_KING_S(to) == ((effectW[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
        _BitScanForward(&id, EFFECT_KING_S(to));
        AddPinInfS(NanohaTbl::Direction[id]);
    }
    if (EFFECT_KING_G(to) /*&& EFFECT_KING_G(to) == ((effectB[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
        _BitScanForward(&id, EFFECT_KING_G(to));
        AddPinInfG(NanohaTbl::Direction[id]);
    }

    if (us == BLACK) handS.h += diff;
    else             handG.h += diff;

    // Finally point our state pointer back to the previous state
    st = st->previous;

    assert(is_ok());
}

// 手を進めずにハッシュ計算のみ行う
uint64_t Position::calc_hash_no_move(const Move m) const
{
    uint64_t new_key = get_key();

    new_key ^= zobSideToMove;    // 手番反転

    // from の計算
    int from = move_from(m);
    int to = move_to(m);
    int piece = move_piece(m);
    if (!move_is_drop(m)) {
        // 空白になったことで変わるハッシュ値
        new_key ^= zobrist[piece][from];
    }

    // to の処理
    // ban[to]にあったものをＨａｓｈから消す
    Piece capture = move_captured(m);
    if (capture) {
        new_key ^= zobrist[ban[to]][to];
    }

    // 新しい駒をＨａｓｈに加える
    if (is_promotion(m)) piece |= PROMOTED;
    new_key ^= zobrist[piece][to];

    return new_key;
}

// 指し手チェック系
// 指し手が王手かどうかチェックする
bool Position::is_check_move(const Color us, Move m) const
{
    const Square kPos = (us == BLACK) ? Square(kingG) : Square(kingS);    // 相手側の玉の位置

    return move_attacks_square(m, kPos);
}

bool Position::move_attacks_square(Move m, Square kPos) const
{
    const Color us = side_to_move();
    const effect_t *akiki = (us == BLACK) ? effectB : effectW;    // 自分の利き
    const Piece piece = is_promotion(m) ? Piece(move_piece(m) | PROMOTED) : move_piece(m);
    const Square to = move_to(m);

    switch (piece) {
    case EMP:break;
    case SFU:
        if (to + DIR_UP == kPos) return true;
        break;
    case SKY:
        if (DirTbl[to][kPos] == EFFECT_UP) {
            if (SkipOverEMP(to, DIR_UP) == kPos) return true;
        }
        break;
    case SKE:
        if (to + DIR_KEUR == kPos) return true;
        if (to + DIR_KEUL == kPos) return true;
        break;
    case SGI:
        if (to + DIR_UP == kPos) return true;
        if (to + DIR_UR == kPos) return true;
        if (to + DIR_UL == kPos) return true;
        if (to + DIR_DR == kPos) return true;
        if (to + DIR_DL == kPos) return true;
        break;
    case SKI:
    case STO:
    case SNY:
    case SNK:
    case SNG:
        if (to + DIR_UP    == kPos) return true;
        if (to + DIR_UR    == kPos) return true;
        if (to + DIR_UL    == kPos) return true;
        if (to + DIR_RIGHT == kPos) return true;
        if (to + DIR_LEFT  == kPos) return true;
        if (to + DIR_DOWN  == kPos) return true;
        break;

    case GFU:
        if (to + DIR_DOWN == kPos) return true;
        break;
    case GKY:
        if (DirTbl[to][kPos] == EFFECT_DOWN) {
            if (SkipOverEMP(to, DIR_DOWN) == kPos) return true;
        }
        break;
    case GKE:
        if (to + DIR_KEDR == kPos) return true;
        if (to + DIR_KEDL == kPos) return true;
        break;
    case GGI:
        if (to + DIR_DOWN == kPos) return true;
        if (to + DIR_DR   == kPos) return true;
        if (to + DIR_DL   == kPos) return true;
        if (to + DIR_UR   == kPos) return true;
        if (to + DIR_UL   == kPos) return true;
        break;
    case GKI:
    case GTO:
    case GNY:
    case GNK:
    case GNG:
        if (to + DIR_DOWN  == kPos) return true;
        if (to + DIR_DR    == kPos) return true;
        if (to + DIR_DL    == kPos) return true;
        if (to + DIR_RIGHT == kPos) return true;
        if (to + DIR_LEFT  == kPos) return true;
        if (to + DIR_UP    == kPos) return true;
        break;

    case SUM:
    case GUM:
        if (to + DIR_UP    == kPos) return true;
        if (to + DIR_RIGHT == kPos) return true;
        if (to + DIR_LEFT  == kPos) return true;
        if (to + DIR_DOWN  == kPos) return true;
        // Through
    case SKA:
    case GKA:
        if ((DirTbl[to][kPos] & (EFFECT_UR | EFFECT_UL | EFFECT_DR | EFFECT_DL)) != 0) {
            if ((DirTbl[to][kPos] & EFFECT_UR) != 0) {
                if (SkipOverEMP(to, DIR_UR) == kPos) return true;
            }
            if ((DirTbl[to][kPos] & EFFECT_UL) != 0) {
                if (SkipOverEMP(to, DIR_UL) == kPos) return true;
            }
            if ((DirTbl[to][kPos] & EFFECT_DR) != 0) {
                if (SkipOverEMP(to, DIR_DR) == kPos) return true;
            }
            if ((DirTbl[to][kPos] & EFFECT_DL) != 0) {
                if (SkipOverEMP(to, DIR_DL) == kPos) return true;
            }
        }
        break;
    case SRY:
    case GRY:
        if (to + DIR_UR == kPos) return true;
        if (to + DIR_UL == kPos) return true;
        if (to + DIR_DR == kPos) return true;
        if (to + DIR_DL == kPos) return true;
        // Through
    case SHI:
    case GHI:
        if ((DirTbl[to][kPos] & (EFFECT_UP | EFFECT_RIGHT | EFFECT_LEFT | EFFECT_DOWN)) != 0) {
            if ((DirTbl[to][kPos] & EFFECT_UP) != 0) {
                if (SkipOverEMP(to, DIR_UP) == kPos) return true;
            }
            if ((DirTbl[to][kPos] & EFFECT_DOWN) != 0) {
                if (SkipOverEMP(to, DIR_DOWN) == kPos) return true;
            }
            if ((DirTbl[to][kPos] & EFFECT_RIGHT) != 0) {
                if (SkipOverEMP(to, DIR_RIGHT) == kPos) return true;
            }
            if ((DirTbl[to][kPos] & EFFECT_LEFT) != 0) {
                if (SkipOverEMP(to, DIR_LEFT) == kPos) return true;
            }
        }
        break;
    case SOU:
    case GOU:
    case WALL:
    case PIECE_NONE:
    default:
        break;
    }

    // 駒が移動することによる王手.
    const int from = move_from(m);
    if (from < 0x11) return false;
    if ((DirTbl[from][kPos]) & (akiki[from] >> EFFECT_LONG_SHIFT)) {
        unsigned long id;
        _BitScanForward(&id, DirTbl[from][kPos]);
        if (from - to == NanohaTbl::Direction[id] || to - from == NanohaTbl::Direction[id]) return false;
        if (SkipOverEMP(from, NanohaTbl::Direction[id]) == kPos) {
            return true;
        }
    }

    return false;
}
bool Position::move_gives_check(Move m) const
{
    const Color us = side_to_move();
    return is_check_move(us, m);
}


// 合法手か確認する
bool Position::pl_move_is_legal(const Move m) const
{
    const Piece piece = move_piece(m);
    const Color us = side_to_move();

    // 自分の駒でない駒を動かしているか？
    if (us != color_of(piece)) return false;

    const PieceType pt = type_of(piece);
    const int to = move_to(m);
    const int from = move_from(m);
    if (from == to) return false;

    if (move_is_drop(m)) {
        // 打つ駒を持っているか？
        const Hand &h = (us == BLACK) ? handS : handG;
        if ( !h.exist(piece)) {
            return false;
        }
        if (ban[to] != EMP) {
            return false;
        }
        if (pt == FU) {
            // 二歩と打ち歩詰めのチェック
            if (is_double_pawn(us, to)) return false;
            if (is_pawn_drop_mate(us, to)) return false;
            // 行き所のない駒打ちは生成しないはずなので、チェックを省略可か？
            if (us == BLACK) return is_drop_pawn<BLACK>(to);
            if (us == WHITE) return is_drop_pawn<WHITE>(to);
        } else if (pt == KY) {
            // 行き所のない駒打ちは生成しないはずなので、チェックを省略可か？
            if (us == BLACK) return is_drop_pawn<BLACK>(to);
            if (us == WHITE) return is_drop_pawn<WHITE>(to);
        } else if (pt == KE) {
            // 行き所のない駒打ちは生成しないはずなので、チェックを省略可か？
            if (us == BLACK) return is_drop_knight<BLACK>(to);
            if (us == WHITE) return is_drop_knight<WHITE>(to);
        }
    } else {
        // 動かす駒が存在するか？
#if !defined(NDEBUG)
        if (DEBUG_LEVEL > 0) {
            std::cerr << "Color=" << int(us) << ", sideToMove=" << int(sideToMove) << std::endl;
            std::cerr << "Move : from=0x" << std::hex << from << ", to=0x" << to << std::endl;
            std::cerr << "   piece=" << int(piece) << ", cap=" << int(move_captured(m)) << std::endl;
            std::cerr << "   ban[from]=" << int(ban[from]) << ", ban[to]=" << int(ban[to]) << std::endl;
        }
#endif
        if (ban[from] != piece) {
            return false;
        }
        if (ban[to] == WALL) {
            return false;
        }
        if (ban[to] != EMP && color_of(ban[to]) == us) {
            // 自分の駒を取っている
            return false;
        }
        // 玉の場合、自殺はできない
        if (move_ptype(m) == OU) {
            Color them = flip(sideToMove);
            if (effect[them][to]) return false;
        }
        // ピンの場合、ピンの方向にしか動けない。
        if (pin[from]) {
            int kPos = (us == BLACK) ? kingS : kingG;
            if (DirTbl[kPos][to] != DirTbl[kPos][from]) return false;
        }
        // TODO:駒を飛び越えないか？
        int d = Max(abs((from >> 4)-(to >> 4)), abs((from & 0x0F)-(to&0x0F)));
        if (pt == KE) {
            if (d != 2) return false;
        } else if (d > 1) {
            // 香、角、飛、馬、龍しかない.
            // 移動の途中をチェックする
            int dir = (to - from) / d;
            if (((to - from) % d) != 0) return false;
            for (int i = 1, z = from + dir; i < d; i++, z += dir) {
                if (ban[z] != EMP) return false;
            }
        }
    }

#if 0
    // TODO:行き所のない駒、二歩、打ち歩詰め、駒を飛び越えないか？等のチェック
    // captureを盤上の駒に設定
    if (IsCorrectMove(m)) {
        if (piece == SOU || piece == GOU) return 1;

        // 自玉に王手をかけていないか、実際に動かして調べる
        Position kk(*this);
        StateInfo newSt;
        kk.do_move(m, newSt);
        if (us == BLACK && kingS && EXIST_EFFECT(kk.effectW[kingS])) {
            return false;
        }
        if (us != BLACK &&  kingG && EXIST_EFFECT(kk.effectB[kingG])) {
            return false;
        }
        return true;
    }
    return false;
#else
    return true;
#endif
}

// 指定場所(to)が打ち歩詰めになるか確認する
bool Position::is_pawn_drop_mate(const Color us, int to) const
{
    // まず、玉の頭に歩を打つ手じゃなければ打ち歩詰めの心配はない。
    if (us == BLACK) {
        if (kingG + DIR_DOWN != to) {
            return 0;
        }
    } else {
        if (kingS + DIR_UP != to) {
            return 0;
        }
    }

    Piece piece;

    // 歩を取れるか？
    if (us == BLACK) {
        // 自分の利きがないなら玉で取れる
        if (! EXIST_EFFECT(effectB[to])) return 0;

        // 取る動きを列挙してみたら玉で取る手しかない
        if ((EXIST_EFFECT(effectW[to]) & ~EFFECT_DOWN) != 0) {
            // 玉以外で取れる手がある【課題】pinの考慮
            effect_t kiki = effectW[to] & (EFFECT_SHORT_MASK & ~EFFECT_DOWN);
            unsigned long id;
            while (kiki) {
                _BitScanForward(&id, kiki);
                kiki &= (kiki - 1);
                if (pin[to - NanohaTbl::Direction[id]] == 0) return 0;
            }
            kiki = effectW[to] & EFFECT_LONG_MASK;
            while (kiki) {
                _BitScanForward(&id, kiki);
                kiki &= (kiki - 1);
                if (pin[SkipOverEMP(to, -NanohaTbl::Direction[id])] == 0) return 0;
            }
        }
        // 玉に逃げ道があるかどうかをチェック
        if (effectB[to] & ((EFFECT_LEFT|EFFECT_RIGHT|EFFECT_UR|EFFECT_UL) << EFFECT_LONG_SHIFT)) {
            if ((effectB[to] & (EFFECT_LEFT  << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_LEFT ] != WALL && (ban[to+DIR_LEFT ] & GOTE) == 0)
              && ((effectB[to+DIR_LEFT ] & ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
            if ((effectB[to] & (EFFECT_RIGHT << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_RIGHT] != WALL && (ban[to+DIR_RIGHT] & GOTE) == 0)
              && ((effectB[to+DIR_RIGHT] & ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
            if ((effectB[to] & (EFFECT_UR    << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_UR   ] != WALL && (ban[to+DIR_UR   ] & GOTE) == 0)
              && ((effectB[to+DIR_UR   ] & ~(EFFECT_UR    << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
            if ((effectB[to] & (EFFECT_UL    << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_UL   ] != WALL && (ban[to+DIR_UL   ] & GOTE) == 0)
              && ((effectB[to+DIR_UL   ] & ~(EFFECT_UL    << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
        }
#define EscapeG(dir)    piece = ban[kingG + DIR_##dir];    \
                        if (piece != WALL && !(piece & GOTE) && !EXIST_EFFECT(effectB[kingG + DIR_##dir])) return 0
        EscapeG(UP);
        EscapeG(UR);
        EscapeG(UL);
        EscapeG(RIGHT);
        EscapeG(LEFT);
        EscapeG(DR);
        EscapeG(DL);
#undef EscapeG

        // 玉の逃げ道もないのなら、打ち歩詰め。
        return 1;
    } else {
        // 自分の利きがないなら玉で取れる
        if (! EXIST_EFFECT(effectW[to])) return 0;

        // 取る動きを列挙してみたら玉で取る手しかない
        if ((EXIST_EFFECT(effectB[to]) & ~EFFECT_UP) != 0) {
            // 玉以外で取れる手がある【課題】pinの考慮
            effect_t kiki = effectB[to] & (EFFECT_SHORT_MASK & ~EFFECT_UP);
            unsigned long id;
            while (kiki) {
                _BitScanForward(&id, kiki);
                kiki &= (kiki - 1);
                if (pin[to - NanohaTbl::Direction[id]] == 0) return 0;
            }
            kiki = effectB[to] & EFFECT_LONG_MASK;
            while (kiki) {
                _BitScanForward(&id, kiki);
                kiki &= (kiki - 1);
                if (pin[SkipOverEMP(to, -NanohaTbl::Direction[id])] == 0) return 0;
            }
        }
        // 玉に逃げ道があるかどうかをチェック
        if (effectW[to] & ((EFFECT_LEFT|EFFECT_RIGHT|EFFECT_DR|EFFECT_DL) << EFFECT_LONG_SHIFT)) {
            if ((effectW[to] & (EFFECT_LEFT  << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_LEFT ] == EMP || (ban[to+DIR_LEFT ] & GOTE))
              && ((effectW[to+DIR_LEFT ] & ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
            if ((effectW[to] & (EFFECT_RIGHT << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_RIGHT] == EMP || (ban[to+DIR_RIGHT] & GOTE))
              && ((effectW[to+DIR_RIGHT] & ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
            if ((effectW[to] & (EFFECT_DR    << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_DR   ] == EMP || (ban[to+DIR_DR   ] & GOTE))
              && ((effectW[to+DIR_DR   ] & ~(EFFECT_DR    << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
            if ((effectW[to] & (EFFECT_DL    << EFFECT_LONG_SHIFT))
              && (ban[to+DIR_DL   ] == EMP || (ban[to+DIR_DL   ] & GOTE))
              && ((effectW[to+DIR_DL   ] & ~(EFFECT_DL    << EFFECT_LONG_SHIFT)) == 0)) {
                return 0;
            }
        }
#define EscapeS(dir)    piece = ban[kingS + DIR_##dir];    \
                        if ((piece == EMP || (piece & GOTE)) && !EXIST_EFFECT(effectW[kingS + DIR_##dir])) return 0
        EscapeS(DOWN);
        EscapeS(DR);
        EscapeS(DL);
        EscapeS(RIGHT);
        EscapeS(LEFT);
        EscapeS(UR);
        EscapeS(UL);
#undef EscapeG

        // 玉の逃げ道もないのなら、打ち歩詰め。
        return 1;
    }
}

// 機能：勝ち宣言できるかどうか判定する
//
// 引数：手番
//
// 戻り値
//   true：勝ち宣言できる
//   false：勝ち宣言できない
//
bool Position::IsKachi(const Color us) const
{
    // 入玉宣言勝ちの条件について下に示す。
    // --
    // (a) 宣言側の手番である。
    // (b) 宣言側の玉が敵陣三段目以内に入っている。
    // (c) 宣言側が(大駒5点小駒1点の計算で)
    //   先手の場合28点以上の持点がある。
    //   後手の場合27点以上の持点がある。
    //   点数の対象となるのは、宣言側の持駒と敵陣三段目以内に存在する玉を除く宣言側の駒のみである
    // (d) 宣言側の敵陣三段目以内の駒は、玉を除いて10枚以上存在する。
    // (e) 宣言側の玉に王手がかかっていない。(詰めろや必死であることは関係ない)
    // (f) 宣言側の持ち時間が残っている。(切れ負けの場合)

    int suji;
    int dan;
    int maisuu = 0;
    unsigned int point = 0;
    // 条件(a)
    if (us == BLACK) {
        // 条件(b) 判定
        if ((kingS & 0x0F) > 3) return false;
        // 条件(e)
        if (EXIST_EFFECT(effectW[kingS])) return false;
        // 条件(c)(d) 判定
        for (suji = 0x10; suji <= 0x90; suji += 0x10) {
            for (dan = 1; dan <= 3; dan++) {
                Piece piece = Piece(ban[suji+dan] & ~PROMOTED);        // 玉は0になるのでカウント外
                if (piece != EMP && !(piece & GOTE)) {
                    if (piece == SHI || piece == SKA) point += 5;
                    else point++;
                    maisuu++;
                }
            }
        }
        // 条件(d) 判定
        if (maisuu < 10) return false;
        point += handS.getFU() + handS.getKY() + handS.getKE() + handS.getGI() + handS.getKI();
        point += 5 * handS.getKA();
        point += 5 * handS.getHI();
        // 条件(c) 判定 (先手28点以上、後手27点以上)
        if (point < 28) return false;
    } else {
        // 条件(b) 判定
        if ((kingG & 0x0F) < 7) return false;
        // 条件(e)
        if (EXIST_EFFECT(effectB[kingG])) return false;
        // 条件(c)(d) 判定
        for (suji = 0x10; suji <= 0x90; suji += 0x10) {
            for (dan = 7; dan <= 9; dan++) {
                Piece piece = Piece(ban[suji+dan] & ~PROMOTED);
                if (piece == (GOU & ~PROMOTED)) continue;
                if (piece & GOTE) {
                    if (piece == GHI || piece == GKA) point += 5;
                    else point++;
                    maisuu++;
                }
            }
        }
        // 条件(d) 判定
        if (maisuu < 10) return false;
        point += handG.getFU() + handG.getKY() + handG.getKE() + handG.getGI() + handG.getKI();
        point += 5 * handG.getKA();
        point += 5 * handG.getHI();
        // 条件(c) 判定 (先手28点以上、後手27点以上)
        if (point < 27) return false;
    }

    return true;
}

namespace {
//
//  ハフマン化
//           盤上(6 + α)  持駒(5 + β)
//            α(S/G + Promoted)、β(S/G)
//    空     xxxxx0 + 0    (none)
//    歩     xxxx01 + 2    xxxx0 + 1
//    香     xx0011 + 2    xx001 + 1
//    桂     xx1011 + 2    xx101 + 1
//    銀     xx0111 + 2    xx011 + 1
//    金     x01111 + 1    x0111 + 1
//    角     011111 + 2    01111 + 1
//    飛     111111 + 2    11111 + 1
//
static const struct HuffmanBoardTBL {
    int code;
    int bits;
} HB_tbl[] = {
    {0x00, 1},    // EMP
    {0x01, 4},    // SFU
    {0x03, 6},    // SKY
    {0x0B, 6},    // SKE
    {0x07, 6},    // SGI
    {0x0F, 6},    // SKI
    {0x1F, 8},    // SKA
    {0x3F, 8},    // SHI
    {0x00, 0},    // SOU
    {0x05, 4},    // STO
    {0x13, 6},    // SNY
    {0x1B, 6},    // SNK
    {0x17, 6},    // SNG
    {0x00,-1},    // ---
    {0x5F, 8},    // SUM
    {0x7F, 8},    // SRY
    {0x00,-1},    // ---
    {0x09, 4},    // GFU
    {0x23, 6},    // GKY
    {0x2B, 6},    // GKE
    {0x27, 6},    // GGI
    {0x2F, 6},    // GKI
    {0x9F, 8},    // GKA
    {0xBF, 8},    // GHI
    {0x00, 0},    // GOU
    {0x0D, 4},    // GTO    // 間違い
    {0x33, 6},    // GNY
    {0x3B, 6},    // GNK
    {0x37, 6},    // GNG
    {0x00,-1},    // ---
    {0xDF, 8},    // GUM
    {0xFF, 8},    // GRY
    {0x00,-1},    // ---
};

static const struct HuffmanHandTBL {
    int code;
    int bits;
} HH_tbl[] = {
    {0x00,-1},    // EMP
    {0x00, 2},    // SFU
    {0x01, 4},    // SKY
    {0x05, 4},    // SKE
    {0x03, 4},    // SGI
    {0x07, 5},    // SKI
    {0x0F, 6},    // SKA
    {0x1F, 6},    // SHI
    {0x00,-1},    // SOU
    {0x00,-1},    // STO
    {0x00,-1},    // SNY
    {0x00,-1},    // SNK
    {0x00,-1},    // SNG
    {0x00,-1},    // ---
    {0x00,-1},    // SUM
    {0x00,-1},    // SRY
    {0x00,-1},    // ---
    {0x02, 2},    // GFU
    {0x09, 4},    // GKY
    {0x0D, 4},    // GKE
    {0x0B, 4},    // GGI
    {0x17, 5},    // GKI
    {0x2F, 6},    // GKA
    {0x3F, 6},    // GHI
    {0x00,-1},    // GOU
    {0x00,-1},    // GTO
    {0x00,-1},    // GNY
    {0x00,-1},    // GNK
    {0x00,-1},    // GNG
    {0x00,-1},    // ---
    {0x00,-1},    // GUM
    {0x00,-1},    // GRY
    {0x00,-1},    // ---
};

//
// 引数
//   const int start_bit;    // 記録開始bit位置
//   const int bits;        // ビット数(幅)
//   const int data;        // 記録するデータ
//   unsigned char buf[];    // 符号化したデータを記録するバッファ
//   const int size;        // バッファサイズ
//
// 戻り値
//   取り出したデータ
//
int set_bit(const int start_bit, const int bits, const int data, unsigned char buf[], const int size)
{
    static const int mask_tbl[] = {
        0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF
    };
    if (start_bit < 0) return -1;
    if (bits <= 0 || bits > 8) return -1;
    if (start_bit + bits > 8*size) return -2;
    if ((data & mask_tbl[bits]) != data) return -3;

    const int n = start_bit / 8;
    const int shift = start_bit % 8;
    buf[n] |= (data << shift);
    if (shift + bits > 8) {
        buf[n+1] = (data >> (8 - shift));
    }
    return start_bit + bits;
}
}

// 機能：局面をハフマン符号化する(定跡ルーチン用)
//
// 引数
//   const int SorG;        // 手番
//   unsigned char buf[];    // 符号化したデータを記録するバッファ
//
// 戻り値
//   マイナス：エラー
//   正の値：エンコードしたときのビット数
//
int Position::EncodeHuffman(unsigned char buf[32]) const
{
    const int KingS = (((kingS >> 4)-1) & 0x0F)*9+(kingS & 0x0F);
    const int KingG = (((kingG >> 4)-1) & 0x0F)*9+(kingG & 0x0F);
    const int size = 32;    // buf[] のサイズ

    int start_bit = 0;

    if (kingS == 0 || kingG == 0) {
        // Error!
        return -1;
    }
///    printf("KingS=%d\n", KingS);
///    printf("KingG=%d\n", KingG);

    memset(buf, 0, size);

    // 手番を符号化
    start_bit = set_bit(start_bit, 1, side_to_move(), buf, size);
    // 玉の位置を符号化
    start_bit = set_bit(start_bit, 7, KingS,               buf, size);
    start_bit = set_bit(start_bit, 7, KingG,               buf, size);

    // 盤上のデータを符号化
    int suji, dan;
    int piece;
    for (suji = 0x10; suji <= 0x90; suji += 0x10) {
        for (dan = 1; dan <= 9; dan++) {
            piece = ban[suji + dan];
            if (piece < EMP || piece > GRY) {
                // Error!
                exit(1);
            }
            if (HB_tbl[piece].bits < 0) {
                // Error!
                exit(1);
            }
            if (HB_tbl[piece].bits == 0) {
                // 玉は別途
                continue;
            }
            start_bit = set_bit(start_bit, HB_tbl[piece].bits, HB_tbl[piece].code, buf, size);
        }
    }

    // 持駒を符号化
    unsigned int i, n;
#define EncodeHand(SG,KOMA)            \
        piece = SG ## KOMA;            \
        n = hand ## SG.get ## KOMA();    \
        for (i = 0; i < n; i++) {    \
            start_bit = set_bit(start_bit, HH_tbl[piece].bits, HH_tbl[piece].code, buf, size);    \
        }

    EncodeHand(G,HI)
    EncodeHand(G,KA)
    EncodeHand(G,KI)
    EncodeHand(G,GI)
    EncodeHand(G,KE)
    EncodeHand(G,KY)
    EncodeHand(G,FU)

    EncodeHand(S,HI)
    EncodeHand(S,KA)
    EncodeHand(S,KI)
    EncodeHand(S,GI)
    EncodeHand(S,KE)
    EncodeHand(S,KY)
    EncodeHand(S,FU)

#undef EncodeHand

    return start_bit;
}


