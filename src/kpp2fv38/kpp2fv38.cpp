
#include <cassert>
#include <cstdio>


#define WCSC25

// 評価関数関連定義
#ifdef WCSC25
#define FV_KPP "KPP_synthesized.bin"
#define FV_KKP "KKP_synthesized.bin"
#define FV_KK  "KK_synthesized.bin"
#else
#define FV_KPP "KPP.bin"
#define FV_KKP "KKP.bin"
#define FV_KK  "KK.bin"
#endif
#define MAKE_FV "KPP_synthesized2.bin"

#define FV_SCALE            32 

#define Inv(sq)             inverseSq(sq)
#define Asq(Bsq)            convertSq(Bsq)
#define PcPcOnSq(k,i,j)     pc_on_sq[k][(i)*((i)+1)/2+(j)]

enum {
	promote = 8, EMPTY = 0,	/* VC++でemptyがぶつかるので変更 */
	pawn, lance, knight, silver, gold, bishop, rook, king, pro_pawn,
	pro_lance, pro_knight, pro_silver, piece_null, horse, dragon
};

enum { nhand = 7, nfile = 9, nrank = 9, nsquare = 81 };

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

	a_f_pawn = fe_hand_end,
	a_e_pawn = a_f_pawn + 81,
	a_f_lance = a_e_pawn + 81,
	a_e_lance = a_f_lance + 81,
	a_f_knight = a_e_lance + 81,
	a_e_knight = a_f_knight + 81,
	a_f_silver = a_e_knight + 81,
	a_e_silver = a_f_silver + 81,
	a_f_gold = a_e_silver + 81,
	a_e_gold = a_f_gold + 81,
	a_f_bishop = a_e_gold + 81,
	a_e_bishop = a_f_bishop + 81,
	a_f_horse = a_e_bishop + 81,
	a_e_horse = a_f_horse + 81,
	a_f_rook = a_e_horse + 81,
	a_e_rook = a_f_rook + 81,
	a_f_dragon = a_e_rook + 81,
	a_e_dragon = a_f_dragon + 81,
	a_fe_end = a_e_dragon + 81,

	f_pawn = 81,
	e_pawn = 162,
	f_lance = 225,
	e_lance = 306,
	f_knight = 360,
	e_knight = 441,
	f_silver = 504,
	e_silver = 585,
	f_gold = 666,
	e_gold = 747,
	f_bishop = 828,
	e_bishop = 909,
	f_horse = 990,
	e_horse = 1071,
	f_rook = 1152,
	e_rook = 1233,
	f_dragon = 1314,
	e_dragon = 1395,
	fe_end = 1476
};

enum { pos_n = a_fe_end * (a_fe_end + 1) / 2 };

namespace {
	short apery_kpp[nsquare][a_fe_end][a_fe_end];
	int apery_kkp[nsquare][nsquare][a_fe_end];
	int apery_kk[nsquare][nsquare];

	short kpp[nsquare][fe_end][fe_end];
	short pc_on_sq[nsquare][pos_n];
	int kkp[nsquare][nsquare][fe_end];
	int kk[nsquare][nsquare];

	const int sqbegin[18] = {
		9,0,9,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0
	};

	const int sqend[18] = {
		81,72,81,72,81,63,81,81,81,81,81,81,81,81,81,81,81,81
	};

	const int aikppb[18] = {
		f_pawn,    e_pawn,    f_lance,  e_lance,  f_knight,  e_knight, 
		f_silver,  e_silver,  f_gold,   e_gold,   f_bishop,  e_bishop, 
		f_horse,   e_horse,   f_rook,   e_rook,   f_dragon,  e_dragon
	};

	const int aikppw[18] = {
		e_pawn,    f_pawn,    e_lance,  f_lance,  e_knight,  f_knight,
		e_silver,  f_silver,  e_gold,   f_gold,   e_bishop,  f_bishop,
		e_horse,   f_horse,   e_rook,   f_rook,   e_dragon,  f_dragon 
	};

	const int a_aikppb[18] = {
		a_f_pawn,    a_e_pawn,    a_f_lance,  a_e_lance,  a_f_knight,  a_e_knight,
		a_f_silver,  a_e_silver,  a_f_gold,   a_e_gold,   a_f_bishop,  a_e_bishop,
		a_f_horse,   a_e_horse,   a_f_rook,   a_e_rook,   a_f_dragon,  a_e_dragon 
	};

	const int a_aikppw[18] = {
		a_e_pawn,    a_f_pawn,    a_e_lance,  a_f_lance,  a_e_knight,  a_f_knight,
		a_e_silver,  a_f_silver,  a_e_gold,   a_f_gold,   a_e_bishop,  a_f_bishop,
		a_e_horse,   a_f_horse,   a_e_rook,   a_f_rook,   a_e_dragon,  a_f_dragon 
	};

}

inline int convertSq(const int Bsq) {
	return ((8 - (Bsq % 9)) * 9 + (Bsq / 9));
}

inline int inverseSq(const int Bsq) {
	return (nsquare - 1 - Bsq);
}

void main()
{
    const char *fname = "評価ベクトル";
	FILE *fp;
	size_t size;
	int iret = 0;

    // Aperyの評価を読み込む
	do {
		fname = FV_KPP;
		fp = fopen(fname, "rb");
		if (fp == NULL) { iret = -2; break; }

		size = nsquare * a_fe_end * a_fe_end;
		if (fread(apery_kpp, sizeof(short), size, fp) != size){
			iret = -2;
			break;
		}
		if (fgetc(fp) != EOF) {
			iret = -2;
			break;
		}
	} while (0);
	if (fp) fclose(fp);

    for (int sq = 0; sq < nfile * nrank; ++sq) {
        for (int i = 0; i < a_fe_end; i++) {
            for (int j = 0; j <= i; j++) {
                PcPcOnSq(sq, i, j) = apery_kpp[sq][i][j];
            }
        }
    }

    // MAKE_FVに書き出す
    do {
        fname = MAKE_FV;
        fp = fopen(fname, "wb");
        if (fp == NULL) { iret = -2; break; }

        //kpp
        size = nsquare * pos_n;
        if (fwrite(pc_on_sq, sizeof(short), size, fp) != size){
            iret = -2;
            break;
        }
    } while (0);
    if (fp) fclose(fp);

#if 0
	do {
		//KKP
		fname = FV_KKP;
		fp = fopen(fname, "rb");
		if (fp == NULL) { iret = -2; break; }

		size = nsquare * nsquare * a_fe_end;
		if (fread(apery_kkp, sizeof(int), size, fp) != size){
			iret = -2;
			break;
		}
		if (fgetc(fp) != EOF) {
			iret = -2;
			break;
		}
	} while (0);
	if (fp) fclose(fp);

	do {
		//kk
		fname = FV_KK;
		fp = fopen(fname, "rb");
		if (fp == NULL) { iret = -2; break; }

		size = nsquare * nsquare;
		if (fread(apery_kk, sizeof(int), size, fp) != size){
			iret = -2;
			break;
		}
		if (fgetc(fp) != EOF) {
			iret = -2;
			break;
		}
	} while (0);
	if (fp) fclose(fp);
	printf("読み込み成功\n");

//変換
	// KK
	for (int ksq = 0; ksq < 81; ksq++) {
		const int ksqA = Asq(ksq);

		for (int ksq2 = 0; ksq2 < 81; ksq2++) {
			const int ksq2A = Asq(ksq2);

			kk[ksq][ksq2] = apery_kk[ksqA][ksq2A];
		}
	}
	printf("　KK変換成功\n");

	// KKH
	for (int ksq = 0; ksq < 81; ksq++){
		const int ksqA = Asq(ksq);

		for (int ksq2 = 0; ksq2 < 81; ksq2++){
			const int ksq2A = Asq(ksq2);

			for (int i = 0; i < fe_hand_end; i++){ //p1駒種

				kkp[ksq][ksq2][i] = apery_kkp[ksqA][ksq2A][i];
			}
		}
	}

	// KKB
	for (int ksq = 0; ksq < 81; ksq++){
		const int ksqA = Asq(ksq); 

		for (int ksq2 = 0; ksq2 < 81; ksq2++){
			const int ksq2A = Asq(ksq2);

			for (int i = 0; i < 18; i++){ //p駒種
				const int begin = sqbegin[i];
				const int end = sqend[i];
				for (int isq = begin; isq < end; isq++){
					const int isqA = Asq(isq);
					const int k0  = aikppb[i] + isq;
					const int k0A = a_aikppb[i] + isqA;

					kkp[ksq][ksq2][k0] = apery_kkp[ksqA][ksq2A][k0A];
				}
			}
		}
	}
	printf("　KKP変換成功\n");

	//KHH
	for (int ksq = 0; ksq < 81; ksq++){
		const int ksqA = Asq(ksq);

		for (int i = 0; i < fe_hand_end; i++){ //p1駒種手駒

			for (int j = 0; j < fe_hand_end; j++){//p2駒種手駒

				kpp[ksq][i][j] = apery_kpp[ksqA][i][j];
			}
		}
	}


	//KBH
	for (int ksq = 0; ksq < 81; ksq++){
		const int ksqA = Asq(ksq);

		for (int i = 0; i < 18; i++){ //p1駒種
			const int ibeg = sqbegin[i];
			const int iend = sqend[i];
			for (int isq = ibeg; isq < iend; isq++){ //p1位置
				const int isqA = Asq(isq);
				const int k0 = aikppb[i] + isq;
				const int k0A = a_aikppb[i] + isqA;

				for (int j = 0; j < fe_hand_end; j++){//p2駒種手駒

					kpp[ksq][k0][j] = apery_kpp[ksqA][k0A][j];
					kpp[ksq][j][k0] = apery_kpp[ksqA][j][k0A];
				}
			}
		}
	}


	// KBB
	for (int ksq = 0; ksq < 81; ksq++){
		const int ksqA = Asq(ksq);

		for (int i = 0; i < 18; i++){ //p1駒種
			const int ibeg = sqbegin[i];
			const int iend = sqend[i];
			for (int isq = ibeg; isq < iend; isq++){ //p1位置
				const int isqA = Asq(isq);
				const int k0 = aikppb[i] + isq;
				const int k0A = a_aikppb[i] + isqA;

				for (int j = 0; j < 18; j++){ //p2駒種
					const int jbeg = sqbegin[j];
					const int jend = sqend[j];
					for (int jsq = jbeg; jsq < iend; jsq++){ //p2位置
						const int jsqA = Asq(jsq);
						const int l0 = aikppb[j] + jsq;
						const int l0A = a_aikppb[j] + jsqA;

						kpp[ksq][k0][l0] = apery_kpp[ksqA][k0A][l0A];
					}
				}
			}
		}
	}
	

	// KPPを三角テーブルに変換
	for (int k = 0; k < 81; k++) {
		for (int i = 0; i < fe_end; i++) {
			for (int j = 0; j < i; j++) {
				PcPcOnSq(k, i, j) = kpp[k][i][j];
			};
		};
	};
	printf("変換成功\n");



//MAKE_FVに書き出す
	do {
		fname = MAKE_FV;
		fp = fopen(fname, "wb");
		if (fp == NULL) { iret = -2; break; }
		
		//kpp
		size = nsquare * pos_n;
		if (fwrite(pc_on_sq, sizeof(short), size, fp) != size){
			iret = -2;
			break;
		}
		
		//kkp
		size = nsquare * nsquare * fe_end;
		if (fwrite(kkp, sizeof(int), size, fp) != size){
			iret = -2;
			break;
		}
		
		//kk
		size = nsquare * nsquare;
		if (fwrite(kk, sizeof(int), size, fp) != size){
			iret = -2;
			break;
		}
		if (fgetc(fp) != EOF) {
			iret = -2;
			break;
		}

	} while (0);
	if (fp) fclose(fp);
#endif

    printf("コンプリート\n");
}
