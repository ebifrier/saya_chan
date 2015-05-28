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

#if !defined(MOVEGEN_H_INCLUDED)
#define MOVEGEN_H_INCLUDED

#include "move.h"

enum MoveType {
    MV_CAPTURE,             // 駒を取る手
    MV_NON_CAPTURE,         // 駒を取らない手
    MV_CHECK,               // 王手
    MV_NON_CAPTURE_CHECK,   // 駒を取らない王手
    MV_EVASION,             // 王手回避手
    MV_NON_EVASION,         // 王手がかかっていない時の合法手生成
    MV_LEGAL                // 合法手生成
};

template<MoveType>
MoveStack* generate(const Position& pos, MoveStack* mlist);

/// The MoveList struct is a simple wrapper around generate(), sometimes comes
/// handy to use this class instead of the low level generate() function.
template<MoveType T>
struct MoveList {

    explicit MoveList(const Position& pos) : cur(mlist), last(generate<T>(pos, mlist)) {}
    void operator++() { cur++; }
    bool end() const { return cur == last; }
    Move move() const { return cur->move; }
    int size() const { return int(last - mlist); }

private:
    MoveStack mlist[MAX_MOVES];
    MoveStack *cur, *last;
};

#endif // !defined(MOVEGEN_H_INCLUDED)
