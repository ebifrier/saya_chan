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
#define FV_KPP  "KPP_synthesized.bin" 
#define FV_KPP2 "KPP_synthesized2.bin" 
#define FV_KKP  "KKP_synthesized.bin" 
#define FV_KK   "KK_synthesized.bin" 
#endif

#define EHASH_MASK          0x3fffffU      //* occupies 32MB 
#define FV_SCALE            32
#define MATERIAL            (this->material)

#define SQ_BKING            NanohaTbl::z2sq[kingS]
#define SQ_WKING            NanohaTbl::z2sq[kingG]
#define HAND_B              (this->hand[BLACK].h)
#define HAND_W              (this->hand[WHITE].h)

#define Inv(sq)             (nsquare-1-sq)
#define PcPcOnSq(sq,i,j)    pc_on_sq[sq][(i)*((i)+1)/2+(j)]

#define I2HandPawn(hand)    (((hand) & HAND_FU_MASK) >> HAND_FU_SHIFT)
#define I2HandLance(hand)   (((hand) & HAND_KY_MASK) >> HAND_KY_SHIFT)
#define I2HandKnight(hand)  (((hand) & HAND_KE_MASK) >> HAND_KE_SHIFT)
#define I2HandSilver(hand)  (((hand) & HAND_GI_MASK) >> HAND_GI_SHIFT)
#define I2HandGold(hand)    (((hand) & HAND_KI_MASK) >> HAND_KI_SHIFT)
#define I2HandBishop(hand)  (((hand) & HAND_KA_MASK) >> HAND_KA_SHIFT)
#define I2HandRook(hand)    (((hand) & HAND_HI_MASK) >> HAND_HI_SHIFT)

enum { pos_n = fe_end * (fe_end + 1) / 2 };
typedef int16_t pc_on_pc_entry[pos_n];
uint64_t ehash_tbl[EHASH_MASK + 1];

typedef int16_t kkp_entry[fe_end];
int16_t kpp3[nsquare][fe_end][fe_end];
int32_t kkp[nsquare][nsquare][fe_end];
int32_t kk[nsquare][nsquare];

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
        none, f_gold, f_gold,  f_gold,   f_gold,   none,   f_horse,  f_dragon,
        none, e_pawn, e_lance, e_knight, e_silver, e_gold, e_bishop, e_rook,
        none, e_gold, e_gold,  e_gold,   e_gold,   none,   e_horse,  e_dragon
    };

    const KPP KppIndex1[32] = {
        none, e_pawn, e_lance, e_knight, e_silver, e_gold, e_bishop, e_rook,
        none, e_gold, e_gold,  e_gold,   e_gold,   none,   e_horse,  e_dragon,
        none, f_pawn, f_lance, f_knight, f_silver, f_gold, f_bishop, f_rook,
        none, f_gold, f_gold,  f_gold,   f_gold,   none,   f_horse,  f_dragon
    };

    const KPP HandIndex0[32] = {
        none,           f_hand_pawn,   f_hand_lance,   f_hand_knight,
        f_hand_silver,  f_hand_gold,   f_hand_bishop,  f_hand_rook,
        none,           none,          none,           none,
        none,           none,          none,           none,
        none,           e_hand_pawn,   e_hand_lance,   e_hand_knight,
        e_hand_silver,  e_hand_gold,   e_hand_bishop,  e_hand_rook,
        none,           none,          none,           none,
        none,           none,          none,           none,
    };

    const KPP HandIndex1[32] = {
        none,           e_hand_pawn,   e_hand_lance,   e_hand_knight,
        e_hand_silver,  e_hand_gold,   e_hand_bishop,  e_hand_rook,
        none,           none,          none,           none,
        none,           none,          none,           none,
        none,           f_hand_pawn,   f_hand_lance,   f_hand_knight,
        f_hand_silver,  f_hand_gold,   f_hand_bishop,  f_hand_rook,
        none,           none,          none,           none,
        none,           none,          none,           none,
    };
}

namespace {
    static const int handB[14] = {
        f_hand_pawn,   e_hand_pawn,   f_hand_lance,  e_hand_lance,
        f_hand_knight, e_hand_knight, f_hand_silver, e_hand_silver,
        f_hand_gold,   e_hand_gold,   f_hand_bishop, e_hand_bishop, 
		f_hand_rook,   e_hand_rook
    };

    static const int handW[14] = {
        e_hand_pawn,   f_hand_pawn,   e_hand_lance,  f_hand_lance,
        e_hand_knight, f_hand_knight, e_hand_silver, f_hand_silver,
        e_hand_gold,   f_hand_gold,   e_hand_bishop, f_hand_bishop,
		e_hand_rook,   f_hand_rook
    };
}

void ehash_clear() {
    memset(ehash_tbl, 0, sizeof(ehash_tbl));
}

int ehash_probe(uint64_t current_key, unsigned int hand_b, int * __restrict pscore)
{
    uint64_t hash_word, hash_key;

    hash_word = ehash_tbl[(unsigned int)current_key & EHASH_MASK];

#if ! defined(__x86_64__)
    hash_word ^= hash_word << 32;
#endif

    current_key ^= (uint64_t)hand_b << 30;
    current_key &= ~(uint64_t)0x1fffffU;

    hash_key = hash_word;
    hash_key &= ~(uint64_t)0x1fffffU;

    if (hash_key != current_key) { return 0; }

    *pscore = (int)((unsigned int)hash_word & 0x1fffffU) - 0x100000;

    return 1;
}

void ehash_store(uint64_t key, unsigned int hand_b, int score)
{
    uint64_t hash_word;

    hash_word = key;
    hash_word ^= (uint64_t)hand_b << 30;
    hash_word &= ~(uint64_t)0x1fffffU;
    hash_word |= (uint64_t)(score + 0x100000);

#if ! defined(__x86_64__)
    hash_word ^= hash_word << 32;
#endif

    ehash_tbl[(unsigned int)key & EHASH_MASK] = hash_word;
}

void Position::init_evaluate()
{
	FILE *fp = NULL;
	size_t size;
    int iret = 0;

#if 0
    //KPP
    do {
        fp = fopen(FV_KPP, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * fe_end * fe_end;
        if (fread(kpp3, sizeof(int16_t), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }

    } while (0);
    if (fp) fclose(fp);
#else
    pc_on_pc_entry *pc_on_sq = new pc_on_pc_entry[nsquare];

    do {
        fp = fopen(FV_KPP2, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * pos_n;
        if (fread(pc_on_sq, sizeof(short), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }
    } while (0);
    if (fp) fclose(fp);

    for (int sq = 0; sq < nsquare; ++sq) {
        for (int k = 0; k < fe_end; k++){
            for (int j = 0; j < fe_end; j++){
                short value = (k <= j ? PcPcOnSq(sq, j, k) : PcPcOnSq(sq, k, j));
#if 0
                if (kpp3[sq][k][j] != value) {
                    std::cerr << "Failed to load '"FV_KPP2"' file." << std::endl;
                    iret = -3;
                    exit(-1);
                }
#endif
                kpp3[sq][k][j] = value;
            }
        }
    }

    delete[] pc_on_sq;
    pc_on_sq = NULL;
#endif

	//KKP
    do {
        fp = fopen(FV_KKP, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * nsquare * fe_end;
        if (fread(kkp, sizeof(int32_t), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }

    } while (0);
    if (fp) fclose(fp);

	//KK
	do {
        fp = fopen(FV_KK, "rb");
        if (fp == NULL) { iret = -2; break; }

        size = nsquare * nsquare;
        if (fread(kk, sizeof(int32_t), size, fp) != size){ iret = -2; break; }
        if (fgetc(fp) != EOF) { iret = -2; break; }

    } while (0);
    if (fp) fclose(fp);

    if (iret < 0) {
        std::cerr << "Can't load '*_synthesized' file." << std::endl;
        exit(-1);
    }

    ehash_clear();
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

// 評価値のスケール前の値を計算します。
int Position::evaluate_raw_correct() const
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
        const int16_t* pkppb = ppkppb[k0];
        const int16_t* pkppw = ppkppw[k1];
        for (int j = 0; j < kn; j++){
            score += pkppb[list0[j]];
            score -= pkppw[list1[j]];
        }
        score += kkp[sq_bk][sq_wk][k0];
    }

    return score;
}

// 評価関数が正しいかどうかを判定するのに使う
Value Position::evaluate_correct(const Color us) const
{
    int score = evaluate_raw_correct();
    score /= FV_SCALE;
    score += MATERIAL;
    score = (us == BLACK) ? score : -score;
    return Value(score);
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
	if (kn < PIECENUMBER_MIN) {
#if defined(EVAL_DIFF)
        st->changeType = 0;
#endif
		return;
	}

    st->oldlist[0] = list0[kn];
    st->oldlist[1] = list1[kn];

    const int sq = conv_z2sq(to);
    list0[kn] = NanohaTbl::KppIndex0[piece] + sq;
    list1[kn] = NanohaTbl::KppIndex1[piece] + Inv(sq);

#if defined(EVAL_DIFF)
    st->newlist[0] = list0[kn];
    st->newlist[1] = list1[kn];
#endif
}

void Position::make_list_undo_move(PieceNumber kn)
{
    if (kn < PIECENUMBER_MIN) return;

    list0[kn] = st->oldlist[0];
    list1[kn] = st->oldlist[1];
}

void Position::make_list_capture(PieceNumber kn, Piece captureType)
{
    assert(PIECENUMBER_MIN <= kn && kn <= PIECENUMBER_MAX);

    // 捕られる駒の情報
    st->oldcap[0] = list0[kn];
    st->oldcap[1] = list1[kn];

    // 捕った持駒の情報
    st->capHand = captureType;

    // 1枚増やす
    const int count = ++handcount[captureType];
    list0[kn] = NanohaTbl::HandIndex0[captureType] + count;
    list1[kn] = NanohaTbl::HandIndex1[captureType] + count;
    listkn[list0[kn]] = kn;

#if defined(EVAL_DIFF)
    st->newcap[0] = list0[kn];
    st->newcap[1] = list1[kn];
    st->changeType = 2;
#endif

    assert(count <= 18);
    assert(list0[kn] < fe_hand_end);
}

void Position::make_list_undo_capture(PieceNumber kn)
{
    // 持駒を戻す
    handcount[st->capHand]--;
    listkn[list0[kn]] = PIECENUMBER_NONE;

    // listを戻す
    list0[kn] = st->oldcap[0];
    list1[kn] = st->oldcap[1];
}

PieceNumber Position::make_list_drop(Piece piece, Square to)
{
    // 持ち駒の中で一番駒番号の多い駒を打ちます。
    const int count = handcount[piece];
    const int handIndex0 = NanohaTbl::HandIndex0[piece] + count;
    const PieceNumber kn = listkn[handIndex0];  // maxの駒番号
    assert(handIndex0 < fe_hand_end);

    // knをセーブ
    st->oldlist[0] = list0[kn];
    st->oldlist[1] = list1[kn];

    listkn[handIndex0] = PIECENUMBER_NONE; // 駒番号の一番大きい持ち駒を消去
    handcount[piece]--;                    // 打つので１枚減らす

    // 打った駒の情報
    const int sq = conv_z2sq(to);
    list0[kn] = NanohaTbl::KppIndex0[piece] + sq;
    list1[kn] = NanohaTbl::KppIndex1[piece] + Inv(sq);

#if defined(EVAL_DIFF)
    st->newlist[0] = list0[kn];
    st->newlist[1] = list1[kn];
#endif
    return kn;
}

void Position::make_list_undo_drop(PieceNumber kn, Piece piece)
{
    list0[kn] = st->oldlist[0];
    list1[kn] = st->oldlist[1];
    //st->drop_hand = piece;

    listkn[list0[kn]] = kn;
    handcount[piece]++;
}

int Position::evaluate_raw_make_list_diff()
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

    return score;
}
#endif


#if defined(EVAL_DIFF)
// 差分計算
// index[2]は動かした駒のlist
int Position::doapc(const int index[2]) const
{
    const int sq_bk = SQ_BKING;
    const int sq_wk = SQ_WKING;

    int sum = kkp[sq_bk][sq_wk][index[0]];
    const auto* pkppb = kpp3[sq_bk][index[0]];
    const auto* pkppw = kpp3[Inv(sq_wk)][index[1]];
    for (int kn = PIECENUMBER_MIN; kn <= PIECENUMBER_MAX; kn++) {
        sum += pkppb[list0[kn]];
        sum -= pkppw[list1[kn]];
    }

    return sum;
}

// 評価値の差分計算を行う
// 値がない時はfalseを返す。
bool Position::calc_difference(SearchStack* ss) const
{
    if ((ss - 1)->staticEvalRaw == INT_MAX) { return false; }
    int diff = 0;

    const auto* ppkppb = kpp3[SQ_BKING];
    const auto* ppkppw = kpp3[Inv(SQ_WKING)];

    /* oldは引く。newは足す。
     * 参照されてない＆２重に参照してるとこに注意
     */

    // king-move
    // TODO: 差分計算できるらしい
    if (st->changeType == 0) { return false; }

    // newlist
    diff += doapc(st->newlist);
    // oldlist
    diff -= doapc(st->oldlist);

    // newlist oldlist 引きすぎたので足す
    diff += ppkppb[st->newlist[0]][st->oldlist[0]];
    diff -= ppkppw[st->newlist[1]][st->oldlist[1]];

    // cap
    if (st->changeType == 2) { // newが２つ

        // newcap oldlist 引きすぎたので足す
        diff += ppkppb[st->newcap[0]][st->oldlist[0]];
        diff -= ppkppw[st->newcap[1]][st->oldlist[1]];

        // newcap
        diff += doapc(st->newcap);
        // newlist newcap (２回足されてるので引く)
        diff -= ppkppb[st->newlist[0]][st->newcap[0]];
        diff += ppkppw[st->newlist[1]][st->newcap[1]];

        // oldcap
        diff -= doapc(st->oldcap);
        // new oldcap 引きすぎたので足す
        diff += ppkppb[st->newlist[0]][st->oldcap[0]];
        diff -= ppkppw[st->newlist[1]][st->oldcap[1]];
        diff += ppkppb[st->newcap[0]][st->oldcap[0]];
        diff -= ppkppw[st->newcap[1]][st->oldcap[1]];

        // oldcap oldlist 参照されてない 
        diff -= ppkppb[st->oldcap[0]][st->oldlist[0]];
        diff += ppkppw[st->oldcap[1]][st->oldlist[1]];
    }
    //else if (st->ct !=1 ){ MYABORT(); }

    // セーブ
    ss->staticEvalRaw = Value(diff) + (ss - 1)->staticEvalRaw;

    return true;
}
#endif


int Position::evaluate_raw_body()
{
#if defined(MAKELIST_DIFF)
    int score = evaluate_raw_make_list_diff();
#else;
    // なぜかevaluate_bodyを通した方が速い
    int score = evaluate_raw_correct();
#endif

    return score;
}

Value Position::evaluate(const Color us, SearchStack* ss)
{
	int score = 0;

#if defined(EVAL_DIFF)
    // null move
    if (ss->staticEvalRaw != INT_MAX) {
        score = int(ss->staticEvalRaw);
    }
    else
#endif

    // ehash
    if (ehash_probe(st->key, HAND_B, &score)) {
#if defined(EVAL_DIFF)
        ss->staticEvalRaw = Value(score);
#endif
    }

#if defined(EVAL_DIFF)
	else if (calc_difference(ss)) {
        score = int(ss->staticEvalRaw);
        ehash_store(st->key, HAND_B, score);
    }
#endif

    else {
        // 普通に評価値を計算
        score = evaluate_raw_body();
        ehash_store(st->key, HAND_B, score);
#if defined(EVAL_DIFF)
        ss->staticEvalRaw = Value(score);
#endif
    }

    assert(score == evaluate_raw_correct());

    score /= FV_SCALE;
    score += MATERIAL;
    score = (us == BLACK) ? score : -score;
    return Value(score);
}
