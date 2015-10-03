/*
  NanohaMini, a USI shogi(japanese-chess) playing engine derived from Stockfish 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2010 Marco Costalba, Joona Kiiski, Tord Romstad (Stockfish author)
  Copyright (C) 2014 Kazuyuki Kawabata

  NanohaMini is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  NanohaMini is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <cstdio>

#include "position.h"
#include "evaluate.h"

// 評価関数関連定義
#if defined(EVAL_MICRO)
#include "param_micro.h"
#else
//Aperyの評価値
#include "param_new.h"
#define FV_KPP "KPP_synthesized.bin" 
#define FV_KKP "KKP_synthesized.bin" 
#define FV_KK "KK_synthesized.bin" 
#endif

#define NLIST    38

#define FV_SCALE                32

#define MATERIAL            (this->material)

#define SQ_BKING            NanohaTbl::z2sq[kingS]
#define SQ_WKING            NanohaTbl::z2sq[kingG]
#define HAND_B              (this->hand[BLACK].h)
#define HAND_W              (this->hand[WHITE].h)

#define Inv(sq)             (nsquare-1-sq)

//KPP
//#define PcPcOnSq(k,i,j)     pc_on_sq[k][(i)*((i)+1)/2+(j)]

#define I2HandPawn(hand)    (((hand) & HAND_FU_MASK) >> HAND_FU_SHIFT)
#define I2HandLance(hand)   (((hand) & HAND_KY_MASK) >> HAND_KY_SHIFT)
#define I2HandKnight(hand)  (((hand) & HAND_KE_MASK) >> HAND_KE_SHIFT)
#define I2HandSilver(hand)  (((hand) & HAND_GI_MASK) >> HAND_GI_SHIFT)
#define I2HandGold(hand)    (((hand) & HAND_KI_MASK) >> HAND_KI_SHIFT)
#define I2HandBishop(hand)  (((hand) & HAND_KA_MASK) >> HAND_KA_SHIFT)
#define I2HandRook(hand)    (((hand) & HAND_HI_MASK) >> HAND_HI_SHIFT)

enum {
    promote = 8, EMPTY = 0,    /* VC++でemptyがぶつかるので変更 */
    pawn, lance, knight, silver, gold, bishop, rook, king, pro_pawn,
    pro_lance, pro_knight, pro_silver, piece_null, horse, dragon
};

enum {/* nhand = 7, nfile = 9,  nrank = 9, */ nsquare = 81 };

#if !defined(EVAL_MICRO)
enum {
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
#endif

namespace
{
#if !defined(EVAL_MICRO)
    short kpp3[nsquare][fe_end][fe_end];
    long kkp[nsquare][nsquare][fe_end];
    long kk[nsquare][nsquare];

    const int handB[14] = { f_hand_pawn, e_hand_pawn, f_hand_lance, e_hand_lance,
        f_hand_knight, e_hand_knight, f_hand_silver, e_hand_silver,
        f_hand_gold, e_hand_gold, f_hand_bishop, e_hand_bishop, f_hand_rook, e_hand_rook };

    const int handW[14] = { e_hand_pawn, f_hand_pawn, e_hand_lance, f_hand_lance,
        e_hand_knight, f_hand_knight, e_hand_silver, f_hand_silver,
        e_hand_gold, f_hand_gold, e_hand_bishop, f_hand_bishop, e_hand_rook, f_hand_rook };
#endif
}

namespace NanohaTbl {
    const short z2sq[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, 72, 73, 74, 75, 76, 77, 78, 79, 80, -1, -1, -1, -1, -1, -1,
        -1, 63, 64, 65, 66, 67, 68, 69, 70, 71, -1, -1, -1, -1, -1, -1,
        -1, 54, 55, 56, 57, 58, 59, 60, 61, 62, -1, -1, -1, -1, -1, -1,
        -1, 45, 46, 47, 48, 49, 50, 51, 52, 53, -1, -1, -1, -1, -1, -1,
        -1, 36, 37, 38, 39, 40, 41, 42, 43, 44, -1, -1, -1, -1, -1, -1,
        -1, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, -1,
        -1, 18, 19, 20, 21, 22, 23, 24, 25, 26, -1, -1, -1, -1, -1, -1,
        -1, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, -1, -1, -1, -1, -1,
    };
}


void Position::init_evaluate()
{
#if !defined(EVAL_MICRO)
    FILE *fp = NULL;
    int iret = 0;
    size_t size;

    //KPP
    do {
        fp = fopen(FV_KPP, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * fe_end * fe_end;
        if (fread(kpp3, sizeof(short), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }

    } while (0);
    if (fp) fclose(fp);

    //KKP
    do {
        fp = fopen(FV_KKP, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * nsquare * fe_end;
        if (fread(kkp, sizeof(long), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }

    } while (0);
    if (fp) fclose(fp);

    //KK
    do {
        fp = fopen(FV_KK, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * nsquare;
        if (fread(kk, sizeof(long), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }

    } while (0);
    if (fp) fclose(fp);

    /* ToDo.動かなくなる
    if (iret < 0) {
    // 評価用ファイルが読めない場合は起動させない。
    std::cerr << "Can't load '" << fname << "'." << std::endl;
    exit(1);

    #if defined(CSADLL) || defined(CSA_DIRECT)
    ::MessageBox(NULL, "評価ベクトルがロードできません\n終了します", "Error!", MB_OK);
    exit(1);
    #endif
    }
    */

#endif

}

int Position::compute_material() const
{
    int v, item, itemp;
    int i;

    item = 0;
    itemp = 0;
    for (i = KNS_FU; i <= KNE_FU; i++) {
        if (knkind[i] == SFU) item++;
        if (knkind[i] == GFU) item--;
        if (knkind[i] == STO) itemp++;
        if (knkind[i] == GTO) itemp--;
    }
    v = item  * DPawn;
    v += itemp * DProPawn;

    item = 0;
    itemp = 0;
    for (i = KNS_KY; i <= KNE_KY; i++) {
        if (knkind[i] == SKY) item++;
        if (knkind[i] == GKY) item--;
        if (knkind[i] == SNY) itemp++;
        if (knkind[i] == GNY) itemp--;
    }
    v += item  * DLance;
    v += itemp * DProLance;

    item = 0;
    itemp = 0;
    for (i = KNS_KE; i <= KNE_KE; i++) {
        if (knkind[i] == SKE) item++;
        if (knkind[i] == GKE) item--;
        if (knkind[i] == SNK) itemp++;
        if (knkind[i] == GNK) itemp--;
    }
    v += item  * DKnight;
    v += itemp * DProKnight;

    item = 0;
    itemp = 0;
    for (i = KNS_GI; i <= KNE_GI; i++) {
        if (knkind[i] == SGI) item++;
        if (knkind[i] == GGI) item--;
        if (knkind[i] == SNG) itemp++;
        if (knkind[i] == GNG) itemp--;
    }
    v += item  * DSilver;
    v += itemp * DProSilver;

    item = 0;
    for (i = KNS_KI; i <= KNE_KI; i++) {
        if (knkind[i] == SKI) item++;
        if (knkind[i] == GKI) item--;
    }
    v += item  * DGold;

    item = 0;
    itemp = 0;
    for (i = KNS_KA; i <= KNE_KA; i++) {
        if (knkind[i] == SKA) item++;
        if (knkind[i] == GKA) item--;
        if (knkind[i] == SUM) itemp++;
        if (knkind[i] == GUM) itemp--;
    }
    v += item  * DBishop;
    v += itemp * DHorse;

    item = 0;
    itemp = 0;
    for (i = KNS_HI; i <= KNE_HI; i++) {
        if (knkind[i] == SHI) item++;
        if (knkind[i] == GHI) item--;
        if (knkind[i] == SRY) itemp++;
        if (knkind[i] == GRY) itemp--;
    }
    v += item  * DRook;
    v += itemp * DDragon;

    return v;
}

#if !defined(EVAL_MICRO)
int Position::make_list(int * pscore, int list0[NLIST], int list1[NLIST]) const
{
    int nlist = 0;
    int Hand[14];
    Hand[0] = I2HandPawn(HAND_B);
    Hand[1] = I2HandPawn(HAND_W);
    Hand[2] = I2HandLance(HAND_B);
    Hand[3] = I2HandLance(HAND_W);
    Hand[4] = I2HandKnight(HAND_B);
    Hand[5] = I2HandKnight(HAND_W);
    Hand[6] = I2HandSilver(HAND_B);
    Hand[7] = I2HandSilver(HAND_W);
    Hand[8] = I2HandGold(HAND_B);
    Hand[9] = I2HandGold(HAND_W);
    Hand[10] = I2HandBishop(HAND_B);
    Hand[11] = I2HandBishop(HAND_W);
    Hand[12] = I2HandRook(HAND_B);
    Hand[13] = I2HandRook(HAND_W);

    for (int i = 0; i < 14; i++){
        const int j = Hand[i];
        for (int k = 1; k <= j; k++){
            list0[nlist] = handB[i] + k;
            list1[nlist++] = handW[i] + k;
        }
    }

    for (int y = RANK_1; y <= RANK_9; y++) {
        for (int x = FILE_1; x <= FILE_9; x++) {
            const int z = (x << 4) + y;
            const int sq = NanohaTbl::z2sq[z];
            switch (ban[z]) {

            case SFU:
                list0[nlist] = f_pawn + sq;
                list1[nlist++] = e_pawn + Inv(sq);
                break;
            case GFU:
                list0[nlist] = e_pawn + sq;
                list1[nlist++] = f_pawn + Inv(sq);
                break;
            case SKY:
                list0[nlist] = f_lance + sq;
                list1[nlist++] = e_lance + Inv(sq);
                break;
            case GKY:
                list0[nlist] = e_lance + sq;
                list1[nlist++] = f_lance + Inv(sq);
                break;
            case SKE:
                list0[nlist] = f_knight + sq;
                list1[nlist++] = e_knight + Inv(sq);
                break;
            case GKE:
                list0[nlist] = e_knight + sq;
                list1[nlist++] = f_knight + Inv(sq);
                break;
            case SGI:
                list0[nlist] = f_silver + sq;
                list1[nlist++] = e_silver + Inv(sq);
                break;
            case GGI:
                list0[nlist] = e_silver + sq;
                list1[nlist++] = f_silver + Inv(sq);
                break;
            case SKI:
            case STO:
            case SNY:
            case SNK:
            case SNG:
                list0[nlist] = f_gold + sq;
                list1[nlist++] = e_gold + Inv(sq);
                break;
            case GKI:
            case GTO:
            case GNY:
            case GNK:
            case GNG:
                list0[nlist] = e_gold + sq;
                list1[nlist++] = f_gold + Inv(sq);
                break;
            case SKA:
                list0[nlist] = f_bishop + sq;
                list1[nlist++] = e_bishop + Inv(sq);
                break;
            case GKA:
                list0[nlist] = e_bishop + sq;
                list1[nlist++] = f_bishop + Inv(sq);
                break;
            case SUM:
                list0[nlist] = f_horse + sq;
                list1[nlist++] = e_horse + Inv(sq);
                break;
            case GUM:
                list0[nlist] = e_horse + sq;
                list1[nlist++] = f_horse + Inv(sq);
                break;
            case SHI:
                list0[nlist] = f_rook + sq;
                list1[nlist++] = e_rook + Inv(sq);
                break;
            case GHI:
                list0[nlist] = e_rook + sq;
                list1[nlist++] = f_rook + Inv(sq);
                break;
            case SRY:
                list0[nlist] = f_dragon + sq;
                list1[nlist++] = e_dragon + Inv(sq);
                break;
            case GRY:
                list0[nlist] = e_dragon + sq;
                list1[nlist++] = f_dragon + Inv(sq);
                break;
            case EMP:
            case WALL:
            case SOU:
            case GOU:
            case PIECE_NONE:
            default:
                ;
            }
        }
    }
    assert(nlist <= NLIST);
    return nlist;
}
#endif

int Position::evaluate(const Color us) const
{
#if !defined(EVAL_MICRO)
    int score = 0;
    int list0[NLIST];
    int list1[NLIST];
    int nlist = make_list(&score, list0, list1);

    const int sq_bk = SQ_BKING;
    const int sq_wk = SQ_WKING;

    const auto* ppkppb = kpp3[sq_bk];
    const auto* ppkppw = kpp3[Inv(sq_wk)];

    score = kk[sq_bk][sq_wk];
    for (int i = 0; i < nlist; i++){
        const int k0 = list0[i];
        const int k1 = list1[i];
        const short* pkppb = ppkppb[k0];
        const short* pkppw = ppkppw[k1];
        for (int j = 0; j < i; j++){
            const int l0 = list0[j];
            const int l1 = list1[j];
            score += pkppb[l0];
            score -= pkppw[l1];
        }
        score += kkp[sq_bk][sq_wk][k0];
    }

    score /= FV_SCALE;
    score += MATERIAL;
    score = (us == BLACK) ? score : -score;
    return score;

#else //MICRO
    return (us == BLACK) ? MATERIAL : -(MATERIAL);
#endif
}

Value evaluate(const Position& pos, Value& margin)
{
    margin = VALUE_ZERO;
    const Color us = pos.side_to_move();
    return Value(pos.evaluate(us));
}
