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

#if !defined(SEARCH_H_INCLUDED)
#define SEARCH_H_INCLUDED

#include <cstring>
#include <vector>
#include <algorithm>

#include "move.h"
#include "types.h"

class Position;
struct SplitPoint;

/// The SearchStack struct keeps track of the information we need to remember
/// from nodes shallower and deeper in the tree during the search.  Each
/// search thread has its own array of SearchStack objects, indexed by the
/// current ply.

struct SearchStack {
    SplitPoint* sp;
    int ply;
    Move currentMove;
    Move excludedMove;
    Move bestMove;
    Move killers[2];
    Depth reduction;
    Value eval;
    Value evalMargin;
    int skipNullMove;
#if defined(NANOHA)
    bool checkmateTested;
#endif
#if defined(EVAL_DIFF)
    Value staticEvalRaw;
#endif
};


/// The SearchLimits struct stores information sent by GUI about available time
/// to search the current move, maximum depth/time, if we are in analysis mode
/// or if we have to ponder while is our opponent's side to move.

struct SearchLimits {

    SearchLimits() { memset(this, 0, sizeof(SearchLimits)); }

    SearchLimits(int t, int i, int mtg, int mt, int md, int mn, bool inf, bool pon)
                : time(t), increment(i), movesToGo(mtg), maxTime(mt), maxDepth(md),
                  maxNodes(mn), infinite(inf), ponder(pon) {}

    bool useTimeManagement() const { return !(maxTime | maxDepth | maxNodes | infinite); }

    int time, increment, movesToGo, maxTime, maxDepth, maxNodes, infinite, ponder;
};

extern void init_search();
extern int64_t perft(Position& pos, Depth depth);
extern bool think(Position& pos, const SearchLimits& limits, Move searchMoves[]);

#if defined(GODWHALE_SERVER) || defined(GODWHALE_CLIENT)
struct SearchResult {
    SearchResult() { memset(this, 0, sizeof(SearchResult)); }

    std::vector<Move> pv;
    Value value;
    Depth depth;
    bool completed;
};

template<class Seq>
inline std::vector<Move> make_valid_pv(const Seq & sequence)
{
    std::vector<Move> result;
    auto begin = sequence.begin();
    auto end = sequence.end();

    for (; begin != end; ++begin) {
        if (*begin == MOVE_NONE) continue;
        result.push_back(*begin);
    }

    return result;
}

template<class Iter>
inline std::vector<Move> make_valid_pv(Iter begin, Iter end)
{
    std::vector<Move> result;

    for (; begin != end; ++begin) {
        if (*begin == MOVE_NONE) continue;
        result.push_back(*begin);
    }

    return result;
}

extern SearchResult think_result(Position& pos, const SearchLimits& limits, Move searchMoves[]);
extern SearchResult search_result(Position& pos, Value alpha, Value beta, Depth depth, Move searchMoves[]);
#endif

#endif // !defined(SEARCH_H_INCLUDED)
