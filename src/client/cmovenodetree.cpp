#include "precomp.h"
#include "stdinc.h"

#include "client.h"
#include "commandpacket.h"
#include "replypacket.h"
#include "scopedmover.h"
#include "cmovenodetree.h"

#define ITERATIVE_DEPTH_MIN 6

namespace godwhale {
namespace client {

CMoveNodeTree::CMoveNodeTree(Client & client)
    : m_client(client), m_position(INITIAL_SFEN, 0), m_positionId(-1)
    , m_iterationDepth(-1), m_lastPlyDepth(-1)
{
}

CMoveNodeTree::~CMoveNodeTree()
{
}

/**
 * @brief pldまでの深さのPVが、今のPVと同じか比較します。
 */
bool CMoveNodeTree::done_exact(int pld, int srd)
{
    return false;
}

/**
 * @brief 局面IDや最左ノードとなるPVを設定します。
 */
void CMoveNodeTree::set_position(std::string const & pos_sfen, int positionId)
{
    m_position.from_fen(pos_sfen);
    m_positionId     = positionId;
    m_iterationDepth = ITERATIVE_DEPTH_MIN;
    m_lastPlyDepth   = -1;
    m_stList.clear();

    m_rows.clear();
    m_pvFromRoot.clear();

    // ログ出力
    LOG_NOTIFICATION() << F("position: pid=%1%\n%2%")
                        % m_positionId % position_to_kif(m_position);
}

/**
 * @brief 指し手を一つ進めます。
 */
void CMoveNodeTree::make_move_root(Move move, int positionId)
{
    // 現在の局面でさせる手かどうかを確認します。
    if (!is_ok(move) || !m_position.pl_move_is_legal(move)) {
        LOG_ERROR() << "不正な指し手が与えられました: "
                    << move_to_kif(move);
        return;
    }

    // 局面から手を一つ進めます。
    auto prevPositionId = m_positionId;
    m_stList.push_back(StateInfo());
    m_position.do_move(move, m_stList.back());
    m_positionId = positionId;
    m_lastPlyDepth = -1;

    m_rows.clear();
    m_pvFromRoot.clear();

    // ログ出力
    LOG_NOTIFICATION() << F("makemoveroot pid=%1%->%2%, move=%3%")
                        % prevPositionId % positionId % move;
    LOG_NOTIFICATION() << m_position;

    // 新しい局面から、手の思考を開始します。
}

/**
 * @brief 指定された反復深化のPVを設定します。
 */
void CMoveNodeTree::set_pv(int iterationDepth, std::vector<Move> const & pv)
{
    // 格段の情報を初期化します。
    m_rows.clear();
    for (int pld = 0; pld < (int)pv.size(); ++pld) {
        CMoveNodeRow row(m_positionId, iterationDepth, pld, pv[pld]);

        m_rows.push_back(row);
    }

    // 各設定
    m_iterationDepth = iterationDepth;
    m_pvFromRoot     = pv;
    m_lastPlyDepth   = pv.size() - 1;

    // ログ出力
    auto moveStr = move_list_to_kif(pv.begin(), pv.end());
    LOG_NOTIFICATION() << F("setpv pid=%1%, itd=%2%, pv=%3%")
                        % m_positionId % m_iterationDepth
                        % boost::join(moveStr, "");
}

/**
 * @brief 指定のplyDepthで、SFENのリストを指し手に直します。
 */
std::vector<Move> CMoveNodeTree::move_list_from_sfen(int plyDepth,
                                                     std::vector<std::string> const & movesSFEN)
{
    if (plyDepth >= (int)m_pvFromRoot.size()) {
        LOG_ERROR() << plyDepth << ": pldの値が大きすぎます。";
        return std::vector<Move>();
    }

    // 手を進め、指定の指定の局面まで移動
    ScopedMover<> mover(m_position);
    for (int pld = 0; pld < plyDepth; ++pld) {
        mover.do_move(m_pvFromRoot[pld]);
    }

    // 指し手の一覧を取得します。
    std::vector<Move> moveList;
    for (auto sfen : movesSFEN) {
        auto move = move_from_uci(m_position, sfen);
        if (move == MOVE_NONE) {
            LOG_ERROR() << sfen << ": SFEN形式が正しくありません。";
            continue;
        }

        moveList.push_back(move);
    }

    return moveList;
}

/**
 * @brief 各指し手深さの候補手一覧を設定します。
 */
void CMoveNodeTree::set_move_list(int plyDepth, std::vector<Move> const & list)
{
    if (plyDepth >= (int)m_rows.size()) {
        LOG_ERROR() << "pld=" << plyDepth
                    << ": 対応していない指し手の深さです。";
        return;
    }

    m_rows[plyDepth].set_move_list(list);

    // ログ出力
    auto movesKif = move_list_to_kif(list.begin(), list.end());
    LOG_NOTIFICATION() << F("setmovelist: pid=%1%, itd=%2%, pld=%3%\n%4%\nlist=%5%")
                        % m_positionId % m_iterationDepth % plyDepth
                        % position_to_kif(m_position)
                        % boost::join(movesKif, "");
}

/**
 * @brief 指定の深さから、与えられた値を基準に探索を開始します。
 */
void CMoveNodeTree::start(int startPld, Value alpha, Value beta)
{
    if (startPld > (int)m_rows.size()) {
        LOG_ERROR() << "pld=" << startPld << ": 対応していない指し手の深さです。";
        return;
    }

    // αβの更新
    auto & row = m_rows[startPld];
    row.update_value(alpha, VALUETYPE_ALPHA);
    row.set_value(-VALUE_INFINITE, VALUETYPE_GAMMA);
    
    LOG_NOTIFICATION() << F("start: pid=%1%, itd=%2%, pld=%3%, alpha=%4%")
                        % m_positionId % m_iterationDepth % startPld % alpha;

    if (startPld > 0) {
        propagate_up(startPld - 1, -alpha);
    }
}

/**
 * @brief ある段の計算完了処理を行います。
 */
void CMoveNodeTree::commit(int plyDepth)
{
    if (plyDepth >= (int)m_rows.size()) {
        LOG_ERROR() << "pld(" << plyDepth << ") is too large.";
        return;
    }

    m_lastPlyDepth = plyDepth - 1;
    if (plyDepth > 0) {
        auto & row = m_rows[plyDepth - 1];
        auto & oldRow = m_rows[plyDepth];
        auto alpha = std::max(row.alpha(), -oldRow.alpha());

        // 一段下のα値も利用します。
        row.set_value(alpha, VALUETYPE_ALPHA);
        row.set_value(-VALUE_INFINITE, VALUETYPE_GAMMA);
    }
}

/**
 * @brief 評価値の更新を伝搬します。
 */
void CMoveNodeTree::notify(int plyDepth, Value value)
{
    if (plyDepth >= (int)m_rows.size()) {
        LOG_ERROR() << "pld(" << plyDepth << ") is too large.";
        return;
    }

    //if (m_rows[plyDepth].first_move() == notifyMove)

    LOG_NOTIFICATION() << F("notify itd=%1%, pld=%2%, value=%3%")
                        % m_iterationDepth % plyDepth % value;

    auto & row = m_rows[plyDepth];
    if (row.alpha() > value) {
        // 何もする必要はありません。
        return;
    }

    row.update_value(value, VALUETYPE_ALPHA);
    if (plyDepth > 0) {
        propagate_up(plyDepth - 1, -value);
    }
    if (plyDepth < m_lastPlyDepth) {
        propagate_down(plyDepth + 1, -value, VALUETYPE_BETA);
    }
}

/**
 * @brief γ値(仮のα値)を上段に伝搬します。
 */
void CMoveNodeTree::propagate_up(int plyDepth, Value value)
{
    if (plyDepth >= (int)m_rows.size()) {
        LOG_ERROR() << F("invalid plyDepth %1%") % plyDepth;
        return;
    }

    for (int pld = plyDepth; pld >= 0; --pld) {
        auto & row = m_rows[pld];

        row.update_value(value, VALUETYPE_GAMMA);
        value = -std::max(row.alpha(), value);
    }
}

/**
 * @brief 確定したα値 or β値を下段に伝搬します。
 */
void CMoveNodeTree::propagate_down(int plyDepth, Value value, ValueType vtype)
{
    if (plyDepth >= (int)m_rows.size()) {
        LOG_ERROR() << F("invalid plyDepth %1%") % plyDepth;
        return;
    }

    if (vtype != VALUETYPE_ALPHA && vtype != VALUETYPE_BETA) {
        LOG_ERROR() << F("invalid vtype %1%") % vtype;
        return;
    }

    for (int pld = plyDepth; pld <= m_lastPlyDepth; ++pld) {
        auto & row = m_rows[pld];

        if (vtype == VALUETYPE_ALPHA && value <= row.alpha()) {
            break;
        }

        // αβ値の更新を行います。
        row.update_value(value, vtype);

        vtype = (vtype == VALUETYPE_ALPHA ? VALUETYPE_BETA : VALUETYPE_ALPHA);
        value = -value;
    }
}

/**
 * @brief 次の探索タスクを取得します。
 */
SearchTask CMoveNodeTree::get_search_task()
{
    for (int pld = m_lastPlyDepth; pld >= 0; --pld) {
        auto & row = m_rows[pld];
        auto sdepth = search_depth(m_iterationDepth, pld);
        auto alpha = row.effective_alpha();
        auto beta = row.beta();

        if (alpha >= beta) {
            continue;
        }

        auto * node = row.find_undone(sdepth, alpha, beta);
        if (node != nullptr) {
            return SearchTask(m_positionId, m_iterationDepth, pld, node);
        }
    }

    return SearchTask();
}

} // namespace client
} // namespace godwhale
