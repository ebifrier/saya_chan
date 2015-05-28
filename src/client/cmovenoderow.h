
#ifndef GODWHALE_CLIENT_CMOVENODEROW_H
#define GODWHALE_CLIENT_CMOVENODEROW_H

#include "movenodelist.h"

namespace godwhale {
namespace client {

/**
 * @brief ある局面・段の探索結果や候補手をまとめて管理します。
 */
class CMoveNodeRow
{
public:
    //explicit CMoveNodeRow();
    explicit CMoveNodeRow(int positionId, int iterationDepth,
                          int plyDepth, Move left);
    ~CMoveNodeRow();

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
     * @brief ルート局面からの指し手の数を取得します。
     */
    int ply_depth() const
    {
        return m_plyDepth;
    }

    /**
     * @brief 実際に有効なα値を取得します。
     */
    Value effective_alpha() const
    {
        return (m_alpha > -VALUE_INFINITE ? m_alpha : m_gamma);
    }

    /**
     * @brief この段のα値を取得します。
     */
    Value alpha() const
    {
        return m_alpha;
    }

    /**
     * @brief この段のβ値を取得します。
     */
    Value beta() const
    {
        return m_beta;
    }

    /**
     * @brief 探索のγ値（仮のα値）を取得します。
     */
    Value gamma() const
    {
        return m_gamma;
    }

    /**
     * @brief 探索結果の評価値を取得します。
     */
    Value best_value() const
    {
        return m_bestValue;
    }

    /**
     * @brief ULEType(exact,lower,upper)を取得します。
     */
    ULEType best_ule() const
    {
        return m_bestULE;
    }

    /**
     * @brief PVに含まれる指し手の数を取得します。
     */
    int best_pv_size() const
    {
        return m_bestPV.size();
    }

    /**
     * @brief PVに含まれる指し手を取得します。
     */
    Move best_pv(int ply) const
    {
        return m_bestPV[ply];
    }

    /**
     * @brief この段がβカットされたか調べます。
     */
    bool betacut() const
    {
        return (m_bestValue >= m_beta);
    }

    void set_move_list(std::vector<Move> const & list);
    void clear();
    MoveNode *find_undone(Depth depth, Value alpha, Value beta);

    void set_value(Value value, ValueType vtype);
    void update_value(Value value, ValueType vtype);
    void update_best(Value value, Move move, std::vector<Move> const & pv);

private:
    //CMoveNodeRow(CMoveNodeRow const & other);
    CMoveNodeRow &operator =(CMoveNodeRow const & other);

private:
    int m_positionId;
    int m_iterationDepth;
    int m_plyDepth;

    Move m_left;
    Value m_alpha;
    Value m_beta;
    Value m_gamma;

    Value m_bestValue;
    ULEType m_bestULE;
    std::vector<Move> m_bestPV;

    MoveNodeList m_nodeList;
};

} // namespace server
} // namespace godwhale

#endif
