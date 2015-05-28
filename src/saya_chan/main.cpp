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

#include <cstdio>
#include <cassert>
#include <iostream>
#include <string>

#if !defined(NANOHA)
#include "bitboard.h"
#include "evaluate.h"
#endif
#include "position.h"
#include "thread.h"
#include "search.h"
#include "ucioption.h"

// この２つの定義を消さないと、gtestでエラーが出る。
#undef Min
#undef Max

using namespace std;

#if defined(USE_GTEST)
#include <gtest/gtest.h>

#if defined(_DEBUG) || defined(DEBUG)
#pragma comment(lib, "gtestd.lib")
#else
#pragma comment(lib, "gtest.lib")
#endif
#endif

extern void uci_loop();
extern void benchmark(int argc, char* argv[]);
#if defined(NANOHA)
extern void bench_mate(int argc, char* argv[]);
extern void bench_genmove(int argc, char* argv[]);
extern void bench_eval(int argc, char* argv[]);
extern void solve_problem(int argc, char* argv[]);
extern void test_qsearch(int argc, char* argv[]);
extern void test_see(int argc, char* argv[]);
#else
extern void kpk_bitbase_init();
#endif

int main(int argc, char* argv[]) {

    // 標準入出力のバッファリングを無効にする
    // Disable IO buffering for C and C++ standard libraries
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    cout.rdbuf()->pubsetbuf(NULL, 0);
    cin.rdbuf()->pubsetbuf(NULL, 0);
#if defined(NANOHA)
    init_application_once();
#endif

    // Startup initializations
#ifndef NANOHA
    init_bitboards();
#endif
    Position::init();
#ifndef NANOHA
    kpk_bitbase_init();
#endif
    init_search();
    Threads.init();

#if 0 && defined(USE_GTEST)
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
#else

    if (argc < 2)
    {
        // 引数1個
#if !defined(NANOHA)
        // Print copyright notice
        cout << engine_name() << " by " << engine_authors() << endl;

        if (CpuHasPOPCNT)
            cout << "Good! CPU has hardware POPCNT." << endl;
#endif

        // USIのコマンド処理
        // Enter the UCI loop waiting for input
        uci_loop();
    }
#if defined(NANOHA)
    else if (string(argv[1]) == "bench" && argc > 2 
             && (string(argv[2]) == "mate1" || string(argv[2]) == "mate3")) {
        bench_mate(--argc, ++argv);
    }
    else if (string(argv[1]) == "bench" && argc > 2 && string(argv[2]) == "genmove") {
        bench_genmove(--argc, ++argv);
    }
    else if (string(argv[1]) == "bench" && argc > 2 && string(argv[2]) == "eval") {
        bench_eval(--argc, ++argv);
    }
    else if (string(argv[1]) == "qsearch") {
        test_qsearch(--argc, ++argv);
    }
    else if (string(argv[1]) == "see") {
        test_see(--argc, ++argv);
    }
    else if (string(argv[1]) == "problem") {
        solve_problem(--argc, ++argv);
    }
#endif
    else if (string(argv[1]) == "bench" && argc < 8)
        benchmark(argc, argv);
    else
#if defined(NANOHA)
    {
        cout << "Usage: Saya_chan [Options]" << endl;
        cout << "Options:\n"
                "   bench [hash size = 128] [threads = 1] "
                         "[limit = 12] [fen positions file = default] "
                         "[limited by depth, time, nodes or perft = depth]\n";
        cout << "   bench genmove "
                         "[fen positions file = default] "
                         "[display moves = no]\n";
        cout << "   bench mate1 "
                         "[fen positions file = default] "
                         "[loop = yes] [display = no]\n";
        cout << "   bench mate3 "
                         "[fen positions file = default] "
                         "[loop = yes] [display moves = no]" << endl;
    }
#else
    cout << "Usage: stockfish bench [hash size = 128] [threads = 1] "
         << "[limit = 12] [fen positions file = default] "
         << "[limited by depth, time, nodes or perft = depth]" << endl;
#endif

    Threads.exit();
    return 0;
#endif
}
