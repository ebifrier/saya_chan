/*
  NanohaMini, a USI shogi(japanese-chess) playing engine derived from Stockfish 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2010 Marco Costalba, Joona Kiiski, Tord Romstad (Stockfish author)
  Copyright (C) 2014 Kazuyuki Kawabata

  NanohaMini is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  NanohaMini is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

namespace {
    ///
    /// @brief aspiration searchを行いながら反復深化を行います。
    ///
    static SearchResult id_loop2(Position& pos, Move searchMoves[]) {

        SearchStack ss[PLY_MAX_PLUS_2];
        Value bestValues[PLY_MAX_PLUS_2];
        int bestMoveChanges[PLY_MAX_PLUS_2];
        int depth, aspirationDelta;
        Value value, alpha, beta;
        Move bestMove, easyMove, skillBest, skillPonder;
        SearchResult result;

        // Initialize stuff before a new search
        memset(ss, 0, 4 * sizeof(SearchStack));
        TT.new_search();
        H.clear();
        bestMove = easyMove = skillBest = skillPonder = MOVE_NONE;
        depth = aspirationDelta = 0;
        value = alpha = -VALUE_INFINITE, beta = VALUE_INFINITE;
        ss->currentMove = MOVE_NULL; // Hack to skip update_gains()

        // Moves to search are verified and copied
        Rml.init(pos, searchMoves);

        // Handle special case of searching on a mate/stalemate position
        if (!Rml.size())
        {
            result.value = value_mated_in(0);
            result.depth = DEPTH_ZERO;
            result.completed = !QuitRequest;
            return result;
        }

        // Iterative deepening loop until requested to stop or target depth reached
        while (!StopRequest && ++depth <= PLY_MAX && (!Limits.maxDepth || depth <= Limits.maxDepth))
        {
            // Save last iteration's scores, this needs to be done now, because in
            // the following MultiPV loop Rml moves could be reordered.
            for (size_t i = 0; i < Rml.size(); i++)
                Rml[i].prevScore = Rml[i].score;

            Rml.bestMoveChanges = 0;

            // MultiPV iteration loop
            for (MultiPVIteration = 0; MultiPVIteration < Min(MultiPV, (int)Rml.size()); MultiPVIteration++)
            {
                // Calculate dynamic aspiration window based on previous iterations
                if (depth >= 5 && abs(Rml[MultiPVIteration].prevScore) < VALUE_KNOWN_WIN)
                {
                    int prevDelta1 = bestValues[depth - 1] - bestValues[depth - 2];
                    int prevDelta2 = bestValues[depth - 2] - bestValues[depth - 3];

                    aspirationDelta = Min(Max(abs(prevDelta1) + abs(prevDelta2) / 2, 16), 24);
                    aspirationDelta = (aspirationDelta + 7) / 8 * 8; // Round to match grainSize

                    alpha = Max(Rml[MultiPVIteration].prevScore - aspirationDelta, -VALUE_INFINITE);
                    beta = Min(Rml[MultiPVIteration].prevScore + aspirationDelta, VALUE_INFINITE);
                }
                else
                {
                    alpha = -VALUE_INFINITE;
                    beta =   VALUE_INFINITE;
                }

                // Start with a small aspiration window and, in case of fail high/low,
                // research with bigger window until not failing high/low anymore.
                do {
                    // Search starting from ss+1 to allow referencing (ss-1). This is
                    // needed by update_gains() and ss copy when splitting at Root.
                    value = search<Root>(pos, ss + 1, alpha, beta, depth * ONE_PLY);

                    // It is critical that sorting is done with a stable algorithm
                    // because all the values but the first are usually set to
                    // -VALUE_INFINITE and we want to keep the same order for all
                    // the moves but the new PV that goes to head.
                    sort<RootMove>(Rml.begin() + MultiPVIteration, Rml.end());

                    // In case we have found an exact score reorder the PV moves
                    // before leaving the fail high/low loop, otherwise leave the
                    // last PV move in its position so to be searched again.
                    if (value > alpha && value < beta)
                        sort<RootMove>(Rml.begin(), Rml.begin() + MultiPVIteration);

                    // Write PV back to transposition table in case the relevant entries
                    // have been overwritten during the search.
                    for (int i = 0; i <= MultiPVIteration; i++)
                        Rml[i].insert_pv_in_tt(pos);

                    // Value cannot be trusted. Break out immediately!
                    if (StopRequest)
                        break;

                    // Send full PV info to GUI if we are going to leave the loop or
                    // if we have a fail high/low and we are deep in the search.
                    if (Options["Output_AllDepth"].value<bool>() || (value > alpha && value < beta) || current_search_time() > 500) {
                        for (int i = 0; i < Min(UCIMultiPV, MultiPVIteration + 1); i++) {
                            /*cout << "info"
                                 << depth_to_uci(depth * ONE_PLY)
                                 << (i == MultiPVIteration ? score_to_uci(Rml[i].score, alpha, beta) :
                                     score_to_uci(Rml[i].score))
                                 << speed_to_uci(pos.nodes_searched())
                                 << pv_to_uci(&Rml[i].pv[0], i + 1, false)
                                 << endl;*/
                        }
                    }

                    // In case of failing high/low increase aspiration window and research,
                    // otherwise exit the fail high/low loop.
                    if (value >= beta)
                    {
                        beta = Min(beta + aspirationDelta, VALUE_INFINITE);
                        aspirationDelta += aspirationDelta / 2;
                    }
                    else if (value <= alpha)
                    {
                        AspirationFailLow = true;
                        StopOnPonderhit = false;

                        alpha = Max(alpha - aspirationDelta, -VALUE_INFINITE);
                        aspirationDelta += aspirationDelta / 2;
                    }
                    else
                        break;

                } while (abs(value) < VALUE_KNOWN_WIN);
            }

            // Collect info about search result
            result.pv = make_valid_pv(Rml[MultiPVIteration - 1].pv);
            result.value = Rml[MultiPVIteration - 1].score;
            result.depth = depth * ONE_PLY;

            bestMove = Rml[MultiPVIteration - 1].pv[0];
            bestValues[depth] = value;
            bestMoveChanges[depth] = Rml.bestMoveChanges;

            if (LogFile.is_open())
                LogFile << pretty_pv(pos, depth, value, current_search_time(), &Rml[0].pv[0]) << endl;

            // Init easyMove after first iteration or drop if differs from the best move
            if (depth == 1 && (Rml.size() == 1 || Rml[0].score > Rml[1].score + EasyMoveMargin))
                easyMove = bestMove;
            else if (bestMove != easyMove)
                easyMove = MOVE_NONE;

            // Check for some early stop condition
            if (!StopRequest && Limits.useTimeManagement())
            {
                // Stop search early if one move seems to be much better than the
                // others or if there is only a single legal move. Also in the latter
                // case we search up to some depth anyway to get a proper score.
                if (   depth >= 7
                    && easyMove == bestMove
                    && (Rml.size() == 1
                       || (Rml[0].nodes > (pos.nodes_searched() * 85) / 100
                    && current_search_time() > TimeMgr.available_time() / 16)
                       || (Rml[0].nodes > (pos.nodes_searched() * 98) / 100
                          && current_search_time() > TimeMgr.available_time() / 32)))
                    StopRequest = true;

                // Take in account some extra time if the best move has changed
                if (depth > 4 && depth < 50)
                    TimeMgr.pv_instability(bestMoveChanges[depth], bestMoveChanges[depth - 1]);

                // Stop search if most of available time is already consumed. We probably don't
                // have enough time to search the first move at the next iteration anyway.
                if (current_search_time() >(TimeMgr.available_time() * 62) / 100)
                    StopRequest = true;

                // If we are allowed to ponder do not stop the search now but keep pondering
                if (StopRequest && Limits.ponder)
                {
                    StopRequest = false;
                    StopOnPonderhit = true;
                }
            }
        }

        result.completed = !QuitRequest;
        return result;
    }
}

///
/// @brief サーバーによるRootMoveの展開に使います。
///
SearchResult think_result(Position& pos, const SearchLimits& limits, Move searchMoves[]) {

    // Initialize global search-related variables
    StopOnPonderhit = StopRequest = QuitRequest = AspirationFailLow = false;
    NodesSincePoll = 0;
    current_search_time(get_system_time());
    Limits = limits;
    TimeMgr.init(Limits, pos.startpos_ply_counter());

    // Set best NodesBetweenPolls interval to avoid lagging under time pressure
    if (Limits.maxNodes)
        NodesBetweenPolls = Min(Limits.maxNodes, 30000);
    else if (Limits.time && Limits.time < 1000)
        NodesBetweenPolls = 1000;
    else if (Limits.time && Limits.time < 5000)
        NodesBetweenPolls = 5000;
    else
        NodesBetweenPolls = 15000;

    // Read UCI options
    UCIMultiPV = Options["MultiPV"].value<int>();
    SkillLevel = Options["Skill Level"].value<int>();
    DrawValue = (Value)(Options["DrawValue"].value<int>() * 2);

    Threads.read_uci_options();

    // Set a new TT size if changed
    TT.set_size(Options["Hash"].value<int>());

    if (Options["Clear Hash"].value<bool>())
    {
        Options["Clear Hash"].set_value("false");
        TT.clear();
    }

    // Do we have to play with skill handicap? In this case enable MultiPV that
    // we will use behind the scenes to retrieve a set of possible moves.
    SkillLevelEnabled = (SkillLevel < 20);
    MultiPV = (SkillLevelEnabled ? Max(UCIMultiPV, 4) : UCIMultiPV);

    // Wake up needed threads and reset maxPly counter
    for (int i = 0; i < Threads.size(); i++)
    {
        Threads[i].wake_up();
        Threads[i].maxPly = 0;
    }

    // We're ready to start thinking. Call the iterative deepening loop function
    SearchResult result = id_loop2(pos, searchMoves);

    // This makes all the threads to go to sleep
    Threads.set_size(1);

    return result;
}

///
/// α、β、Depthなどを指定した探索を行います。
///
SearchResult search_result(Position& pos, Value alpha, Value beta, Depth depth,
                           Move searchMoves[]) {

    SearchStack ss[PLY_MAX_PLUS_2];
    SearchResult result;
    SearchLimits limits;
    limits.maxDepth = depth;

    StopOnPonderhit = StopRequest = QuitRequest = AspirationFailLow = false;
    NodesSincePoll = 0;
    current_search_time(get_system_time());
    Limits = limits;
    TimeMgr.init(Limits, pos.startpos_ply_counter());

    // Set best NodesBetweenPolls interval to avoid lagging under time pressure
    NodesBetweenPolls = 15000;

    // Read UCI options
    UCIMultiPV = MultiPV = 1;
    MultiPVIteration = 0;
    SkillLevelEnabled = false;
    DrawValue = (Value)(Options["DrawValue"].value<int>() * 2);

    Threads.read_uci_options();

    // Set a new TT size if changed
    TT.set_size(Options["Hash"].value<int>());

    // Initialize stuff before a new search
    memset(ss, 0, 4 * sizeof(SearchStack));
    TT.new_search();
    H.clear();
    ss->currentMove = MOVE_NULL; // Hack to skip update_gains()

    // Moves to search are verified and copied
    Rml.init(pos, searchMoves);

    // Handle special case of searching on a mate/stalemate position
    if (Rml.empty())
    {
        result.value = value_mated_in(0);
        result.depth = depth;
        result.completed = !QuitRequest;
        return result;
    }

    // RootMoveをPickする順番に並べてみます。
    /*MovePickerExt<false> mp(pos, Rml[0].pv[0], depth, H, ss, -VALUE_INFINITE);
    Move m;
    while ((m = mp.get_next_move()) != MOVE_NONE) {
        cout << " " << move_to_kif(m);
    }
    cout << endl;*/

    // 指し手のオーダリング結果がいまいち。。。
    /*for (auto m : Rml) {
        cout << " " << move_to_kif(m.pv[0]);
    }
    cout << endl;*/

    // Wake up needed threads and reset maxPly counter
    for (int i = 0; i < Threads.size(); i++)
    {
        Threads[i].wake_up();
        Threads[i].maxPly = 0;
    }

    // α・βを指定した探索を実行
    auto value = search<Root>(pos, ss + 1, alpha, beta, depth);

    sort<RootMove>(Rml.begin(), Rml.end());
    /*for (auto m : Rml) {
        cout << " " << move_to_kif(m.pv[0]) << "(" << m.score << ")";
    }
    cout << endl;*/

    result.pv = make_valid_pv(Rml[0].pv);
    result.value = Rml[0].score;
    result.depth = depth;
    result.completed = !QuitRequest;

    // This makes all the threads to go to sleep
    Threads.set_size(1);

    return result;
}
