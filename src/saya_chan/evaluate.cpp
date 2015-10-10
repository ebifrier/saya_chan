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

enum {
	promote = 8, EMPTY = 0,	/* VC++でemptyがぶつかるので変更 */
	pawn, lance, knight, silver, gold, bishop, rook, king, pro_pawn,
	pro_lance, pro_knight, pro_silver, piece_null, horse, dragon
};

#if !defined(EVAL_MICRO)
typedef short kkp_entry[fe_end];
short kpp3[nsquare][fe_end][fe_end];
long kkp[nsquare][nsquare][fe_end];
long kk[nsquare][nsquare];
#endif

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
}

void Position::init_evaluate()
{
	int iret=0;
#if !defined(EVAL_MICRO)
	FILE *fp = NULL;
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

	if (iret < 0) {
		std::cerr << "Can't load *_synthesized." << std::endl;
        exit(-1);
	}
#endif
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

#if !defined(EVAL_MICRO)
void Position::make_list()
{
    int sq, piece, hands;

	///初期化
	memset(list0, 0, sizeof(list0));
	memset(list1, 0, sizeof(list1));
	memset(I2hand, 0, sizeof(I2hand));
	//memset(listkn, 0, sizeof(listkn));

    //i=kn  kn=0=none, kn=1=SOU, kn=2=GOU,,,
	for (PieceNumber_t kn = 3; kn <= MAX_PIECENUMBER; kn++){
		const int z = knpos[kn]; //位置
        assert(z != 0);

        switch (z) {
        case 1: //先手持駒
        case 2: //後手持駒
            piece = knkind[kn];
            I2hand[piece]++;
            hands = I2hand[piece];
            list0[kn] = aihand0[piece] + hands;
            list1[kn] = aihand1[piece] + hands;
            //listkn[list0[kn]] = kn;
            break;
        default: //盤上
			sq = NanohaTbl::z2sq[z];
			switch (knkind[kn]) {
			case SFU:
				list0[kn] = f_pawn + sq;
				list1[kn] = e_pawn + Inv(sq);
				break;
			case GFU:
				list0[kn] = e_pawn + sq;
				list1[kn] = f_pawn + Inv(sq);
				break;
			case SKY:
				list0[kn] = f_lance + sq;
				list1[kn] = e_lance + Inv(sq);
				break;
			case GKY:
				list0[kn] = e_lance + sq;
				list1[kn] = f_lance + Inv(sq);
				break;
			case SKE:
				list0[kn] = f_knight + sq;
				list1[kn] = e_knight + Inv(sq);
				break;
			case GKE:
				list0[kn] = e_knight + sq;
				list1[kn] = f_knight + Inv(sq);
				break;
			case SGI:
				list0[kn] = f_silver + sq;
				list1[kn] = e_silver + Inv(sq);
				break;
			case GGI:
				list0[kn] = e_silver + sq;
				list1[kn] = f_silver + Inv(sq);
				break;
			case SKI:
			case STO:
			case SNY:
			case SNK:
			case SNG:
				list0[kn] = f_gold + sq;
				list1[kn] = e_gold + Inv(sq);
				break;
			case GKI:
			case GTO:
			case GNY:
			case GNK:
			case GNG:
				list0[kn] = e_gold + sq;
				list1[kn] = f_gold + Inv(sq);
				break;
			case SKA:
				list0[kn] = f_bishop + sq;
				list1[kn] = e_bishop + Inv(sq);
				break;
			case GKA:
				list0[kn] = e_bishop + sq;
				list1[kn] = f_bishop + Inv(sq);
				break;
			case SUM:
				list0[kn] = f_horse + sq;
				list1[kn] = e_horse + Inv(sq);
				break;
			case GUM:
				list0[kn] = e_horse + sq;
				list1[kn] = f_horse + Inv(sq);
				break;
			case SHI:
				list0[kn] = f_rook + sq;
				list1[kn] = e_rook + Inv(sq);
				break;
			case GHI:
				list0[kn] = e_rook + sq;
				list1[kn] = f_rook + Inv(sq);
				break;
			case SRY:
				list0[kn] = f_dragon + sq;
				list1[kn] = e_dragon + Inv(sq);
				break;
			case GRY:
				list0[kn] = e_dragon + sq;
				list1[kn] = f_dragon + Inv(sq);
				break;
			case EMP:
			case WALL:
			case SOU:
			case GOU:
			case PIECE_NONE:
				break;
			}
		}
	}
}
#endif

int Position::evaluate(const Color us) 
{
	const int sq_bk = SQ_BKING;
	const int sq_wk = SQ_WKING;

    const kkp_entry* ppkppb = kpp3[sq_bk];
    const kkp_entry* ppkppw = kpp3[Inv(sq_wk)];

	int score = kk[sq_bk][sq_wk];
	for (int kn = 3; kn <= MAX_PIECENUMBER; kn++){
		const int k0 = list0[kn];
		const int k1 = list1[kn];
		const short* pkppb = ppkppb[k0];
		const short* pkppw = ppkppw[k1];
		for (int j = 3; j < kn; j++){
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

Value evaluate(Position& pos, Value& margin)
{
	margin = VALUE_ZERO;
	const Color us = pos.side_to_move();
	return Value(pos.evaluate(us));
}
