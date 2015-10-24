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

#if defined(NANOHA)
#include <stdio.h>
#else
#include "bitcount.h"
#endif
#include "movegen.h"
#include "position.h"

// x が定数のときの絶対値(プリプロセッサレベルで確定するので…)
#define ABS(x)    ((x) > 0 ? (x) : -(x))

#if !defined(NANOHA)
// Simple macro to wrap a very common while loop, no facny, no flexibility,
// hardcoded list name 'mlist' and from square 'from'.
#define SERIALIZE_MOVES(b) while (b) (*mlist++).move = make_move(from, pop_1st_bit(&b))

// Version used for pawns, where the 'from' square is given as a delta from the 'to' square
#define SERIALIZE_MOVES_D(b, d) while (b) { to = pop_1st_bit(&b); (*mlist++).move = make_move(to + (d), to); }
#endif

namespace {

#if !defined(NANOHA)
    enum CastlingSide {
        KING_SIDE,
        QUEEN_SIDE
    };

    template<CastlingSide>
    MoveStack* generate_castle_moves(const Position&, MoveStack*, Color us);

    template<Color, MoveType>
    MoveStack* generate_pawn_moves(const Position&, MoveStack*, Bitboard, Square);

    template<PieceType Pt>
    inline MoveStack* generate_discovered_checks(const Position& pos, MoveStack* mlist, Square from) {

        assert(Pt != QUEEN);
        assert(Pt != PAWN);

        Bitboard b = pos.attacks_from<Pt>(from) & pos.empty_squares();

        if (Pt == KING)
            b &= ~QueenPseudoAttacks[pos.king_square(flip(pos.side_to_move()))];

        SERIALIZE_MOVES(b);
        return mlist;
    }

    template<PieceType Pt>
    inline MoveStack* generate_direct_checks(const Position& pos, MoveStack* mlist, Color us,
                                             Bitboard dc, Square ksq) {
        assert(Pt != KING);
        assert(Pt != PAWN);

        Bitboard checkSqs, b;
        Square from;
        const Square* pl = pos.piece_list(us, Pt);

        if ((from = *pl++) == SQ_NONE)
            return mlist;

        checkSqs = pos.attacks_from<Pt>(ksq) & pos.empty_squares();

        do
        {
            if (   (Pt == QUEEN  && !(QueenPseudoAttacks[from]  & checkSqs))
                || (Pt == ROOK   && !(RookPseudoAttacks[from]   & checkSqs))
                || (Pt == BISHOP && !(BishopPseudoAttacks[from] & checkSqs)))
                continue;

            if (dc && bit_is_set(dc, from))
                continue;

            b = pos.attacks_from<Pt>(from) & checkSqs;
            SERIALIZE_MOVES(b);

        } while ((from = *pl++) != SQ_NONE);

        return mlist;
    }

    template<>
    FORCE_INLINE MoveStack* generate_direct_checks<PAWN>(const Position& p, MoveStack* m, Color us, Bitboard dc, Square ksq) {

        return (us == WHITE ? generate_pawn_moves<WHITE, MV_CHECK>(p, m, dc, ksq)
                            : generate_pawn_moves<BLACK, MV_CHECK>(p, m, dc, ksq));
    }

    template<PieceType Pt, MoveType Type>
    FORCE_INLINE MoveStack* generate_piece_moves(const Position& p, MoveStack* m, Color us, Bitboard t) {

        assert(Pt == PAWN);
        assert(Type == MV_CAPTURE || Type == MV_NON_CAPTURE || Type == MV_EVASION);

        return (us == WHITE ? generate_pawn_moves<WHITE, Type>(p, m, t, SQ_NONE)
                            : generate_pawn_moves<BLACK, Type>(p, m, t, SQ_NONE));
    }

    template<PieceType Pt>
    FORCE_INLINE MoveStack* generate_piece_moves(const Position& pos, MoveStack* mlist, Color us, Bitboard target) {

        Bitboard b;
        Square from;
        const Square* pl = pos.piece_list(us, Pt);

        if (*pl != SQ_NONE)
        {
            do {
                from = *pl;
                b = pos.attacks_from<Pt>(from) & target;
                SERIALIZE_MOVES(b);
            } while (*++pl != SQ_NONE);
        }
        return mlist;
    }

    template<>
    FORCE_INLINE MoveStack* generate_piece_moves<KING>(const Position& pos, MoveStack* mlist, Color us, Bitboard target) {

        Bitboard b;
        Square from = pos.king_square(us);

        b = pos.attacks_from<KING>(from) & target;
        SERIALIZE_MOVES(b);
        return mlist;
    }
#endif
}


/// generate<MV_CAPTURE> generates all pseudo-legal captures and queen
/// promotions. Returns a pointer to the end of the move list.
///
/// generate<MV_NON_CAPTURE> generates all pseudo-legal non-captures and
/// underpromotions. Returns a pointer to the end of the move list.
///
/// generate<MV_NON_EVASION> generates all pseudo-legal captures and
/// non-captures. Returns a pointer to the end of the move list.

#if defined(NANOHA)
//
// 汎用バージョン(MV_CAPTURE, MV_NON_EVASION, MV_NON_CAPTURE を想定)
//
template<MoveType Type>
MoveStack* generate(const Position& pos, MoveStack* mlist)
{
    assert(pos.is_ok());
    assert(!pos.in_check());

    Color us = pos.side_to_move();

    assert(Type == MV_CAPTURE || Type == MV_NON_CAPTURE || Type == MV_NON_EVASION);

    if (Type == MV_NON_EVASION) {
        mlist = (us == BLACK)
            ? pos.generate_non_evasion<BLACK>(mlist)
            : pos.generate_non_evasion<WHITE>(mlist);
    } else if (Type == MV_CAPTURE) {
        mlist = (us == BLACK)
            ? pos.generate_capture<BLACK>(mlist)
            : pos.generate_capture<WHITE>(mlist);
    } else if (Type == MV_NON_CAPTURE) {
        mlist = (us == BLACK)
            ? pos.generate_non_capture<BLACK>(mlist)
            : pos.generate_non_capture<WHITE>(mlist);
    } else {
        assert(false);
    }

    return mlist;
}
#else
template<MoveType Type>
MoveStack* generate(const Position& pos, MoveStack* mlist) {

    assert(pos.is_ok());
    assert(!pos.in_check());

    Color us = pos.side_to_move();
    Bitboard target;

    if (Type == MV_CAPTURE || Type == MV_NON_EVASION)
        target = pos.pieces(flip(us));
    else if (Type == MV_NON_CAPTURE)
        target = pos.empty_squares();
    else
        assert(false);

    if (Type == MV_NON_EVASION)
    {
        // PAWNは取る手と取らない手は非対称なので、個別に生成する。
        mlist = generate_piece_moves<PAWN, MV_CAPTURE>(pos, mlist, us, target);
        mlist = generate_piece_moves<PAWN, MV_NON_CAPTURE>(pos, mlist, us, pos.empty_squares());
        // その後、移動先を自分以外(取る手と取らない手)にする
        target |= pos.empty_squares();
    }
    else
        mlist = generate_piece_moves<PAWN, Type>(pos, mlist, us, target);

    mlist = generate_piece_moves<KNIGHT>(pos, mlist, us, target);
    mlist = generate_piece_moves<BISHOP>(pos, mlist, us, target);
    mlist = generate_piece_moves<ROOK>(pos, mlist, us, target);
    mlist = generate_piece_moves<QUEEN>(pos, mlist, us, target);
    mlist = generate_piece_moves<KING>(pos, mlist, us, target);

    if (Type != MV_CAPTURE && pos.can_castle(us))
    {
        if (pos.can_castle(us == WHITE ? WHITE_OO : BLACK_OO))
            mlist = generate_castle_moves<KING_SIDE>(pos, mlist, us);

        if (pos.can_castle(us == WHITE ? WHITE_OOO : BLACK_OOO))
            mlist = generate_castle_moves<QUEEN_SIDE>(pos, mlist, us);
    }

    return mlist;
}
#endif

// Explicit template instantiations
#if defined(NANOHA)
template MoveStack* generate<MV_CAPTURE>(const Position& pos, MoveStack* mlist);
template MoveStack* generate<MV_NON_CAPTURE>(const Position& pos, MoveStack* mlist);
template MoveStack* generate<MV_NON_EVASION>(const Position& pos, MoveStack* mlist);
#endif

/// generate_non_capture_checks() generates all pseudo-legal non-captures and knight
/// underpromotions that give check. Returns a pointer to the end of the move list.
#if defined(NANOHA)
template<>
MoveStack* generate<MV_CHECK>(const Position& pos, MoveStack* mlist)
{
    assert(pos.is_ok());
    assert(!pos.in_check());

    Color us = pos.side_to_move();

    bool bUchifudume = false;
    mlist = pos.generate_check(us, mlist, bUchifudume);

    return mlist;
}
template<>
MoveStack* generate<MV_NON_CAPTURE_CHECK>(const Position& pos, MoveStack* mlist)
{
    MoveStack *last, *cur = mlist;
    last = generate<MV_CHECK>(pos, mlist);

    // Remove capture moves from the list
    while (cur != last) {
        if (move_captured(cur->move) == EMP) {
            (mlist++)->move = cur->move;
        }
        cur++;
    }

    return mlist;
}
/// generate_evasions() generates all pseudo-legal check evasions when
/// the side to move is in check. Returns a pointer to the end of the move list.
template<>
MoveStack* generate<MV_EVASION>(const Position& pos, MoveStack* mlist)
{
    assert(pos.is_ok());
    assert(pos.in_check());

    Color us = pos.side_to_move();
#if !defined(NDEBUG)
    Square ksq = pos.king_square(us);
    assert(pos.piece_on(ksq) == make_piece(us, OU));
#endif

    return (us == BLACK) ? pos.generate_evasion<BLACK>(mlist) : pos.generate_evasion<WHITE>(mlist);
}
#else
template<>
MoveStack* generate<MV_NON_CAPTURE_CHECK>(const Position& pos, MoveStack* mlist) {

    assert(!pos.in_check());

    Bitboard b, dc;
    Square from;
    Color us = pos.side_to_move();
    Square ksq = pos.king_square(flip(us));

    assert(pos.piece_on(ksq) == make_piece(flip(us), KING));

    // Discovered non-capture checks
    b = dc = pos.discovered_check_candidates();

    while (b)
    {
        from = pop_1st_bit(&b);
        switch (type_of(pos.piece_on(from)))
        {
         case PAWN:   /* Will be generated togheter with pawns direct checks */     break;
         case KNIGHT: mlist = generate_discovered_checks<KNIGHT>(pos, mlist, from); break;
         case BISHOP: mlist = generate_discovered_checks<BISHOP>(pos, mlist, from); break;
         case ROOK:   mlist = generate_discovered_checks<ROOK>(pos, mlist, from);   break;
         case KING:   mlist = generate_discovered_checks<KING>(pos, mlist, from);   break;
         default: assert(false); break;
        }
    }

    // Direct non-capture checks
    mlist = generate_direct_checks<PAWN>(pos, mlist, us, dc, ksq);
    mlist = generate_direct_checks<KNIGHT>(pos, mlist, us, dc, ksq);
    mlist = generate_direct_checks<BISHOP>(pos, mlist, us, dc, ksq);
    mlist = generate_direct_checks<ROOK>(pos, mlist, us, dc, ksq);
    return  generate_direct_checks<QUEEN>(pos, mlist, us, dc, ksq);
}


/// generate_evasions() generates all pseudo-legal check evasions when
/// the side to move is in check. Returns a pointer to the end of the move list.
template<>
MoveStack* generate<MV_EVASION>(const Position& pos, MoveStack* mlist) {

    assert(pos.in_check());

    Bitboard b, target;
    Square from, checksq;
    int checkersCnt = 0;
    Color us = pos.side_to_move();
    Square ksq = pos.king_square(us);
    Bitboard checkers = pos.checkers();
    Bitboard sliderAttacks = EmptyBoardBB;

    assert(pos.piece_on(ksq) == make_piece(us, KING));
    assert(checkers);

    // Find squares attacked by slider checkers, we will remove
    // them from the king evasions set so to early skip known
    // illegal moves and avoid an useless legality check later.
    b = checkers;
    do
    {
        checkersCnt++;
        checksq = pop_1st_bit(&b);

        assert(color_of(pos.piece_on(checksq)) == flip(us));

        switch (type_of(pos.piece_on(checksq)))
        {
        case BISHOP: sliderAttacks |= BishopPseudoAttacks[checksq]; break;
        case ROOK:   sliderAttacks |= RookPseudoAttacks[checksq];   break;
        case QUEEN:
            // If queen and king are far we can safely remove all the squares attacked
            // in the other direction becuase are not reachable by the king anyway.
            if (squares_between(ksq, checksq) || (RookPseudoAttacks[checksq] & (1ULL << ksq)))
                sliderAttacks |= QueenPseudoAttacks[checksq];

            // Otherwise, if king and queen are adjacent and on a diagonal line, we need to
            // use real rook attacks to check if king is safe to move in the other direction.
            // For example: king in B2, queen in A1 a knight in B1, and we can safely move to C1.
            else
                sliderAttacks |= BishopPseudoAttacks[checksq] | pos.attacks_from<ROOK>(checksq);

        default:
            break;
        }
    } while (b);

    // Generate evasions for king, capture and non capture moves
    b = pos.attacks_from<KING>(ksq) & ~pos.pieces(us) & ~sliderAttacks;
    from = ksq;
    SERIALIZE_MOVES(b);

    // Generate evasions for other pieces only if not double check
    if (checkersCnt > 1)
        return mlist;

    // Find squares where a blocking evasion or a capture of the
    // checker piece is possible.
    target = squares_between(checksq, ksq) | checkers;

    mlist = generate_piece_moves<PAWN, MV_EVASION>(pos, mlist, us, target);
    mlist = generate_piece_moves<KNIGHT>(pos, mlist, us, target);
    mlist = generate_piece_moves<BISHOP>(pos, mlist, us, target);
    mlist = generate_piece_moves<ROOK>(pos, mlist, us, target);
    return  generate_piece_moves<QUEEN>(pos, mlist, us, target);
}
#endif


/// generate<MV_LEGAL> computes a complete list of legal moves in the current position

template<>
MoveStack* generate<MV_LEGAL>(const Position& pos, MoveStack* mlist) {

#if defined(NANOHA)
    return pos.in_check() ? generate<MV_EVASION>(pos, mlist)
                          : generate<MV_NON_EVASION>(pos, mlist);
#else
    MoveStack *last, *cur = mlist;
    Bitboard pinned = pos.pinned_pieces();

    last = pos.in_check() ? generate<MV_EVASION>(pos, mlist)
                          : generate<MV_NON_EVASION>(pos, mlist);

    // Remove illegal moves from the list
    while (cur != last)
        if (!pos.pl_move_is_legal(cur->move, pinned))
            cur->move = (--last)->move;
        else
            cur++;

    return last;
#endif
}


#if !defined(NANOHA)
namespace {
    template<Square Delta>
    inline Bitboard move_pawns(Bitboard p) {

        return Delta == DELTA_N  ? p << 8 : Delta == DELTA_S  ? p >> 8 :
               Delta == DELTA_NE ? p << 9 : Delta == DELTA_SE ? p >> 7 :
               Delta == DELTA_NW ? p << 7 : Delta == DELTA_SW ? p >> 9 : p;
    }

    template<MoveType Type, Square Delta>
    inline MoveStack* generate_pawn_captures(MoveStack* mlist, Bitboard pawns, Bitboard target) {

        const Bitboard TFileABB = (Delta == DELTA_NE || Delta == DELTA_SE ? FileABB : FileHBB);

        Bitboard b;
        Square to;

        // Captures in the a1-h8 (a8-h1 for black) diagonal or in the h1-a8 (h8-a1 for black)
        b = move_pawns<Delta>(pawns) & target & ~TFileABB;
        SERIALIZE_MOVES_D(b, -Delta);
        return mlist;
    }

    template<MoveType Type, Square Delta>
    inline MoveStack* generate_promotions(const Position& pos, MoveStack* mlist, Bitboard pawnsOn7, Bitboard target) {

        const Bitboard TFileABB = (Delta == DELTA_NE || Delta == DELTA_SE ? FileABB : FileHBB);

        Bitboard b;
        Square to;

        // Promotions and under-promotions, both captures and non-captures
        b = move_pawns<Delta>(pawnsOn7) & target;

        if (Delta != DELTA_N && Delta != DELTA_S)
            b &= ~TFileABB;

        while (b)
        {
            to = pop_1st_bit(&b);

            if (Type == MV_CAPTURE || Type == MV_EVASION)
                (*mlist++).move = make_promotion_move(to - Delta, to, QUEEN);

            if (Type == MV_NON_CAPTURE || Type == MV_EVASION)
            {
                (*mlist++).move = make_promotion_move(to - Delta, to, ROOK);
                (*mlist++).move = make_promotion_move(to - Delta, to, BISHOP);
                (*mlist++).move = make_promotion_move(to - Delta, to, KNIGHT);
            }

            // This is the only possible under promotion that can give a check
            // not already included in the queen-promotion.
            if (   Type == MV_CHECK
                && bit_is_set(pos.attacks_from<KNIGHT>(to), pos.king_square(Delta > 0 ? BLACK : WHITE)))
                (*mlist++).move = make_promotion_move(to - Delta, to, KNIGHT);
            else (void)pos; // Silence a warning under MSVC
        }
        return mlist;
    }

    template<Color Us, MoveType Type>
    MoveStack* generate_pawn_moves(const Position& pos, MoveStack* mlist, Bitboard target, Square ksq) {

        // Calculate our parametrized parameters at compile time, named
        // according to the point of view of white side.
        const Color    Them      = (Us == WHITE ? BLACK    : WHITE);
        const Bitboard TRank7BB  = (Us == WHITE ? Rank7BB  : Rank2BB);
        const Bitboard TRank3BB  = (Us == WHITE ? Rank3BB  : Rank6BB);
        const Square   UP        = (Us == WHITE ? DELTA_N  : DELTA_S);
        const Square   RIGHT_UP  = (Us == WHITE ? DELTA_NE : DELTA_SW);
        const Square   LEFT_UP   = (Us == WHITE ? DELTA_NW : DELTA_SE);

        Square to;
        Bitboard b1, b2, dc1, dc2, pawnPushes, emptySquares;
        Bitboard pawns = pos.pieces(PAWN, Us);
        Bitboard pawnsOn7 = pawns & TRank7BB;
        Bitboard enemyPieces = (Type == MV_CAPTURE ? target : pos.pieces(Them));

        // Pre-calculate pawn pushes before changing emptySquares definition
        if (Type != MV_CAPTURE)
        {
            emptySquares = (Type == MV_NON_CAPTURE ? target : pos.empty_squares());
            pawnPushes = move_pawns<UP>(pawns & ~TRank7BB) & emptySquares;
        }

        if (Type == MV_EVASION)
        {
            emptySquares &= target; // Only blocking squares
            enemyPieces  &= target; // Capture only the checker piece
        }

        // Promotions and underpromotions
        if (pawnsOn7)
        {
            if (Type == MV_CAPTURE)
                emptySquares = pos.empty_squares();

            pawns &= ~TRank7BB;
            mlist = generate_promotions<Type, RIGHT_UP>(pos, mlist, pawnsOn7, enemyPieces);
            mlist = generate_promotions<Type, LEFT_UP>(pos, mlist, pawnsOn7, enemyPieces);
            mlist = generate_promotions<Type, UP>(pos, mlist, pawnsOn7, emptySquares);
        }

        // Standard captures
        if (Type == MV_CAPTURE || Type == MV_EVASION)
        {
            mlist = generate_pawn_captures<Type, RIGHT_UP>(mlist, pawns, enemyPieces);
            mlist = generate_pawn_captures<Type, LEFT_UP>(mlist, pawns, enemyPieces);
        }

        // Single and double pawn pushes
        if (Type != MV_CAPTURE)
        {
            b1 = (Type != MV_EVASION ? pawnPushes : pawnPushes & emptySquares);
            b2 = move_pawns<UP>(pawnPushes & TRank3BB) & emptySquares;

            if (Type == MV_CHECK)
            {
                // Consider only pawn moves which give direct checks
                b1 &= pos.attacks_from<PAWN>(ksq, Them);
                b2 &= pos.attacks_from<PAWN>(ksq, Them);

                // Add pawn moves which gives discovered check. This is possible only
                // if the pawn is not on the same file as the enemy king, because we
                // don't generate captures.
                if (pawns & target) // For CHECK type target is dc bitboard
                {
                    dc1 = move_pawns<UP>(pawns & target & ~file_bb(ksq)) & emptySquares;
                    dc2 = move_pawns<UP>(dc1 & TRank3BB) & emptySquares;

                    b1 |= dc1;
                    b2 |= dc2;
                }
            }
            SERIALIZE_MOVES_D(b1, -UP);
            SERIALIZE_MOVES_D(b2, -UP -UP);
        }

        // En passant captures
        if ((Type == MV_CAPTURE || Type == MV_EVASION) && pos.ep_square() != SQ_NONE)
        {
            assert(Us != WHITE || rank_of(pos.ep_square()) == RANK_6);
            assert(Us != BLACK || rank_of(pos.ep_square()) == RANK_3);

            // An en passant capture can be an evasion only if the checking piece
            // is the double pushed pawn and so is in the target. Otherwise this
            // is a discovery check and we are forced to do otherwise.
            if (Type == MV_EVASION && !bit_is_set(target, pos.ep_square() - UP))
                return mlist;

            b1 = pawns & pos.attacks_from<PAWN>(pos.ep_square(), Them);

            assert(b1 != EmptyBoardBB);

            while (b1)
            {
                to = pop_1st_bit(&b1);
                (*mlist++).move = make_enpassant_move(to, pos.ep_square());
            }
        }
        return mlist;
    }

    template<CastlingSide Side>
    MoveStack* generate_castle_moves(const Position& pos, MoveStack* mlist, Color us) {

        CastleRight f = CastleRight((Side == KING_SIDE ? WHITE_OO : WHITE_OOO) << us);
        Color them = flip(us);

        // After castling, the rook and king's final positions are exactly the same
        // in Chess960 as they would be in standard chess.
        Square kfrom = pos.king_square(us);
        Square rfrom = pos.castle_rook_square(f);
        Square kto = relative_square(us, Side == KING_SIDE ? SQ_G1 : SQ_C1);
        Square rto = relative_square(us, Side == KING_SIDE ? SQ_F1 : SQ_D1);

        assert(!pos.in_check());
        assert(pos.piece_on(kfrom) == make_piece(us, KING));
        assert(pos.piece_on(rfrom) == make_piece(us, ROOK));

        // Unimpeded rule: All the squares between the king's initial and final squares
        // (including the final square), and all the squares between the rook's initial
        // and final squares (including the final square), must be vacant except for
        // the king and castling rook.
        for (Square s = Min(kfrom, kto); s <= Max(kfrom, kto); s++)
            if (  (s != kfrom && s != rfrom && !pos.square_is_empty(s))
                ||(pos.attackers_to(s) & pos.pieces(them)))
                return mlist;

        for (Square s = Min(rfrom, rto); s <= Max(rfrom, rto); s++)
            if (s != kfrom && s != rfrom && !pos.square_is_empty(s))
                return mlist;

        // Because we generate only legal castling moves we need to verify that
        // when moving the castling rook we do not discover some hidden checker.
        // For instance an enemy queen in SQ_A1 when castling rook is in SQ_B1.
        if (pos.is_chess960())
        {
            Bitboard occ = pos.occupied_squares();
            clear_bit(&occ, rfrom);
            if (pos.attackers_to(kto, occ) & pos.pieces(them))
                return mlist;
        }

        (*mlist++).move = make_castle_move(kfrom, rfrom);

        return mlist;
    }
} // namespace
#endif

// 手生成
template<Color us>
MoveStack* Position::add_straight(MoveStack* mlist, const int from, const int dir) const
{
    int z_pin = this->pin[from];
    if (z_pin == 0 || abs(z_pin) == abs(dir)) {
        // 空白の間、動く手を生成する
        int to;
        int dan;
        int fromDan = from & 0x0f;
        bool promote = can_promotion<us>(fromDan);
        const Piece piece = ban[from];
        unsigned int tmp = From2Move(from) | Piece2Move(piece);
#if !defined(NO_NARAZU)
        for (to = from + dir; ban[to] == EMP; to += dir) {
            dan = to & 0x0f;
            promote |= can_promotion<us>(dan);
            tmp &= ~TO_MASK;
            tmp |= To2Move(to);
            if (promote && (piece & PROMOTED) == 0) {
                (mlist++)->move = Move(tmp | FLAG_PROMO);
                if (us == BLACK && piece == SKY) {
                    if (dan > 1) {
                        (mlist++)->move = Move(tmp);
                    }
                }
                else if (us == WHITE && piece == GKY) {
                    if (dan < 9) {
                        (mlist++)->move = Move(tmp);
                    }
                }
                else {
                    // 角・飛車
                    // 成らない手も生成する。
                    (mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
                }
            }
            else {
                // 成れないときと馬・龍
                (mlist++)->move = Move(tmp);
            }
        }
        // 味方の駒でないなら、そこへ動く
        if ((us == BLACK && (ban[to] != WALL) && (ban[to] & GOTE))
            || (us == WHITE && (ban[to] != WALL) && (ban[to] & GOTE) == 0)) {
            dan = to & 0x0f;
            promote |= can_promotion<us>(dan);
            tmp &= ~TO_MASK;
            tmp |= To2Move(to) | Cap2Move(ban[to]);
            if (promote && (piece & PROMOTED) == 0) {
                (mlist++)->move = Move(tmp | FLAG_PROMO);
                if (piece == SKY) {
                    if (dan > 1) {
                        (mlist++)->move = Move(tmp);
                    }
                }
                else if (piece == GKY) {
                    if (dan < 9) {
                        (mlist++)->move = Move(tmp);
                    }
                }
                else {
                    // 角・飛車
                    // 成らない手も生成する。
                    (mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
                }
            }
            else {
                // 成れないときと馬・龍
                (mlist++)->move = Move(tmp);
            }
        }
#else
		//通常時
		if (get_USI_flag() == false){
			for (to = from + dir; ban[to] == EMP; to += dir) {
				dan = to & 0x0f;
				promote |= can_promotion<us>(dan);
				tmp &= ~TO_MASK;
				tmp |= To2Move(to);
				if (promote && (piece & PROMOTED) == 0) {
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (us == BLACK && piece == SKY) {
						if (dan > 1) {
							(mlist++)->move = Move(tmp);
						}
					}
					else if (us == WHITE && piece == GKY) {
						if (dan < 9) {
							(mlist++)->move = Move(tmp);
						}
					}
					else {
						// 角・飛車
						// 成らない手は生成しない。
						//	(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU); 
					}
				}
				else {
					// 成れないときと馬・龍
					(mlist++)->move = Move(tmp);
				}
			}
			// 味方の駒でないなら、そこへ動く
			if ((us == BLACK && (ban[to] != WALL) && (ban[to] & GOTE))
				|| (us == WHITE && (ban[to] != WALL) && (ban[to] & GOTE) == 0)) {
				dan = to & 0x0f;
				promote |= can_promotion<us>(dan);
				tmp &= ~TO_MASK;
				tmp |= To2Move(to) | Cap2Move(ban[to]);
				if (promote && (piece & PROMOTED) == 0) {
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (piece == SKY) {
						if (dan > 1) {
							(mlist++)->move = Move(tmp);
						}
					}
					else if (piece == GKY) {
						if (dan < 9) {
							(mlist++)->move = Move(tmp);
						}
					}
					else {
						// 角・飛車
						// 成らない手は生成しない。
						//	(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU); 
					}
				}
				else {
					// 成れないときと馬・龍
					(mlist++)->move = Move(tmp);
				}
			}
		}
		//USI_TO_MOVE_FLAGオンの時
		else{
			for (to = from + dir; ban[to] == EMP; to += dir) {
				dan = to & 0x0f;
				promote |= can_promotion<us>(dan);
				tmp &= ~TO_MASK;
				tmp |= To2Move(to);
				if (promote && (piece & PROMOTED) == 0) {
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (us == BLACK && piece == SKY) {
						if (dan > 1) {
							(mlist++)->move = Move(tmp);
						}
					}
					else if (us == WHITE && piece == GKY) {
						if (dan < 9) {
							(mlist++)->move = Move(tmp);
						}
					}
					else {
						// 角・飛車
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
					}
				}
				else {
					// 成れないときと馬・龍
					(mlist++)->move = Move(tmp);
				}
			}
			// 味方の駒でないなら、そこへ動く
			if ((us == BLACK && (ban[to] != WALL) && (ban[to] & GOTE))
				|| (us == WHITE && (ban[to] != WALL) && (ban[to] & GOTE) == 0)) {
				dan = to & 0x0f;
				promote |= can_promotion<us>(dan);
				tmp &= ~TO_MASK;
				tmp |= To2Move(to) | Cap2Move(ban[to]);
				if (promote && (piece & PROMOTED) == 0) {
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (piece == SKY) {
						if (dan > 1) {
							(mlist++)->move = Move(tmp);
						}
					}
					else if (piece == GKY) {
						if (dan < 9) {
							(mlist++)->move = Move(tmp);
						}
					}
					else {
						// 角・飛車
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
					}
				}
				else {
					// 成れないときと馬・龍
					(mlist++)->move = Move(tmp);
				}
			}
		}
#endif
    }
    return mlist;
}

template<Color us>
MoveStack* Position::add_move(MoveStack* mlist, const int from, const int dir) const
{
    const int to = from + dir;
    const Piece capture = ban[to];
    if ((capture == EMP)
        || (us == BLACK && (capture & GOTE))
        || (us == WHITE && ((capture & GOTE) == 0 && capture != WALL))
        ) {
        const int piece = ban[from];
        int dan = to & 0x0f;
        int fromDan = from & 0x0f;
        bool promote = can_promotion<us>(dan) || can_promotion<us>(fromDan);
        unsigned int tmp = From2Move(from) | To2Move(to) | Piece2Move(piece) | Cap2Move(capture);
        if (promote) {
            const int kind = piece & ~GOTE;

#if !defined(NO_NARAZU2)
			switch (kind) {
			case SFU:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (is_drop_pawn<us>(dan)) {
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
				}
				break;
			case SKY:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (is_drop_pawn<us>(dan)) {
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp);
				}
				break;
			case SKE:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (is_drop_knight<us>(dan)) {
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp);
				}
				break;
			case SGI:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				(mlist++)->move = Move(tmp);
				break;
			case SKA:
			case SHI:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				// 成らない手も生成する。
				(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
				break;
			default:
				(mlist++)->move = Move(tmp);
				break;
			}
#else
			if (get_USI_flag() == false){//通常時
				switch (kind) {
				case SFU:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					// 成らない手を生成しない。
					/*if (is_drop_pawn<us>(dan)) {
					(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
					}*/
					break;
				case SKY:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (is_drop_pawn<us>(dan)) {
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp);
					}
					break;
				case SKE:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (is_drop_knight<us>(dan)) {
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp);
					}
					break;
				case SGI:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					(mlist++)->move = Move(tmp);
					break;
				case SKA:
				case SHI:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					// 成らない手を生成しない。
					//(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
					break;
				default:
					(mlist++)->move = Move(tmp);
					break;
				}
			}
			else{//USI_TO_MOVE_FLAGオンの時
				switch (kind) {
				case SFU:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (is_drop_pawn<us>(dan)) {
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
					}
					break;
				case SKY:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (is_drop_pawn<us>(dan)) {
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp);
					}
					break;
				case SKE:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					if (is_drop_knight<us>(dan)) {
						// 成らない手も生成する。
						(mlist++)->move = Move(tmp);
					}
					break;
				case SGI:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					(mlist++)->move = Move(tmp);
					break;
				case SKA:
				case SHI:
					(mlist++)->move = Move(tmp | FLAG_PROMO);
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
					break;
				default:
					(mlist++)->move = Move(tmp);
					break;

				}
			}
#endif
        }
        else {
            // 成れない
            (mlist++)->move = Move(tmp);
        }
    }
    return mlist;
}

// 指定場所(to)に動く手の生成（玉以外）
MoveStack* Position::gen_move_to(const Color us, MoveStack* mlist, int to) const
{
    effect_t efft = (us == BLACK) ? this->effectB[to] : this->effectW[to];

    // 指定場所に利いている駒がない
    if ((efft & (EFFECT_SHORT_MASK | EFFECT_LONG_MASK)) == 0) return mlist;

    int z;
    int pn;

    // 飛びの利き
    effect_t long_effect = efft & EFFECT_LONG_MASK;
    while (long_effect) {
        unsigned long id;
        _BitScanForward(&id, long_effect);
        id -= EFFECT_LONG_SHIFT;
        long_effect &= long_effect - 1;

        z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
        pn = pin[z];
        if (pn == 0 || abs(pn) == abs(NanohaTbl::Direction[id])) {
            mlist = (us == BLACK) ? add_moveB(mlist, z, to - z) : add_moveW(mlist, z, to - z);
        }
    }

    // 短い利き
    efft &= EFFECT_SHORT_MASK;
    while (efft) {
        unsigned long id;
        _BitScanForward(&id, efft);
        efft &= efft - 1;

        z = to - NanohaTbl::Direction[id];
        pn = pin[z];
        if (pn == 0 || abs(pn) == abs(NanohaTbl::Direction[id])) {
            if (us == BLACK) {
                if (ban[z] != SOU) mlist = add_moveB(mlist, z, to - z);
            }
            else {
                if (ban[z] != GOU) mlist = add_moveW(mlist, z, to - z);
            }
        }
    }
    return mlist;
}

// 指定場所(to)に駒を打つ手の生成
MoveStack* Position::gen_drop_to(const Color us, MoveStack* mlist, int to) const
{
    int dan = to & 0x0f;
    if (us != BLACK) {
        dan = 10 - dan;
    }
    const Hand &h = (us == BLACK) ? handS : handG;
    const int SorG = (us == BLACK) ? SENTE : GOTE;
#define SetTe(koma)    \
    if (h.get ## koma() > 0) {        \
        (mlist++)->move = Move(To2Move(to) | Piece2Move(SorG|koma));        \
        }

    if (h.getFU() > 0 && dan > 1) {
        // 歩を打つ手を生成
        // 二歩チェック
        int nifu = is_double_pawn(us, to & 0xF0);
        // 打ち歩詰めもチェック
        if (!nifu && !is_pawn_drop_mate(us, to)) {
            (mlist++)->move = Move(To2Move(to) | Piece2Move(SorG | FU));
        }
    }
    if (h.getKY() > 0 && dan > 1) {
        // 香を打つ手を生成
        (mlist++)->move = Move(To2Move(to) | Piece2Move(SorG | KY));
    }
    if (h.getKE() > 0 && dan > 2) {
        (mlist++)->move = Move(To2Move(to) | Piece2Move(SorG | KE));
    }
    SetTe(GI)
        SetTe(KI)
        SetTe(KA)
        SetTe(HI)
#undef SetTe
        return mlist;
}

// 駒を打つ手の生成
template <Color us>
MoveStack* Position::gen_drop(MoveStack* mlist) const
{
    int z;
    int suji;
    unsigned int tmp;
    int StartDan;
    ///    int teNum = teNumM;    // アドレスを取らない
    // 歩を打つ
    uint32_t exists;
    exists = (us == BLACK) ? handS.existFU() : handG.existFU();
    if (exists > 0) {
        tmp = (us == BLACK) ? Piece2Move(SFU) : Piece2Move(GFU);    // From = 0;
        //(先手なら２段目より下に、後手なら８段目より上に打つ）
        StartDan = (us == BLACK) ? 2 : 1;
        for (suji = 0x10; suji <= 0x90; suji += 0x10) {
            // 二歩チェック
            if (is_double_pawn(us, suji)) continue;
            z = suji + StartDan;
            // 打ち歩詰めもチェック
#define FU_FUNC(z)    \
    if (ban[z] == EMP && !is_pawn_drop_mate(us, z)) {    \
        (mlist++)->move = Move(tmp | To2Move(z));    \
        }
            FU_FUNC(z)
                FU_FUNC(z + 1)
                FU_FUNC(z + 2)
                FU_FUNC(z + 3)
                FU_FUNC(z + 4)
                FU_FUNC(z + 5)
                FU_FUNC(z + 6)
                FU_FUNC(z + 7)
#undef FU_FUNC
        }
    }

    // 香を打つ
    exists = (us == BLACK) ? handS.existKY() : handG.existKY();
    if (exists > 0) {
        tmp = (us == BLACK) ? Piece2Move(SKY) : Piece2Move(GKY); // From = 0
        //(先手なら２段目より下に、後手なら８段目より上に打つ）
        z = (us == BLACK) ? 0x12 : 0x11;
        for (; z <= 0x99; z += 0x10) {
#define KY_FUNC(z)    \
            if (ban[z] == EMP) {    \
                (mlist++)->move = Move(tmp | To2Move(z));    \
                        }
            KY_FUNC(z)
                KY_FUNC(z + 1)
                KY_FUNC(z + 2)
                KY_FUNC(z + 3)
                KY_FUNC(z + 4)
                KY_FUNC(z + 5)
                KY_FUNC(z + 6)
                KY_FUNC(z + 7)
#undef KY_FUNC
        }
    }

    //桂を打つ
    exists = (us == BLACK) ? handS.existKE() : handG.existKE();
    if (exists > 0) {
        //(先手なら３段目より下に、後手なら７段目より上に打つ）
        tmp = (us == BLACK) ? Piece2Move(SKE) : Piece2Move(GKE); // From = 0
        z = (us == BLACK) ? 0x13 : 0x11;
        for (; z <= 0x99; z += 0x10) {
#define KE_FUNC(z)    \
            if (ban[z] == EMP) {    \
                (mlist++)->move = Move(tmp | To2Move(z));    \
                        }
            KE_FUNC(z)
                KE_FUNC(z + 1)
                KE_FUNC(z + 2)
                KE_FUNC(z + 3)
                KE_FUNC(z + 4)
                KE_FUNC(z + 5)
                KE_FUNC(z + 6)
#undef KE_FUNC
        }
    }

    // 銀～飛車は、どこにでも打てる
    const uint32_t koma_start = (us == BLACK) ? SGI : GGI;
    const uint32_t koma_end = (us == BLACK) ? SHI : GHI;
    uint32_t a[4];
    a[0] = (us == BLACK) ? handS.existGI() : handG.existGI();
    a[1] = (us == BLACK) ? handS.existKI() : handG.existKI();
    a[2] = (us == BLACK) ? handS.existKA() : handG.existKA();
    a[3] = (us == BLACK) ? handS.existHI() : handG.existHI();
    for (uint32_t koma = koma_start, i = 0; koma <= koma_end; koma++, i++) {
        if (a[i] > 0) {
            tmp = Piece2Move(koma); // From = 0
            for (z = 0x11; z <= 0x99; z += 0x10) {
#define GI_FUNC(z)    \
                if (ban[z] == EMP) {    \
                    (mlist++)->move = Move(tmp | To2Move(z));    \
                                }
                GI_FUNC(z)
                    GI_FUNC(z + 1)
                    GI_FUNC(z + 2)
                    GI_FUNC(z + 3)
                    GI_FUNC(z + 4)
                    GI_FUNC(z + 5)
                    GI_FUNC(z + 6)
                    GI_FUNC(z + 7)
                    GI_FUNC(z + 8)
#undef GI_FUNC
            }
        }
    }

#if defined(DEBUG_GENERATE)
    while (top != mlist) {
        Move m = top->move;
        if (!m.IsDrop()) {
            if (piece_on(Square(m.From())) == EMP) {
                assert(false);
            }
        }
        top++;
    }
#endif
    return mlist;
}

//玉の動く手の生成
MoveStack* Position::gen_move_king(const Color us, MoveStack* mlist, int pindir) const
{
    int to;
    Piece koma;
    unsigned int tmp = (us == BLACK) ? From2Move(kingS) | Piece2Move(SOU) : From2Move(kingG) | Piece2Move(GOU);

#define MoveKB(dir) to = kingS - DIR_##dir;    \
                    if (EXIST_EFFECT(effectW[to]) == 0) {    \
                        koma = ban[to];        \
                        if (koma == EMP || (koma & GOTE)) {        \
                            (mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));    \
                                                }        \
                                        }
#define MoveKW(dir) to = kingG - DIR_##dir;    \
                    if (EXIST_EFFECT(effectB[to]) == 0) {    \
                        koma = ban[to];        \
                        if (koma != WALL && !(koma & GOTE)) {        \
                            (mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));    \
                                                }        \
                                        }

    if (us == BLACK) {
        if (pindir == 0) {
            MoveKB(UP)
                MoveKB(UR)
                MoveKB(UL)
                MoveKB(RIGHT)
                MoveKB(LEFT)
                MoveKB(DR)
                MoveKB(DL)
                MoveKB(DOWN)
        }
        else {
            if (pindir != ABS(DIR_UP)) { MoveKB(UP) }
            if (pindir != ABS(DIR_UR)) { MoveKB(UR) }
            if (pindir != ABS(DIR_UL)) { MoveKB(UL) }
            if (pindir != ABS(DIR_RIGHT)) { MoveKB(RIGHT) }
            if (pindir != ABS(DIR_LEFT)) { MoveKB(LEFT) }
            if (pindir != ABS(DIR_DR)) { MoveKB(DR) }
            if (pindir != ABS(DIR_DL)) { MoveKB(DL) }
            if (pindir != ABS(DIR_DOWN)) { MoveKB(DOWN) }
        }
    }
    else {
        if (pindir == 0) {
            MoveKW(UP)
                MoveKW(UR)
                MoveKW(UL)
                MoveKW(RIGHT)
                MoveKW(LEFT)
                MoveKW(DR)
                MoveKW(DL)
                MoveKW(DOWN)
        }
        else {
            if (pindir != ABS(DIR_UP)) { MoveKW(UP) }
            if (pindir != ABS(DIR_UR)) { MoveKW(UR) }
            if (pindir != ABS(DIR_UL)) { MoveKW(UL) }
            if (pindir != ABS(DIR_RIGHT)) { MoveKW(RIGHT) }
            if (pindir != ABS(DIR_LEFT)) { MoveKW(LEFT) }
            if (pindir != ABS(DIR_DR)) { MoveKW(DR) }
            if (pindir != ABS(DIR_DL)) { MoveKW(DL) }
            if (pindir != ABS(DIR_DOWN)) { MoveKW(DOWN) }
        }
    }
#undef MoveKB
#undef MoveKW
    return mlist;
}


//fromから動く手の生成
// 盤面のfromにある駒を動かす手を生成する。
// pindir        動けない方向(pinされている)
MoveStack* Position::gen_move_from(const Color us, MoveStack* mlist, int from, int pindir) const
{
    int z_pin = abs(this->pin[from]);
    pindir = abs(pindir);
#define AddMoveM1(teban,dir)     if (pindir != ABS(DIR_ ## dir)) if (z_pin == ABS(DIR_ ## dir)) mlist = add_move##teban(mlist, from, DIR_ ## dir)
#define AddStraightM1(teban,dir) if (pindir != ABS(DIR_ ## dir)) if (z_pin == ABS(DIR_ ## dir)) mlist = add_straight##teban(mlist, from, DIR_ ## dir)
#define AddMoveM2(teban,dir)     if (pindir != ABS(DIR_ ## dir)) mlist = add_move##teban(mlist, from, DIR_ ## dir)
#define AddStraightM2(teban,dir) if (pindir != ABS(DIR_ ## dir)) mlist = add_straight##teban(mlist, from, DIR_ ## dir)
    switch (ban[from]) {
    case SFU:
        if (z_pin) {
            AddMoveM1(B, UP);
        }
        else if (pindir) {
            AddMoveM2(B, UP);
        }
        else {
            mlist = add_moveB(mlist, from, DIR_UP);
        }
        break;
    case SKY:
        if (z_pin) {
            AddStraightM1(B, UP);
        }
        else if (pindir) {
            AddStraightM2(B, UP);
        }
        else {
            mlist = add_straightB(mlist, from, DIR_UP);
        }
        break;
    case SKE:
        // 桂馬はpinとなる方向がない
        if (z_pin == 0) {
            mlist = add_moveB(mlist, from, DIR_KEUR);
            mlist = add_moveB(mlist, from, DIR_KEUL);
        }
        break;
    case SGI:
        if (z_pin) {
            AddMoveM1(B, UP);
            AddMoveM1(B, UR);
            AddMoveM1(B, UL);
            AddMoveM1(B, DR);
            AddMoveM1(B, DL);
        }
        else if (pindir) {
            AddMoveM2(B, UP);
            AddMoveM2(B, UR);
            AddMoveM2(B, UL);
            AddMoveM2(B, DR);
            AddMoveM2(B, DL);
        }
        else {
            mlist = add_moveB(mlist, from, DIR_UP);
            mlist = add_moveB(mlist, from, DIR_UR);
            mlist = add_moveB(mlist, from, DIR_UL);
            mlist = add_moveB(mlist, from, DIR_DR);
            mlist = add_moveB(mlist, from, DIR_DL);
        }
        break;
    case SKI:case STO:case SNY:case SNK:case SNG:
        if (z_pin) {
            AddMoveM1(B, UP);
            AddMoveM1(B, UR);
            AddMoveM1(B, UL);
            AddMoveM1(B, DOWN);
            AddMoveM1(B, RIGHT);
            AddMoveM1(B, LEFT);
        }
        else if (pindir) {
            AddMoveM2(B, UP);
            AddMoveM2(B, UR);
            AddMoveM2(B, UL);
            AddMoveM2(B, DOWN);
            AddMoveM2(B, RIGHT);
            AddMoveM2(B, LEFT);
        }
        else {
            mlist = add_moveB(mlist, from, DIR_UP);
            mlist = add_moveB(mlist, from, DIR_UR);
            mlist = add_moveB(mlist, from, DIR_UL);
            mlist = add_moveB(mlist, from, DIR_DOWN);
            mlist = add_moveB(mlist, from, DIR_RIGHT);
            mlist = add_moveB(mlist, from, DIR_LEFT);
        }
        break;
    case SUM:
        if (z_pin) {
            AddMoveM1(B, UP);
            AddMoveM1(B, RIGHT);
            AddMoveM1(B, LEFT);
            AddMoveM1(B, DOWN);
            AddStraightM1(B, UR);
            AddStraightM1(B, UL);
            AddStraightM1(B, DR);
            AddStraightM1(B, DL);
        }
        else if (pindir) {
            AddMoveM2(B, UP);
            AddMoveM2(B, RIGHT);
            AddMoveM2(B, LEFT);
            AddMoveM2(B, DOWN);
            AddStraightM2(B, UR);
            AddStraightM2(B, UL);
            AddStraightM2(B, DR);
            AddStraightM2(B, DL);
        }
        else {
            mlist = add_moveB(mlist, from, DIR_UP);
            mlist = add_moveB(mlist, from, DIR_RIGHT);
            mlist = add_moveB(mlist, from, DIR_LEFT);
            mlist = add_moveB(mlist, from, DIR_DOWN);
            mlist = add_straightB(mlist, from, DIR_UR);
            mlist = add_straightB(mlist, from, DIR_UL);
            mlist = add_straightB(mlist, from, DIR_DR);
            mlist = add_straightB(mlist, from, DIR_DL);
        }
        break;
    case SKA:
        if (z_pin) {
            AddStraightM1(B, UR);
            AddStraightM1(B, UL);
            AddStraightM1(B, DR);
            AddStraightM1(B, DL);
        }
        else if (pindir) {
            AddStraightM2(B, UR);
            AddStraightM2(B, UL);
            AddStraightM2(B, DR);
            AddStraightM2(B, DL);
        }
        else {
            mlist = add_straightB(mlist, from, DIR_UR);
            mlist = add_straightB(mlist, from, DIR_UL);
            mlist = add_straightB(mlist, from, DIR_DR);
            mlist = add_straightB(mlist, from, DIR_DL);
        }
        break;
    case SRY:
        if (z_pin) {
            AddMoveM1(B, UR);
            AddMoveM1(B, UL);
            AddMoveM1(B, DR);
            AddMoveM1(B, DL);
            AddStraightM1(B, UP);
            AddStraightM1(B, RIGHT);
            AddStraightM1(B, LEFT);
            AddStraightM1(B, DOWN);
        }
        else if (pindir) {
            AddMoveM2(B, UR);
            AddMoveM2(B, UL);
            AddMoveM2(B, DR);
            AddMoveM2(B, DL);
            AddStraightM2(B, UP);
            AddStraightM2(B, RIGHT);
            AddStraightM2(B, LEFT);
            AddStraightM2(B, DOWN);
        }
        else {
            mlist = add_moveB(mlist, from, DIR_UR);
            mlist = add_moveB(mlist, from, DIR_UL);
            mlist = add_moveB(mlist, from, DIR_DR);
            mlist = add_moveB(mlist, from, DIR_DL);
            mlist = add_straightB(mlist, from, DIR_UP);
            mlist = add_straightB(mlist, from, DIR_RIGHT);
            mlist = add_straightB(mlist, from, DIR_LEFT);
            mlist = add_straightB(mlist, from, DIR_DOWN);
        }
        break;
    case SHI:
        if (z_pin) {
            AddStraightM1(B, UP);
            AddStraightM1(B, RIGHT);
            AddStraightM1(B, LEFT);
            AddStraightM1(B, DOWN);
        }
        else if (pindir) {
            AddStraightM2(B, UP);
            AddStraightM2(B, RIGHT);
            AddStraightM2(B, LEFT);
            AddStraightM2(B, DOWN);
        }
        else {
            mlist = add_straightB(mlist, from, DIR_UP);
            mlist = add_straightB(mlist, from, DIR_RIGHT);
            mlist = add_straightB(mlist, from, DIR_LEFT);
            mlist = add_straightB(mlist, from, DIR_DOWN);
        }
        break;
    case SOU:
        mlist = gen_move_king(us, mlist, pindir);
        break;

    case GFU:
        if (z_pin) {
            AddMoveM1(W, DOWN);
        }
        else if (pindir) {
            AddMoveM2(W, DOWN);
        }
        else {
            mlist = add_moveW(mlist, from, DIR_DOWN);
        }
        break;
    case GKY:
        if (z_pin) {
            AddStraightM1(W, DOWN);
        }
        else if (pindir) {
            AddStraightM2(W, DOWN);
        }
        else {
            mlist = add_straightW(mlist, from, DIR_DOWN);
        }
        break;
    case GKE:
        // 桂馬はpinとなる方向がない
        if (z_pin == 0) {
            mlist = add_moveW(mlist, from, DIR_KEDR);
            mlist = add_moveW(mlist, from, DIR_KEDL);
        }
        break;
    case GGI:
        if (z_pin) {
            AddMoveM1(W, DOWN);
            AddMoveM1(W, DR);
            AddMoveM1(W, DL);
            AddMoveM1(W, UR);
            AddMoveM1(W, UL);
        }
        else if (pindir) {
            AddMoveM2(W, DOWN);
            AddMoveM2(W, DR);
            AddMoveM2(W, DL);
            AddMoveM2(W, UR);
            AddMoveM2(W, UL);
        }
        else {
            mlist = add_moveW(mlist, from, DIR_DOWN);
            mlist = add_moveW(mlist, from, DIR_DR);
            mlist = add_moveW(mlist, from, DIR_DL);
            mlist = add_moveW(mlist, from, DIR_UR);
            mlist = add_moveW(mlist, from, DIR_UL);
        }
        break;
    case GKI:case GTO:case GNY:case GNK:case GNG:
        if (z_pin) {
            AddMoveM1(W, DOWN);
            AddMoveM1(W, DR);
            AddMoveM1(W, DL);
            AddMoveM1(W, UP);
            AddMoveM1(W, RIGHT);
            AddMoveM1(W, LEFT);
        }
        else if (pindir) {
            AddMoveM2(W, DOWN);
            AddMoveM2(W, DR);
            AddMoveM2(W, DL);
            AddMoveM2(W, UP);
            AddMoveM2(W, RIGHT);
            AddMoveM2(W, LEFT);
        }
        else {
            mlist = add_moveW(mlist, from, DIR_DOWN);
            mlist = add_moveW(mlist, from, DIR_DR);
            mlist = add_moveW(mlist, from, DIR_DL);
            mlist = add_moveW(mlist, from, DIR_UP);
            mlist = add_moveW(mlist, from, DIR_RIGHT);
            mlist = add_moveW(mlist, from, DIR_LEFT);
        }
        break;
    case GRY:
        if (z_pin) {
            AddMoveM1(W, UR);
            AddMoveM1(W, UL);
            AddMoveM1(W, DR);
            AddMoveM1(W, DL);
            AddStraightM1(W, UP);
            AddStraightM1(W, RIGHT);
            AddStraightM1(W, LEFT);
            AddStraightM1(W, DOWN);
        }
        else if (pindir) {
            AddMoveM2(W, UR);
            AddMoveM2(W, UL);
            AddMoveM2(W, DR);
            AddMoveM2(W, DL);
            AddStraightM2(W, UP);
            AddStraightM2(W, RIGHT);
            AddStraightM2(W, LEFT);
            AddStraightM2(W, DOWN);
        }
        else {
            mlist = add_moveW(mlist, from, DIR_UR);
            mlist = add_moveW(mlist, from, DIR_UL);
            mlist = add_moveW(mlist, from, DIR_DR);
            mlist = add_moveW(mlist, from, DIR_DL);
            mlist = add_straightW(mlist, from, DIR_UP);
            mlist = add_straightW(mlist, from, DIR_RIGHT);
            mlist = add_straightW(mlist, from, DIR_LEFT);
            mlist = add_straightW(mlist, from, DIR_DOWN);
        }
        break;
    case GHI:
        if (z_pin) {
            AddStraightM1(W, UP);
            AddStraightM1(W, RIGHT);
            AddStraightM1(W, LEFT);
            AddStraightM1(W, DOWN);
        }
        else if (pindir) {
            AddStraightM2(W, UP);
            AddStraightM2(W, RIGHT);
            AddStraightM2(W, LEFT);
            AddStraightM2(W, DOWN);
        }
        else {
            mlist = add_straightW(mlist, from, DIR_UP);
            mlist = add_straightW(mlist, from, DIR_RIGHT);
            mlist = add_straightW(mlist, from, DIR_LEFT);
            mlist = add_straightW(mlist, from, DIR_DOWN);
        }
        break;
    case GUM:
        if (z_pin) {
            AddMoveM1(W, UP);
            AddMoveM1(W, RIGHT);
            AddMoveM1(W, LEFT);
            AddMoveM1(W, DOWN);
            AddStraightM1(W, UR);
            AddStraightM1(W, UL);
            AddStraightM1(W, DR);
            AddStraightM1(W, DL);
        }
        else if (pindir) {
            AddMoveM2(W, UP);
            AddMoveM2(W, RIGHT);
            AddMoveM2(W, LEFT);
            AddMoveM2(W, DOWN);
            AddStraightM2(W, UR);
            AddStraightM2(W, UL);
            AddStraightM2(W, DR);
            AddStraightM2(W, DL);
        }
        else {
            mlist = add_moveW(mlist, from, DIR_UP);
            mlist = add_moveW(mlist, from, DIR_RIGHT);
            mlist = add_moveW(mlist, from, DIR_LEFT);
            mlist = add_moveW(mlist, from, DIR_DOWN);
            mlist = add_straightW(mlist, from, DIR_UR);
            mlist = add_straightW(mlist, from, DIR_UL);
            mlist = add_straightW(mlist, from, DIR_DR);
            mlist = add_straightW(mlist, from, DIR_DL);
        }
        break;
    case GKA:
        if (z_pin) {
            AddStraightM1(W, UR);
            AddStraightM1(W, UL);
            AddStraightM1(W, DR);
            AddStraightM1(W, DL);
        }
        else if (pindir) {
            AddStraightM2(W, UR);
            AddStraightM2(W, UL);
            AddStraightM2(W, DR);
            AddStraightM2(W, DL);
        }
        else {
            mlist = add_straightW(mlist, from, DIR_UR);
            mlist = add_straightW(mlist, from, DIR_UL);
            mlist = add_straightW(mlist, from, DIR_DR);
            mlist = add_straightW(mlist, from, DIR_DL);
        }
        break;
    case GOU:
        mlist = gen_move_king(us, mlist, pindir);
        break;
    case EMP: case WALL: case PIECE_NONE:
    default:
        break;
    }
#undef AddMoveM
    return mlist;
}


// 取る手(＋歩を成る手)を生成
template<Color us>
MoveStack* Position::generate_capture(MoveStack* mlist) const
{
    ///    int teNum = 0;
    int z;
    int from;

    int kno;    // 駒番号
    effect_t k;
    unsigned long id;
#if defined(DEBUG_GENERATE)
    MoveStack* top = mlist;
#endif

    for (kno = 1; kno <= MAX_KOMANO; kno++) {
        z = knpos[kno];
        if (OnBoard(z)) {
            if (us == BLACK) {
                // 先手番のとき
                if ((knkind[kno] & GOTE) && EXIST_EFFECT(effectB[z])) {
                    // 後手の駒に先手の利きがあれば取れる？(要pin情報の考慮)
                    k = effectB[z] & EFFECT_SHORT_MASK;
                    while (k) {
                        _BitScanForward(&id, k);
                        k &= k - 1;
                        from = z - NanohaTbl::Direction[id];
                        if (pin[from] && abs(pin[from]) != abs(NanohaTbl::Direction[id])) continue;
                        if (ban[from] == SOU) {
                            // 玉は相手の利きがある駒を取れない
                            if (EXIST_EFFECT(effectW[z]) == 0) {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else if ((z & 0x0F) <= 0x03) {
                            // 成れる位置？
                            if (ban[from] == SGI) {
                                // 桂銀は成と不成を生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                            else if (ban[from] == SFU) {
                                // 歩は成のみ生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                            }
                            else if (ban[from] == SKE) {
                                // 桂は成を生成し3段目のみ不成を生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                                if ((z & 0xF) == 0x3) {
                                    mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                    mlist++;
                                }
                            }
                            else {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else {
                            mlist->move = cons_move(from, z, ban[from], ban[z]);
                            mlist++;
                        }
                    }
                    k = effectB[z] & EFFECT_LONG_MASK;
                    while (k) {
                        _BitScanForward(&id, k);
                        k &= k - 1;
                        from = SkipOverEMP(z, -NanohaTbl::Direction[id]);
                        if (pin[from] && abs(pin[from]) != abs(NanohaTbl::Direction[id])) continue;
                        if (ban[from] == SKA || ban[from] == SHI) {
                            // 角飛は成れるときは成のみ生成する
                            if ((z & 0x0F) <= 0x3 || (from & 0x0F) <= 0x3) {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                            }
                            else {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else if (ban[from] == SKY) {
                            if ((z & 0x0F) <= 0x3) {
                                // 香は3段目のときのみ不成を生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                                if ((z & 0xF) == 0x3) {
                                    mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                    mlist++;
                                }
                            }
                            else {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else {
                            mlist->move = cons_move(from, z, ban[from], ban[z]);
                            mlist++;
                        }
                    }
                }
            }
            else {
                // 後手番のとき
                if ((knkind[kno] & GOTE) == 0 && EXIST_EFFECT(effectW[z])) {
                    // 先手の駒に後手の利きがあれば取れる？(要pin情報の考慮)
                    k = effectW[z] & EFFECT_SHORT_MASK;
                    while (k) {
                        _BitScanForward(&id, k);
                        k &= k - 1;
                        from = z - NanohaTbl::Direction[id];
                        if (pin[from] && abs(pin[from]) != abs(NanohaTbl::Direction[id])) continue;
                        if (ban[from] == GOU) {
                            // 玉は相手の利きがある駒を取れない
                            if (EXIST_EFFECT(effectB[z]) == 0) {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else if ((z & 0x0F) >= 0x07) {
                            // 成れる位置？
                            if (ban[from] == GGI) {
                                // 桂銀は成と不成を生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                            else if (ban[from] == GFU) {
                                // 歩は成のみ生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                            }
                            else if (ban[from] == GKE) {
                                // 桂は成を生成し7段目のみ不成を生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                                if ((z & 0xF) == 0x7) {
                                    mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                    mlist++;
                                }
                            }
                            else {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else {
                            mlist->move = cons_move(from, z, ban[from], ban[z]);
                            mlist++;
                        }
                    }
                    k = effectW[z] & EFFECT_LONG_MASK;
                    while (k) {
                        _BitScanForward(&id, k);
                        k &= k - 1;
                        from = SkipOverEMP(z, -NanohaTbl::Direction[id]);
                        if (pin[from] && abs(pin[from]) != abs(NanohaTbl::Direction[id])) continue;
                        if (ban[from] == GKA || ban[from] == GHI) {
                            // 角飛は成れるときは成のみ生成する
                            if ((z & 0x0F) >= 0x7 || (from & 0x0F) >= 0x7) {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                            }
                            else {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else if (ban[from] == GKY) {
                            if ((z & 0x0F) >= 0x7) {
                                // 香は7段目のときのみ不成を生成する
                                mlist->move = cons_move(from, z, ban[from], ban[z], 1);
                                mlist++;
                                if ((z & 0xF) == 0x7) {
                                    mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                    mlist++;
                                }
                            }
                            else {
                                mlist->move = cons_move(from, z, ban[from], ban[z], 0);
                                mlist++;
                            }
                        }
                        else {
                            mlist->move = cons_move(from, z, ban[from], ban[z]);
                            mlist++;
                        }
                    }
                }
            }
        }
    }
    // 敵陣に飛を成り込む手を生成する
    if (us == BLACK) {
        for (kno = KNS_HI; kno <= KNE_HI; kno++) {
            if (knkind[kno] == SHI) {
                from = knpos[kno];
                int dan = from & 0x0F;
                if (OnBoard(from) && dan >= 4 && (pin[from] == 0 || pin[from] == DIR_UP || pin[from] == DIR_DOWN)) {
                    int to = SkipOverEMP(from, DIR_UP);
                    while ((to & 0x0F) <= 2) {
                        to -= DIR_UP;
                        mlist->move = cons_move(from, to, ban[from], ban[to], 1);
                        mlist++;
                    }
                }
            }
        }
    }
    else {
        for (kno = KNS_HI; kno <= KNE_HI; kno++) {
            if (knkind[kno] == GHI) {
                from = knpos[kno];
                int dan = from & 0x0F;
                if (OnBoard(from) && dan <= 6 && (pin[from] == 0 || pin[from] == DIR_UP || pin[from] == DIR_DOWN)) {
                    int to = SkipOverEMP(from, DIR_DOWN);
                    while ((to & 0x0F) >= 8) {
                        to -= DIR_DOWN;
                        mlist->move = cons_move(from, to, ban[from], ban[to], 1);
                        mlist++;
                    }
                }
            }
        }
    }
    // 歩の成る手を生成
    if (us == BLACK) {
        for (kno = KNS_FU; kno <= KNE_FU; kno++) {
            if (knkind[kno] == SFU) {
                from = knpos[kno];
                if (OnBoard(from) && ((from & 0x0F) <= 4) && ban[from + DIR_UP] == EMP) {
                    if (pin[from] == 0 || abs(pin[from]) == 1) {
                        mlist->move = cons_move(from, from + DIR_UP, ban[from], ban[from + DIR_UP], 1);
                        mlist++;
                    }
                }
            }
        }
    }
    else {
        for (kno = KNS_FU; kno <= KNE_FU; kno++) {
            if (knkind[kno] == GFU) {
                from = knpos[kno];
                if (OnBoard(from) && ((from & 0x0F) >= 6) && ban[from + DIR_DOWN] == EMP) {
                    if (pin[from] == 0 || abs(pin[from]) == 1) {
                        mlist->move = cons_move(from, from + DIR_DOWN, ban[from], ban[from + DIR_DOWN], 1);
                        mlist++;
                    }
                }
            }
        }
    }
#if defined(DEBUG_GENERATE)
    while (top != mlist) {
        Move m = top->move;
        if (!m.IsDrop()) {
            if (piece_on(Square(m.From())) == EMP) {
                assert(false);
            }
        }
        top++;
    }
#endif

    return mlist;
}

// 玉を動かす手の生成(駒を取らない)
// 普通の駒と違い、相手の利きのあるところには動けない
// pindir        動けない方向
MoveStack* Position::gen_king_noncapture(const Color us, MoveStack* mlist, const int pindir) const
{
    int to;
    Piece koma;
    unsigned int tmp = (us == BLACK) ? From2Move(kingS) | Piece2Move(SOU) : From2Move(kingG) | Piece2Move(GOU);

#define MoveKS(dir) to = kingS - DIR_##dir;    \
                    if (EXIST_EFFECT(effectW[to]) == 0) {    \
                        koma = ban[to];        \
                        if (koma == EMP) {        \
                            (mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));    \
                                                }        \
                                        }
#define MoveKG(dir) to = kingG - DIR_##dir;    \
                    if (EXIST_EFFECT(effectB[to]) == 0) {    \
                        koma = ban[to];        \
                        if (koma == EMP) {        \
                            (mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));    \
                                                }        \
                                        }

    if (us == BLACK) {
        if (pindir == 0) {
            MoveKS(UP)
                MoveKS(UR)
                MoveKS(UL)
                MoveKS(RIGHT)
                MoveKS(LEFT)
                MoveKS(DR)
                MoveKS(DL)
                MoveKS(DOWN)
        }
        else {
            if (pindir != ABS(DIR_UP)) { MoveKS(UP) }
            if (pindir != ABS(DIR_UR)) { MoveKS(UR) }
            if (pindir != ABS(DIR_UL)) { MoveKS(UL) }
            if (pindir != ABS(DIR_RIGHT)) { MoveKS(RIGHT) }
            if (pindir != ABS(DIR_LEFT)) { MoveKS(LEFT) }
            if (pindir != ABS(DIR_DR)) { MoveKS(DR) }
            if (pindir != ABS(DIR_DL)) { MoveKS(DL) }
            if (pindir != ABS(DIR_DOWN)) { MoveKS(DOWN) }
        }
    }
    else {
        if (pindir == 0) {
            MoveKG(UP)
                MoveKG(UR)
                MoveKG(UL)
                MoveKG(RIGHT)
                MoveKG(LEFT)
                MoveKG(DR)
                MoveKG(DL)
                MoveKG(DOWN)
        }
        else {
            if (pindir != ABS(DIR_UP)) { MoveKG(UP) }
            if (pindir != ABS(DIR_UR)) { MoveKG(UR) }
            if (pindir != ABS(DIR_UL)) { MoveKG(UL) }
            if (pindir != ABS(DIR_RIGHT)) { MoveKG(RIGHT) }
            if (pindir != ABS(DIR_LEFT)) { MoveKG(LEFT) }
            if (pindir != ABS(DIR_DR)) { MoveKG(DR) }
            if (pindir != ABS(DIR_DL)) { MoveKG(DL) }
            if (pindir != ABS(DIR_DOWN)) { MoveKG(DOWN) }
        }
    }
#undef MoveKS
#undef MoveKG
    return mlist;
}

// 盤上の駒を動かす手のうち generate_capture() で生成する手を除いて生成する(動かす手で取らない手(－歩を成る手)を生成)
template <Color us>
MoveStack* Position::generate_non_capture(MoveStack* mlist) const
{
    //    int teNum = 0;
    int kn;
    int z;
    MoveStack* p = mlist;
#if defined(DEBUG_GENERATE)
    MoveStack* top = mlist;
#endif

    if (us == BLACK) {
        z = knpos[1];    // 先手玉
        if (z) mlist = gen_king_noncapture(us, mlist);
        for (kn = KNS_HI; kn <= KNE_FU; kn++) {
            z = knpos[kn];
            if (OnBoard(z)) {
                if ((knkind[kn] & GOTE) == 0) {
                    // 先手番のとき
                    mlist = gen_move_from(us, mlist, z);
                }
            }
        }
    }
    else {
        z = knpos[2];    // 後手玉
        if (z) mlist = gen_king_noncapture(us, mlist);
        for (kn = KNS_HI; kn <= KNE_FU; kn++) {
            z = knpos[kn];
            if (OnBoard(z)) {
                if ((knkind[kn] & GOTE) != 0) {
                    // 後手番のとき
                    mlist = gen_move_from(us, mlist, z);
                }
            }
        }
    }

    // generate_capture()で生成される手を除く
    MoveStack *last = mlist;
    for (mlist = p; mlist < last; mlist++) {
        Move &tmp = mlist->move;
        // 取る手はほぼ生成済み
        //   取る手で成れるのに成らない手は生成していない
        if (move_captured(tmp) != EMP) {
            if (is_promotion(tmp)) continue;
            PieceType pt = move_ptype(tmp);
            switch (pt) {
            case FU:
                if (can_promotion<us>(move_to(tmp))) break;
                continue;
            case KE:
            case GI:
            case KI:
            case OU:
            case TO:
            case NY:
            case NK:
            case NG:
            case UM:
            case RY:
                continue;
            case KY:
                if (((us == BLACK && (move_to(tmp) & 0x0F) != 2) ||
                     (us == WHITE && (move_to(tmp) & 0x0F) != 8))) continue;
                break;
            case KA:
                if (!can_promotion<us>(move_to(tmp)) && !can_promotion<us>(move_from(tmp))) continue;
                break;
            case HI:
                if (!can_promotion<us>(move_to(tmp)) && !can_promotion<us>(move_from(tmp))) continue;
                break;
            case PIECE_TYPE_NONE:
            default:
                print_csa(tmp);
                MYABORT();
                break;
            }
        }
        // 飛車が敵陣に成り込む手は生成済み
        if (move_ptype(tmp) == HI && is_promotion(tmp) && !can_promotion<us>(move_from(tmp)) && can_promotion<us>(move_to(tmp))) continue;
        // 歩の成る手は生成済み
        if (move_ptype(tmp) == FU && is_promotion(tmp)) continue;
        if (mlist != p) (p++)->move = tmp;
        else p++;
    }

#if defined(DEBUG_GENERATE)
    mlist = p;
    while (top != mlist) {
        Move m = top->move;
        if (!m.IsDrop()) {
            if (piece_on(Square(m.From())) == EMP) {
                assert(false);
            }
        }
        top++;
    }
#endif

    return gen_drop<us>(p);
}

// 王手回避手の生成
template<Color us>
MoveStack* Position::generate_evasion(MoveStack* mlist) const
{
    const effect_t efft = (us == BLACK) ? effectW[kingS] & (EFFECT_LONG_MASK | EFFECT_SHORT_MASK) : effectB[kingG] & (EFFECT_LONG_MASK | EFFECT_SHORT_MASK);
#if defined(DEBUG_GENERATE)
    MoveStack* top = mlist;
#endif

    if ((efft & (efft - 1)) != 0) {
        // 両王手(利きが2以上)の場合は玉を動かすしかない
        return gen_move_king(us, mlist);
    }
    else {
        Square ksq = (us == BLACK) ? Square(kingS) : Square(kingG);
        unsigned long id = 0;    // 初期化不要だが warning が出るため0を入れる
        int check;    // 王手をかけている駒の座標
        if (efft & EFFECT_SHORT_MASK) {
            // 跳びのない利きによる王手 → 回避手段：王手している駒を取る、玉を動かす
            _BitScanForward(&id, efft);
            check = ksq - NanohaTbl::Direction[id];
            //王手駒を取る
            mlist = gen_move_to(us, mlist, check);
            //玉を動かす
            mlist = gen_move_king(us, mlist);
        }
        else {
            // 跳び利きによる王手 → 回避手段：王手している駒を取る、玉を動かす、合駒
            _BitScanForward(&id, efft);
            id -= EFFECT_LONG_SHIFT;
            check = SkipOverEMP(ksq, -NanohaTbl::Direction[id]);
            //王手駒を取る
            mlist = gen_move_to(us, mlist, check);
            //玉を動かす
            mlist = gen_move_king(us, mlist);
            //合駒をする手を生成する
            int sq;
            for (sq = ksq - NanohaTbl::Direction[id]; ban[sq] == EMP; sq -= NanohaTbl::Direction[id]) {
                mlist = gen_move_to(us, mlist, sq);
            }
            for (sq = ksq - NanohaTbl::Direction[id]; ban[sq] == EMP; sq -= NanohaTbl::Direction[id]) {
                mlist = gen_drop_to(us, mlist, sq);  //駒を打つ合;
            }
        }
    }
#if defined(DEBUG_GENERATE)
    while (top != mlist) {
        Move m = top->move;
        if (!m.IsDrop()) {
            if (piece_on(Square(m.From())) == EMP) {
                assert(false);
            }
        }
        top++;
    }
#endif

    return mlist;
}

template<Color us>
MoveStack* Position::generate_non_evasion(MoveStack* mlist) const
{
    int z;

#if defined(DEBUG_GENERATE)
    MoveStack* top = mlist;
#endif
    // 盤上の駒を動かす
    int kn;
    if (us == BLACK) {
        z = knpos[1];    // 先手玉
        if (z) mlist = gen_move_king(us, mlist);
        for (kn = KNS_HI; kn <= KNE_FU; kn++) {
            z = knpos[kn];
            if (OnBoard(z)) {
                Piece kind = ban[z];
                if (!(kind & GOTE)) {
                    mlist = gen_move_from(us, mlist, z);
                }
            }
        }
    }
    else {
        z = knpos[2];    // 後手玉
        if (z) mlist = gen_move_king(us, mlist);
        for (kn = KNS_HI; kn <= KNE_FU; kn++) {
            z = knpos[kn];
            if (OnBoard(z)) {
                Piece kind = ban[z];
                if (kind & GOTE) {
                    mlist = gen_move_from(us, mlist, z);
                }
            }
        }
    }

    return gen_drop<us>(mlist);;
}

// インスタンス化.
template MoveStack* Position::generate_capture<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_capture<WHITE>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_capture<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_capture<WHITE>(MoveStack* mlist) const;
template MoveStack* Position::generate_evasion<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_evasion<WHITE>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_evasion<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_evasion<WHITE>(MoveStack* mlist) const;
