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

#if !defined(HISTORY_H_INCLUDED)
#define HISTORY_H_INCLUDED

#include <iostream>
#include <cstring>
#include <cassert>
#include "types.h"

/// The History class stores statistics about how often different moves
/// have been successful or unsuccessful during the current search. These
/// statistics are used for reduction and move ordering decisions. History
/// entries are stored according only to moving piece and destination square,
/// in particular two moves with different origin but same destination and
/// same piece will be considered identical.

class History {

public:
    void clear();
    Value value(Piece p, Square to) const;
    void update(Piece p, Square to, Value bonus);
    Value gain(Piece p, Square to) const;
    void update_gain(Piece p, Square to, Value g);

    static const Value MaxValue = Value(2000);

private:
#if defined(NANOHA)
    Value history[32][0x100];  // [piece][to_square]
    Value maxGains[32][0x100]; // [piece][to_square]
#else
    Value history[16][64];  // [piece][to_square]
    Value maxGains[16][64]; // [piece][to_square]
#endif
};

inline void History::clear() {
#if defined(NANOHA)
    memset(history,  0, 32 * 0x100 * sizeof(Value));
    memset(maxGains, 0, 32 * 0x100 * sizeof(Value));
#else
    memset(history,  0, 16 * 64 * sizeof(Value));
    memset(maxGains, 0, 16 * 64 * sizeof(Value));
#endif
}

inline Value History::value(Piece p, Square to) const {
#if defined(NANOHA)
#if !defined(NDEBUG)
    if (p == WALL) {
        std::cerr << "p = " << int(p) << ", to = " << int(to) << std::endl;
    }
    assert(0 <= p && p <= 31);
#endif
    const int idx = NanohaTbl::Piece2Index[p];
    return history[idx][to];
#else
    return history[p][to];
#endif
}

inline void History::update(Piece p, Square to, Value bonus) {
#if defined(NANOHA)
    const int idx = NanohaTbl::Piece2Index[p];
    if (abs(history[idx][to] + bonus) < MaxValue) history[idx][to] += bonus;
#else
    if (abs(history[p][to] + bonus) < MaxValue) history[p][to] += bonus;
#endif
}

inline Value History::gain(Piece p, Square to) const {
#if defined(NANOHA)
    const int idx = NanohaTbl::Piece2Index[p];
    return maxGains[idx][to];
#else
    return maxGains[p][to];
#endif
}

inline void History::update_gain(Piece p, Square to, Value g) {
#if defined(NANOHA)
    const int idx = NanohaTbl::Piece2Index[p];
    maxGains[idx][to] = Max(g, maxGains[idx][to] - 1);
#else
    maxGains[p][to] = Max(g, maxGains[p][to] - 1);
#endif
}

#endif // !defined(HISTORY_H_INCLUDED)
