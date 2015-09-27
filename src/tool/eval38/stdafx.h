// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#include <stdio.h>
#include <tchar.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <io.h>
#include <conio.h>

#define PcOnSq(k,i)         pc_on_sq[k][(i)*((i)+3)/2]
#define PcPcOnSq(k,i,j)     pc_on_sq[k][(i)*((i)+1)/2+(j)]
#define PcPcOnSq2(k,i,j)    pc_on_sq2[k][(i) * fe_end + (j)]

enum {
    f_hand_pawn = 0,
    e_hand_pawn = 19,
    f_hand_lance = 38,
    e_hand_lance = 43,
    f_hand_knight = 48,
    e_hand_knight = 53,
    f_hand_silver = 58,
    e_hand_silver = 63,
    f_hand_gold = 68,
    e_hand_gold = 73,
    f_hand_bishop = 78,
    e_hand_bishop = 81,
    f_hand_rook = 84,
    e_hand_rook = 87,
    fe_hand_end = 90,
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
    fe_end = 1476,

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
enum { pos_n = fe_end * (fe_end + 1) / 2 };

enum { nhand = 7, nfile = 9, nrank = 9, nsquare = 81 };


// TODO: プログラムに必要な追加ヘッダーをここで参照してください。
