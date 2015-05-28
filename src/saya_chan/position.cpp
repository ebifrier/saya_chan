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
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(NANOHA)
#include <map>
#endif
#if !defined(NANOHA)
#include "bitcount.h"
#endif
#include "movegen.h"
#include "position.h"
#if !defined(NANOHA)
#include "psqtab.h"
#endif
#include "rkiss.h"
#include "thread.h"
#include "tt.h"
///#include "ucioption.h"
#if defined(NANOHA)
#if defined(EVAL_MICRO)
#include "param_micro.h"
#elif defined(EVAL_OLD)
#include "param_old.h"
#else
#include "param_new.h"
#endif
#endif
using std::string;
using std::cout;
using std::endl;

#if defined(NANOHA)
Key Position::zobrist[GRY+1][0x100];
#else
Key Position::zobrist[2][8][64];
Key Position::zobEp[64];
Key Position::zobCastle[16];
#endif
Key Position::zobSideToMove;    // 手番を区別する
Key Position::zobExclusion;        // NULL MOVEかどうか区別する

#if !defined(NANOHA)
Score Position::pieceSquareTable[16][64];
#endif

// Material values arrays, indexed by Piece
#if defined(NANOHA)
const Value PieceValueMidgame[32] = {
    VALUE_ZERO,
    Value(DPawn   *2),
    Value(DLance  *2),
    Value(DKnight *2),
    Value(DSilver *2),
    Value(DGold   *2),
    Value(DBishop *2),
    Value(DRook   *2),
    Value(DKing   *2), 
    Value(DPawn   +DProPawn),
    Value(DLance  +DProLance),
    Value(DKnight +DProKnight),
    Value(DSilver +DProSilver),
    Value(DGold   *2),
    Value(DBishop +DHorse),
    Value(DRook   +DDragon),
    Value(DKing   *2), 
    Value(DPawn   *2),
    Value(DLance  *2),
    Value(DKnight *2),
    Value(DSilver *2),
    Value(DGold   *2),
    Value(DBishop *2),
    Value(DRook   *2),
    Value(DKing   *2), 
    Value(DPawn   +DProPawn),
    Value(DLance  +DProLance),
    Value(DKnight +DProKnight),
    Value(DSilver +DProSilver),
    Value(DGold   *2),
    Value(DBishop +DHorse),
    Value(DRook   +DDragon),
};
const Value PieceValueEndgame[32] = {
    VALUE_ZERO,
    Value(DPawn   *2),
    Value(DLance  *2),
    Value(DKnight *2),
    Value(DSilver *2),
    Value(DGold   *2),
    Value(DBishop *2),
    Value(DRook   *2),
    Value(DKing   *2), 
    Value(DPawn   +DProPawn),
    Value(DLance  +DProLance),
    Value(DKnight +DProKnight),
    Value(DSilver +DProSilver),
    Value(DGold   *2),
    Value(DBishop +DHorse),
    Value(DRook   +DDragon),
    Value(DKing   *2), 
    Value(DPawn   *2),
    Value(DLance  *2),
    Value(DKnight *2),
    Value(DSilver *2),
    Value(DGold   *2),
    Value(DBishop *2),
    Value(DRook   *2),
    Value(DKing   *2), 
    Value(DPawn   +DProPawn),
    Value(DLance  +DProLance),
    Value(DKnight +DProKnight),
    Value(DSilver +DProSilver),
    Value(DGold   *2),
    Value(DBishop +DHorse),
    Value(DRook   +DDragon),
};
unsigned char Position::DirTbl[0xA0][0x100];    // 方向用[from][to]

#else
const Value PieceValueMidgame[17] = {
    VALUE_ZERO,
    PawnValueMidgame, KnightValueMidgame, BishopValueMidgame,
    RookValueMidgame, QueenValueMidgame,
    VALUE_ZERO, VALUE_ZERO, VALUE_ZERO,
    PawnValueMidgame, KnightValueMidgame, BishopValueMidgame,
    RookValueMidgame, QueenValueMidgame
};

const Value PieceValueEndgame[17] = {
    VALUE_ZERO,
    PawnValueEndgame, KnightValueEndgame, BishopValueEndgame,
    RookValueEndgame, QueenValueEndgame,
    VALUE_ZERO, VALUE_ZERO, VALUE_ZERO,
    PawnValueEndgame, KnightValueEndgame, BishopValueEndgame,
    RookValueEndgame, QueenValueEndgame
};
#endif

// Material values array used by SEE, indexed by PieceType
namespace {

#if !defined(NANOHA)
    // Bonus for having the side to move (modified by Joona Kiiski)
    const Score TempoValue = make_score(48, 22);

    // To convert a Piece to and from a FEN char
    const string PieceToChar(".PNBRQK  pnbrqk  ");
#endif

#if defined(NANOHA)
    // To convert a Piece to and from a FEN char
    const string PieceToChar(".PLNSGBRKPLNS.BR.plnsgbrkplns.br");

    struct PieceType2Str : public std::map<PieceType, std::string> {
    PieceType2Str() {
        operator[](PIECE_TYPE_NONE) = "* ";
        operator[](FU) = "FU";
        operator[](KY) = "KY";
        operator[](KE) = "KE";
        operator[](GI) = "GI";
        operator[](KI) = "KI";
        operator[](KA) = "KA";
        operator[](HI) = "HI";
        operator[](OU) = "OU";
        operator[](TO) = "TO";
        operator[](NY) = "NY";
        operator[](NK) = "NK";
        operator[](NG) = "NG";
        operator[](UM) = "UM";
        operator[](RY) = "RY";
    }
    string from_piece(Piece p, bool bc=true) const {
        if (p == EMP) return string(" * ");

        Color c = color_of(p);
        string str = bc ? ((c == BLACK) ? "+" : "-") : "";
        std::map<PieceType, std::string>::const_iterator it;
        it = find(type_of(p));
        if (it == end()) {
            std::cerr << "Error!" << endl;
            abort();
        }
        str += it->second;
        return str;
    }
    };

    struct PieceLetters : public std::map<char, Piece> {

        PieceLetters() {
            operator[]('K') = SOU;    operator[]('k') = GOU;
            operator[]('R') = SHI;    operator[]('r') = GHI;
            operator[]('B') = SKA;    operator[]('b') = GKA;
            operator[]('G') = SKI;    operator[]('g') = GKI;
            operator[]('S') = SGI;    operator[]('s') = GGI;
            operator[]('N') = SKE;    operator[]('n') = GKE;
            operator[]('L') = SKY;    operator[]('l') = GKY;
            operator[]('P') = SFU;    operator[]('p') = GFU;
            operator[]('.') = EMP;
        }

        string from_piece(Piece p) const {
            if (p == SOU) return string("K");
            if (p == GOU) return string("k");

            std::map<char, Piece>::const_iterator it;
            for (it = begin(); it != end(); ++it)
            if (it->second == (int(p) & ~PROMOTED)) {
                return ((int(p) & PROMOTED) ? string("+") : string(""))+string(1,it->first);
            }

            assert(false);
            return 0;
        }
    };

    PieceLetters pieceLetters;
    PieceType2Str pieceCSA;
#endif
}


#if !defined(NANOHA)
/// CheckInfo c'tor

CheckInfo::CheckInfo(const Position& pos) {

    Color them = flip(pos.side_to_move());
    Square ksq = pos.king_square(them);

    pinned = pos.pinned_pieces();
    dcCandidates = pos.discovered_check_candidates();

    checkSq[PAWN]   = pos.attacks_from<PAWN>(ksq, them);
    checkSq[KNIGHT] = pos.attacks_from<KNIGHT>(ksq);
    checkSq[BISHOP] = pos.attacks_from<BISHOP>(ksq);
    checkSq[ROOK]   = pos.attacks_from<ROOK>(ksq);
    checkSq[QUEEN]  = checkSq[BISHOP] | checkSq[ROOK];
    checkSq[KING]   = EmptyBoardBB;
}
#endif

/// Position c'tors. Here we always create a copy of the original position
/// or the FEN string, we want the new born Position object do not depend
/// on any external data so we detach state pointer from the source one.

Position::Position(const Position& pos, int th) {

    memcpy(this, &pos, sizeof(Position));
    threadID = th;
    nodes = 0;
#if defined(NANOHA)
    tnodes = 0;
#if defined(CHK_PERFORM)
    count_Mate1plyDrop = 0;        // 駒打ちで詰んだ回数
    count_Mate1plyMove = 0;        // 駒移動で詰んだ回数
    count_Mate3ply = 0;            // Mate3()で詰んだ回数
#endif // defined(CHK_PERFORM)
#endif

    assert(is_ok());
}

#if defined(NANOHA)
Position::Position(const string& fen, int th) {
#else
Position::Position(const string& fen, bool isChess960, int th) {
#endif

#if defined(NANOHA)
    from_fen(fen);
#else
    from_fen(fen, isChess960);
#endif
    threadID = th;
}

#if defined(NANOHA)
bool operator==(const Position &a, const Position &b) {
    return (a.thread() == b.thread() && a.to_fen() == b.to_fen());
}

bool operator!=(const Position &a, const Position &b) {
    return !(a == b);
}
#endif


/// Position::from_fen() initializes the position object with the given FEN
/// string. This function is not very robust - make sure that input FENs are
/// correct (this is assumed to be the responsibility of the GUI).

#if defined(NANOHA)
void Position::from_fen(const string& fenStr) {
#else
void Position::from_fen(const string& fenStr, bool isChess960) {
#endif
/*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields. The separator between fields is a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting with rank 8 and ending
      with rank 1; within each rank, the contents of each square are described from file A through file H.
      Following the Standard Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case letters ("PNBRQK")
      while Black take lowercase ("pnbrqk"). Blank squares are noted using digits 1 through 8 (the number
      of blank squares), and "/" separate ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise, this has one or more
      letters: "K" (White can castle kingside), "Q" (White can castle queenside), "k" (Black can castle
      kingside), and/or "q" (Black can castle queenside).

   4) En passant target square in algebraic notation. If there's no en passant target square, this is "-".
      If a pawn has just made a 2-square move, this is the position "behind" the pawn. This is recorded
      regardless of whether there is a pawn in position to make an en passant capture.

   5) Halfmove clock: This is the number of halfmoves since the last pawn advance or capture. This is used
      to determine if a draw can be claimed under the fifty-move rule.

   6) Fullmove number: The number of the full move. It starts at 1, and is incremented after Black's move.
*/

#if defined(NANOHA)
    char token;
    std::istringstream fen(fenStr);

    unsigned char tmp_ban[9][9] = {{'\0'}};
    int tmp_hand[GRY+1] = {0};

    clear();
    fen >> std::noskipws;

    // 1. Piece placement
    int dan = 0;
    int suji = 0; // この「筋」というのは、普通とは反転したものになる。
    while ((fen >> token) && !isspace(token))
    {
        if (token == '+') {
            // 成り駒
            fen >> token;
            if (pieceLetters.find(token) == pieceLetters.end()) goto incorrect_fen;
            tmp_ban[dan][suji++] = static_cast<unsigned char>(pieceLetters[token] | PROMOTED);
        } else if (pieceLetters.find(token) != pieceLetters.end())
        {
            tmp_ban[dan][suji++] = static_cast<unsigned char>(pieceLetters[token]);
        }
        else if (isdigit(token)) {
            suji += (token - '0');    // 数字の分空白
        } else if (token == '/') {
            if (suji != 9) goto incorrect_fen;
            suji = 0;
            dan++;
        } else {
            goto incorrect_fen;
        }
        if (dan > 9 || suji > 9)
            goto incorrect_fen;
    }
    if (dan != 9 && suji != 9)
        goto incorrect_fen;

    // 手番取得
    if (!fen.get(token) || (token != 'w' && token != 'b')) goto incorrect_fen;
    sideToMove = (token == 'b') ? BLACK : WHITE;
    // スペース飛ばす
    if (!fen.get(token) || token != ' ') goto incorrect_fen;

    // 持ち駒
    while ((fen >> token) && token != ' ') {
        int num = 1;
        if (token == '-') {
            break;
        } else if (isdigit(token)) {
            num = token - '0';
            fen >> token;
            if (isdigit(token)) {
                num = 10*num + token - '0';
                fen >> token;
            }
        }
        if (pieceLetters.find(token) == pieceLetters.end()) goto incorrect_fen;
        tmp_hand[pieceLetters[token]] = num;
    }
    init_position(tmp_ban, tmp_hand);

#else
    char col, row, token;
    size_t p;
    Square sq = SQ_A8;
    std::istringstream fen(fenStr);

    clear();
    fen >> std::noskipws;

    // 1. Piece placement
    while ((fen >> token) && !isspace(token))
    {
        if (token == '/')
            sq -= Square(16); // Jump back of 2 rows

        else if (isdigit(token))
            sq += Square(token - '0'); // Skip the given number of files

        else if ((p = PieceToChar.find(token)) != string::npos)
        {
            put_piece(Piece(p), sq);
            sq++;
        }
    }

    // 2. Active color
    fen >> token;
    sideToMove = (token == 'w' ? WHITE : BLACK);
    fen >> token;

    // 3. Castling availability
    while ((fen >> token) && !isspace(token))
        set_castling_rights(token);

    // 4. En passant square. Ignore if no pawn capture is possible
    if (   ((fen >> col) && (col >= 'a' && col <= 'h'))
        && ((fen >> row) && (row == '3' || row == '6')))
    {
        st->epSquare = make_square(File(col - 'a'), Rank(row - '1'));
        Color them = flip(sideToMove);

        if (!(attacks_from<PAWN>(st->epSquare, them) & pieces(PAWN, sideToMove)))
            st->epSquare = SQ_NONE;
    }
#endif

    // 5-6. Halfmove clock and fullmove number
#if defined(NANOHA)
    fen >> std::skipws >> startPosPly;
#else
    fen >> std::skipws >> st->rule50 >> startPosPly;
#endif

    // Convert from fullmove starting from 1 to ply starting from 0,
    // handle also common incorrect FEN with fullmove = 0.
#if defined(NANOHA)
// チェスと将棋で先手のBLACK/WHITEが違うが、そのままでいいか？
// ⇒将棋は駒落ちで手番が変わるので、補正しない。
    // startPosPly = startPosPly;
#else
    startPosPly = Max(2 * (startPosPly - 1), 0) + int(sideToMove == BLACK);

    // Various initialisations
    chess960 = isChess960;
    st->checkersBB = attackers_to(king_square(sideToMove)) & pieces(flip(sideToMove));
#endif

    st->key = compute_key();
#if defined(NANOHA)
    st->hand = hand[sideToMove].h;
    st->effect = (sideToMove == BLACK) ? effectB[kingG] : effectW[kingS];
    material = compute_material();
#else
    st->pawnKey = compute_pawn_key();
    st->materialKey = compute_material_key();
    st->value = compute_value();
    st->npMaterial[WHITE] = compute_non_pawn_material(WHITE);
    st->npMaterial[BLACK] = compute_non_pawn_material(BLACK);
#endif

    assert(is_ok());
#if defined(NANOHA)
    return;

incorrect_fen:
    std::cerr << "Error in SFEN string: " << fenStr << endl;
#endif
}


#if !defined(NANOHA)
/// Position::set_castle() is an helper function used to set
/// correct castling related flags.

void Position::set_castle(int f, Square ksq, Square rsq) {

    st->castleRights |= f;
    castleRightsMask[ksq] ^= f;
    castleRightsMask[rsq] ^= f;
    castleRookSquare[f] = rsq;
}


/// Position::set_castling_rights() sets castling parameters castling avaiability.
/// This function is compatible with 3 standards: Normal FEN standard, Shredder-FEN
/// that uses the letters of the columns on which the rooks began the game instead
/// of KQkq and also X-FEN standard that, in case of Chess960, if an inner Rook is
/// associated with the castling right, the traditional castling tag will be replaced
/// by the file letter of the involved rook as for the Shredder-FEN.

void Position::set_castling_rights(char token) {

    Color c = islower(token) ? BLACK : WHITE;

    Square sqA = relative_square(c, SQ_A1);
    Square sqH = relative_square(c, SQ_H1);
    Square rsq, ksq = king_square(c);

    token = char(toupper(token));

    if (token == 'K')
        for (rsq = sqH; piece_on(rsq) != make_piece(c, ROOK); rsq--) {}

    else if (token == 'Q')
        for (rsq = sqA; piece_on(rsq) != make_piece(c, ROOK); rsq++) {}

    else if (token >= 'A' && token <= 'H')
        rsq = make_square(File(token - 'A'), relative_rank(c, RANK_1));

    else return;

    if (file_of(rsq) < file_of(ksq))
        set_castle(WHITE_OOO << c, ksq, rsq);
    else
        set_castle(WHITE_OO << c, ksq, rsq);
}
#endif

/// Position::to_fen() returns a FEN representation of the position. In case
/// of Chess960 the Shredder-FEN notation is used. Mainly a debugging function.

const string Position::to_fen() const {

    std::ostringstream fen;
    Square sq;
    int emptyCnt;

#if defined(NANOHA)
    for (Rank rank = RANK_1; rank <= RANK_9; rank++)
#else
    for (Rank rank = RANK_8; rank >= RANK_1; rank--)
#endif
    {
        emptyCnt = 0;

#if defined(NANOHA)
        for (File file = FILE_9; file >= FILE_1; file--)
#else
        for (File file = FILE_A; file <= FILE_H; file++)
#endif
        {
            sq = make_square(file, rank);

            if (!square_is_empty(sq))
            {
                if (emptyCnt)
                {
                    fen << emptyCnt;
                    emptyCnt = 0;
                }
#if defined(NANOHA)
                if ((piece_on(sq) & ~GOTE) != OU &&
                    (piece_on(sq) & PROMOTED) != 0) {
                    fen << "+";
                }
#endif
                fen << PieceToChar[piece_on(sq)];
            }
            else
                emptyCnt++;
        }

        if (emptyCnt)
            fen << emptyCnt;

        if (rank < RANK_9)
            fen << '/';
    }

    fen << (sideToMove == WHITE ? " w " : " b ");

#if defined(NANOHA)
    // 持ち駒
    if (handS.h == 0 && handG.h == 0) {
        fen << "-";
    } else {
        unsigned int n;
        static const char *tbl[] = {
             "",   "",   "2",  "3",  "4",  "5",  "6",  "7",
             "8",  "9", "10", "11", "12", "13", "14", "15",
            "16", "17", "18"
        };
#define ADD_HAND(piece,c)    n = h.get ## piece(); if (n) {fen << tbl[n]; fen << #c; }

        Hand h = handS;
        ADD_HAND(HI,R)
        ADD_HAND(KA,B)
        ADD_HAND(KI,G)
        ADD_HAND(GI,S)
        ADD_HAND(KE,N)
        ADD_HAND(KY,L)
        ADD_HAND(FU,P)

        h = handG;
        ADD_HAND(HI,r)
        ADD_HAND(KA,b)
        ADD_HAND(KI,g)
        ADD_HAND(GI,s)
        ADD_HAND(KE,n)
        ADD_HAND(KY,l)
        ADD_HAND(FU,p)

#undef ADD_HAND
    }
#else
    if (st->castleRights != CASTLES_NONE)
    {
        if (can_castle(WHITE_OO))
            fen << (chess960 ? char(toupper(file_to_char(file_of(castle_rook_square(WHITE_OO))))) : 'K');

        if (can_castle(WHITE_OOO))
            fen << (chess960 ? char(toupper(file_to_char(file_of(castle_rook_square(WHITE_OOO))))) : 'Q');

        if (can_castle(BLACK_OO))
            fen << (chess960 ? file_to_char(file_of(castle_rook_square(BLACK_OO))) : 'k');

        if (can_castle(BLACK_OOO))
            fen << (chess960 ? file_to_char(file_of(castle_rook_square(BLACK_OOO))) : 'q');
    } else
        fen << '-';

    fen << (ep_square() == SQ_NONE ? " -" : " " + square_to_string(ep_square()))
        << " " << st->rule50 << " " << 1 + (startPosPly - int(sideToMove == BLACK)) / 2;
#endif
    return fen.str();
}


/// Position::print() prints an ASCII representation of the position to
/// the standard output. If a move is given then also the san is printed.

void Position::print(Move move) const {

#if defined(NANOHA)
    const char* dottedLine = "\n+---+---+---+---+---+---+---+---+---+\n";

    if (move != MOVE_NONE)
#else
    const char* dottedLine = "\n+---+---+---+---+---+---+---+---+\n";

    if (move)
#endif
    {
        Position p(*this, thread());
        string dd = (sideToMove == BLACK ? ".." : "");
        cout << "\nMove is: " << dd << move_to_san(p, move);
    }
#if defined(NANOHA)
    for (Rank rank = RANK_1; rank <= RANK_9; rank++)
    {
        cout << dottedLine << '|';
        for (File file = FILE_9; file >= FILE_1; file--)
        {
            Square sq = make_square(file, rank);
            Piece piece = piece_on(sq);

            char c = (color_of(piece_on(sq)) == BLACK ? ' ' : ' ');
            cout << c << pieceLetters.from_piece(piece) << c << '|';
        }
    }
    cout << dottedLine << "Fen is: " << to_fen() << "\nKey is: 0x" << std::hex << st->key << std::dec << endl;
#else
    for (Rank rank = RANK_8; rank >= RANK_1; rank--)
    {
        cout << dottedLine << '|';
        for (File file = FILE_A; file <= FILE_H; file++)
        {
            Square sq = make_square(file, rank);
            Piece piece = piece_on(sq);

            if (piece == PIECE_NONE && color_of(sq) == DARK)
                piece = PIECE_NONE_DARK_SQ;

            char c = (color_of(piece_on(sq)) == BLACK ? '=' : ' ');
            cout << c << PieceToChar[piece] << c << '|';
        }
    }
    cout << dottedLine << "Fen is: " << to_fen() << "\nKey is: " << st->key << endl;
#endif
}

#if defined(NANOHA)
void Position::print_csa(Move move) const {

    if (move != MOVE_NONE)
    {
        cout << "\nMove is: " << move_to_csa(move) << endl;
    }
    // 盤面
    for (Rank rank = RANK_1; rank <= RANK_9; rank++)
    {
        cout << "P" << int(rank);
        for (File file = FILE_9; file >= FILE_1; file--)
        {
            Square sq = make_square(file, rank);
            Piece piece = piece_on(sq);
            cout << pieceCSA.from_piece(piece);
        }
        cout << endl;
    }
    // 持ち駒
    unsigned int n;
    cout << "P+";
    {
    const Hand &h = hand[BLACK];
    if ((n = h.getFU()) > 0) while (n--){ cout << "00FU"; }
    if ((n = h.getKY()) > 0) while (n--){ cout << "00KY"; }
    if ((n = h.getKE()) > 0) while (n--){ cout << "00KE"; }
    if ((n = h.getGI()) > 0) while (n--){ cout << "00GI"; }
    if ((n = h.getKI()) > 0) while (n--){ cout << "00KI"; }
    if ((n = h.getKA()) > 0) while (n--){ cout << "00KA"; }
    if ((n = h.getHI()) > 0) while (n--){ cout << "00HI"; }
    }
    cout << endl << "P-";
    {
    const Hand &h = hand[WHITE];
    if ((n = h.getFU()) > 0) while (n--){ cout << "00FU"; }
    if ((n = h.getKY()) > 0) while (n--){ cout << "00KY"; }
    if ((n = h.getKE()) > 0) while (n--){ cout << "00KE"; }
    if ((n = h.getGI()) > 0) while (n--){ cout << "00GI"; }
    if ((n = h.getKI()) > 0) while (n--){ cout << "00KI"; }
    if ((n = h.getKA()) > 0) while (n--){ cout << "00KA"; }
    if ((n = h.getHI()) > 0) while (n--){ cout << "00HI"; }
    }
    cout << endl << (sideToMove == BLACK ? '+' : '-') << endl;
    cout << "SFEN is: " << to_fen() << "\nKey is: 0x" << std::hex << st->key << endl;
}
#endif

#if !defined(NANOHA)
/// Position:hidden_checkers<>() returns a bitboard of all pinned (against the
/// king) pieces for the given color. Or, when template parameter FindPinned is
/// false, the function return the pieces of the given color candidate for a
/// discovery check against the enemy king.

template<bool FindPinned>
Bitboard Position::hidden_checkers() const {

    // Pinned pieces protect our king, dicovery checks attack the enemy king
    Bitboard b, result = EmptyBoardBB;
    Bitboard pinners = pieces(FindPinned ? flip(sideToMove) : sideToMove);
    Square ksq = king_square(FindPinned ? sideToMove : flip(sideToMove));

    // Pinners are sliders, that give check when candidate pinned is removed
    pinners &=  (pieces(ROOK, QUEEN) & RookPseudoAttacks[ksq])
              | (pieces(BISHOP, QUEEN) & BishopPseudoAttacks[ksq]);

    while (pinners)
    {
        b = squares_between(ksq, pop_1st_bit(&pinners)) & occupied_squares();

        // Only one bit set and is an our piece?
        if (b && !(b & (b - 1)) && (b & pieces(sideToMove)))
            result |= b;
    }
    return result;
}


/// Position:pinned_pieces() returns a bitboard of all pinned (against the
/// king) pieces for the side to move.

Bitboard Position::pinned_pieces() const {

    return hidden_checkers<true>();
}


/// Position:discovered_check_candidates() returns a bitboard containing all
/// pieces for the side to move which are candidates for giving a discovered
/// check.

Bitboard Position::discovered_check_candidates() const {

    return hidden_checkers<false>();
}

/// Position::attackers_to() computes a bitboard containing all pieces which
/// attacks a given square.

Bitboard Position::attackers_to(Square s) const {

    return  (attacks_from<PAWN>(s, BLACK) & pieces(PAWN, WHITE))
          | (attacks_from<PAWN>(s, WHITE) & pieces(PAWN, BLACK))
          | (attacks_from<KNIGHT>(s)      & pieces(KNIGHT))
          | (attacks_from<ROOK>(s)        & pieces(ROOK, QUEEN))
          | (attacks_from<BISHOP>(s)      & pieces(BISHOP, QUEEN))
          | (attacks_from<KING>(s)        & pieces(KING));
}

Bitboard Position::attackers_to(Square s, Bitboard occ) const {

    return  (attacks_from<PAWN>(s, BLACK) & pieces(PAWN, WHITE))
          | (attacks_from<PAWN>(s, WHITE) & pieces(PAWN, BLACK))
          | (attacks_from<KNIGHT>(s)      & pieces(KNIGHT))
          | (rook_attacks_bb(s, occ)      & pieces(ROOK, QUEEN))
          | (bishop_attacks_bb(s, occ)    & pieces(BISHOP, QUEEN))
          | (attacks_from<KING>(s)        & pieces(KING));
}

/// Position::attacks_from() computes a bitboard of all attacks
/// of a given piece put in a given square.

Bitboard Position::attacks_from(Piece p, Square s) const {

    assert(square_is_ok(s));

    switch (p)
    {
    case WB: case BB: return attacks_from<BISHOP>(s);
    case WR: case BR: return attacks_from<ROOK>(s);
    case WQ: case BQ: return attacks_from<QUEEN>(s);
    default: return StepAttacksBB[p][s];
    }
}

Bitboard Position::attacks_from(Piece p, Square s, Bitboard occ) {

    assert(square_is_ok(s));

    switch (p)
    {
    case WB: case BB: return bishop_attacks_bb(s, occ);
    case WR: case BR: return rook_attacks_bb(s, occ);
    case WQ: case BQ: return bishop_attacks_bb(s, occ) | rook_attacks_bb(s, occ);
    default: return StepAttacksBB[p][s];
    }
}


/// Position::move_attacks_square() tests whether a move from the current
/// position attacks a given square.
/// Position::move_attacks_square() は現局面で move が指定された升目を
/// 攻撃するかテストする
bool Position::move_attacks_square(Move m, Square s) const {

    assert(is_ok(m));
    assert(square_is_ok(s));

    Bitboard occ, xray;
    Square f = move_from(m), t = move_to(m);

    assert(!square_is_empty(f));

    if (bit_is_set(attacks_from(piece_on(f), t), s))
        return true;

    // Move the piece and scan for X-ray attacks behind it
    occ = occupied_squares();
    do_move_bb(&occ, make_move_bb(f, t));
    xray = ( (rook_attacks_bb(s, occ)   & pieces(ROOK, QUEEN))
            |(bishop_attacks_bb(s, occ) & pieces(BISHOP, QUEEN)))
           & pieces(color_of(piece_on(f)));

    // If we have attacks we need to verify that are caused by our move
    // and are not already existent ones.
    return xray && (xray ^ (xray & attacks_from<QUEEN>(s)));
}
#endif


/// Position::pl_move_is_legal() tests whether a pseudo-legal move is legal

#if defined(NANOHA)
// bool Position::pl_move_is_legal(Move m) const; これは shogi.cpp に定義.
#else
bool Position::pl_move_is_legal(Move m, Bitboard pinned) const {

    assert(is_ok(m));
    assert(pinned == pinned_pieces());

    Color us = side_to_move();
    Square from = move_from(m);

    assert(color_of(piece_on(from)) == us);
    assert(piece_on(king_square(us)) == make_piece(us, KING));

    // En passant captures are a tricky special case. Because they are rather
    // uncommon, we do it simply by testing whether the king is attacked after
    // the move is made.
    if (is_enpassant(m))
    {
        Color them = flip(us);
        Square to = move_to(m);
        Square capsq = to + pawn_push(them);
        Square ksq = king_square(us);
        Bitboard b = occupied_squares();

        assert(to == ep_square());
        assert(piece_on(from) == make_piece(us, PAWN));
        assert(piece_on(capsq) == make_piece(them, PAWN));
        assert(piece_on(to) == PIECE_NONE);

        clear_bit(&b, from);
        clear_bit(&b, capsq);
        set_bit(&b, to);

        return   !(rook_attacks_bb(ksq, b) & pieces(ROOK, QUEEN, them))
              && !(bishop_attacks_bb(ksq, b) & pieces(BISHOP, QUEEN, them));
    }

    // If the moving piece is a king, check whether the destination
    // square is attacked by the opponent. Castling moves are checked
    // for legality during move generation.
    if (type_of(piece_on(from)) == KING)
        return is_castle(m) || !(attackers_to(move_to(m)) & pieces(flip(us)));

    // A non-king move is legal if and only if it is not pinned or it
    // is moving along the ray towards or away from the king.
    return   !pinned
          || !bit_is_set(pinned, from)
          ||  squares_aligned(from, move_to(m), king_square(us));
}
#endif

/// Position::move_is_legal() takes a random move and tests whether the move
/// is legal. This version is not very fast and should be used only
/// in non time-critical paths.

#if defined(NANOHA)
bool Position::move_is_legal(const Move m) const {
    bool b = pl_move_is_legal(m);
    if (b) {
        Piece p1 = move_captured(m);
        Piece p2 = piece_on(move_to(m));
        if (p1 == p2) return true;
    }
    return false;
}
#else
bool Position::move_is_legal(const Move m) const {

    for (MoveList<MV_LEGAL> ml(*this); !ml.end(); ++ml)
        if (ml.move() == m)
            return true;

    return false;
}
#endif

/// Position::is_pseudo_legal() takes a random move and tests whether the move
/// is pseudo legal. It is used to validate moves from TT that can be corrupted
/// due to SMP concurrent access or hash position key aliasing.

#if !defined(NANOHA)
bool Position::is_pseudo_legal(const Move m) const {

    Color us = sideToMove;
    Color them = flip(sideToMove);
    Square from = move_from(m);
    Square to = move_to(m);
    Piece pc = piece_on(from);

    // Use a slower but simpler function for uncommon cases
    if (is_special(m))
        return move_is_legal(m);

    // Is not a promotion, so promotion piece must be empty
    if (promotion_piece_type(m) - 2 != PIECE_TYPE_NONE)
        return false;

    // If the from square is not occupied by a piece belonging to the side to
    // move, the move is obviously not legal.
    if (pc == PIECE_NONE || color_of(pc) != us)
        return false;

    // The destination square cannot be occupied by a friendly piece
    if (color_of(piece_on(to)) == us)
        return false;

    // Handle the special case of a pawn move
    if (type_of(pc) == PAWN)
    {
        // Move direction must be compatible with pawn color
        int direction = to - from;
        if ((us == WHITE) != (direction > 0))
            return false;

        // We have already handled promotion moves, so destination
        // cannot be on the 8/1th rank.
        if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1)
            return false;

        // Proceed according to the square delta between the origin and
        // destination squares.
        switch (direction)
        {
        case DELTA_NW:
        case DELTA_NE:
        case DELTA_SW:
        case DELTA_SE:
        // Capture. The destination square must be occupied by an enemy
        // piece (en passant captures was handled earlier).
        if (color_of(piece_on(to)) != them)
            return false;

        // From and to files must be one file apart, avoids a7h5
        if (abs(file_of(from) - file_of(to)) != 1)
            return false;
        break;

        case DELTA_N:
        case DELTA_S:
        // Pawn push. The destination square must be empty.
            if (!square_is_empty(to))
                return false;
            break;

        case DELTA_NN:
        // Double white pawn push. The destination square must be on the fourth
        // rank, and both the destination square and the square between the
        // source and destination squares must be empty.
        if (   rank_of(to) != RANK_4
            || !square_is_empty(to)
            || !square_is_empty(from + DELTA_N))
            return false;
            break;

        case DELTA_SS:
        // Double black pawn push. The destination square must be on the fifth
        // rank, and both the destination square and the square between the
        // source and destination squares must be empty.
            if (   rank_of(to) != RANK_5
                || !square_is_empty(to)
                || !square_is_empty(from + DELTA_S))
                return false;
            break;

        default:
            return false;
        }
    }
    else if (!bit_is_set(attacks_from(pc, from), to))
        return false;

    if (in_check())
    {
        // In case of king moves under check we have to remove king so to catch
        // as invalid moves like b1a1 when opposite queen is on c1.
        if (type_of(piece_on(from)) == KING)
        {
            Bitboard b = occupied_squares();
            clear_bit(&b, from);
            if (attackers_to(move_to(m), b) & pieces(flip(us)))
                return false;
        }
        else
        {
            Bitboard target = checkers();
            Square checksq = pop_1st_bit(&target);

            if (target) // double check ? In this case a king move is required
                return false;

            // Our move must be a blocking evasion or a capture of the checking piece
            target = squares_between(checksq, king_square(us)) | checkers();
            if (!bit_is_set(target, move_to(m)))
                return false;
        }
    }

    return true;
}
#endif

/// Position::move_gives_check() tests whether a pseudo-legal move gives a check

#if !defined(NANOHA)
bool Position::move_gives_check(Move m, const CheckInfo& ci) const {

    assert(is_ok(m));
    assert(ci.dcCandidates == discovered_check_candidates());
    assert(color_of(piece_on(move_from(m))) == side_to_move());

    Square from = move_from(m);
    Square to = move_to(m);
    PieceType pt = type_of(piece_on(from));

    // Direct check ?
    if (bit_is_set(ci.checkSq[pt], to))
        return true;

    // Discovery check ?
    if (ci.dcCandidates && bit_is_set(ci.dcCandidates, from))
    {
        // For pawn and king moves we need to verify also direction
        if (  (pt != PAWN && pt != KING)
            || !squares_aligned(from, to, king_square(flip(side_to_move()))))
            return true;
    }

    // Can we skip the ugly special cases ?
    if (!is_special(m))
        return false;

    Color us = side_to_move();
    Bitboard b = occupied_squares();
    Square ksq = king_square(flip(us));

    // Promotion with check ?
    if (is_promotion(m))
    {
        clear_bit(&b, from);

        switch (promotion_piece_type(m))
        {
        case KNIGHT:
            return bit_is_set(attacks_from<KNIGHT>(to), ksq);
        case BISHOP:
            return bit_is_set(bishop_attacks_bb(to, b), ksq);
        case ROOK:
            return bit_is_set(rook_attacks_bb(to, b), ksq);
        case QUEEN:
            return bit_is_set(queen_attacks_bb(to, b), ksq);
        default:
            assert(false);
        }
    }

    // En passant capture with check ? We have already handled the case
    // of direct checks and ordinary discovered check, the only case we
    // need to handle is the unusual case of a discovered check through
    // the captured pawn.
    if (is_enpassant(m))
    {
        Square capsq = make_square(file_of(to), rank_of(from));
        clear_bit(&b, from);
        clear_bit(&b, capsq);
        set_bit(&b, to);
        return  (rook_attacks_bb(ksq, b) & pieces(ROOK, QUEEN, us))
              ||(bishop_attacks_bb(ksq, b) & pieces(BISHOP, QUEEN, us));
    }

    // Castling with check ?
    if (is_castle(m))
    {
        Square kfrom, kto, rfrom, rto;
        kfrom = from;
        rfrom = to;

        if (rfrom > kfrom)
        {
            kto = relative_square(us, SQ_G1);
            rto = relative_square(us, SQ_F1);
        } else {
            kto = relative_square(us, SQ_C1);
            rto = relative_square(us, SQ_D1);
        }
        clear_bit(&b, kfrom);
        clear_bit(&b, rfrom);
        set_bit(&b, rto);
        set_bit(&b, kto);
        return bit_is_set(rook_attacks_bb(rto, b), ksq);
    }

    return false;
}
#endif

/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.
#if !defined(NANOHA)    // shogi.cppに定義
void Position::do_move(Move m, StateInfo& newSt) {

    CheckInfo ci(*this);
    do_move(m, newSt, ci, move_gives_check(m, ci));
}

void Position::do_move(Move m, StateInfo& newSt, const CheckInfo& ci, bool moveIsCheck) {

    assert(is_ok(m));
    assert(&newSt != st);

    nodes++;
    Key key = st->key;

    // Copy some fields of old state to our new StateInfo object except the
    // ones which are recalculated from scratch anyway, then switch our state
    // pointer to point to the new, ready to be updated, state.
    struct ReducedStateInfo {
        Key pawnKey, materialKey;
        Value npMaterial[2];
        int castleRights, rule50, pliesFromNull;
        Score value;
        Square epSquare;
    };

    memcpy(&newSt, st, sizeof(ReducedStateInfo));

    newSt.previous = st;
    st = &newSt;

    // Update side to move
    key ^= zobSideToMove;

    // Increment the 50 moves rule draw counter. Resetting it to zero in the
    // case of non-reversible moves is taken care of later.
    st->rule50++;
    st->pliesFromNull++;

    if (is_castle(m))
    {
        st->key = key;
        do_castle_move(m);
        return;
    }

    Color us = side_to_move();
    Color them = flip(us);
    Square from = move_from(m);
    Square to = move_to(m);
    bool ep = is_enpassant(m);
    bool pm = is_promotion(m);

    Piece piece = piece_on(from);
    PieceType pt = type_of(piece);
    PieceType capture = ep ? PAWN : type_of(piece_on(to));

    assert(color_of(piece_on(from)) == us);
    assert(color_of(piece_on(to)) == them || square_is_empty(to));
    assert(!(ep || pm) || piece == make_piece(us, PAWN));
    assert(!pm || relative_rank(us, to) == RANK_8);

    if (capture)
        do_capture_move(key, capture, them, to, ep);

    // Update hash key
    key ^= zobrist[us][pt][from] ^ zobrist[us][pt][to];

    // Reset en passant square
    if (st->epSquare != SQ_NONE)
    {
        key ^= zobEp[st->epSquare];
        st->epSquare = SQ_NONE;
    }

    // Update castle rights if needed
    if (    st->castleRights != CASTLES_NONE
        && (castleRightsMask[from] & castleRightsMask[to]) != ALL_CASTLES)
    {
        key ^= zobCastle[st->castleRights];
        st->castleRights &= castleRightsMask[from] & castleRightsMask[to];
        key ^= zobCastle[st->castleRights];
    }

    // Prefetch TT access as soon as we know key is updated
    prefetch((char*)TT.first_entry(key));

    // Move the piece
    Bitboard move_bb = make_move_bb(from, to);
    do_move_bb(&byColorBB[us], move_bb);
    do_move_bb(&byTypeBB[pt], move_bb);
    do_move_bb(&byTypeBB[0], move_bb); // HACK: byTypeBB[0] == occupied squares

    board[to] = board[from];
    board[from] = PIECE_NONE;

    // Update piece lists, note that index[from] is not updated and
    // becomes stale. This works as long as index[] is accessed just
    // by known occupied squares.
    index[to] = index[from];
    pieceList[us][pt][index[to]] = to;

    // If the moving piece was a pawn do some special extra work
    if (pt == PAWN)
    {
        // Reset rule 50 draw counter
        st->rule50 = 0;

        // Update pawn hash key and prefetch in L1/L2 cache
        st->pawnKey ^= zobrist[us][PAWN][from] ^ zobrist[us][PAWN][to];

        // Set en passant square, only if moved pawn can be captured
        if ((to ^ from) == 16)
        {
            if (attacks_from<PAWN>(from + pawn_push(us), us) & pieces(PAWN, them))
            {
                st->epSquare = Square((int(from) + int(to)) / 2);
                key ^= zobEp[st->epSquare];
            }
        }

        if (pm) // promotion ?
        {
            PieceType promotion = promotion_piece_type(m);

            assert(promotion >= KNIGHT && promotion <= QUEEN);

            // Insert promoted piece instead of pawn
            clear_bit(&byTypeBB[PAWN], to);
            set_bit(&byTypeBB[promotion], to);
            board[to] = make_piece(us, promotion);

            // Update piece counts
            pieceCount[us][promotion]++;
            pieceCount[us][PAWN]--;

            // Update material key
            st->materialKey ^= zobrist[us][PAWN][pieceCount[us][PAWN]];
            st->materialKey ^= zobrist[us][promotion][pieceCount[us][promotion]-1];

            // Update piece lists, move the last pawn at index[to] position
            // and shrink the list. Add a new promotion piece to the list.
            Square lastPawnSquare = pieceList[us][PAWN][pieceCount[us][PAWN]];
            index[lastPawnSquare] = index[to];
            pieceList[us][PAWN][index[lastPawnSquare]] = lastPawnSquare;
            pieceList[us][PAWN][pieceCount[us][PAWN]] = SQ_NONE;
            index[to] = pieceCount[us][promotion] - 1;
            pieceList[us][promotion][index[to]] = to;

            // Partially revert hash keys update
            key ^= zobrist[us][PAWN][to] ^ zobrist[us][promotion][to];
            st->pawnKey ^= zobrist[us][PAWN][to];

            // Partially revert and update incremental scores
            st->value -= pst(make_piece(us, PAWN), to);
            st->value += pst(make_piece(us, promotion), to);

            // Update material
            st->npMaterial[us] += PieceValueMidgame[promotion];
        }
    }

    // Prefetch pawn and material hash tables
    Threads[threadID].pawnTable.prefetch(st->pawnKey);
    Threads[threadID].materialTable.prefetch(st->materialKey);

    // Update incremental scores
    st->value += pst_delta(piece, from, to);

    // Set capture piece
    st->capturedType = capture;

    // Update the key with the final value
    st->key = key;

    // Update checkers bitboard, piece must be already moved
    st->checkersBB = EmptyBoardBB;

    if (moveIsCheck)
    {
        if (ep | pm)
            st->checkersBB = attackers_to(king_square(them)) & pieces(us);
        else
        {
            // Direct checks
            if (bit_is_set(ci.checkSq[pt], to))
                st->checkersBB = SetMaskBB[to];

            // Discovery checks
            if (ci.dcCandidates && bit_is_set(ci.dcCandidates, from))
            {
                if (pt != ROOK)
                    st->checkersBB |= (attacks_from<ROOK>(king_square(them)) & pieces(ROOK, QUEEN, us));

                if (pt != BISHOP)
                    st->checkersBB |= (attacks_from<BISHOP>(king_square(them)) & pieces(BISHOP, QUEEN, us));
            }
        }
    }

    // Finish
    sideToMove = flip(sideToMove);
    st->value += (sideToMove == WHITE ?  TempoValue : -TempoValue);

    assert(is_ok());
}


/// Position::do_capture_move() is a private method used to update captured
/// piece info. It is called from the main Position::do_move function.

void Position::do_capture_move(Key& key, PieceType capture, Color them, Square to, bool ep) {

    assert(capture != KING);

    Square capsq = to;

    // If the captured piece was a pawn, update pawn hash key,
    // otherwise update non-pawn material.
    if (capture == PAWN)
    {
        if (ep) // en passant ?
        {
            capsq = to + pawn_push(them);

            assert(to == st->epSquare);
            assert(relative_rank(flip(them), to) == RANK_6);
            assert(piece_on(to) == PIECE_NONE);
            assert(piece_on(capsq) == make_piece(them, PAWN));

            board[capsq] = PIECE_NONE;
        }
        st->pawnKey ^= zobrist[them][PAWN][capsq];
    }
    else
        st->npMaterial[them] -= PieceValueMidgame[capture];

    // Remove captured piece
    clear_bit(&byColorBB[them], capsq);
    clear_bit(&byTypeBB[capture], capsq);
    clear_bit(&byTypeBB[0], capsq);

    // Update hash key
    key ^= zobrist[them][capture][capsq];

    // Update incremental scores
    st->value -= pst(make_piece(them, capture), capsq);

    // Update piece count
    pieceCount[them][capture]--;

    // Update material hash key
    st->materialKey ^= zobrist[them][capture][pieceCount[them][capture]];

    // Update piece list, move the last piece at index[capsq] position
    //
    // WARNING: This is a not perfectly revresible operation. When we
    // will reinsert the captured piece in undo_move() we will put it
    // at the end of the list and not in its original place, it means
    // index[] and pieceList[] are not guaranteed to be invariant to a
    // do_move() + undo_move() sequence.
    Square lastPieceSquare = pieceList[them][capture][pieceCount[them][capture]];
    index[lastPieceSquare] = index[capsq];
    pieceList[them][capture][index[lastPieceSquare]] = lastPieceSquare;
    pieceList[them][capture][pieceCount[them][capture]] = SQ_NONE;

    // Reset rule 50 counter
    st->rule50 = 0;
}


/// Position::do_castle_move() is a private method used to make a castling
/// move. It is called from the main Position::do_move function. Note that
/// castling moves are encoded as "king captures friendly rook" moves, for
/// instance white short castling in a non-Chess960 game is encoded as e1h1.

void Position::do_castle_move(Move m) {

    assert(is_ok(m));
    assert(is_castle(m));

    Color us = side_to_move();
    Color them = flip(us);

    // Find source squares for king and rook
    Square kfrom = move_from(m);
    Square rfrom = move_to(m);
    Square kto, rto;

    assert(piece_on(kfrom) == make_piece(us, KING));
    assert(piece_on(rfrom) == make_piece(us, ROOK));

    // Find destination squares for king and rook
    if (rfrom > kfrom) // O-O
    {
        kto = relative_square(us, SQ_G1);
        rto = relative_square(us, SQ_F1);
    }
    else // O-O-O
    {
        kto = relative_square(us, SQ_C1);
        rto = relative_square(us, SQ_D1);
    }

    // Remove pieces from source squares
    clear_bit(&byColorBB[us], kfrom);
    clear_bit(&byTypeBB[KING], kfrom);
    clear_bit(&byTypeBB[0], kfrom);
    clear_bit(&byColorBB[us], rfrom);
    clear_bit(&byTypeBB[ROOK], rfrom);
    clear_bit(&byTypeBB[0], rfrom);

    // Put pieces on destination squares
    set_bit(&byColorBB[us], kto);
    set_bit(&byTypeBB[KING], kto);
    set_bit(&byTypeBB[0], kto);
    set_bit(&byColorBB[us], rto);
    set_bit(&byTypeBB[ROOK], rto);
    set_bit(&byTypeBB[0], rto);

    // Update board
    Piece king = make_piece(us, KING);
    Piece rook = make_piece(us, ROOK);
    board[kfrom] = board[rfrom] = PIECE_NONE;
    board[kto] = king;
    board[rto] = rook;

    // Update piece lists
    pieceList[us][KING][index[kfrom]] = kto;
    pieceList[us][ROOK][index[rfrom]] = rto;
    int tmp = index[rfrom]; // In Chess960 could be kto == rfrom
    index[kto] = index[kfrom];
    index[rto] = tmp;

    // Reset capture field
    st->capturedType = PIECE_TYPE_NONE;

    // Update incremental scores
    st->value += pst_delta(king, kfrom, kto);
    st->value += pst_delta(rook, rfrom, rto);

    // Update hash key
    st->key ^= zobrist[us][KING][kfrom] ^ zobrist[us][KING][kto];
    st->key ^= zobrist[us][ROOK][rfrom] ^ zobrist[us][ROOK][rto];

    // Clear en passant square
    if (st->epSquare != SQ_NONE)
    {
        st->key ^= zobEp[st->epSquare];
        st->epSquare = SQ_NONE;
    }

    // Update castling rights
    st->key ^= zobCastle[st->castleRights];
    st->castleRights &= castleRightsMask[kfrom];
    st->key ^= zobCastle[st->castleRights];

    // Reset rule 50 counter
    st->rule50 = 0;

    // Update checkers BB
    st->checkersBB = attackers_to(king_square(them)) & pieces(us);

    // Finish
    sideToMove = flip(sideToMove);
    st->value += (sideToMove == WHITE ?  TempoValue : -TempoValue);

    assert(is_ok());
}


/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.

void Position::undo_move(Move m) {

    assert(is_ok(m));

    sideToMove = flip(sideToMove);

    if (is_castle(m))
    {
        undo_castle_move(m);
        return;
    }

    Color us = side_to_move();
    Color them = flip(us);
    Square from = move_from(m);
    Square to = move_to(m);
    bool ep = is_enpassant(m);
    bool pm = is_promotion(m);

    PieceType pt = type_of(piece_on(to));

    assert(square_is_empty(from));
    assert(color_of(piece_on(to)) == us);
    assert(!pm || relative_rank(us, to) == RANK_8);
    assert(!ep || to == st->previous->epSquare);
    assert(!ep || relative_rank(us, to) == RANK_6);
    assert(!ep || piece_on(to) == make_piece(us, PAWN));

    if (pm) // promotion ?
    {
        PieceType promotion = promotion_piece_type(m);
        pt = PAWN;

        assert(promotion >= KNIGHT && promotion <= QUEEN);
        assert(piece_on(to) == make_piece(us, promotion));

        // Replace promoted piece with a pawn
        clear_bit(&byTypeBB[promotion], to);
        set_bit(&byTypeBB[PAWN], to);

        // Update piece counts
        pieceCount[us][promotion]--;
        pieceCount[us][PAWN]++;

        // Update piece list replacing promotion piece with a pawn
        Square lastPromotionSquare = pieceList[us][promotion][pieceCount[us][promotion]];
        index[lastPromotionSquare] = index[to];
        pieceList[us][promotion][index[lastPromotionSquare]] = lastPromotionSquare;
        pieceList[us][promotion][pieceCount[us][promotion]] = SQ_NONE;
        index[to] = pieceCount[us][PAWN] - 1;
        pieceList[us][PAWN][index[to]] = to;
    }

    // Put the piece back at the source square
    Bitboard move_bb = make_move_bb(to, from);
    do_move_bb(&byColorBB[us], move_bb);
    do_move_bb(&byTypeBB[pt], move_bb);
    do_move_bb(&byTypeBB[0], move_bb); // HACK: byTypeBB[0] == occupied squares

    board[from] = make_piece(us, pt);
    board[to] = PIECE_NONE;

    // Update piece list
    index[from] = index[to];
    pieceList[us][pt][index[from]] = from;

    if (st->capturedType)
    {
        Square capsq = to;

        if (ep)
            capsq = to - pawn_push(us);

        assert(st->capturedType != KING);
        assert(!ep || square_is_empty(capsq));

        // Restore the captured piece
        set_bit(&byColorBB[them], capsq);
        set_bit(&byTypeBB[st->capturedType], capsq);
        set_bit(&byTypeBB[0], capsq);

        board[capsq] = make_piece(them, st->capturedType);

        // Update piece count
        pieceCount[them][st->capturedType]++;

        // Update piece list, add a new captured piece in capsq square
        index[capsq] = pieceCount[them][st->capturedType] - 1;
        pieceList[them][st->capturedType][index[capsq]] = capsq;
    }

    // Finally point our state pointer back to the previous state
    st = st->previous;

    assert(is_ok());
}


/// Position::undo_castle_move() is a private method used to unmake a castling
/// move. It is called from the main Position::undo_move function. Note that
/// castling moves are encoded as "king captures friendly rook" moves, for
/// instance white short castling in a non-Chess960 game is encoded as e1h1.

void Position::undo_castle_move(Move m) {

    assert(is_ok(m));
    assert(is_castle(m));

    // When we have arrived here, some work has already been done by
    // Position::undo_move.  In particular, the side to move has been switched,
    // so the code below is correct.
    Color us = side_to_move();

    // Find source squares for king and rook
    Square kfrom = move_from(m);
    Square rfrom = move_to(m);
    Square kto, rto;

    // Find destination squares for king and rook
    if (rfrom > kfrom) // O-O
    {
        kto = relative_square(us, SQ_G1);
        rto = relative_square(us, SQ_F1);
    }
    else // O-O-O
    {
        kto = relative_square(us, SQ_C1);
        rto = relative_square(us, SQ_D1);
    }

    assert(piece_on(kto) == make_piece(us, KING));
    assert(piece_on(rto) == make_piece(us, ROOK));

    // Remove pieces from destination squares
    clear_bit(&byColorBB[us], kto);
    clear_bit(&byTypeBB[KING], kto);
    clear_bit(&byTypeBB[0], kto);
    clear_bit(&byColorBB[us], rto);
    clear_bit(&byTypeBB[ROOK], rto);
    clear_bit(&byTypeBB[0], rto);

    // Put pieces on source squares
    set_bit(&byColorBB[us], kfrom);
    set_bit(&byTypeBB[KING], kfrom);
    set_bit(&byTypeBB[0], kfrom);
    set_bit(&byColorBB[us], rfrom);
    set_bit(&byTypeBB[ROOK], rfrom);
    set_bit(&byTypeBB[0], rfrom);

    // Update board
    Piece king = make_piece(us, KING);
    Piece rook = make_piece(us, ROOK);
    board[kto] = board[rto] = PIECE_NONE;
    board[kfrom] = king;
    board[rfrom] = rook;

    // Update piece lists
    pieceList[us][KING][index[kto]] = kfrom;
    pieceList[us][ROOK][index[rto]] = rfrom;
    int tmp = index[rto];  // In Chess960 could be rto == kfrom
    index[kfrom] = index[kto];
    index[rfrom] = tmp;

    // Finally point our state pointer back to the previous state
    st = st->previous;

    assert(is_ok());
}
#endif


/// Position::do_null_move makes() a "null move": It switches the side to move
/// and updates the hash key without executing any move on the board.

void Position::do_null_move(StateInfo& backupSt) {

    assert(!in_check());

    // Back up the information necessary to undo the null move to the supplied
    // StateInfo object.
    // Note that differently from normal case here backupSt is actually used as
    // a backup storage not as a new state to be used.
    backupSt.key      = st->key;
#if !defined(NANOHA)
    backupSt.epSquare = st->epSquare;
    backupSt.value    = st->value;
#endif
    backupSt.previous = st->previous;
    backupSt.pliesFromNull = st->pliesFromNull;
    st->previous = &backupSt;

#if !defined(NANOHA)
    // Update the necessary information
    if (st->epSquare != SQ_NONE)
        st->key ^= zobEp[st->epSquare];
#endif

    st->key ^= zobSideToMove;
    prefetch((char*)TT.first_entry(st->key));

    sideToMove = flip(sideToMove);
#if !defined(NANOHA)
    st->epSquare = SQ_NONE;
    st->rule50++;
#endif
    st->pliesFromNull = 0;
#if !defined(NANOHA)
    st->value += (sideToMove == WHITE) ?  TempoValue : -TempoValue;
#endif
#if defined(NANOHA)
    if (in_check()) {
    print_csa();
    MYABORT();
    }
#endif
    assert(is_ok());
}


/// Position::undo_null_move() unmakes a "null move".

void Position::undo_null_move() {

#if defined(NANOHA)
    if (in_check()) {
    print_csa();
    MYABORT();
    }
#else
    assert(!in_check());
#endif

    // Restore information from the our backup StateInfo object
    StateInfo* backupSt = st->previous;
    st->key      = backupSt->key;
#if !defined(NANOHA)
    st->epSquare = backupSt->epSquare;
    st->value    = backupSt->value;
#endif
    st->previous = backupSt->previous;
    st->pliesFromNull = backupSt->pliesFromNull;

    // Update the necessary information
    sideToMove = flip(sideToMove);
#if !defined(NANOHA)
    st->rule50--;
#endif
    assert(is_ok());
}


/// Position::see() is a static exchange evaluator: It tries to estimate the
/// material gain or loss resulting from a move. There are three versions of
/// this function: One which takes a destination square as input, one takes a
/// move, and one which takes a 'from' and a 'to' square. The function does
/// not yet understand promotions captures.

#if defined(NANOHA)
static int SEERec(int kind, int *attacker, int *defender)
{
    //  手番の一番安い駒を動かす
    //  value = 取った駒の交換値(ふつうの2枚分)
    if (*attacker == 0) return 0;

    int value = NanohaTbl::KomaValueEx[kind];
    return Max(value - SEERec(*attacker, defender, attacker+1), 0);
}
#endif

int Position::see_sign(Move m) const {

    assert(::is_ok(m));

    Square from = move_from(m);
    Square to = move_to(m);

    // Early return if SEE cannot be negative because captured piece value
    // is not less then capturing one. Note that king moves always return
    // here because king midgame value is set to 0.
#if defined(NANOHA)
    if (move_is_drop(m) == false && piece_value_midgame(piece_on(to)) >= piece_value_midgame(piece_on(from)))
        return 1;

    return see(m);
#else
    if (piece_value_midgame(piece_on(to)) >= piece_value_midgame(piece_on(from)))
        return 1;

    return see(m);
#endif
}

int Position::see(Move m) const {

    assert(::is_ok(m));
#if defined(NANOHA)
    //  value = 成りによる加点 + 取りによる加点;
    int from = move_from(m);
    int to = move_to(m);
    int fKind = move_ptype(m);
    int tKind = is_promotion(m) ? Piece(fKind | PROMOTED) : fKind;
    int cap = move_captured(m) & ~GOTE;
    Color us = side_to_move();
    int value = is_promotion(m) ? NanohaTbl::KomaValuePro[fKind] + NanohaTbl::KomaValueEx[cap] : NanohaTbl::KomaValueEx[cap]; // = (promote + capture) value

    //
    const effect_t *dKiki = (us == BLACK) ? effectW : effectB;
    int defender[32];    // to に利いている守りの駒
    int ndef = 0;
    effect_t k = EXIST_EFFECT(dKiki[to]);
    while (k) {
        unsigned long id;
        _BitScanForward(&id, k);
        k &= k-1;
        if (id < 16) {
            int z = to - NanohaTbl::Direction[id];
            defender[ndef++] = ban[z] & ~GOTE;
            if (dKiki[z] & (0x100u << id)) {
                z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
                defender[ndef++] = ban[z] & ~GOTE;
            }
        } else {
            int z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
            defender[ndef++] = ban[z] & ~GOTE;
            if (dKiki[z] & (0x1u << id)) {
                z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
                defender[ndef++] = ban[z] & ~GOTE;
            }
        }
    }
    defender[ndef] = 0;
    assert(ndef < 32-1);

    if (ndef == 0) return value; // no defender -> stop SEE

    const effect_t *aKiki = (us == BLACK) ? effectB : effectW;
    int attacker[32];    // to に利いている攻めの駒
    int natk = 0;
    k = EXIST_EFFECT(aKiki[to]);
    while (k) {
        unsigned long id;
        _BitScanForward(&id, k);
        k &= k-1;
        if (id < 16) {
            int z = to - NanohaTbl::Direction[id];
            if (from != z) attacker[natk++] = ban[z] & ~GOTE;
            if (dKiki[z] & (0x100u << id)) {
                z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
                attacker[natk++] = ban[z] & ~GOTE;
            }
        } else {
            int z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
            if (from != z) attacker[natk++] = ban[z] & ~GOTE;
            if (dKiki[z] & (0x1u << id)) {
                z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
                attacker[natk++] = ban[z] & ~GOTE;
            }
        }
    }
    attacker[natk] = 0;
    assert(natk < 32-1);

    int i, j;
    int *p;
    int n;
    p = defender;
    n = ndef;
    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if (NanohaTbl::KomaValueEx[p[i]] > NanohaTbl::KomaValueEx[p[j]]) {
                int tmp = p[i];
                p[i] = p[j];
                p[j] = tmp;
            }
        }
    }
    p = attacker;
    n = natk;
    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if (NanohaTbl::KomaValueEx[p[i]] > NanohaTbl::KomaValueEx[p[j]]) {
                int tmp = p[i];
                p[i] = p[j];
                p[j] = tmp;
            }
        }
    }
    return value - SEERec(tKind, defender, attacker); // counter
#else

    Square from, to;
    Bitboard occupied, attackers, stmAttackers, b;
    int swapList[32], slIndex = 1;
    PieceType capturedType, pt;
    Color stm;

    assert(is_ok(m));

    // As castle moves are implemented as capturing the rook, they have
    // SEE == RookValueMidgame most of the times (unless the rook is under
    // attack).
    if (is_castle(m))
        return 0;

    from = move_from(m);
    to = move_to(m);
    capturedType = type_of(piece_on(to));
    occupied = occupied_squares();

    // Handle en passant moves
    if (st->epSquare == to && type_of(piece_on(from)) == PAWN)
    {
        Square capQq = to - pawn_push(side_to_move());

        assert(capturedType == PIECE_TYPE_NONE);
        assert(type_of(piece_on(capQq)) == PAWN);

        // Remove the captured pawn
        clear_bit(&occupied, capQq);
        capturedType = PAWN;
    }

    // Find all attackers to the destination square, with the moving piece
    // removed, but possibly an X-ray attacker added behind it.
    clear_bit(&occupied, from);
    attackers = attackers_to(to, occupied);

    // If the opponent has no attackers we are finished
    stm = flip(color_of(piece_on(from)));
    stmAttackers = attackers & pieces(stm);
    if (!stmAttackers)
        return PieceValueMidgame[capturedType];

    // The destination square is defended, which makes things rather more
    // difficult to compute. We proceed by building up a "swap list" containing
    // the material gain or loss at each stop in a sequence of captures to the
    // destination square, where the sides alternately capture, and always
    // capture with the least valuable piece. After each capture, we look for
    // new X-ray attacks from behind the capturing piece.
    swapList[0] = PieceValueMidgame[capturedType];
    capturedType = type_of(piece_on(from));

    do {
        // Locate the least valuable attacker for the side to move. The loop
        // below looks like it is potentially infinite, but it isn't. We know
        // that the side to move still has at least one attacker left.
        for (pt = PAWN; !(stmAttackers & pieces(pt)); pt++)
            assert(pt < KING);

        // Remove the attacker we just found from the 'occupied' bitboard,
        // and scan for new X-ray attacks behind the attacker.
        b = stmAttackers & pieces(pt);
        occupied ^= (b & (~b + 1));
        attackers |=  (rook_attacks_bb(to, occupied)   & pieces(ROOK, QUEEN))
                    | (bishop_attacks_bb(to, occupied) & pieces(BISHOP, QUEEN));

        attackers &= occupied; // Cut out pieces we've already done

        // Add the new entry to the swap list
        assert(slIndex < 32);
        swapList[slIndex] = -swapList[slIndex - 1] + PieceValueMidgame[capturedType];
        slIndex++;

        // Remember the value of the capturing piece, and change the side to
        // move before beginning the next iteration.
        capturedType = pt;
        stm = flip(stm);
        stmAttackers = attackers & pieces(stm);

        // Stop before processing a king capture
        if (capturedType == KING && stmAttackers)
        {
            assert(slIndex < 32);
            swapList[slIndex++] = QueenValueMidgame*10;
            break;
        }
    } while (stmAttackers);

    // Having built the swap list, we negamax through it to find the best
    // achievable score from the point of view of the side to move.
    while (--slIndex)
        swapList[slIndex-1] = Min(-swapList[slIndex], swapList[slIndex-1]);

    return swapList[0];
#endif
}


/// Position::clear() erases the position object to a pristine state, with an
/// empty board, white to move, and no castling rights.

void Position::clear() {

    st = &startState;
    memset(st, 0, sizeof(StateInfo));
#if !defined(NANOHA)
    st->epSquare = SQ_NONE;

    memset(byColorBB,  0, sizeof(Bitboard) * 2);
    memset(byTypeBB,   0, sizeof(Bitboard) * 8);
    memset(pieceCount, 0, sizeof(int) * 2 * 8);
    memset(index,      0, sizeof(int) * 64);

    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 16; j++)
            pieceList[0][i][j] = pieceList[1][i][j] = SQ_NONE;

    for (Square sq = SQ_A1; sq <= SQ_H8; sq++)
    {
        board[sq] = PIECE_NONE;
        castleRightsMask[sq] = ALL_CASTLES;
    }
#endif

#if defined(NANOHA)
    // 将棋はBLACKが先番.
    sideToMove = BLACK;
    tnodes = 0;
#if defined(CHK_PERFORM)
    count_Mate1plyDrop = 0;        // 駒打ちで詰んだ回数
    count_Mate1plyMove = 0;        // 駒移動で詰んだ回数
    count_Mate3ply = 0;            // Mate3()で詰んだ回数
#endif // defined(CHK_PERFORM)
#define FILL_ZERO(x)    memset(x, 0, sizeof(x))
    FILL_ZERO(banpadding);
    FILL_ZERO(ban);
    FILL_ZERO(komano);
    FILL_ZERO(effect);
    FILL_ZERO(pin);
    hand[0] = hand[1] = 0;
    FILL_ZERO(knkind);
    FILL_ZERO(knpos);
    material = 0;
    bInaniwa = false;
#undef FILL_ZERO
#else
    sideToMove = WHITE;
#endif
    nodes = 0;
}


/// Position::put_piece() puts a piece on the given square of the board,
/// updating the board array, pieces list, bitboards, and piece counts.

#if !defined(NANOHA)
void Position::put_piece(Piece p, Square s) {

    Color c = color_of(p);
    PieceType pt = type_of(p);

    board[s] = p;
    index[s] = pieceCount[c][pt]++;
    pieceList[c][pt][index[s]] = s;

    set_bit(&byTypeBB[pt], s);
    set_bit(&byColorBB[c], s);
    set_bit(&byTypeBB[0], s); // HACK: byTypeBB[0] contains all occupied squares.
}
#endif

/// Position::compute_key() は局面のハッシュキーの計算をする。
/// ハッシュキーは通常は手を進めたり戻すことで差分計算される。
/// compute_key()は新たに局面がセットされた時のみに使われる
/// (あとデバッグモードでのハッシュキーの検証のため)

/// Position::compute_key() computes the hash key of the position. The hash
/// key is usually updated incrementally as moves are made and unmade, the
/// compute_key() function is only used when a new position is set up, and
/// to verify the correctness of the hash key when running in debug mode.

Key Position::compute_key() const {

#if defined(NANOHA)
    Key result = 0;
    int z;
    for(int dan = 1; dan <= 9; dan++) {
        for(int suji = 0x10; suji <= 0x90; suji += 0x10) {
            z = suji + dan;
            if (!square_is_empty(Square(z))) {
                result ^= zobrist[piece_on(Square(z))][z];
            }
        }
    }
    if (side_to_move() != BLACK)
        result ^= zobSideToMove;
#else
    Key result = zobCastle[st->castleRights];

    for (Square s = SQ_A1; s <= SQ_H8; s++)
        if (!square_is_empty(s))
            result ^= zobrist[color_of(piece_on(s))][type_of(piece_on(s))][s];

    if (ep_square() != SQ_NONE)
        result ^= zobEp[ep_square()];

    if (side_to_move() == BLACK)
        result ^= zobSideToMove;
#endif

    return result;
}


/// Position::compute_pawn_key() computes the hash key of the position. The
/// hash key is usually updated incrementally as moves are made and unmade,
/// the compute_pawn_key() function is only used when a new position is set
/// up, and to verify the correctness of the pawn hash key when running in
/// debug mode.

#if !defined(NANOHA)
Key Position::compute_pawn_key() const {

    Bitboard b;
    Key result = 0;

    for (Color c = WHITE; c <= BLACK; c++)
    {
        b = pieces(PAWN, c);
        while (b)
            result ^= zobrist[c][PAWN][pop_1st_bit(&b)];
    }
    return result;
}


/// Position::compute_material_key() computes the hash key of the position.
/// The hash key is usually updated incrementally as moves are made and unmade,
/// the compute_material_key() function is only used when a new position is set
/// up, and to verify the correctness of the material hash key when running in
/// debug mode.

Key Position::compute_material_key() const {

    Key result = 0;

    for (Color c = WHITE; c <= BLACK; c++)
        for (PieceType pt = PAWN; pt <= QUEEN; pt++)
            for (int i = 0, cnt = piece_count(c, pt); i < cnt; i++)
                result ^= zobrist[c][pt][i];

    return result;
}
#endif


/// Position::compute_value() compute the incremental scores for the middle
/// game and the endgame. These functions are used to initialize the incremental
/// scores when a new position is set up, and to verify that the scores are correctly
/// updated by do_move and undo_move when the program is running in debug mode.
#if !defined(NANOHA)
Score Position::compute_value() const {

    Bitboard b;
    Score result = SCORE_ZERO;

    for (Color c = WHITE; c <= BLACK; c++)
        for (PieceType pt = PAWN; pt <= KING; pt++)
        {
            b = pieces(pt, c);
            while (b)
                result += pst(make_piece(c, pt), pop_1st_bit(&b));
        }

    result += (side_to_move() == WHITE ? TempoValue / 2 : -TempoValue / 2);
    return result;
}
#endif

/// Position::compute_non_pawn_material() computes the total non-pawn middle
/// game material value for the given side. Material values are updated
/// incrementally during the search, this function is only used while
/// initializing a new Position object.

#if !defined(NANOHA)
Value Position::compute_non_pawn_material(Color c) const {

    Value result = VALUE_ZERO;

    for (PieceType pt = KNIGHT; pt <= QUEEN; pt++)
        result += piece_count(c, pt) * PieceValueMidgame[pt];

    return result;
}
#endif

/// Position::is_draw() tests whether the position is drawn by material,
/// repetition, or the 50 moves rule. It does not detect stalemates, this
/// must be done by the search.

#if defined(NANOHA)
bool Position::is_draw(int *ret) const {
//bool Position::is_draw(int& ret) const {
    //ret=0;
    int i = 5, e = st->pliesFromNull;

    if (i <= e)
    {
        StateInfo* stp = st->previous->previous;
        int rept = 0;
        bool cont_check = (st->previous->effect && stp->previous->effect) ? true : false;

        do {
            stp = stp->previous->previous;
            if (stp->previous->effect == 0) cont_check = false;

            if (stp->key == st->key && stp->hand == st->hand) {
                rept++;
                // 過去に3回(現局面含めて4回)出現していたら千日手.
                if (rept >= 3) {
                    if (cont_check) {*ret = -1; return false; }
                    return true;
                }
            }

            i +=2;

        } while (i < e);
    }
    return false;
}
#else
template<bool SkipRepetition>
bool Position::is_draw() const {

    // Draw by material?
    if (   !pieces(PAWN)
        && (non_pawn_material(WHITE) + non_pawn_material(BLACK) <= BishopValueMidgame))
        return true;

    // Draw by the 50 moves rule?
    if (st->rule50 > 99 && !is_mate())
        return true;

    // Draw by repetition?
    if (!SkipRepetition)
    {
        int i = 4, e = Min(st->rule50, st->pliesFromNull);

        if (i <= e)
        {
            StateInfo* stp = st->previous->previous;

            do {
                stp = stp->previous->previous;

                if (stp->key == st->key)
                    return true;

                i +=2;

            } while (i <= e);
        }
    }

    return false;
}

// Explicit template instantiations
template bool Position::is_draw<false>() const;
template bool Position::is_draw<true>() const;
#endif


/// Position::is_mate() returns true or false depending on whether the
/// side to move is checkmated.

bool Position::is_mate() const {

    return in_check() && !MoveList<MV_LEGAL>(*this).size();
}


/// Position::init() is a static member function which initializes at
/// startup the various arrays used to compute hash keys and the piece
/// square tables. The latter is a two-step operation: First, the white
/// halves of the tables are copied from the MgPST[][] and EgPST[][] arrays.
/// Second, the black halves of the tables are initialized by flipping
/// and changing the sign of the corresponding white scores.

void Position::init() {

    RKISS rk;

#if defined(NANOHA)
    int j, k;
    for (j = 0; j < GRY+1; j++) for (k = 0; k < 0x100; k++)
        zobrist[j][k] = rk.rand<Key>() << 1;
#else
    for (Color c = WHITE; c <= BLACK; c++)
        for (PieceType pt = PAWN; pt <= KING; pt++)
            for (Square s = SQ_A1; s <= SQ_H8; s++)
                zobrist[c][pt][s] = rk.rand<Key>();

    for (Square s = SQ_A1; s <= SQ_H8; s++)
        zobEp[s] = rk.rand<Key>();

    for (int i = 0; i < 16; i++)
        zobCastle[i] = rk.rand<Key>();
#endif

#if defined(NANOHA)
    zobSideToMove = (rk.rand<Key>() << 1) | 1;
    zobExclusion  = (rk.rand<Key>() << 1);
#else
    zobSideToMove = rk.rand<Key>();
    zobExclusion  = rk.rand<Key>();

    for (Piece p = WP; p <= WK; p++)
        for (Square s = SQ_A1; s <= SQ_H8; s++)
        {
            pieceSquareTable[p][s] = make_score(MgPST[p][s], EgPST[p][s]);
            pieceSquareTable[p+8][flip(s)] = -pieceSquareTable[p][s];
        }
#endif
}


/// Position::flip_me() flips position with the white and black sides reversed. This
/// is only useful for debugging especially for finding evaluation symmetry bugs.

#if !defined(NANOHA)
void Position::flip_me() {

    // Make a copy of current position before to start changing
    const Position pos(*this, threadID);

    clear();
    threadID = pos.thread();

    // Board
    for (Square s = SQ_A1; s <= SQ_H8; s++)
        if (!pos.square_is_empty(s))
            put_piece(Piece(pos.piece_on(s) ^ 8), flip(s));

    // Side to move
    sideToMove = flip(pos.side_to_move());

    // Castling rights
    if (pos.can_castle(WHITE_OO))
        set_castle(BLACK_OO,  king_square(BLACK), flip(pos.castle_rook_square(WHITE_OO)));
    if (pos.can_castle(WHITE_OOO))
        set_castle(BLACK_OOO, king_square(BLACK), flip(pos.castle_rook_square(WHITE_OOO)));
    if (pos.can_castle(BLACK_OO))
        set_castle(WHITE_OO,  king_square(WHITE), flip(pos.castle_rook_square(BLACK_OO)));
    if (pos.can_castle(BLACK_OOO))
        set_castle(WHITE_OOO, king_square(WHITE), flip(pos.castle_rook_square(BLACK_OOO)));

    // En passant square
    if (pos.st->epSquare != SQ_NONE)
        st->epSquare = flip(pos.st->epSquare);

    // Checkers
    st->checkersBB = attackers_to(king_square(sideToMove)) & pieces(flip(sideToMove));

    // Hash keys
    st->key = compute_key();
    st->pawnKey = compute_pawn_key();
    st->materialKey = compute_material_key();

    // Incremental scores
    st->value = compute_value();

    // Material
    st->npMaterial[WHITE] = compute_non_pawn_material(WHITE);
    st->npMaterial[BLACK] = compute_non_pawn_material(BLACK);

    assert(is_ok());
}
#endif

/// Position::is_ok() performs some consitency checks for the position object.
/// This is meant to be helpful when debugging.

bool Position::is_ok(int* failedStep) const {

#ifndef MINIMAL
    // What features of the position should be verified?
    const bool debugAll = false;
#if defined(NANOHA)
    const bool debugKingCount       = debugAll || false;
    const bool debugKingCapture     = debugAll || false;
#else
    const bool debugBitboards       = debugAll || false;
    const bool debugKingCount       = debugAll || false;
    const bool debugKingCapture     = debugAll || false;
    const bool debugCheckerCount    = debugAll || false;
#endif
    const bool debugKey             = debugAll || false;
#if defined(NANOHA)
//  const bool debugIncrementalEval = debugAll || false;
    const bool debugPieceCounts     = debugAll || false;
#else
    const bool debugMaterialKey     = debugAll || false;
    const bool debugPawnKey         = debugAll || false;
    const bool debugIncrementalEval = debugAll || false;
    const bool debugNonPawnMaterial = debugAll || false;
    const bool debugPieceCounts     = debugAll || false;
    const bool debugPieceList       = debugAll || false;
    const bool debugCastleSquares   = debugAll || false;
#endif

    if (failedStep) *failedStep = 1;

    // Side to move OK?
    if (side_to_move() != WHITE && side_to_move() != BLACK)
        return false;

#if defined(NANOHA)
    // Are the king squares in the position correct?
    if (failedStep) (*failedStep)++;
    if (king_square(WHITE) != 0 && piece_on(king_square(WHITE)) != GOU) {
        std::cerr << "kposW=0x" << std::hex << int(king_square(WHITE)) << ", Piece=0x" << int(piece_on(king_square(WHITE))) << endl;
        print_csa();
        return false;
    }

    if (failedStep) (*failedStep)++;
    if (king_square(BLACK) != 0 && piece_on(king_square(BLACK)) != SOU) {
        std::cerr << "kposB=0x" << std::hex << int(king_square(BLACK)) << ", Piece=0x" << int(piece_on(king_square(BLACK))) << endl;
        print_csa();
        return false;
    }
#else
    // Are the king squares in the position correct?
    if (failedStep) (*failedStep)++;
    if (piece_on(king_square(WHITE)) != WK)
        return false;

    if (failedStep) (*failedStep)++;
    if (piece_on(king_square(BLACK)) != BK)
        return false;

#endif

#if defined(NANOHA)
    // Do both sides have exactly one king?
    if (failedStep) (*failedStep)++;
    if (debugKingCount)
    {
        int kingCount[2] = {0, 0};
        for (Square s = SQ_A1; s <= SQ_I9; s++)
            if (type_of(piece_on(s)) == OU)
                kingCount[color_of(piece_on(s))]++;

        if (kingCount[0] != 1 || kingCount[1] != 1)
            return false;
    }

    // Can the side to move capture the opponent's king?
    if (failedStep) (*failedStep)++;
    if (debugKingCapture)
    {
        // TODO: 玉に相手駒の利きがあるか？
//      Color us = side_to_move();
//      Color them = flip(us);
//      Square ksq = king_square(them);
//      if (attackers_to(ksq) & pieces_of_color(us))
//          return false;
    }

    // Is there more than 2 checkers?
    if (failedStep) (*failedStep)++;
    // TODO:玉に3駒以上の利きがあったら不正な状態
//  if (debugCheckerCount && count_1s<CNT32>(st->checkersBB) > 2)
//      return false;
#else
    // Do both sides have exactly one king?
    if (failedStep) (*failedStep)++;
    if (debugKingCount)
    {
        int kingCount[2] = {0, 0};
        for (Square s = SQ_A1; s <= SQ_H8; s++)
            if (type_of(piece_on(s)) == KING)
                kingCount[color_of(piece_on(s))]++;

        if (kingCount[0] != 1 || kingCount[1] != 1)
            return false;
    }

    // Can the side to move capture the opponent's king?
    if (failedStep) (*failedStep)++;
    if (debugKingCapture)
    {
        Color us = side_to_move();
        Color them = flip(us);
        Square ksq = king_square(them);
        if (attackers_to(ksq) & pieces(us))
            return false;
    }

    // Is there more than 2 checkers?
    if (failedStep) (*failedStep)++;
    if (debugCheckerCount && count_1s<CNT32>(st->checkersBB) > 2)
        return false;

    // Bitboards OK?
    if (failedStep) (*failedStep)++;
    if (debugBitboards)
    {
        // The intersection of the white and black pieces must be empty
        if ((pieces(WHITE) & pieces(BLACK)) != EmptyBoardBB)
            return false;

        // The union of the white and black pieces must be equal to all
        // occupied squares
        if ((pieces(WHITE) | pieces(BLACK)) != occupied_squares())
            return false;

        // Separate piece type bitboards must have empty intersections
        for (PieceType p1 = PAWN; p1 <= KING; p1++)
            for (PieceType p2 = PAWN; p2 <= KING; p2++)
                if (p1 != p2 && (pieces(p1) & pieces(p2)))
                    return false;
    }

    // En passant square OK?
    if (failedStep) (*failedStep)++;
    if (ep_square() != SQ_NONE)
    {
        // The en passant square must be on rank 6, from the point of view of the
        // side to move.
        if (relative_rank(side_to_move(), ep_square()) != RANK_6)
            return false;
    }
#endif

    // Hash key OK?
    if (failedStep) (*failedStep)++;
    if (debugKey && st->key != compute_key())
        return false;

#if !defined(NANOHA)
    // Pawn hash key OK?
    if (failedStep) (*failedStep)++;
    if (debugPawnKey && st->pawnKey != compute_pawn_key())
        return false;

    // Material hash key OK?
    if (failedStep) (*failedStep)++;
    if (debugMaterialKey && st->materialKey != compute_material_key())
        return false;
#endif

    // Incremental eval OK?
    if (failedStep) (*failedStep)++;
#if defined(NANOHA)
    // TODO:
//  if (debugIncrementalEval && st->value != compute_value())
//      return false;

    // TODO:行き所のない駒(1段目の歩、香、桂、2段目の桂)

    // Piece counts OK?
    if (failedStep) (*failedStep)++;
    if (debugPieceCounts) {
    }
    // TODO:盤面情報と駒番号の情報のチェック.
#else
    if (debugIncrementalEval && st->value != compute_value())
        return false;

    // Non-pawn material OK?
    if (failedStep) (*failedStep)++;
    if (debugNonPawnMaterial)
    {
        if (st->npMaterial[WHITE] != compute_non_pawn_material(WHITE))
            return false;

        if (st->npMaterial[BLACK] != compute_non_pawn_material(BLACK))
            return false;
    }

    // Piece counts OK?
    if (failedStep) (*failedStep)++;
    if (debugPieceCounts)
        for (Color c = WHITE; c <= BLACK; c++)
            for (PieceType pt = PAWN; pt <= KING; pt++)
                if (pieceCount[c][pt] != count_1s<CNT32>(pieces(pt, c)))
                    return false;

    if (failedStep) (*failedStep)++;
    if (debugPieceList)
        for (Color c = WHITE; c <= BLACK; c++)
            for (PieceType pt = PAWN; pt <= KING; pt++)
                for (int i = 0; i < pieceCount[c][pt]; i++)
                {
                    if (piece_on(piece_list(c, pt)[i]) != make_piece(c, pt))
                        return false;

                    if (index[piece_list(c, pt)[i]] != i)
                        return false;
                }

    if (failedStep) (*failedStep)++;
    if (debugCastleSquares)
        for (CastleRight f = WHITE_OO; f <= BLACK_OOO; f = CastleRight(f << 1))
        {
            if (!can_castle(f))
                continue;

            Piece rook = (f & (WHITE_OO | WHITE_OOO) ? WR : BR);

            if (   castleRightsMask[castleRookSquare[f]] != (ALL_CASTLES ^ f)
                || piece_on(castleRookSquare[f]) != rook)
                return false;
        }
#endif

    if (failedStep) *failedStep = 0;
#endif
    return true;
}
