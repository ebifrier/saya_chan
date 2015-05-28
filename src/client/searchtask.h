#ifndef GODWHALE_CLIENT_SEARCHTASK_H
#define GODWHALE_CLIENT_SEARCHTASK_H

#include "movenode.h"

namespace godwhale {
namespace client {

/**
 * @brief クライアントに割り当てられた探索タスクを管理します。
 */
class SearchTask
{
public:
    explicit SearchTask()
        : m_positionId(-1), m_iterationDepth(-1), m_plyDepth(-1)
        , m_alpha(-VALUE_INFINITE), m_beta(VALUE_INFINITE)
        , m_gamma(-VALUE_INFINITE), m_node(nullptr)
    {
    }

    explicit SearchTask(int pid, int itd, int pld, MoveNode * node)
        : m_positionId(pid), m_iterationDepth(itd), m_plyDepth(pld)
        , m_alpha(-VALUE_INFINITE), m_beta(VALUE_INFINITE)
        , m_gamma(-VALUE_INFINITE), m_node(node)
    {
    }

    /**
     * @brief 局面IDを取得します。
     */
    int position_id() const
    {
        return m_positionId;
    }

    /**
     * @brief 反復深化の探索深さを取得します。
     */
    int iteration_depth() const
    {
        return m_iterationDepth;
    }

    /**
     * @brief ルート局面からの指し手の深さを取得します。
     */
    int ply_depth() const
    {
        return m_plyDepth;
    }

    /**
     * @brief 探索する指し手のノードを取得します。
     */
    MoveNode * node()
    {
        return m_node;
    }

    /**
     * @brief 探索する指し手を取得します。
     */
    Move move() const
    {
        return m_node->move();
    }

    /**
     * @brief タスクがない場合は真となります。
     */
    bool empty() const
    {
        return (m_node == nullptr);
    }

    /**
     * @brief 実際に有効なα値を取得します。
     */
    Value effective_alpha() const
    {
        return (m_alpha > -VALUE_INFINITE ? m_alpha : m_gamma);
    }

    /**
     * @brief 探索のα値を取得します。
     */
    Value alpha() const
    {
        return m_alpha;
    }

    /**
     * @brief 探索のα値を設定します。
     */
    void set_alpha(Value value)
    {
        m_alpha = value;
    }

    /**
     * @brief 探索のβ値を取得します。
     */
    Value beta() const
    {
        return m_beta;
    }

    /**
     * @brief 探索のβ値を設定します。
     */
    void set_beta(Value value)
    {
        m_beta = value;
    }

    /**
     * @brief 探索のγ値（仮のα値）を取得します。
     */
    Value gamma() const
    {
        return m_gamma;
    }

    /**
     * @brief 探索のγ値（仮のα値）を設定します。
     */
    void set_gamma(Value value)
    {
        m_gamma = value;
    }

private:
    int m_positionId;
    int m_iterationDepth;
    int m_plyDepth;
    Value m_alpha;
    Value m_beta;
    Value m_gamma;
    MoveNode * m_node;
};

} // namespace client
} // namespace godwhale

#endif
