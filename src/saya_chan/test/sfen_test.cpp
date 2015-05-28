#if defined(USE_GTEST)
#include <list>
#include <algorithm>
#include <gtest/gtest.h>

#include "../position.h"

#define SQ make_square

static const std::string INITIAL_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

using namespace std;

namespace test {

// strを空白で分割し、新たな配列を作成します。
static vector<string> split(const string& str) {

    istringstream iss(str);

    return vector<string>(
        istream_iterator<string>{ iss },
        istream_iterator<string>{ });
}

///
/// @brief 単純な指し手が着手できるか確認します。
///
TEST (SFENTest, simple_move_test)
{
    Position position(INITIAL_SFEN, 0);
    Move move;
    string sfen;

    move = cons_move(SQ(7,7), SQ(7,6), SFU, EMP);
    sfen = move_to_uci(move);
    ASSERT_EQ("7g7f", sfen);
    ASSERT_EQ(move, move_from_uci(position, sfen));

    move = cons_move(SQ(2,8), SQ(5,8), SHI, EMP);
    sfen = move_to_uci(move);
    ASSERT_EQ("2h5h", sfen);
    ASSERT_EQ(move, move_from_uci(position, sfen));
}

///
/// @brief SFEN形式の局面を読み込めるか確認します。
///
TEST (SFENTest, sfen_to_position_test)
{
    // 初期局面
    ASSERT_EQ(
        Position(INITIAL_SFEN, 0),
        Position(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", 0));

    // 最後の数字はなくてもよい
    ASSERT_EQ(
        Position(INITIAL_SFEN, 0),
        Position(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -", 0));

    // 持ち駒はないとダメ
    /*ASSERT_THROW(
        sfenToPosition("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b"),
        SfenException);

    // 筋が少ない
    ASSERT_THROW(
        sfenToPosition("9/9/p/9/9/9/9/9/9 b -"),
        SfenException);

    // 筋が多い
   ASSERT_THROW(
        sfenToPosition("9/9/p9/9/9/9/9/9/9 b -"),
        SfenException);

    // 段が多い
    ASSERT_THROW(
        sfenToPosition("9/9/9/9/9/9/9/9/9/p b -"),
        SfenException);*/

    // 段が少ないのは問題にしない
    ASSERT_EQ(
        Position("9/9/9/9/9/9/9/9/9 b -", 0),
        Position("9/9/9/9/9/9/9/9 b -", 0));

    // 成れない駒を成る
    /*ASSERT_THROW(
        sfenToPosition("9/9/9/9/9/9/9/+G8/9 b -"),
        SfenException);*/
}

// 文字列から指し手のリストを作成します。
static std::vector<Move> make_move_list(std::string const & sfen)
{
    Position position(INITIAL_SFEN, 0);
    vector<string> list = split(sfen);
    
    return move_list_from_uci(position, list.begin(), list.end());
}

///
/// @brief 正しいor正しくないSFEN形式の指し手を指せるか確認します。
///
TEST (SFENTest, sfen_to_move_test)
{
    // すべて正しい指し手
    vector<Move> moveList = make_move_list("1g1f 4a3b 6i7h");
    ASSERT_EQ(3, moveList.size());

    // 2つめの指し手が正しくない
    moveList = make_move_list("1g1f 4a8b 6i7h");
    ASSERT_EQ(1, moveList.size());
}

// 指し手を進めて、与えられた局面と同じになるか確認します。
static void position_and_move_test(const string& boardSFEN,
                                   const string& moveSFEN)
{
    std::list<StateInfo> stList;

    // 指し手から局面を作ります。
    auto movesSFEN = split(moveSFEN);
    movesSFEN.erase(
        std::remove(movesSFEN.begin(), movesSFEN.end(), ""),
        movesSFEN.end());

    // 指し手を読み込みます。
    Position position1(INITIAL_SFEN, 0);
    auto moves = move_list_from_uci(position1, movesSFEN.begin(), movesSFEN.end());
    ASSERT_EQ(movesSFEN.size(), moves.size());

    for (size_t i = 0; i < moves.size(); ++i) {
        stList.push_back(StateInfo());
        ASSERT_TRUE(position1.pl_move_is_legal(moves[i]));
        position1.do_move(moves[i], stList.back());
    }

    // 出力したSFEN形式の指し手が同じかどうかの確認もします。
    vector<string> result;
    for (size_t i = 0; i < moves.size(); ++i) {
        //std::cout << result.size() << m.str() << std::endl;
        result.push_back(move_to_uci(moves[i]));
    }
    ASSERT_EQ(movesSFEN, result);


    // 局面を直接読み込み、
    // 出力したSFEN形式の局面が同じかどうかの確認もします。
    Position position2(boardSFEN, 0);
    ASSERT_EQ(boardSFEN, position2.to_fen());


    // ２つの局面を比較します。
    ASSERT_EQ(position1, position2);
}

///
/// @brief 指し手を進めて指定の局面になるか確認します。
///
TEST (SFENTest, complex_test)
{
    position_and_move_test(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -",
        "");

    position_and_move_test(
        "l8/4+R4/1L2pgn2/1Ps1k2S1/P2p1SP2/LlP2b2p/ppNGP4/2S6/2KG5 w RBG6P2n2p",

        "7g7f 3c3d 2g2f 2b3c 8h3c+ 2a3c 5i6h 8b4b 6h7h 5a6b "
        "7h8h 6b7b 3i4h 2c2d 7i7h 4b2b 4i5h 3a3b 6g6f 7b8b "
        "9g9f 9c9d 5h6g 7a7b 3g3f 2d2e 2f2e 2b2e P*2f 2e2a "
        "4g4f 4a5b 4h4g 4c4d 2i3g B*1e 4g3h 1e2f 2h2i 6c6d "
        "8i7g 1c1d 1g1f 3b4c 8g8f P*2e 2i2g 2a2d 3h4g 4c5d "
        "5g5f 5d6c 5f5e 8c8d 6f6e 6d6e B*3b 6c7d 3b4a+ 6a5a "
        "4a3a 3d3e 5e5d 5c5d 3a3b 5a4b 3b5d 5b5c 5d5e 4b4c "
        "3f3e 9d9e 9f9e P*9f 9i9f 5c5d 5e5f P*9g 4g3f 8d8e "
        "8f8e 4d4e 4f4e 1d1e 1f1e 1a1e P*1f 1e1f 1i1f P*1e "
        "P*6c 7b6c 8e8d 1e1f L*8c 8b7b 3f2e 2f3g+ 2e2d 3g2g "
        "R*8b 7b6a 8b8a+ 6a5b N*5e R*9h 8h7i 5d5e 5f5e 2g4e "
        "5e7c 9h9i+ G*8i 9i8i 7i8i 5b5c 8a5a G*5b 7c6b 5c5d "
        "6b5b 6c5b 5a5b P*5c S*5f L*8f 8i7i B*4f P*5g P*8g 5f4e");
}

}
#endif
