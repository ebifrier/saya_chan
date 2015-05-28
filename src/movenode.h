#ifndef GODWHALE_MOVENODE_H
#define GODWHALE_MOVENODE_H

#include "saya_chan/move.h"

namespace godwhale {

/**
 * @brief 指し手とその探索結果を保持します。
 */
class MoveNode
{
public:
    explicit MoveNode(Move move);
    ~MoveNode();

    /**
     * @brief このノードに対応する指し手を取得します。
     */
    Move move() const
    {
        return m_move;
    }

    /**
     * @brief 探索した次の手の最善手を取得します。
     */
    Move best_move() const
    {
        return m_bestMove;
    }

    /**
     * @brief 指し手の探索深さを取得します。
     */
    Depth depth() const
    {
        return m_depth;
    }

    /**
     * @brief 探索したノード数を取得します。
     */
    uint64_t nodes() const
    {
        return m_nodes;
    }

    /**
     * @brief 評価値の上限値を取得します。
     */
    Value upper() const
    {
        return m_upper;
    }

    /**
     * @brief 評価値の下限値を取得します。
     */
    Value lower() const
    {
        return m_lower;
    }

    bool done(Depth depth, Value alpha, Value beta) const;
    bool done_exact(Depth depth) const;

    void update(Depth depth, Value value, ULEType ule, uint64_t nodes, Move bestMove);
    void update(Depth depth, Value value, Value lower, Value upper, uint64_t nodes, Move bestMove);

private:
    Move m_move;
    Move m_bestMove;
    Depth m_depth;
    uint64_t m_nodes;
    Value m_upper, m_lower;
};

} // namespace godwhale

#endif
