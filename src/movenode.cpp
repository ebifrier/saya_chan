#include "precomp.h"
#include "stdinc.h"

#include "movenode.h"

namespace godwhale {

MoveNode::MoveNode(Move move)
    : m_move(move), m_bestMove(MOVE_NONE), m_depth(DEPTH_NONE), m_nodes(0)
    , m_lower(-VALUE_INFINITE), m_upper(VALUE_INFINITE)
{
}

MoveNode::~MoveNode()
{
}

/**
 * @brief ノードの計算が完了したかどうかを取得します。
 *
 * alphaとbetaには親ノードを基準とした値を与えます。
 */
bool MoveNode::done(Depth depth, Value alpha, Value beta) const
{
    return (m_depth >= depth &&
            (m_upper == m_lower || -beta >= m_upper || -alpha <= m_lower));
}

/**
 * @brief 評価値がEXACTでノードの計算が完了したか調べます。
 */
bool MoveNode::done_exact(Depth depth) const
{
    return (m_depth >= depth && m_upper == m_lower);
}

/**
 * @brief 評価値の更新を行います。
 */
void MoveNode::update(Depth depth, Value value, ULEType ule,
                      uint64_t nodes, Move bestMove)
{
    switch (ule) {
    case ULE_EXACT:
        m_upper = m_lower = value;
        break;
    case ULE_UPPER:
        if (depth > m_depth) {
            m_upper = value;
            m_lower = -VALUE_INFINITE;
        }
        else {
            m_upper = value;
        }
        break;
    case ULE_LOWER:
        if (depth > m_depth) {
            m_upper = VALUE_INFINITE;
            m_lower = value;
        }
        else {
            m_lower = value;
        }
        break;
    default:
        unreachable();
    }

    m_bestMove = (ule == ULE_EXACT || ule == ULE_LOWER ? bestMove : MOVE_NONE);
    m_nodes = nodes;
    m_depth = depth;
}

/**
 * @brief 評価値の更新を行います。
 */
void MoveNode::update(Depth depth, Value value, Value lower, Value upper,
                      uint64_t nodes, Move bestMove)
{
    ULEType ule;
    
    if (value <= lower) {
        ule = ULE_UPPER;
        value = lower;
    }
    else if (value >= upper) {
        ule = ULE_LOWER;
        value = upper;
    }
    else { // lower < value && value < upper
        ule = ULE_EXACT;
    }

    update(depth, value, ule, nodes, bestMove);
}

} // namespace godwhale
