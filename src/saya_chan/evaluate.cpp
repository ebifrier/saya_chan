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
#define PcOnSq2(k,i)         pc_on_sq[k][(i)*((i)+3)/2]
//KPP
#define PcPcOnSq(k,i,j)     pc_pc_on_sq[k][(i)*((i)+1)/2+(j)]
#define PcPcOnSqAny(k,i,j) ( i >= j ? PcPcOnSq(k,i,j) : PcPcOnSq(k,j,i) )

#define PcPcOnSq2(k,i,j)    pc_pc_on_sq2[k][i * kp_end + j]

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
    //short fv_pp[pp_bend][pp_end];
    //short fv_kp[nsquare][kp_end];
    //short pc_pc_on_sq[nsquare][kp_end * (kp_end + 1) / 2];
    short kkp[nsquare][nsquare][kkp_end];
    short pc_pc_on_sq2[nsquare][kp_end * (kp_end + 1)];
	short fv_kk[nsquare][nsquare];
	short fv_pp[kp_end][kp_end];
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

void Position::init_evaluate()
{
    int iret=0;
#if !defined(EVAL_MICRO)
    FILE *fp;
    FILE *fp2;
    const char *fname = "評価ベクトル";
    const char *fname2 = "bonanza評価ベクトル";

    do {
        size_t size;
        size_t size2;
        fname = "fv_mini.bin"/*FV_BIN*/;
        //fname2 = "fv.bin";
        fname2 = "eval38.bin";
        //fp = fopen(fname, "rb");
        fp2 = fopen(fname2, "rb");
        //if ( fp == NULL ) { iret = -2; break;}
        if (fp2 == NULL) { iret = -2; break; }

		//PP-nanoha
       /* size = pp_bend * pp_end;
        if ( fread( fv_pp, sizeof(short), size, fp ) != size )
        {
            iret = -2;
            break;
        }*/

		//KP
        /*size = nsquare * kp_end;
        if ( fread( fv_kp, sizeof(short), size, fp ) != size ) {
            iret = -2;
            break;
        }*/

        //KPP
        size2 = nsquare * (kp_end * (kp_end + 1) );
        if (fread(pc_pc_on_sq2, sizeof(short), size2, fp2) != size2)
        {
            iret = -2;
            break;
        }
        //KKP
        size2 =  nsquare * nsquare * kkp_end;
        if (fread(kkp, sizeof(short), size2, fp2) != size2)
        {
            iret = -2;
            break;
        }

		//KK
		size2 = nsquare * nsquare;
		if (fread(fv_kk, sizeof(short), size2, fp2) != size2)
		{
			iret = -2;
			break;
		}

		//PP
		/*size2 = kp_end * kp_end;
		if (fread(fv_pp, sizeof(short), size2, fp2) != size2)
		{
			iret = -2;
			break;
		}*/

        /*if (fgetc(fp) != EOF) {
            iret = -2;
            break;
        }*/
        if(fgetc(fp2) != EOF) {
            iret = -2;
            break;
        }
    } while (0);
    //if (fp) fclose( fp );
    if (fp2) fclose(fp2);

    if (iret < 0) {
#if !defined(NDEBUG)
        std::cerr << "Can't load " FV_BIN "." << std::endl;
#endif
#if defined(CSADLL) || defined(CSA_DIRECT)
        ::MessageBox(NULL, "評価ベクトルがロードできません\n終了します", "Error!", MB_OK);
        exit(1);
#endif    // defined(CSA_DLL) || defined(CSA_DIRECT)
    }
#endif

    for (int i = 0; i < 31; i++) { p_value[i]       = 0; }

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

    /**/
    
    //int sfu_list0[9];
    //int sfu_list1[9];
    //int sfu_nlist = 0;
    //int gfu_list0[9];
    //int gfu_list1[9];
    //int gfu_nlist = 0;
    //int sky_list0[4];
    //int sky_list1[4];
    //int sky_nlist = 0;
    //int gky_list0[4];
    //int gky_list1[4];
    //int gky_nlist = 0;
    //int ske_list0[4];
    //int ske_list1[4];
    //int ske_nlist = 0;
    //int gke_list0[4];
    //int gke_list1[4];
    //int gke_nlist = 0;
    //int sgi_list0[4];
    //int sgi_list1[4];
    //int sgi_nlist = 0;
    //int ggi_list0[4];
    //int ggi_list1[4];
    //int ggi_nlist = 0;
    // 金の動きをする駒の数なので最大金＋銀＋桂＋香＋歩の枚数
    //int ski_list0[34];
    //int ski_list1[34];
    //int ski_nlist = 0;
    //int gki_list0[34];    
    //int gki_list1[34];
    //int gki_nlist = 0;
    //int ska_list0[2];
    //int ska_list1[2];
    ////int gka_list0[2];
    //int gka_list1[2];
    //int gka_nlist = 0;
    //int shi_list0[2];
    //int shi_list1[2];
    //int shi_nlist = 0;
    //int ghi_list0[2];
    //int ghi_list1[2];
    //int ghi_nlist = 0;
    //int sum_list0[2];
    //int sum_list1[2];
    //int sum_nlist = 0;
    //int gum_list0[2];
    //int gum_list1[2];
    //int gum_nlist = 0;
    //int sry_list0[2];
    //int sry_list1[2];
    //int sry_nlist = 0;
    //int gry_list0[2];
    //int gry_list1[2];
    //int gry_nlist = 0;

    /*int sfu_listA[9];
    int sfu_listB[9];
    int gfu_listA[9];
    int gfu_listB[9];
    int sky_listA[4];
    int sky_listB[4];
    int gky_listA[4];
    int gky_listB[4];
    int ske_listA[4];
    int ske_listB[4];
    int gke_listA[4];
    int gke_listB[4];
    int sgi_listA[4];
    int sgi_listB[4];
    int ggi_listA[4];
    int ggi_listB[4];*/
     //金の動きをする駒の数なので最大金＋銀＋桂＋香＋歩の枚数
    /*int ski_listA[34];
    int ski_listB[34];
    int gki_listA[34];
    int gki_listB[34];
    int ska_listA[2];
    int ska_listB[2];
    int gka_listA[2];
    int gka_listB[2];
    int shi_listA[2];
    int shi_listB[2];
    int ghi_listA[2];
    int ghi_listB[2];
    int sum_listA[2];
    int sum_listB[2];
    int gum_listA[2];
    int gum_listB[2];
    int sry_listA[2];
    int sry_listB[2];
    int gry_listA[2];
    int gry_listB[2];*/

    int x, y/*,nlist*/;
    //nlist = NPiece;
    for (y = RANK_1; y <= RANK_9; y++) {
        for (x = FILE_1; x <= FILE_9; x++) {
            const int z = (x << 4)+y;
            /*sq = NanohaTbl::z2sq[z];*/
            convertedsq = conv_z2sq(z);
            switch (ban[z]) {
            case SFU:
                /*sfu_list0[sfu_nlist] = pp_bpawn + sq;
                sfu_list1[sfu_nlist] = pp_wpawn + Inv(sq);*/
                /*score += PcOnSq(sq_bk0, kp_bpawn + sq);
                score -= PcOnSq(sq_bk1, kp_wpawn + Inv(sq));*/
                //sfu_listA[sfu_nlist] = kp_bpawn + convertedsq;
                //sfu_listB[sfu_nlist] = kp_wpawn + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_pawn + convertedsq];
                /*assert(sfu_list0[sfu_nlist] >= kp_hand_end);
                assert(sfu_list1[sfu_nlist] >= kp_hand_end);
                sfu_nlist += 1;*/
                listA[nlist] = kp_bpawn + convertedsq;
                listB[nlist] = kp_wpawn + Inv(convertedsq);
                nlist++;
                break;
            case GFU:
                /*gfu_list0[gfu_nlist] = pp_wpawn + sq;
                gfu_list1[gfu_nlist] = pp_bpawn + Inv(sq);*/
                //gfu_listA[gfu_nlist] = kp_wpawn + convertedsq;
                //gfu_listB[gfu_nlist] = kp_bpawn + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_pawn + Inv(convertedsq)];
                /*score += PcOnSq(sq_bk0, kp_wpawn + sq);
                score -= PcOnSq(sq_bk1, kp_bpawn + Inv(sq));*/
                /*assert(gfu_list0[gfu_nlist] >= kp_hand_end);
                assert(gfu_list1[gfu_nlist] >= kp_hand_end);
                gfu_nlist += 1;*/
                listA[nlist] = kp_wpawn + convertedsq;
                listB[nlist] = kp_bpawn + Inv(convertedsq);
                nlist++;
                break;
            case SKY:
                /*sky_list0[sky_nlist] = pp_blance + sq;
                sky_list1[sky_nlist] = pp_wlance + Inv(sq);*/
                //sky_listA[sky_nlist] = kp_blance + convertedsq;
                //sky_listB[sky_nlist] = kp_wlance + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_lance + convertedsq];
                //score += PcOnSq(sq_bk0, kp_blance + sq);
                //score -= PcOnSq(sq_bk1, kp_wlance + Inv(sq));
                //sky_nlist += 1;
                listA[nlist] = kp_blance + convertedsq;
                listB[nlist] = kp_wlance + Inv(convertedsq);
                nlist++;
                break;
            case GKY:
                /*gky_list0[gky_nlist] = pp_wlance + sq;
                gky_list1[gky_nlist] = pp_blance + Inv(sq);*/
                //gky_listA[gky_nlist] = kp_wlance + convertedsq;
                //gky_listB[gky_nlist] = kp_blance + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_lance + Inv(convertedsq)];
                /*score += PcOnSq(sq_bk0, kp_wlance + sq);
                score -= PcOnSq(sq_bk1, kp_blance + Inv(sq));*/
                //gky_nlist += 1;
                listA[nlist] = kp_wlance + convertedsq;
                listB[nlist] = kp_blance + Inv(convertedsq);
                nlist++;
                break;
            case SKE:
                //ske_list0[ske_nlist] = pp_bknight + sq;
                //ske_list1[ske_nlist] = pp_wknight + Inv(sq);
                //ske_listA[ske_nlist] = kp_bknight + convertedsq;
                //ske_listB[ske_nlist] = kp_wknight + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_knight + convertedsq];
                //score += PcOnSq(sq_bk0, kp_bknight + sq);
                //score -= PcOnSq(sq_bk1, kp_wknight + Inv(sq));
                //ske_nlist += 1;
                listA[nlist] = kp_bknight + convertedsq;
                listB[nlist] = kp_wknight + Inv(convertedsq);
                nlist++;
                break;
            case GKE:
                //gke_list0[gke_nlist] = pp_wknight + sq;
                //gke_list1[gke_nlist] = pp_bknight + Inv(sq);
                //gke_listA[gke_nlist] = kp_wknight + convertedsq;
                //gke_listB[gke_nlist] = kp_bknight + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_knight + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wknight + sq);
                //score -= PcOnSq(sq_bk1, kp_bknight + Inv(sq));
                //gke_nlist += 1;
                listA[nlist] = kp_wknight + convertedsq;
                listB[nlist] = kp_bknight + Inv(convertedsq);
                nlist++;
                break;
            case SGI:
                //sgi_list0[sgi_nlist] = pp_bsilver + sq;
                //sgi_list1[sgi_nlist] = pp_wsilver + Inv(sq);
                //sgi_listA[sgi_nlist] = kp_bsilver + convertedsq;
                //sgi_listB[sgi_nlist] = kp_wsilver + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_silver + convertedsq];
                //score += PcOnSq(sq_bk0, kp_bsilver + sq);
                //score -= PcOnSq(sq_bk1, kp_wsilver + Inv(sq));
                //sgi_nlist += 1;
                listA[nlist] = kp_bsilver + convertedsq;
                listB[nlist] = kp_wsilver + Inv(convertedsq);
                nlist++;
                break;
            case GGI:
                //ggi_list0[ggi_nlist] = pp_wsilver + sq;
                //ggi_list1[ggi_nlist] = pp_bsilver + Inv(sq);
                //ggi_listA[ggi_nlist] = kp_wsilver + convertedsq;
                //ggi_listB[ggi_nlist] = kp_bsilver + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_silver + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wknight + sq);
                //score -= PcOnSq(sq_bk1, kp_bknight + Inv(sq));
                //ggi_nlist += 1;
                listA[nlist] = kp_wsilver + convertedsq;
                listB[nlist] = kp_bsilver + Inv(convertedsq);
                nlist++;
                break;
            case SKI:
            case STO:
            case SNY:
            case SNK:
            case SNG:
                //ski_list0[ski_nlist] = pp_bgold + sq;
                //ski_list1[ski_nlist] = pp_wgold + Inv(sq);
                //ski_listA[ski_nlist] = kp_bgold + convertedsq;
                //ski_listB[ski_nlist] = kp_wgold + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_gold + convertedsq];
                //score += PcOnSq(sq_bk0, kp_bgold + sq);
                //score -= PcOnSq(sq_bk1, kp_wgold + Inv(sq));
                //ski_nlist += 1;
                listA[nlist] = kp_bgold + convertedsq;
                listB[nlist] = kp_wgold + Inv(convertedsq);
                nlist++;
                break;
            case GKI:
            case GTO:
            case GNY:
            case GNK:
            case GNG:
                //gki_list0[gki_nlist] = pp_wgold + sq;
                //gki_list1[gki_nlist] = pp_bgold + Inv(sq);
                //gki_listA[gki_nlist] = kp_wgold + convertedsq;
                //gki_listB[gki_nlist] = kp_bgold + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_gold + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wgold + sq);
                //score -= PcOnSq(sq_bk1, kp_bgold + Inv(sq));
                //gki_nlist += 1;
                listA[nlist] = kp_wgold + convertedsq;
                listB[nlist] = kp_bgold + Inv(convertedsq);
                nlist++;
                break;
            case SKA:
                //ska_list0[ska_nlist] = pp_bbishop + sq;
                //ska_list1[ska_nlist] = pp_wbishop + Inv(sq);
                //ska_listA[ska_nlist] = kp_bbishop + convertedsq;
                //ska_listB[ska_nlist] = kp_wbishop + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_bishop + convertedsq];
                //score += PcOnSq(sq_bk0, kp_bbishop + sq);
                //score -= PcOnSq(sq_bk1, kp_wbishop + Inv(sq));
                //ska_nlist += 1;
                listA[nlist] = kp_bbishop + convertedsq;
                listB[nlist] = kp_wbishop + Inv(convertedsq);
                nlist++;
                break;
            case GKA:
                //gka_list0[gka_nlist] = pp_wbishop + sq;
                //gka_list1[gka_nlist] = pp_bbishop + Inv(sq);
                //gka_listA[gka_nlist] = kp_wbishop + convertedsq;
                //gka_listB[gka_nlist] = kp_bbishop + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_bishop + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wbishop + sq);
                //score -= PcOnSq(sq_bk1, kp_bbishop + Inv(sq));
                //gka_nlist += 1;
                listA[nlist] = kp_wbishop + convertedsq;
                listB[nlist] = kp_bbishop + Inv(convertedsq);
                nlist++;
                break;
            case SHI:
                //shi_list0[shi_nlist] = pp_brook + sq;
                //shi_list1[shi_nlist] = pp_wrook + Inv(sq);
                //shi_listA[shi_nlist] = kp_brook + convertedsq;
                //shi_listB[shi_nlist] = kp_wrook + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_rook + convertedsq];
                //score += PcOnSq(sq_bk0, kp_brook + sq);
                //score -= PcOnSq(sq_bk1, kp_wrook + Inv(sq));
                //shi_nlist += 1;
                listA[nlist] = kp_brook + convertedsq;
                listB[nlist] = kp_wrook + Inv(convertedsq);
                nlist++;
                break;
            case GHI:
                //ghi_list0[ghi_nlist] = pp_wrook + sq;
                //ghi_list1[ghi_nlist] = pp_brook + Inv(sq);
                //ghi_listA[ghi_nlist] = kp_wrook + convertedsq;
                //ghi_listB[ghi_nlist] = kp_brook + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_rook + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wrook + sq);
                //score -= PcOnSq(sq_bk1, kp_brook + Inv(sq));
                //ghi_nlist += 1;
                listA[nlist] = kp_wrook + convertedsq;
                listB[nlist] = kp_brook + Inv(convertedsq);
                nlist++;
                break;
            case SUM:
                //sum_list0[sum_nlist] = pp_bhorse + sq;
                //sum_list1[sum_nlist] = pp_whorse + Inv(sq);
                //sum_listA[sum_nlist] = kp_bhorse + convertedsq;
                //sum_listB[sum_nlist] = kp_whorse + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_horse + convertedsq];
                //score += PcOnSq(sq_bk0, kp_bhorse + sq);
                //score -= PcOnSq(sq_bk1, kp_whorse + Inv(sq));
                //sum_nlist += 1;
                listA[nlist] = kp_bhorse + convertedsq;
                listB[nlist] = kp_whorse + Inv(convertedsq);
                nlist++;
                break;
            case GUM:
                //gum_list0[gum_nlist] = pp_whorse + sq;
                //gum_list1[gum_nlist] = pp_bhorse + Inv(sq);
                //gum_listA[gum_nlist] = kp_whorse + convertedsq;
                //gum_listB[gum_nlist] = kp_bhorse + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_horse + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wbishop + sq);
                //score -= PcOnSq(sq_bk1, kp_bbishop + Inv(sq));
                //gum_nlist += 1;
                listA[nlist] = kp_whorse + convertedsq;
                listB[nlist] = kp_bhorse + Inv(convertedsq);
                nlist++;
                break;
            case SRY:
                //sry_list0[sry_nlist] = pp_bdragon + sq;
                //sry_list1[sry_nlist] = pp_wdragon + Inv(sq);
                //sry_listA[sry_nlist] = kp_bdragon + convertedsq;
                //sry_listB[sry_nlist] = kp_wdragon + Inv(convertedsq);
                score += kkp[sq_bk0][sq_wk0][kkp_dragon + convertedsq];
                //score += PcOnSq(sq_bk0, kp_bdragon + sq);
                //score -= PcOnSq(sq_bk1, kp_wdragon + Inv(sq));
                //sry_nlist += 1;
                listA[nlist] = kp_bhorse + convertedsq;
                listB[nlist] = kp_whorse + Inv(convertedsq);
                nlist++;
                break;
            case GRY:
                //gry_list0[gry_nlist] = pp_wdragon + sq;
                //gry_list1[gry_nlist] = pp_bdragon + Inv(sq);
                //gry_listA[gry_nlist] = kp_wdragon + convertedsq;
                //gry_listB[gry_nlist] = kp_bdragon + Inv(convertedsq);
                score -= kkp[sq_bk1][sq_wk1][kkp_dragon + Inv(convertedsq)];
                //score += PcOnSq(sq_bk0, kp_wbishop + sq);
                //score -= PcOnSq(sq_bk1, kp_bbishop + Inv(sq));
                //gry_nlist += 1;
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
#endif    //#if defined(CHK_DETAIL)
    //int nlist  =  0;

    //int nlist = NPiece;
    //int n2 = 0;
    //int list2[34];
    //// SFU
    //for (i = 0; i < sfu_nlist; i++, nlist++) {
    //    //list0[nlist] = sfu_list0[i];
    //    //list1[nlist] = sfu_list1[i];
    //    listA[nlist /*+ NPiece*/ ] = sfu_listA[i];
    //    list2[n2] = sfu_listB[i];
    //    n2++;
    //}

    //// GFU
    //for (i = 0; i < gfu_nlist; i++, nlist++) {
    //    //list0[nlist] = gfu_list0[i];
    //    //list1[nlist] = gfu_list1[i];
    //    listA[nlist /*+ NPiece*/] = gfu_listA[i];
    //    list2[n2] = gfu_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }

    //n2 = 0;
    //// SKY
    //for (i = 0; i < sky_nlist; i++, nlist++) {
    //    //list0[nlist] = sky_list0[i];
    //    //list1[nlist] = sky_list1[i];
    //    listA[nlist /*+ NPiece*/] = sky_listA[i];
    //    list2[n2] = sky_listB[i];
    //    n2++;
    //}

    //// GKY
    //for (i = 0; i < gky_nlist; i++, nlist++) {
    //    //list0[nlist] = gky_list0[i];
    //    //list1[nlist] = gky_list1[i];
    //    listA[nlist /*+ NPiece*/] = gky_listA[i];
    //    list2[n2] = gky_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }
    //
    //n2 = 0;
    //// SKE
    //for (i = 0; i < ske_nlist; i++, nlist++) {
    //    //list0[nlist] = ske_list0[i];
    //    //list1[nlist] = ske_list1[i];
    //    listA[nlist /*+ NPiece*/] = ske_listA[i];
    //    list2[n2] = ske_listB[i];
    //    n2++;
    //}

    //// GKE
    //for (i = 0; i < gke_nlist; i++, nlist++) {
    //    //list0[nlist] = gke_list0[i];
    //    //list1[nlist] = gke_list1[i];
    //    listA[nlist /*+ NPiece*/] = gke_listA[i];
    //    list2[n2] = gke_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }
    //
    //n2 = 0;
    //// SGI
    //for (i = 0; i < sgi_nlist; i++, nlist++) {
    //    //list0[nlist] = sgi_list0[i];
    //    //list1[nlist] = sgi_list1[i];
    //    listA[nlist /*+ NPiece*/] = sgi_listA[i];
    //    list2[n2] = sgi_listB[i];
    //    n2++;
    //}

    //// GGI
    //for (i = 0; i < ggi_nlist; i++, nlist++) {
    //    //list0[nlist] = ggi_list0[i];
    //    //list1[nlist] = ggi_list1[i];
    //    listA[nlist /*+ NPiece*/] = ggi_listA[i];
    //    list2[n2] = ggi_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }
    //
    //n2 = 0;
    //// SKI
    //for (i = 0; i < ski_nlist; i++, nlist++) {
    //    //list0[nlist] = ski_list0[i];
    //    //list1[nlist] = ski_list1[i];
    //    listA[nlist /*+ NPiece*/] = ski_listA[i];
    //    list2[n2] = ski_listB[i];
    //    n2++;
    //}

    //// GKI
    //for (i = 0; i < gki_nlist; i++, nlist++) {
    //    //list0[nlist] = gki_list0[i];
    //    //list1[nlist] = gki_list1[i];
    //    listA[nlist /*+ NPiece*/] = gki_listA[i];
    //    list2[n2] = gki_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }

    //n2 = 0;
    //// SKA
    //for (i = 0; i < ska_nlist; i++, nlist++) {
    //    //list0[nlist] = ska_list0[i];
    //    //list1[nlist] = ska_list1[i];
    //    listA[nlist /*+ NPiece*/] = ska_listA[i];
    //    list2[n2] = ska_listB[i];
    //    n2++;
    //}

    //// GKA
    //for (i = 0; i < gka_nlist; i++, nlist++) {
    //    //list0[nlist] = gka_list0[i];
    //    //list1[nlist] = gka_list1[i];
    //    listA[nlist /*+ NPiece*/] = gka_listA[i];
    //    list2[n2] = gka_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }
    //
    //n2 = 0;
    //// SUM
    //for (i = 0; i < sum_nlist; i++, nlist++) {
    //    //list0[nlist] = sum_list0[i];
    //    //list1[nlist] = sum_list1[i];
    //    listA[nlist /*+ NPiece*/] = sum_listA[i];
    //    list2[n2] = sum_listB[i];
    //    n2++;
    //}

    //// GUM
    //for (i = 0; i < gum_nlist; i++, nlist++) {
    //    //list0[nlist] = gum_list0[i];
    //    //list1[nlist] = gum_list1[i];
    //    listA[nlist /*+ NPiece*/] = gum_listA[i];
    //    list2[n2] = gum_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }
    //
    //n2 = 0;
    //// SHI
    //for (i = 0; i < shi_nlist; i++, nlist++) {
    //    //list0[nlist] = shi_list0[i];
    ////    list1[nlist] = shi_list1[i];
    //    listA[nlist /*+ NPiece*/] = shi_listA[i];
    //    list2[n2] = shi_listB[i];
    //    n2++;
    //}
    //// GHI
    //for (i = 0; i < ghi_nlist; i++, nlist++) {
    //    //list0[nlist] = ghi_list0[i];
    //    //list1[nlist] = ghi_list1[i];
    //    listA[nlist /*+ NPiece*/] = ghi_listA[i];
    //    list2[n2] = ghi_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }
    //
    //n2 = 0;
    //// SRY
    //for (i = 0; i < sry_nlist; i++, nlist++) {
    //    //list0[nlist] = sry_list0[i];
    //    //list1[nlist] = sry_list1[i];
    //    listA[nlist /*+ NPiece*/] = sry_listA[i];
    //    list2[n2] = sry_listB[i];
    //    n2++;
    //}

    //// GRY
    //for (i = 0; i < gry_nlist; i++, nlist++) {
    //    //list0[nlist] = gry_list0[i];
    //    //list1[nlist] = gry_list1[i];
    //    listA[nlist /*+ NPiece*/] = gry_listA[i];
    //    list2[n2] = gry_listB[i];
    //    n2++;
    //}
    //for (i = 0; i < n2; i++) { listB[nlist /*+ NPiece*/ - i - 1] = list2[i]; }

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
    /*
    sum += fv_kp[sq_bk][kp_hand_bpawn   + I2HandPawn(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wpawn   + I2HandPawn(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_bpawn   + I2HandPawn(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wpawn   + I2HandPawn(HAND_B)];

    sum += fv_kp[sq_bk][kp_hand_blance  + I2HandLance(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wlance  + I2HandLance(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_blance  + I2HandLance(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wlance  + I2HandLance(HAND_B)];

    sum += fv_kp[sq_bk][kp_hand_bknight + I2HandKnight(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wknight + I2HandKnight(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_bknight + I2HandKnight(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wknight + I2HandKnight(HAND_B)];

    sum += fv_kp[sq_bk][kp_hand_bsilver + I2HandSilver(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wsilver + I2HandSilver(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_bsilver + I2HandSilver(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wsilver + I2HandSilver(HAND_B)];

    sum += fv_kp[sq_bk][kp_hand_bgold   + I2HandGold(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wgold   + I2HandGold(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_bgold   + I2HandGold(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wgold   + I2HandGold(HAND_B)];

    sum += fv_kp[sq_bk][kp_hand_bbishop + I2HandBishop(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wbishop + I2HandBishop(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_bbishop + I2HandBishop(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wbishop + I2HandBishop(HAND_B)];

    sum += fv_kp[sq_bk][kp_hand_brook   + I2HandRook(HAND_B)];
    sum += fv_kp[sq_bk][kp_hand_wrook   + I2HandRook(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_brook   + I2HandRook(HAND_W)];
    sum -= fv_kp[sq_wk][kp_hand_wrook   + I2HandRook(HAND_B)];
    */


    /*listA[0] = kp_hand_bpawn + I2HandPawn(HAND_B);
    listA[1] = kp_hand_wpawn + I2HandPawn(HAND_W);
    listA[2] = kp_hand_blance + I2HandLance(HAND_B);
    listA[3] = kp_hand_wlance + I2HandLance(HAND_W);
    listA[4] = kp_hand_bknight + I2HandKnight(HAND_B);
    listA[5] = kp_hand_wknight + I2HandKnight(HAND_W);
    listA[6] = kp_hand_bsilver + I2HandSilver(HAND_B);
    listA[7] = kp_hand_wsilver + I2HandSilver(HAND_W);
    listA[8] = kp_hand_bgold + I2HandGold(HAND_B);
    listA[9] = kp_hand_wgold + I2HandGold(HAND_W);
    listA[10] = kp_hand_bbishop + I2HandBishop(HAND_B);
    listA[11] = kp_hand_wbishop + I2HandBishop(HAND_W);
    listA[12] = kp_hand_brook + I2HandRook(HAND_B);
    listA[13] = kp_hand_wrook + I2HandRook(HAND_W);

    listB[0] = kp_hand_bpawn + I2HandPawn(HAND_W);
    listB[1] = kp_hand_wpawn + I2HandPawn(HAND_B);
    listB[2] = kp_hand_blance + I2HandLance(HAND_W);
    listB[3] = kp_hand_wlance + I2HandLance(HAND_B);
    listB[4] = kp_hand_bknight + I2HandKnight(HAND_W);
    listB[5] = kp_hand_wknight + I2HandKnight(HAND_B);
    listB[6] = kp_hand_bsilver + I2HandSilver(HAND_W);
    listB[7] = kp_hand_wsilver + I2HandSilver(HAND_B);
    listB[8] = kp_hand_bgold + I2HandGold(HAND_W);
    listB[9] = kp_hand_wgold + I2HandGold(HAND_B);
    listB[10] = kp_hand_bbishop + I2HandBishop(HAND_W);
    listB[11] = kp_hand_wbishop + I2HandBishop(HAND_B);
    listB[12] = kp_hand_brook + I2HandRook(HAND_W);
    listB[13] = kp_hand_wrook + I2HandRook(HAND_B);*/

    score = 0;
    nlist2 = make_list( &score,/* list0, list1 ,*/listA,listB);
    
	//PP-nanoha
	/**/
    /*for ( i = 0; list0[i] < pp_bend; i++ )/
    {
        assert(i < nlist2);
        k0 = list0[i];
        for ( j = i+1; j < nlist2; j++ )
        {
            l0 = list0[j];
            sum += PcPcOn( k0, l0 );
        }
    }

    for ( ; i < nlist2; i++ )
    {
        k1 = list1i];
        assert(k1 < pp_bend);
        for ( j = i+1; j < nlist2; j++ )
        {
            l1 = list1[j];
            sum -= PcPcOn( k1, l1 );
        }
    }*/

    /*KPP*/
    //int nlist2 =  nlist + 14;

    /*for (i = 0; i < nlist2 ; i++)
    {
        k0 = listA[i];
        k1 = listB[i];
        for (j = 0; j <= i; j++)
        {
            
            l0 = listA[j];
            l1 = listB[j];
            assert(k0 >= l0 && k1 >= l1);
            sum += PcPcOnSq(sq_bk, k0, l0);
            sum -= PcPcOnSq(sq_wk, k1, l1);
            
        }
    }*/

	
	

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

			/*PP ??? */
			/*sum += fv_pp[k0][l0];
			sum -= fv_pp[k1][l1];*/
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
