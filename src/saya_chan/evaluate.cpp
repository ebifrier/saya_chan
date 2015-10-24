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
// Aperyの評価値
#include "param_new.h"
#define FV_KPP "KPP_synthesized.bin" 
#define FV_KKP "KKP_synthesized.bin" 
#define FV_KK  "KK_synthesized.bin" 
#endif

#define FV_SCALE            32
#define MATERIAL            (this->material)

#define SQ_BKING            NanohaTbl::z2sq[kingS]
#define SQ_WKING            NanohaTbl::z2sq[kingG]
#define HAND_B              (this->hand[BLACK].h)
#define HAND_W              (this->hand[WHITE].h)

#define Inv(sq)             (nsquare-1-sq)

#define I2HandPawn(hand)    (((hand) & HAND_FU_MASK) >> HAND_FU_SHIFT)
#define I2HandLance(hand)   (((hand) & HAND_KY_MASK) >> HAND_KY_SHIFT)
#define I2HandKnight(hand)  (((hand) & HAND_KE_MASK) >> HAND_KE_SHIFT)
#define I2HandSilver(hand)  (((hand) & HAND_GI_MASK) >> HAND_GI_SHIFT)
#define I2HandGold(hand)    (((hand) & HAND_KI_MASK) >> HAND_KI_SHIFT)
#define I2HandBishop(hand)  (((hand) & HAND_KA_MASK) >> HAND_KA_SHIFT)
#define I2HandRook(hand)    (((hand) & HAND_HI_MASK) >> HAND_HI_SHIFT)

typedef short kkp_entry[fe_end];
short kpp3[nsquare][fe_end][fe_end];
long kkp[nsquare][nsquare][fe_end];
long kk[nsquare][nsquare];

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
        -1,  9, 10, 11, 12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, -1, -1, -1, -1,
    };

    const KPP KppIndex0[32] = {
        none, f_pawn, f_lance, f_knight, f_silver, f_gold, f_bishop, f_rook,
        none, f_gold, f_gold, f_gold, f_gold, none, f_horse, f_dragon,
        none, e_pawn, e_lance, e_knight, e_silver, e_gold, e_bishop, e_rook,
        none, e_gold, e_gold, e_gold, e_gold, none, e_horse, e_dragon
    };

    const KPP KppIndex1[32] = {
        none, e_pawn, e_lance, e_knight, e_silver, e_gold, e_bishop, e_rook,
        none, e_gold, e_gold, e_gold, e_gold, none, e_horse, e_dragon,
        none, f_pawn, f_lance, f_knight, f_silver, f_gold, f_bishop, f_rook,
        none, f_gold, f_gold, f_gold, f_gold, none, f_horse, f_dragon
    };

    const KPP HandIndex0[32] = {
        none, f_hand_pawn, f_hand_lance, f_hand_knight,
        f_hand_silver, f_hand_gold, f_hand_bishop, f_hand_rook,
        none, none, none, none, none, none, none, none,
        none, e_hand_pawn, e_hand_lance, e_hand_knight,
        e_hand_silver, e_hand_gold, e_hand_bishop, e_hand_rook,
        none, none, none, none, none, none, none, none
    };

    const KPP HandIndex1[32] = {
        none, e_hand_pawn, e_hand_lance, e_hand_knight,
        e_hand_silver, e_hand_gold, e_hand_bishop, e_hand_rook,
        none, none, none, none, none, none, none, none,
        none, f_hand_pawn, f_hand_lance, f_hand_knight,
        f_hand_silver, f_hand_gold, f_hand_bishop, f_hand_rook,
        none, none, none, none, none, none, none, none
    };
}

namespace {
    static const int handB[14] = {
        f_hand_pawn, e_hand_pawn, f_hand_lance, e_hand_lance,
        f_hand_knight, e_hand_knight, f_hand_silver, e_hand_silver,
        f_hand_gold, e_hand_gold, f_hand_bishop, e_hand_bishop, f_hand_rook, e_hand_rook
    };

    static const int handW[14] = {
        e_hand_pawn, f_hand_pawn, e_hand_lance, f_hand_lance,
        e_hand_knight, f_hand_knight, e_hand_silver, f_hand_silver,
        e_hand_gold, f_hand_gold, e_hand_bishop, f_hand_bishop, e_hand_rook, f_hand_rook
    };
}

void Position::init_evaluate()
{
	FILE *fp = NULL;
	size_t size;
    int iret = 0;

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

	if (iret < 0) {
		std::cerr << "Can't load '*_synthesized' file." << std::endl;
        exit(-1);
	}
}

int Position::compute_material() const
{
	int v, item, itemp;
	int i;

	item  = 0;
	itemp = 0;
	for (i = KNS_FU; i <= KNE_FU; i++) {
		if (knkind[i] == SFU) item++;
		if (knkind[i] == GFU) item--;
		if (knkind[i] == STO) itemp++;
		if (knkind[i] == GTO) itemp--;
	}
	v = item  * DPawn;
	v += itemp * DProPawn;

	item  = 0;
	itemp = 0;
	for (i = KNS_KY; i <= KNE_KY; i++) {
		if (knkind[i] == SKY) item++;
		if (knkind[i] == GKY) item--;
		if (knkind[i] == SNY) itemp++;
		if (knkind[i] == GNY) itemp--;
	}
	v += item  * DLance;
	v += itemp * DProLance;

	item  = 0;
	itemp = 0;
	for (i = KNS_KE; i <= KNE_KE; i++) {
		if (knkind[i] == SKE) item++;
		if (knkind[i] == GKE) item--;
		if (knkind[i] == SNK) itemp++;
		if (knkind[i] == GNK) itemp--;
	}
	v += item  * DKnight;
	v += itemp * DProKnight;

	item  = 0;
	itemp = 0;
	for (i = KNS_GI; i <= KNE_GI; i++) {
		if (knkind[i] == SGI) item++;
		if (knkind[i] == GGI) item--;
		if (knkind[i] == SNG) itemp++;
		if (knkind[i] == GNG) itemp--;
	}
	v += item  * DSilver;
	v += itemp * DProSilver;

	item  = 0;
	for (i = KNS_KI; i <= KNE_KI; i++) {
		if (knkind[i] == SKI) item++;
		if (knkind[i] == GKI) item--;
	}
	v += item  * DGold;

	item  = 0;
	itemp = 0;
	for (i = KNS_KA; i <= KNE_KA; i++) {
		if (knkind[i] == SKA) item++;
		if (knkind[i] == GKA) item--;
		if (knkind[i] == SUM) itemp++;
		if (knkind[i] == GUM) itemp--;
	}
	v += item  * DBishop;
	v += itemp * DHorse;

	item  = 0;
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

int Position::make_list_correct(int list0[PIECENUMBER_MAX + 1],
                                int list1[PIECENUMBER_MAX + 1]) const
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

    return nlist;
}

// 評価関数が正しいかどうかを判定するのに使う
int Position::evaluate_correct(const Color us) const
{
    int list0[PIECENUMBER_MAX + 1]; //駒番号numのlist0
    int list1[PIECENUMBER_MAX + 1]; //駒番号numのlist1
    int nlist = make_list_correct(list0, list1);

    const int sq_bk = SQ_BKING;
    const int sq_wk = SQ_WKING;

    const kkp_entry* ppkppb = kpp3[sq_bk];
    const kkp_entry* ppkppw = kpp3[Inv(sq_wk)];

    int score = kk[sq_bk][sq_wk];
    for (int kn = 0; kn < nlist; kn++){
        const int k0 = list0[kn];
        const int k1 = list1[kn];
        const short* pkppb = ppkppb[k0];
        const short* pkppw = ppkppw[k1];
        for (int j = 0; j < kn; j++){
            score += pkppb[list0[j]];
            score -= pkppw[list1[j]];
        }
        score += kkp[sq_bk][sq_wk][k0];
    }

    score /= FV_SCALE;
    score += MATERIAL;
    score = (us == BLACK) ? score : -score;
    return score;
}

int Position::make_list(int list0[NLIST], int list1[NLIST])
{
    /* 0:歩 1:香 2:桂 3:銀 4:金 5:角 6:飛車 */
    int SHandCount[7] = {}, GHandCount[7] = {};
    int nlist = 0;

    for (int kn = KNE_FU; kn >= KNS_HI; --kn) {
        int Kkind = knkind[kn];
        int Kpos = knpos[kn];   // 駒の位置(1:先手持駒, 2:後手持駒)

        /* (盤上は0x11から0x99のため右に2ビットシフトして0になるのは持ち駒 */
        /* 盤上 */
        if (Kpos >> 2){
            const int sq = conv_z2sq(Kpos);
            switch (Kkind) {
            case SFU:
                list0[nlist] = f_pawn + sq;
                list1[nlist] = e_pawn + Inv(sq);
                ++nlist;
                break;
            case GFU:
                list0[nlist] = e_pawn + sq;
                list1[nlist] = f_pawn + Inv(sq);
                ++nlist;
                break;
            case STO:
                list0[nlist] = f_gold + sq;
                list1[nlist] = e_gold + Inv(sq);
                ++nlist;
                break;
            case GTO:
                list0[nlist] = e_gold + sq;
                list1[nlist] = f_gold + Inv(sq);
                ++nlist;
                break;
            case SKY:
                list0[nlist] = f_lance + sq;
                list1[nlist] = e_lance + Inv(sq);
                ++nlist;
                break;
            case GKY:
                list0[nlist] = e_lance + sq;
                list1[nlist] = f_lance + Inv(sq);
                ++nlist;
                break;
            case SNY:
                list0[nlist] = f_gold + sq;
                list1[nlist] = e_gold + Inv(sq);
                ++nlist;
                break;
            case GNY:
                list0[nlist] = e_gold + sq;
                list1[nlist] = f_gold + Inv(sq);
                ++nlist;
                break;
            case SKE:
                list0[nlist] = f_knight + sq;
                list1[nlist] = e_knight + Inv(sq);
                ++nlist;
                break;
            case GKE:
                list0[nlist] = e_knight + sq;
                list1[nlist] = f_knight + Inv(sq);
                ++nlist;
                break;
            case SNK:
                list0[nlist] = f_gold + sq;
                list1[nlist] = e_gold + Inv(sq);
                ++nlist;
                break;
            case GNK:
                list0[nlist] = e_gold + sq;
                list1[nlist] = f_gold + Inv(sq);
                ++nlist;
                break;
            case SGI:
                list0[nlist] = f_silver + sq;
                list1[nlist] = e_silver + Inv(sq);
                ++nlist;
                break;
            case GGI:
                list0[nlist] = e_silver + sq;
                list1[nlist] = f_silver + Inv(sq);
                ++nlist;
                break;
            case SNG:
                list0[nlist] = f_gold + sq;
                list1[nlist] = e_gold + Inv(sq);
                ++nlist;
                break;
            case GNG:
                list0[nlist] = e_gold + sq;
                list1[nlist] = f_gold + Inv(sq);
                ++nlist;
                break;
            case SKI:
                list0[nlist] = f_gold + sq;
                list1[nlist] = e_gold + Inv(sq);
                ++nlist;
                break;
            case GKI:
                list0[nlist] = e_gold + sq;
                list1[nlist] = f_gold + Inv(sq);
                ++nlist;
                break;
            case SKA:
                list0[nlist] = f_bishop + sq;
                list1[nlist] = e_bishop + Inv(sq);
                ++nlist;
                break;
            case GKA:
                list0[nlist] = e_bishop + sq;
                list1[nlist] = f_bishop + Inv(sq);
                ++nlist;
                break;
            case SUM:
                list0[nlist] = f_horse + sq;
                list1[nlist] = e_horse + Inv(sq);
                ++nlist;
                break;
            case GUM:
                list0[nlist] = e_horse + sq;
                list1[nlist] = f_horse + Inv(sq);
                ++nlist;
                break;
            case SHI:
                list0[nlist] = f_rook + sq;
                list1[nlist] = e_rook + Inv(sq);
                ++nlist;
                break;
            case GHI:
                list0[nlist] = e_rook + sq;
                list1[nlist] = f_rook + Inv(sq);
                ++nlist;
                break;
            case SRY:
                list0[nlist] = f_dragon + sq;
                list1[nlist] = e_dragon + Inv(sq);
                ++nlist;
                break;
            case GRY:
                list0[nlist] = e_dragon + sq;
                list1[nlist] = f_dragon + Inv(sq);
                ++nlist;
                break;
            default:
                __assume(false);
                break;
            }
        }
        else if (Kpos == 1) { // 先手持駒
            switch (Kkind) {
            case SFU:
                ++SHandCount[0];
                list0[nlist] = f_hand_pawn + SHandCount[0];
                list1[nlist] = e_hand_pawn + SHandCount[0];
                ++nlist;
                break;
            case SKY:
                ++SHandCount[1];
                list0[nlist] = f_hand_lance + SHandCount[1];
                list1[nlist] = e_hand_lance + SHandCount[1];
                ++nlist;
                break;
            case SKE:
                ++SHandCount[2];
                list0[nlist] = f_hand_knight + SHandCount[2];
                list1[nlist] = e_hand_knight + SHandCount[2];
                ++nlist;
                break;
            case SGI:
                ++SHandCount[3];
                list0[nlist] = f_hand_silver + SHandCount[3];
                list1[nlist] = e_hand_silver + SHandCount[3];
                ++nlist;
                break;
            case SKI:
                ++SHandCount[4];
                list0[nlist] = f_hand_gold + SHandCount[4];
                list1[nlist] = e_hand_gold + SHandCount[4];
                ++nlist;
                break;
            case SKA:
                ++SHandCount[5];
                list0[nlist] = f_hand_bishop + SHandCount[5];
                list1[nlist] = e_hand_bishop + SHandCount[5];
                ++nlist;
                break;
            case SHI:
                ++SHandCount[6];
                list0[nlist] = f_hand_rook + SHandCount[6];
                list1[nlist] = e_hand_rook + SHandCount[6];
                ++nlist;
                break;
            default:
                __assume(false); /* 心持ち高速化 */
                break;
            }
        }
        else /*(Kpos == 2)*/ {  // 後手持駒
            switch (Kkind) {
            case GFU:
                ++GHandCount[0];
                list0[nlist] = e_hand_pawn + GHandCount[0];
                list1[nlist] = f_hand_pawn + GHandCount[0];
                ++nlist;
                break;
            case GKY:
                ++GHandCount[1];
                list0[nlist] = e_hand_lance + GHandCount[1];
                list1[nlist] = f_hand_lance + GHandCount[1];
                ++nlist;
                break;
            case GKE:
                ++GHandCount[2];
                list0[nlist] = e_hand_knight + GHandCount[2];
                list1[nlist] = f_hand_knight + GHandCount[2];
                ++nlist;
                break;
            case GGI:
                ++GHandCount[3];
                list0[nlist] = e_hand_silver + GHandCount[3];
                list1[nlist] = f_hand_silver + GHandCount[3];
                ++nlist;
                break;
            case GKI:
                ++GHandCount[4];
                list0[nlist] = e_hand_gold + GHandCount[4];
                list1[nlist] = f_hand_gold + GHandCount[4];
                ++nlist;
                break;
            case GKA:
                ++GHandCount[5];
                list0[nlist] = e_hand_bishop + GHandCount[5];
                list1[nlist] = f_hand_bishop + GHandCount[5];
                ++nlist;
                break;
            case GHI:
                ++GHandCount[6];
                list0[nlist] = e_hand_rook + GHandCount[6];
                list1[nlist] = f_hand_rook + GHandCount[6];
                ++nlist;
                break;
            default:
                __assume(false); /* 心持ち高速化 */
                break;
            }
        }
    }

    assert(nlist == NLIST);
    return nlist;
}

int Position::evaluate_body(const Color us)
{
    int list0[NLIST]; // 駒番号numのlist0
    int list1[NLIST]; // 駒番号numのlist1
    int nlist = make_list(list0, list1);

    const int sq_bk = SQ_BKING;
    const int sq_wk = SQ_WKING;

    const kkp_entry* ppkppb = kpp3[sq_bk];
    const kkp_entry* ppkppw = kpp3[Inv(sq_wk)];

    int score = kk[sq_bk][sq_wk];
    for (int kn = 0; kn < NLIST; kn++){
        const int k0 = list0[kn];
        const int k1 = list1[kn];
        const short* pkppb = ppkppb[k0];
        const short* pkppw = ppkppw[k1];
        for (int j = 0; j < kn; j++){
            score += pkppb[list0[j]];
            score -= pkppw[list1[j]];
        }
        score += kkp[sq_bk][sq_wk][k0];
    }

    score /= FV_SCALE;
    score += MATERIAL;
    score = (us == BLACK) ? score : -score;
    return score;
}


#if defined(MAKELIST_DIFF)
void Position::init_make_list()
{
    memset(list0, 0, sizeof(list0));
    memset(list1, 0, sizeof(list1));
    memset(listkn, 0, sizeof(listkn));
    memset(handcount, 0, sizeof(handcount));
        
    for (PieceNumber kn = PIECENUMBER_MIN; kn <= PIECENUMBER_MAX; ++kn){
        const int kpos = knpos[kn];
        const Piece piece = Piece(knkind[kn]);
        int count, sq;

        switch (kpos) {
        case 0:
            break;
        case 1: // 先手持駒
        case 2: // 後手持駒
            count = ++handcount[piece];
            list0[kn] = NanohaTbl::HandIndex0[piece] + count;
            list1[kn] = NanohaTbl::HandIndex1[piece] + count;
            listkn[list0[kn]] = kn;
            break;
        default:
            if ((SFU <= piece && piece <= SRY && piece != SOU) ||
                (GFU <= piece && piece <= GRY && piece != GOU)) {
                sq = conv_z2sq(kpos);
                list0[kn] = NanohaTbl::KppIndex0[piece] + sq;
                list1[kn] = NanohaTbl::KppIndex1[piece] + Inv(sq);
            }
            break;
        }
    }
}

void Position::make_list_move(PieceNumber kn, Piece piece, Square to)
{
    if (kn < PIECENUMBER_MIN) return;

    st->fromlist[0] = list0[kn];
    st->fromlist[1] = list1[kn];

    const int sq = conv_z2sq(to);
    list0[kn] = NanohaTbl::KppIndex0[piece] + sq;
    list1[kn] = NanohaTbl::KppIndex1[piece] + Inv(sq);
}

void Position::make_list_undo_move(PieceNumber kn)
{
    if (kn < PIECENUMBER_MIN) return;

    list0[kn] = st->fromlist[0];
    list1[kn] = st->fromlist[1];
}

void Position::make_list_capture(PieceNumber kn, Piece captureType)
{
    assert(MIN_PIECENUMBER <= kn && kn <= MAX_PIECENUMBER);

    // 捕られる駒の情報
    st->caplist[0] = list0[kn];
    st->caplist[1] = list1[kn];

    // 捕った持駒の情報
    st->cap_hand = captureType;

    // 1枚増やす
    const int count = ++handcount[captureType];
    list0[kn] = NanohaTbl::HandIndex0[captureType] + count;
    list1[kn] = NanohaTbl::HandIndex1[captureType] + count;
    listkn[list0[kn]] = kn;

    assert(count <= 18);
    assert(list0[kn] < fe_hand_end);
}

void Position::make_list_undo_capture(PieceNumber kn)
{
    // 持駒を戻す
    handcount[st->cap_hand]--;
    listkn[list0[kn]] = PIECENUMBER_NONE;

    // listを戻す
    list0[kn] = st->caplist[0];
    list1[kn] = st->caplist[1];
}

PieceNumber Position::make_list_drop(Piece piece, Square to)
{
    // 持ち駒の中で一番駒番号の多い駒を打ちます。
    const int count = handcount[piece];
    const int handIndex0 = NanohaTbl::HandIndex0[piece] + count;
    const PieceNumber kn = listkn[handIndex0];  // maxの駒番号
    assert(handIndex0 < fe_hand_end);

    // knをセーブ
    st->droplist[0] = list0[kn];
    st->droplist[1] = list1[kn];

    listkn[handIndex0] = PIECENUMBER_NONE; // 駒番号の一番大きい持ち駒を消去
    handcount[piece]--;                    // 打つので１枚減らす

    // 打った駒の情報
    const int sq = conv_z2sq(to);
    list0[kn] = NanohaTbl::KppIndex0[piece] + sq;
    list1[kn] = NanohaTbl::KppIndex1[piece] + Inv(sq);

    return kn;
}

void Position::make_list_undo_drop(PieceNumber kn, Piece piece)
{
    list0[kn] = st->droplist[0];
    list1[kn] = st->droplist[1];
    //st->drop_hand = piece;

    listkn[list0[kn]] = kn;
    handcount[piece]++;
}

int Position::evaluate_make_list_diff(const Color us)
{
    const int sq_bk = SQ_BKING;
    const int sq_wk = SQ_WKING;

    const kkp_entry* ppkppb = kpp3[sq_bk];
    const kkp_entry* ppkppw = kpp3[Inv(sq_wk)];

    int score = kk[sq_bk][sq_wk];
    for (int kn = PIECENUMBER_MIN; kn <= PIECENUMBER_MAX; kn++){
        const int k0 = list0[kn];
        const int k1 = list1[kn];
        const short* pkppb = ppkppb[k0];
        const short* pkppw = ppkppw[k1];
        for (int j = PIECENUMBER_MIN; j < kn; j++){
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
}
#endif


int Position::evaluate(const Color us)
{
#if defined(MAKELIST_DIFF)
    int score = evaluate_make_list_diff(us);
#else
    // なぜかevaluate_bodyを通した方が速い
    int score = evaluate_body(us);
    //int score = evaluate_correct(us);
#endif

    return score;
}

Value evaluate(Position& pos, Value& margin)
{
	margin = VALUE_ZERO;
	const Color us = pos.side_to_move();
	return Value(pos.evaluate(us));
}
