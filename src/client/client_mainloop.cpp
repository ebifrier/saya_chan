#include "precomp.h"
#include "stdinc.h"

#include "saya_chan/search.h"
#include "sayachan_if.h"
#include "replypacket.h"
#include "scopedmover.h"
#include "cmovenodetree.h"
#include "client.h"

namespace godwhale {
namespace client {

/**
 * @brief 受信した各コマンドの処理を行います。
 */
int Client::run()
{
    while (m_available) {
        proce(false /* searching */);

        // 探索中でない場合は休憩します。
        /*if (m_currentPlyDepth < 0) {
            sleep(100);
            continue;
        }*/

        // 仕事がない場合
        auto stask = m_tree->get_search_task();
        if (stask.empty()) {
            sleep(100);
            continue;
        }

        search_main(stask);
    }

    return 0;
}

static void log(std::string const & title, SearchResult const & result)
{
    auto list = move_list_to_kif(result.pv.begin(), result.pv.end());

    LOG_NOTIFICATION() << title << ": " << result.value
                       << " " << move_to_kif(result.pv[0]);
}

/**
 * @brief 指し手の探索処理を行います。
 */
void Client::search_main(SearchTask & stask)
{
    auto itd = stask.iteration_depth();
    auto pld = stask.ply_depth();
    auto move = stask.move();
    auto & row = m_tree->row(pld);
    auto alpha = row.effective_alpha();
    auto beta = row.beta();
    Move moves[] = { MOVE_NONE };

    if (m_tree->position_id() != stask.position_id()) {
        return;
    }

    if (m_tree->iteration_depth() != stask.iteration_depth()) {
        return;
    }

    std::vector<Move> bestSeq;
    Value value;

    // 局面まで指し手を進めます。
    auto & position = m_tree->position();
    ScopedMover<> mover(position);
    for (int d = 0; d < pld; ++d) {
        mover.do_move(m_tree->pv_from_root()[d]);
    }
    mover.do_move(move);

    // 王手している場合(本来は実行されないはず)
    if (position.at_checking()) {
        value = value_mate_in(1);
        auto * node = stask.node();
        node->update(DEPTH_DECISIVE, value, value, value + 1, 0,
                     (bestSeq.empty() ? MOVE_NONE : bestSeq[0]));

        auto reply = ReplyPacket::create_updatevalue(m_tree->position_id(),
                                                     itd, pld, move, value,
                                                     value, value + 1, 0, bestSeq);
        send_reply(reply);
        return;
    }

    // 探索タスクの実行
#if defined(PUBLISHED)
    LOG_DEBUG() << F("start search: itd=%1%, pld=%2%, move=%3%, alpha=%4%, beta=%5%")
                 % itd % pld % move % -beta % -alpha;
#endif

    auto sdepth = search_depth(itd, pld) - ONE_PLY;
    auto startNodes = position.nodes_searched();
    auto updated = false;
    auto result0 = search_result(position, -alpha - 1, -alpha, sdepth, moves);
    value = -result0.value;

    if (!result0.completed) {
        return;
    }

    if (value > alpha) {
        auto result = search_result(position, -beta, -alpha, sdepth, &moves[0]);
        value = -result.value;
        LOG_NOTIFICATION() << F("retry value: move=%1%, value=%2%")
                            % move % result.value;

        /*auto result3 = search_result(position, -beta, -alpha, sdepth, &moves[0]);
        result3.value = -result3.value;
        log("Value limit ab", result3);

        auto result4 = search_result(position, -VALUE_INFINITE, VALUE_INFINITE, sdepth, &moves[0]);
        result4.value = -result4.value;
        log("Value correct", result4);*/

        /*SearchLimits limits;
        limits.maxDepth = sdepth / ONE_PLY;
        auto result2 = think_result(position, limits, &moves2[0]);
        LOG_NOTIFICATION() << "Value correct: " << result2.value;*/

        if (alpha < value) {
            // 送信するPVを設定します。
            bestSeq = make_valid_pv(result.pv);

            updated = true;
            //alpha = value;
            m_tree->notify(pld, value);

            auto kifpv = move_list_to_kif(bestSeq.begin(), bestSeq.end());
            LOG_NOTIFICATION() << F("updated value move=%1%, pv=%2%")
                                % move % boost::join(kifpv, "");
        }
    }

    // 各子ノードの評価値を設定します。
    auto * node = stask.node();
    node->update(sdepth + ONE_PLY, -value, -beta, -alpha, 0,
                (bestSeq.empty() ? MOVE_NONE : bestSeq[0]));

    // ノードの評価値を渡します。
    auto reply = ReplyPacket::create_updatevalue(m_tree->position_id(),
                                                 itd, pld, move, -value,
                                                 -beta, -alpha, 0, bestSeq);
    send_reply(reply, updated);
}

} // namespace client
} // namespace godwhale
