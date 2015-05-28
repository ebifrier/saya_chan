#include "precomp.h"
#include "stdinc.h"

#include "commandpacket.h"
#include "cmovenoderow.h"

namespace godwhale {
namespace client {

/*CMoveNodeRow::CMoveNodeRow()
{
    throw std::logic_error("Not supported.");
}*/

CMoveNodeRow::CMoveNodeRow(int positionId, int iterationDepth,
                           int plyDepth, Move left)
    : m_positionId(positionId), m_iterationDepth(iterationDepth)
    , m_plyDepth(plyDepth), m_left(left), m_alpha(-VALUE_INFINITE)
    , m_beta(+VALUE_INFINITE), m_gamma(-VALUE_INFINITE)
    , m_bestValue(-VALUE_INFINITE), m_bestULE(ULE_NONE)
{
}

CMoveNodeRow::~CMoveNodeRow()
{
}

/**
 * @brief 指し手リストをクリアします。
 */
void CMoveNodeRow::clear()
{
    m_nodeList.clear();
}

/**
 * @brief 探索すべき指し手を設定します。
 */
void CMoveNodeRow::set_move_list(std::vector<Move> const & list)
{
    // 指し手の分配を行います。
    for (auto move : list) {
        m_nodeList.add_move(move);
    }

    // ログ出力
    LOG_NOTIFICATION() << F("move list: pid=%1%, itd=%2%, pld=%3%, moves=%4%")
                        % m_positionId % m_iterationDepth % m_plyDepth
                        % boost::join(m_nodeList.to_kif_list(), "");
}

/**
 * @brief 探索の終わっていない手があればそれを取得します。
 */
MoveNode *CMoveNodeRow::find_undone(Depth depth, Value alpha, Value beta)
{
    auto index = m_nodeList.find_undone(depth, alpha, beta);
    if (index < 0) {
        return nullptr;
    }

    return &m_nodeList[index];
}

/**
 * @brief 評価値の設定を行います。
 */
void CMoveNodeRow::set_value(Value value, ValueType vtype)
{
    switch (vtype) {
    case VALUETYPE_ALPHA:
        m_alpha = value;
        break;
    case VALUETYPE_BETA:
        m_beta = value;
        break;
    case VALUETYPE_GAMMA:
        m_gamma = value;
        break;
    default:
        unreachable();
        break;
    }
}

/**
 * @brief 評価値の更新を行います。
 */
void CMoveNodeRow::update_value(Value value, ValueType vtype)
{
    switch (vtype) {
    case VALUETYPE_ALPHA:
        if (value > m_alpha) {
            m_alpha = value;
        }
        break;
    case VALUETYPE_BETA:
        if (value < m_beta) {
            m_beta = value;
        }
        break;
    case VALUETYPE_GAMMA:
        m_gamma = value;
        break;
    default:
        unreachable();
        break;
    }
}

/**
 * @brief 最善手とその評価値の更新を行います。
 */
void CMoveNodeRow::update_best(Value value, Move move,
                               std::vector<Move> const & pv)
{
    if (move == MOVE_NONE) {
        LOG_ERROR() << "空の指し手が設定されました。";
        return;
    }

    m_bestValue = value;
    m_bestULE = (value >= m_beta ? ULE_LOWER : ULE_EXACT);

    m_bestPV.assign(1, move);
    m_bestPV.insert(m_bestPV.end(), pv.begin(), pv.end());

    // α値の更新も行います。
    update_value(value, VALUETYPE_ALPHA);
}

} // namespace client
} // namespace godwhale
