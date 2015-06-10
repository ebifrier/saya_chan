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
#elif defined(EVAL_OLD)
//#include "param_old.h"
#include "param.h"
#define FV_BIN "fv_mini.bin"
#else
#include "param_new.h"
#define FV_BIN "fv_mini2.bin"
#endif
#define NLIST    38
#define NLIST2  38
#define NPiece  14

#define FV_SCALE                32

#define MATERIAL            (this->material)

#define SQ_BKING            NanohaTbl::z2sq[kingS]
#define SQ_WKING            NanohaTbl::z2sq[kingG]
#define HAND_B              (this->hand[BLACK].h)
#define HAND_W              (this->hand[WHITE].h)

#define Inv(sq)             (nsquare-1-sq)
//KP
#define PcOnSq(k,i)         fv_kp[k][i]
//PP
#define PcPcOn(i,j)         fv_pp[i][j]

//KKP
#define PcOnSq2(k,i)        pc_on_sq[k][(i)*((i)+3)/2]
//KPP
#define PcPcOnSq(k,i,j)     pc_pc_on_sq[k][(i)*((i)+1)/2+(j)]
#define PcPcOnSqAny(k,i,j)  ( i >= j ? PcPcOnSq(k,i,j) : PcPcOnSq(k,j,i) )

#define PcPcOnSq2(k,i,j)    pc_pc_on_sq2[k][(i) * kp_end + (j)]

//KK
#define KK(k,i)	　          fv_kk[k][i]

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

enum { nhand = 7, nfile = 9,  nrank = 9,  nsquare = 81 };

#if !defined(EVAL_MICRO)
enum {
    // PP用の定義
    pp_bpawn      =  -9,                    //   -9: 先手Pの位置は 9-80(72箇所)、これを  0- 71にマップ
    pp_blance     = pp_bpawn   + 72,        //   63: 先手Lの位置は 9-80(72箇所)、これを 72-143にマップ
    pp_bknight    = pp_blance  + 63,        //  126: 先手Nの位置は18-80(63箇所)、これを144-206にマップ
    pp_bsilver    = pp_bknight + 81,        //  207: 先手Sの位置は 0-80(81箇所)、これを207-287にマップ
    pp_bgold      = pp_bsilver + 81,        //  288: 先手Gの位置は 0-80(81箇所)、これを288-368にマップ
    pp_bbishop    = pp_bgold   + 81,        //  369: 先手Bの位置は 0-80(81箇所)、これを369-449にマップ
    pp_bhorse     = pp_bbishop + 81,        //  450: 先手Hの位置は 0-80(81箇所)、これを450-530にマップ
    pp_brook      = pp_bhorse  + 81,        //  531: 先手Rの位置は 0-80(81箇所)、これを531-611にマップ
    pp_bdragon    = pp_brook   + 81,        //  612: 先手Dの位置は 0-80(81箇所)、これを612-692にマップ
    pp_bend       = pp_bdragon + 81,        //  693: 先手の最終位置

    pp_wpawn      = pp_bdragon + 81,        //  693: 後手Pの位置は 0-71(72箇所)、これを 693- 764にマップ
    pp_wlance     = pp_wpawn   + 72,        //  765: 後手Lの位置は 0-71(72箇所)、これを 765- 836にマップ
    pp_wknight    = pp_wlance  + 72,        //  837: 後手Nの位置は 0-62(63箇所)、これを 837- 899にマップ
    pp_wsilver    = pp_wknight + 63,        //  900: 後手Sの位置は 0-80(81箇所)、これを 900- 980にマップ
    pp_wgold      = pp_wsilver + 81,        //  981: 後手Gの位置は 0-80(81箇所)、これを 981-1061にマップ
    pp_wbishop    = pp_wgold   + 81,        // 1062: 後手Bの位置は 0-80(81箇所)、これを1062-1142にマップ
    pp_whorse     = pp_wbishop + 81,        // 1143: 後手Hの位置は 0-80(81箇所)、これを1143-1223にマップ
    pp_wrook      = pp_whorse  + 81,        // 1224: 後手Rの位置は 0-80(81箇所)、これを1224-1304にマップ
    pp_wdragon    = pp_wrook   + 81,        // 1305: 後手Dの位置は 0-80(81箇所)、これを1305-1385にマップ
    pp_end        = pp_wdragon + 81,        // 1386: 後手の最終位置

    // K(P+H)用の定義
    //KPP用はこれと全部同じ
    kp_hand_bpawn   =    0,
    kp_hand_wpawn   =   19,
    kp_hand_blance  =   38,
    kp_hand_wlance  =   43,
    kp_hand_bknight =   48,
    kp_hand_wknight =   53,
    kp_hand_bsilver =   58,
    kp_hand_wsilver =   63,
    kp_hand_bgold   =   68,
    kp_hand_wgold   =   73,
    kp_hand_bbishop =   78,
    kp_hand_wbishop =   81,
    kp_hand_brook   =   84,
    kp_hand_wrook   =   87,
    kp_hand_end     =   90,
    kp_bpawn        =   81,
    kp_wpawn        =  162,
    kp_blance       =  225,
    kp_wlance       =  306,
    kp_bknight      =  360,
    kp_wknight      =  441,
    kp_bsilver      =  504,
    kp_wsilver      =  585,
    kp_bgold        =  666,
    kp_wgold        =  747,
    kp_bbishop      =  828,
    kp_wbishop      =  909,
    kp_bhorse       =  990,
    kp_whorse       = 1071,
    kp_brook        = 1152,
    kp_wrook        = 1233,
    kp_bdragon      = 1314,
    kp_wdragon      = 1395,
    kp_end          = 1476,
    //KKP用
    kkp_hand_pawn = 0,
    kkp_hand_lance = 19,
    kkp_hand_knight = 24,
    kkp_hand_silver = 29,
    kkp_hand_gold = 34,
    kkp_hand_bishop = 39,
    kkp_hand_rook = 42,
    kkp_hand_end = 45,
    kkp_pawn = 36,
    kkp_lance = 108,
    kkp_knight = 171,
    kkp_silver = 252,
    kkp_gold = 333,
    kkp_bishop = 414,
    kkp_horse = 495,
    kkp_rook = 576,
    kkp_dragon = 657,
    kkp_end = 738
};
#endif

namespace {
    short p_value[31];

#if !defined(EVAL_MICRO)
    short kkp[nsquare][nsquare][kkp_end];
    short pc_pc_on_sq2[nsquare][kp_end * (kp_end + 1)];
	short fv_kk[nsquare][nsquare];
	//short fv_pp[kp_end][kp_end];
#endif
}

namespace NanohaTbl {
    const short z2sq[] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1,  0,  9, 18, 27, 36, 45, 54, 63, 72, -1, -1, -1, -1, -1, -1,
        -1,  1, 10, 19, 28, 37, 46, 55, 64, 73, -1, -1, -1, -1, -1, -1,
        -1,  2, 11, 20, 29, 38, 47, 56, 65, 74, -1, -1, -1, -1, -1, -1,
        -1,  3, 12, 21, 30, 39, 48, 57, 66, 75, -1, -1, -1, -1, -1, -1,
        -1,  4, 13, 22, 31, 40, 49, 58, 67, 76, -1, -1, -1, -1, -1, -1,
        -1,  5, 14, 23, 32, 41, 50, 59, 68, 77, -1, -1, -1, -1, -1, -1,
        -1,  6, 15, 24, 33, 42, 51, 60, 69, 78, -1, -1, -1, -1, -1, -1,
        -1,  7, 16, 25, 34, 43, 52, 61, 70, 79, -1, -1, -1, -1, -1, -1,
        -1,  8, 17, 26, 35, 44, 53, 62, 71, 80, -1, -1, -1, -1, -1, -1,
    };
}

typedef short kpp_entry_t[kp_end * (kp_end + 1) / 2];

void Position::init_evaluate()
{
#if 0
    const char *fname = "eval38.bin";
#else
    const char *fname = "fv38.bin";
#endif
    FILE *fp = NULL;
    int iret = 0;

    do {
        fp = fopen(fname, "rb");
        if (fp == NULL) {
            iret = -2;
            break;
        }

        //KPP
#if 0
        size_t size = nsquare * (kp_end * (kp_end + 1));
        if (fread(pc_pc_on_sq2, sizeof(short), size, fp) != size) {
            iret = -2;
            break;
        }
#else
        size_t size = nsquare * kp_end * (kp_end + 1) / 2;
        kpp_entry_t *pc_pc_on_sq = (kpp_entry_t *)malloc(size * sizeof(short));
        if (fread(pc_pc_on_sq, sizeof(short), size, fp) != size)
        {
            free(pc_pc_on_sq);
            iret = -2;
            break;
        }

        //KPP計算
        int i, j, k;
        for (k = 0; k < nsquare; k++)
        {
            for (i = 0; i < kp_end; i++)
            {
                for (j = 0; j < kp_end; j++)
                {
                    if (j <= i)
                    {
                        PcPcOnSq2(k, i, j) = PcPcOnSq(k, i, j);
                    }
                    else {
                        PcPcOnSq2(k, i, j) = PcPcOnSq(k, j, i);
                    }
                };
            };
        };

        free(pc_pc_on_sq);
        pc_pc_on_sq = nullptr;
#endif

        //KKP
        size = nsquare * nsquare * kkp_end;
        if (fread(kkp, sizeof(short), size, fp) != size) {
            iret = -2;
            break;
        }

		//KK
        size = nsquare * nsquare;
        size_t s = fread(fv_kk, sizeof(short), size, fp);
        if (s != size) {
			iret = -2;
			break;
		}

		//EOF判定
        if (fgetc(fp) != EOF) {
            iret = -2;
            break;
        }
    } while (0);

    if (fp) fclose(fp);

    if (iret < 0) {
        // 評価用ファイルが読めない場合は起動させない。
        std::cerr << "Can't load '" << fname << "'." << std::endl;
        exit(1);

#if defined(CSADLL) || defined(CSA_DIRECT)
        ::MessageBox(NULL, "評価ベクトルがロードできません\n終了します", "Error!", MB_OK);
        exit(1);
#endif
    }

    for (int i = 0; i < 31; i++) { p_value[i] = 0; }

    p_value[15+pawn]       = DPawn;
    p_value[15+lance]      = DLance;
    p_value[15+knight]     = DKnight;
    p_value[15+silver]     = DSilver;
    p_value[15+gold]       = DGold;
    p_value[15+bishop]     = DBishop;
    p_value[15+rook]       = DRook;
    p_value[15+king]       = DKing;
    p_value[15+pro_pawn]   = DProPawn;
    p_value[15+pro_lance]  = DProLance;
    p_value[15+pro_knight] = DProKnight;
    p_value[15+pro_silver] = DProSilver;
    p_value[15+horse]      = DHorse;
    p_value[15+dragon]     = DDragon;

    p_value[15-pawn]          = p_value[15+pawn];
    p_value[15-lance]         = p_value[15+lance];
    p_value[15-knight]        = p_value[15+knight];
    p_value[15-silver]        = p_value[15+silver];
    p_value[15-gold]          = p_value[15+gold];
    p_value[15-bishop]        = p_value[15+bishop];
    p_value[15-rook]          = p_value[15+rook];
    p_value[15-king]          = p_value[15+king];
    p_value[15-pro_pawn]      = p_value[15+pro_pawn];
    p_value[15-pro_lance]     = p_value[15+pro_lance];
    p_value[15-pro_knight]    = p_value[15+pro_knight];
    p_value[15-pro_silver]    = p_value[15+pro_silver];
    p_value[15-horse]         = p_value[15+horse];
    p_value[15-dragon]        = p_value[15+dragon];
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
    v  = item  * p_value[15+pawn];
    v += itemp * p_value[15+pro_pawn];

    item  = 0;
    itemp = 0;
    for (i = KNS_KY; i <= KNE_KY; i++) {
        if (knkind[i] == SKY) item++;
        if (knkind[i] == GKY) item--;
        if (knkind[i] == SNY) itemp++;
        if (knkind[i] == GNY) itemp--;
    }
    v += item  * p_value[15+lance];
    v += itemp * p_value[15+pro_lance];

    item  = 0;
    itemp = 0;
    for (i = KNS_KE; i <= KNE_KE; i++) {
        if (knkind[i] == SKE) item++;
        if (knkind[i] == GKE) item--;
        if (knkind[i] == SNK) itemp++;
        if (knkind[i] == GNK) itemp--;
    }
    v += item  * p_value[15+knight];
    v += itemp * p_value[15+pro_knight];

    item  = 0;
    itemp = 0;
    for (i = KNS_GI; i <= KNE_GI; i++) {
        if (knkind[i] == SGI) item++;
        if (knkind[i] == GGI) item--;
        if (knkind[i] == SNG) itemp++;
        if (knkind[i] == GNG) itemp--;
    }
    v += item  * p_value[15+silver];
    v += itemp * p_value[15+pro_silver];

    item  = 0;
    for (i = KNS_KI; i <= KNE_KI; i++) {
        if (knkind[i] == SKI) item++;
        if (knkind[i] == GKI) item--;
    }
    v += item  * p_value[15+gold];

    item  = 0;
    itemp = 0;
    for (i = KNS_KA; i <= KNE_KA; i++) {
        if (knkind[i] == SKA) item++;
        if (knkind[i] == GKA) item--;
        if (knkind[i] == SUM) itemp++;
        if (knkind[i] == GUM) itemp--;
    }
    v += item  * p_value[15+bishop];
    v += itemp * p_value[15+horse];

    item  = 0;
    itemp = 0;
    for (i = KNS_HI; i <= KNE_HI; i++) {
        if (knkind[i] == SHI) item++;
        if (knkind[i] == GHI) item--;
        if (knkind[i] == SRY) itemp++;
        if (knkind[i] == GRY) itemp--;
    }
    v += item  * p_value[15+rook];
    v += itemp * p_value[15+dragon];

    return v;
}

#if !defined(EVAL_MICRO)
int Position::make_list(int * pscore,/* int list0[NLIST], int list1[NLIST],*/ int listA[NLIST2], int listB[NLIST2]) const
{
    int /*sq,*/ i, score, sq_bk0, sq_bk1/**/,convertedsq,sq_wk0,sq_wk1,j,/*nhand_b,nhand_w,*/nlist;

    score  = 0;
    nlist = 0;
    
    sq_bk0 = SQ_BKING;
    sq_bk1 = Inv(SQ_WKING);
    /**/
    sq_wk0 = SQ_WKING;
    sq_wk1 = Inv(SQ_BKING);

	//KKの評価
	score += fv_kk[sq_bk0][sq_wk0];

    //持ち駒のmake_list
    //先手の持ち駒
    i = I2HandPawn(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_pawn + j + 1];
        listA[nlist + j] = kp_hand_bpawn + j + 1;
        listB[nlist + j] = kp_hand_wpawn + j + 1;
    }
    nlist += i;

    i = I2HandLance(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_lance + j + 1];
        listA[nlist + j] = kp_hand_blance + j + 1;
        listB[nlist + j] = kp_hand_wlance + j + 1;
    }
    nlist += i;

    i = I2HandKnight(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_knight + j + 1];
        listA[nlist + j] = kp_hand_bknight + j + 1;
        listB[nlist + j] = kp_hand_wknight + j + 1;

    }
    nlist += i;

    i = I2HandSilver(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_silver + j + 1];
        listA[nlist + j] = kp_hand_bsilver + j + 1;
        listB[nlist + j] = kp_hand_wsilver + j + 1;
    }
    nlist += i;

    i = I2HandGold(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_gold + j + 1];
        listA[nlist + j] = kp_hand_bgold + j + 1;
        listB[nlist + j] = kp_hand_wgold + j + 1;
    }
    nlist += i;

    i = I2HandBishop(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_bishop + j + 1];
        listA[nlist + j] = kp_hand_bbishop + j + 1;
        listB[nlist + j] = kp_hand_wbishop + j + 1;
    }
    nlist += i;

    i = I2HandRook(HAND_B);
    for (j = 0; j < i; j++){
        score += kkp[sq_bk0][sq_wk0][kkp_hand_rook + j + 1];
        listA[nlist + j] = kp_hand_brook + j + 1;
        listB[nlist + j] = kp_hand_wrook + j + 1;
    }
    nlist += i;

    //後手の持ち駒
    i = I2HandPawn(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_pawn + j + 1];
        listA[nlist + j] = kp_hand_wpawn + j + 1;
        listB[nlist + j] = kp_hand_bpawn + j + 1;
    }
    nlist += i;

    i = I2HandLance(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_lance + j + 1];
        listA[nlist + j] = kp_hand_wlance + j + 1;
        listB[nlist + j] = kp_hand_blance + j + 1;
    }
    nlist += i;

    i = I2HandKnight(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_knight + j + 1];
        listA[nlist + j] = kp_hand_wknight + j + 1;
        listB[nlist + j] = kp_hand_bknight + j + 1;

    }
    nlist += i;

    i = I2HandSilver(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_silver + j + 1];
        listA[nlist + j] = kp_hand_wsilver + j + 1;
        listB[nlist + j] = kp_hand_bsilver + j + 1;
    }
    nlist += i;

    i = I2HandGold(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_gold + j + 1];
        listA[nlist + j] = kp_hand_wgold + j + 1;
        listB[nlist + j] = kp_hand_bgold + j + 1;
    }
    nlist += i;

    i = I2HandBishop(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_bishop + j + 1];
        listA[nlist + j] = kp_hand_wbishop + j + 1;
        listB[nlist + j] = kp_hand_bbishop + j + 1;
    }
    nlist += i;

    i = I2HandRook(HAND_W);
    for (j = 0; j < i; j++){
        score -= kkp[sq_bk1][sq_wk1][kkp_hand_rook + j + 1];
        listA[nlist + j] = kp_hand_wrook + j + 1;
        listB[nlist + j] = kp_hand_brook + j + 1;
    }
    nlist += i;

    int x, y/*,nlist*/;
    //nlist = NPiece;
    for (y = RANK_1; y <= RANK_9; y++) {
        for (x = FILE_1; x <= FILE_9; x++) {
            const int z = (x << 4)+y;
            convertedsq = conv_z2sq(z);
            switch (ban[z]) {
            case SFU:
                score += kkp[sq_bk0][sq_wk0][kkp_pawn + convertedsq];
                listA[nlist] = kp_bpawn + convertedsq;
                listB[nlist] = kp_wpawn + Inv(convertedsq);
                nlist++;
                break;
            case GFU:
                score -= kkp[sq_bk1][sq_wk1][kkp_pawn + Inv(convertedsq)];
                listA[nlist] = kp_wpawn + convertedsq;
                listB[nlist] = kp_bpawn + Inv(convertedsq);
                nlist++;
                break;
            case SKY:
                score += kkp[sq_bk0][sq_wk0][kkp_lance + convertedsq];
                listA[nlist] = kp_blance + convertedsq;
                listB[nlist] = kp_wlance + Inv(convertedsq);
                nlist++;
                break;
            case GKY:
                score -= kkp[sq_bk1][sq_wk1][kkp_lance + Inv(convertedsq)];
                listA[nlist] = kp_wlance + convertedsq;
                listB[nlist] = kp_blance + Inv(convertedsq);
                nlist++;
                break;
            case SKE:
                score += kkp[sq_bk0][sq_wk0][kkp_knight + convertedsq];
                listA[nlist] = kp_bknight + convertedsq;
                listB[nlist] = kp_wknight + Inv(convertedsq);
                nlist++;
                break;
            case GKE:
                score -= kkp[sq_bk1][sq_wk1][kkp_knight + Inv(convertedsq)];
                listA[nlist] = kp_wknight + convertedsq;
                listB[nlist] = kp_bknight + Inv(convertedsq);
                nlist++;
                break;
            case SGI:
                score += kkp[sq_bk0][sq_wk0][kkp_silver + convertedsq];
                listA[nlist] = kp_bsilver + convertedsq;
                listB[nlist] = kp_wsilver + Inv(convertedsq);
                nlist++;
                break;
            case GGI:
                score -= kkp[sq_bk1][sq_wk1][kkp_silver + Inv(convertedsq)];
                listA[nlist] = kp_wsilver + convertedsq;
                listB[nlist] = kp_bsilver + Inv(convertedsq);
                nlist++;
                break;
            case SKI:
            case STO:
            case SNY:
            case SNK:
            case SNG:
                score += kkp[sq_bk0][sq_wk0][kkp_gold + convertedsq];
                listA[nlist] = kp_bgold + convertedsq;
                listB[nlist] = kp_wgold + Inv(convertedsq);
                nlist++;
                break;
            case GKI:
            case GTO:
            case GNY:
            case GNK:
            case GNG:
                score -= kkp[sq_bk1][sq_wk1][kkp_gold + Inv(convertedsq)];
                listA[nlist] = kp_wgold + convertedsq;
                listB[nlist] = kp_bgold + Inv(convertedsq);
                nlist++;
                break;
            case SKA:
                score += kkp[sq_bk0][sq_wk0][kkp_bishop + convertedsq];
                listA[nlist] = kp_bbishop + convertedsq;
                listB[nlist] = kp_wbishop + Inv(convertedsq);
                nlist++;
                break;
            case GKA:
                score -= kkp[sq_bk1][sq_wk1][kkp_bishop + Inv(convertedsq)];
                listA[nlist] = kp_wbishop + convertedsq;
                listB[nlist] = kp_bbishop + Inv(convertedsq);
                nlist++;
                break;
            case SHI:
                score += kkp[sq_bk0][sq_wk0][kkp_rook + convertedsq];
                listA[nlist] = kp_brook + convertedsq;
                listB[nlist] = kp_wrook + Inv(convertedsq);
                nlist++;
                break;
            case GHI:
                score -= kkp[sq_bk1][sq_wk1][kkp_rook + Inv(convertedsq)];
                listA[nlist] = kp_wrook + convertedsq;
                listB[nlist] = kp_brook + Inv(convertedsq);
                nlist++;
                break;
            case SUM:
                score += kkp[sq_bk0][sq_wk0][kkp_horse + convertedsq];
                listA[nlist] = kp_bhorse + convertedsq;
                listB[nlist] = kp_whorse + Inv(convertedsq);
                nlist++;
                break;
            case GUM:
                score -= kkp[sq_bk1][sq_wk1][kkp_horse + Inv(convertedsq)];
                listA[nlist] = kp_whorse + convertedsq;
                listB[nlist] = kp_bhorse + Inv(convertedsq);
                nlist++;
                break;
            case SRY:
                score += kkp[sq_bk0][sq_wk0][kkp_dragon + convertedsq];
                /*listA[nlist] = kp_bhorse + convertedsq;
                listB[nlist] = kp_whorse + Inv(convertedsq);*/
                listA[nlist] = kp_bdragon + convertedsq;
                listB[nlist] = kp_wdragon + Inv(convertedsq);
                nlist++;
                break;
            case GRY:
                score -= kkp[sq_bk1][sq_wk1][kkp_dragon + Inv(convertedsq)];
                listA[nlist] = kp_wdragon + convertedsq;
                listB[nlist] = kp_bdragon + Inv(convertedsq);
                nlist++;
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
#if defined(CHK_DETAIL)
    if (sfu_nlist > 9 || gfu_nlist > 9) {
        Print();
        MYABORT();
    }
#endif
    assert( nlist <= NLIST2 );

    *pscore += score;
    return nlist /*+ NPiece*/;
}
#endif

int Position::evaluate(const Color us) const
{
#if !defined(EVAL_MICRO)
    int /*list0[NLIST], list1[NLIST], */listA[NLIST2], listB[NLIST2];
    int /*nlist,*/ nlist2, score, sq_bk, sq_wk, k0, k1, l0, l1, i, j, sum;
    static int count=0;
    count++;

    sum = 0;
    sq_bk = SQ_BKING;
    sq_wk = Inv( SQ_WKING );

    score = 0;
    nlist2 = make_list( &score,/* list0, list1 ,*/listA,listB);
    
    /*KPP*/
    for (i = 0; i < nlist2; i++)
    {
        k0 = listA[i];
        k1 = listB[i];
        for (j = 0; j <= i; j++)
        {
            l0 = listA[j];
            l1 = listB[j];
            sum += PcPcOnSq2(sq_bk, k0, l0);
            sum -= PcPcOnSq2(sq_wk, k1, l1);
        }
    }

    score += sum;
    score /= FV_SCALE;
    score += MATERIAL;
    score = (us == BLACK) ? score : -score;

    return score;
#else
    return (us == BLACK) ? MATERIAL : -(MATERIAL);
#endif
}

Value evaluate(const Position& pos, Value& margin)
{
    margin = VALUE_ZERO;
    const Color us = pos.side_to_move();
    return Value(pos.evaluate(us));
}
