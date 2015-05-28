
#ifndef GODWHALE_CLIENT_CMOVENODETREE_H
#define GODWHALE_CLIENT_CMOVENODETREE_H

#include "saya_chan/position.h"
#include "cmovenoderow.h"
#include "searchtask.h"

namespace godwhale {
namespace client {

class Client;

/**
 * @brief ある局面の探索結果や候補手を管理します。
 */
class CMoveNodeTree
{
public:
    explicit CMoveNodeTree(Client & client);
    ~CMoveNodeTree();

    /**
     * @brief 現局面を取得します。
     */
    Position &position()
    {
        return m_position;
    }

    /**
     * @brief 現局面を取得します。
     */
    Position const &position() const
    {
        return m_position;
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
     * @brief 
     */
    int last_ply_depth() const
    {
        return m_lastPlyDepth;
    }

    /**
     * @brief ルート局面からのPVを取得します。
     */
    std::vector<Move> const &pv_from_root() const
    {
        return m_pvFromRoot;
    }

    /**
     * @brief 指定の深さのノードブランチを取得します。
     */
    CMoveNodeRow &row(int pld)
    {
        return m_rows[pld];
    }

    /**
     * @brief 指定の深さのノードブランチを取得します。
     */
    CMoveNodeRow const &row(int pld) const
    {
        return m_rows[pld];
    }

    bool has_same_pv(int pld, std::vector<Move> const & pv);
    bool done_exact(int pld, int srd);
    std::vector<Move> move_list_from_sfen(int plyDepth,
                                          std::vector<std::string> const & movesSFEN);

    void set_position(std::string const & pos_sfen, int positionId);
    void make_move_root(Move move, int positionId);
    void set_pv(int iterationDepth, std::vector<Move> const & pv);
    void set_move_list(int pld, std::vector<Move> const & list);
    void start(int startPld, Value alpha, Value beta);

    void commit(int plyDepth);
    void notify(int plyDepth, Value value);
    void propagate_up(int plyDepth, Value value);
    void propagate_down(int plyDepth, Value value, ValueType vtype);

    SearchTask get_search_task();

private:
    Client & m_client;
    Position m_position;
    std::list<StateInfo> m_stList;
    int m_positionId;
    int m_iterationDepth;
    int m_lastPlyDepth;

    std::vector<Move> m_pvFromRoot;
    std::vector<CMoveNodeRow> m_rows;
};

} // namespace client
} // namespace godwhale

#endif
