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

#include <cassert>
#include <cstring>
#include <string>

#include "move.h"
#include "movegen.h"
#include "position.h"

using namespace std;

/// move_to_uci() converts a move to a string in coordinate notation
/// (g1f3, a7a8q, etc.). The only special case is castling moves, where we
/// print in the e1g1 notation in normal chess mode, and in e1h1 notation in
/// Chess960 mode. Instead internally Move is coded as "king captures rook".

#if defined(NANOHA)
//
// 将棋所のドキュメントからSFEN形式に関する記載抜粋
//
// 指し手の表記について解説します。
// 筋に関しては１から９までの数字で表記され、段に関してはaからiまでのアルファベット
// （１段目がa、２段目がb、・・・、９段目がi）というように表記されます。位置の表記は、
// この２つを組み合わせます。５一なら5a、１九なら1iとなります。
// そして、指し手に関しては、駒の移動元の位置と移動先の位置を並べて書きます。
// ７七の駒が７六に移動したのであれば、7g7fと表記します。（駒の種類を表記する
// 必要はありません。）
// 駒が成るときは、最後に+を追加します。８八の駒が２二に移動して成るなら8h2b+
// と表記します。
// 持ち駒を打つときは、最初に駒の種類を大文字で書き、それに*を追加し、さらに
// 打った場所を追加します。金を５二に打つ場合はG*5bとなります

// SFEN形式における指し手の表示で、段の数字をアルファベットに変換する配列
namespace {
    char mochiGomaSFENNameArray[] = {
        ' ', 'P','L','N','S','G','B','R',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ', 'P','L','N','S','G','B','R',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ', ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    };

    // CSA形式の駒を表現します。
    static const char* CsaPieceList[] = {
        "..", "FU", "KY", "KE", "GI", "KI", "KA", "HI",
        "OU", "TO", "NY", "NK", "NG", "--", "UM", "RY",
        NULL
    };

#if defined(__GNUC__)
    // 8C5C はCodePage932(シフトJIS)での文字コードなので、文字コードを変更する場合は要変更.
    static const char* piece_ja_str[] = {
        "　", "歩", "香", "桂",       "銀", "金", "角", "飛", 
        "玉", "と", "杏", "\x8C\x5C", "全", "〓", "馬", "龍",
    };
#else
    static const char* piece_ja_str[] = {
        "　", "歩", "香", "桂", "銀", "金", "角", "飛", 
        "玉", "と", "杏", "圭", "全", "〓", "馬", "龍", 
    };
#endif

    ///
    /// @brief CSA形式の駒文字列を駒の形式に変換します。
    ///
    static PieceType piece_type_from_csa(const string& csa,
                                         string::size_type index) {

        if (csa.length() < index + 2) {
            return PIECE_TYPE_NONE;
        }

        for (size_t i = 0; CsaPieceList[i] != NULL; ++i) {
            // ２文字のみを調べます。
            if (csa.compare(index, 2, CsaPieceList[i]) == 0) {
                return PieceType(i);
            }
        }

        return PIECE_TYPE_NONE;
    }
}

///
/// @brief 指し手をSFEN形式の文字列に変換します。
///
const string move_to_uci(Move m) {

    if (m == MOVE_NONE)
        return "(none)";

    if (m == MOVE_NULL)
        return "0000";

    const Square from = move_from(m);
    const Square to = move_to(m);
    char buf[8];

    if (!move_is_drop(m)) { // 通常の指し手なら
        int fromFile = file_of(from);
        int fromRank = rank_of(from);
        buf[0] = '1' + (fromFile - 1);
        buf[1] = 'a' + (fromRank - 1);
    } else {
        const Piece piece = move_piece(m);
        buf[0] = mochiGomaSFENNameArray[piece];
        buf[1] = '*';
    }

    int toFile = file_of(to);
    int toRank = rank_of(to);
    buf[2] = '1' + (toFile - 1);
    buf[3] = 'a' + (toRank - 1);
    if (is_promotion(m)) {
        buf[4] = '+';
        buf[5] = '\0';
    } else {
        buf[4] = '\0';
    }

    return string(buf);
}


///
/// @brief 指し手をCSA形式の文字列に変換します。
///
const string move_to_csa(Move m) {

    const Piece p = move_piece(m);
    const int from = move_from(m);
    const int to = move_to(m);
    char buf[128];
    string str;

    if (m == MOVE_NONE) {
        str = "(xxxx)";
    } else if (m == MOVE_NULL) {
        str = "(NULL)";
    } else if (move_is_drop(m)) {
        snprintf(buf, sizeof(buf), "00%02X%s", to, CsaPieceList[type_of(p)]);
        str = buf;
    } else {
        int np = type_of(p) | (is_promotion(m) ? PROMOTED : 0);
        snprintf(buf, sizeof(buf), "%02X%02X%s", from, to, CsaPieceList[np]);
        str = buf;
    }

    string colorStr = (color_of(p) == BLACK) ? "+" : "-";
    return (colorStr + str);
}

///
/// @brief CSA形式の文字列から指し手を取得します。
///
/// 形式は
///   +7776FU (先手が77の歩を76に動かす)
///   -0055HI (後手が55に飛車を打つ)
/// などになっています。
///
Move move_from_csa(const Position& pos, const string& csa) {

    // CSA形式の文字列は、文字数が決まっています。
    if (csa.length() < 7) {
        return MOVE_NONE;
    }

    if ((csa[0] != '+' && csa[0] != '-') ||
        (csa[1] < '0' || '9' < csa[1]) ||
        (csa[2] < '0' || '9' < csa[2]) ||
        (csa[3] < '0' || '9' < csa[3]) ||
        (csa[4] < '0' || '9' < csa[4])) {
        return MOVE_NONE;
    }

    const int mask = (csa[0] == '+' ? SENTE : GOTE);
    const File fromFile = File(csa[1] - '0');
    const Rank fromRank = Rank(csa[2] - '0');
    const File toFile = File(csa[3] - '0');
    const Rank toRank = Rank(csa[4] - '0');
    const Square from = make_square(fromFile, fromRank);
    const Square to = make_square(toFile, toRank);

    PieceType pieceType = piece_type_from_csa(csa, 5);
    Piece piece = Piece(pieceType | mask);

    if (from != SQ_NONE) {
        // 駒の移動の場合
        Piece fromPiece = pos.piece_on(from);
        int promote = 0;

        if (fromPiece != piece && (fromPiece | PROMOTED) == piece) {
            piece = Piece(piece & ~PROMOTED);
            promote = 1;
        }
        else if (fromPiece != piece) {
            // 不正な駒の指定
        }

        return cons_move(from, to, piece, pos.piece_on(to), promote);
    }
    else {
        // 駒打ちの場合
        return cons_move(from, to, piece, EMP);
    }
}


const string move_to_kif(Move m)
{
    const PieceType p = move_ptype(m);
    const int from = move_from(m);
    const int to = move_to(m);
    const string colorStr = (
        color_of(move_piece(m)) == BLACK ? "▲" :
        color_of(move_piece(m)) == WHITE ? "△" : "");
    char buf[32];

    // "76歩(77)"   ；移動
    // "22角(88)成" ；移動＋成
    // "24歩打"     ；駒打ち
    // "(null)"     ；MOVE_NULL
    if (m == MOVE_NONE) {
        return "(empty)";
    } else if (m == MOVE_NULL) {
        return "(null)";
    } else if (move_is_drop(m)) {
        // 駒打ち
        sprintf(buf, "%s%02x%s打", colorStr.c_str(), to, piece_ja_str[p]);
    } else {
        // 移動
        if (is_promotion(m)) {
            sprintf(buf, "%s%02x%s成(%02x)", colorStr.c_str(), to, piece_ja_str[p], from);
        } else {
            sprintf(buf, "%s%02x%s(%02x)", colorStr.c_str(), to, piece_ja_str[p], from);
        }
    }

    return buf;
}

void move_fprint(FILE *fp, Move m, int rotate)
{
    if (fp == NULL) return;
    if (m == MOVE_NONE) {
        foutput_log(fp, "パス　　　");
    }
    int to = move_to(m);
    int from = move_from(m);
    int rto = to;
    int rfrom = from;
    int x, y;
    switch (rotate) {
    case 0:    // 入換えなし
        break;
    case 2:    // 左右入換え
        x = to & 0xF0;
        y = to & 0x0F;
        rto = (0xA0 - x) + y;
        x = from & 0xF0;
        y = from & 0x0F;
        rfrom = (0xA0 - x) + y;
        break;
    case 1:    // 左右入換え＋上下入換え
        x = to & 0xF0;
        y = to & 0x0F;
        rto = (0xA0 - x) + (10 - y);
        x = from & 0xF0;
        y = from & 0x0F;
        rfrom = (0xA0 - x) + (10 - y);
        break;
    case 3:    // 上下入換え
        x = to & 0xF0;
        y = to & 0x0F;
        rto = x + (10 - y);
        x = from & 0xF0;
        y = from & 0x0F;
        rfrom = x + (10 - y);
        break;
    default:
        break;
    }

    foutput_log(fp, "%02x", rto);
    foutput_log(fp, "%2s", piece_ja_str[move_ptype(m)]);
    if (is_promotion(m)) {
        foutput_log(fp, "成");
    }
    if (from<OU) {
        foutput_log(fp, "打　");
    } else {
        foutput_log(fp, "(%02x)", rfrom);
    }
    if (!is_promotion(m)) {
        foutput_log(fp, "　");
    }
}

#else
const string move_to_uci(Move m, bool chess960) {

    Square from = move_from(m);
    Square to = move_to(m);
    string promotion;

    if (m == MOVE_NONE)
        return "(none)";

    if (m == MOVE_NULL)
        return "0000";

    if (is_castle(m) && !chess960)
        to = from + (file_of(to) == FILE_H ? Square(2) : -Square(2));

    if (is_promotion(m))
        promotion = char(tolower(piece_type_to_char(promotion_piece_type(m))));

    return square_to_string(from) + square_to_string(to) + promotion;
}
#endif

/// move_from_uci() takes a position and a string representing a move in
/// simple coordinate notation and returns an equivalent Move if any.
/// Moves are guaranteed to be legal.
#if defined(NO_NARAZU)
bool MOVE_FROM_USI_FLAG;
#endif
Move move_from_uci(const Position& pos, const string& str) {

#if defined(NANOHA)
#if !defined(NO_NARAZU)
    for (MoveList<MV_LEGAL> ml(pos); !ml.end(); ++ml)
        if (str == move_to_uci(ml.move()))
            return ml.move();

    return MOVE_NONE;
#else
	MOVE_FROM_USI_FLAG = true;
	for (MoveList<MV_LEGAL> ml(pos); !ml.end(); ++ml)
		if (str == move_to_uci(ml.move())){
			MOVE_FROM_USI_FLAG = false;
			return ml.move();
		}
		else{}

		MOVE_FROM_USI_FLAG = false;
		return MOVE_NONE;
#endif
#else
    for (MoveList<MV_LEGAL> ml(pos); !ml.end(); ++ml)
        if (str == move_to_uci(ml.move(), pos.is_chess960()))
            return ml.move();

    return MOVE_NONE;
#endif
}

#if defined(NO_NARAZU)
extern bool get_USI_flag(void){
	return MOVE_FROM_USI_FLAG;
}
#endif

/// move_to_san() takes a position and a move as input, where it is assumed
/// that the move is a legal move for the position. The return value is
/// a string containing the move in short algebraic notation.

#if defined(NANOHA)
// TODO:とりあえず、USI形式で返す
const string move_to_san(Position& pos, Move m) {
    return move_to_uci(m);
}
#else
const string move_to_san(Position& pos, Move m) {

    if (m == MOVE_NONE)
        return "(none)";

    if (m == MOVE_NULL)
        return "(null)";

    assert(is_ok(m));

    Bitboard attackers;
    bool ambiguousMove, ambiguousFile, ambiguousRank;
    Square sq, from = move_from(m);
    Square to = move_to(m);
    PieceType pt = type_of(pos.piece_on(from));
    string san;

    if (is_castle(m))
        san = (move_to(m) < move_from(m) ? "O-O-O" : "O-O");
    else
    {
        if (pt != PAWN)
        {
            san = piece_type_to_char(pt);

            // Disambiguation if we have more then one piece with destination 'to'
            // note that for pawns is not needed because starting file is explicit.
            attackers = pos.attackers_to(to) & pos.pieces(pt, pos.side_to_move());
            clear_bit(&attackers, from);
            ambiguousMove = ambiguousFile = ambiguousRank = false;

            while (attackers)
            {
                sq = pop_1st_bit(&attackers);

                if (file_of(sq) == file_of(from))
                    ambiguousFile = true;

                if (rank_of(sq) == rank_of(from))
                    ambiguousRank = true;

                ambiguousMove = true;
            }

            if (ambiguousMove)
            {
                if (!ambiguousFile)
                    san += file_to_char(file_of(from));
                else if (!ambiguousRank)
                    san += rank_to_char(rank_of(from));
                else
                    san += square_to_string(from);
            }
        }

        if (pos.is_capture(m))
        {
            if (pt == PAWN)
                san += file_to_char(file_of(from));

            san += 'x';
        }

        san += square_to_string(to);

        if (is_promotion(m))
        {
            san += '=';
            san += piece_type_to_char(promotion_piece_type(m));
        }
    }

    // The move gives check? We don't use pos.move_gives_check() here
    // because we need to test for a mate after the move is done.
    StateInfo st;
    pos.do_move(m, st);
    if (pos.in_check())
        san += pos.is_mate() ? "#" : "+";
    pos.undo_move(m);

    return san;
}
#endif
