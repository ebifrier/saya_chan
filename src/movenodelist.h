#ifndef GODWHALE_MOVENODELIST_H
#define GODWHALE_MOVENODELIST_H

#include "movenode.h"

namespace godwhale {

/**
 * @brief 指し手とその探索結果をまとめて保持します。
 */
class MoveNodeList
{
public:
    explicit MoveNodeList(
#if defined(GODWHALE_SERVER)
        server::ServerClientPtr sclient
#endif
        );
    ~MoveNodeList();

#if defined(GODWHALE_SERVER)
    /**
     * @brief 関連付けられたクライアントを取得します。
     */
    server::ServerClientPtr sclient() const
    {
        return m_sclient;
    }
#endif

    /**
     * @brief ノードリストを取得します。
     */
    std::vector<MoveNode> const &node_list() const
    {
        return m_nodeList;
    }

    /**
     * @brief ノード数を取得します。
     */
    int size() const
    {
        return m_nodeList.size();
    }

    /**
     * @brief 指定のインデックスのノードを取得します。
     */
    MoveNode &operator[](int index)
    {
        return m_nodeList[index];
    }

    /**
     * @brief 指定のインデックスのノードを取得します。
     */
    MoveNode const &operator[](int index) const
    {
        return m_nodeList[index];
    }

    /**
     * @brief ノードリストのbegin iteratorを取得します。
     */
    std::vector<MoveNode>::iterator begin()
    {
        return m_nodeList.begin();
    }

    /**
     * @brief ノードリストのbegin iteratorを取得します。
     */
    std::vector<MoveNode>::const_iterator begin() const
    {
        return m_nodeList.begin();
    }

    /**
     * @brief ノードリストのend iteratorを取得します。
     */
    std::vector<MoveNode>::iterator end()
    {
        return m_nodeList.end();
    }

    /**
    * @brief ノードリストのend iteratorを取得します。
    */
    std::vector<MoveNode>::const_iterator end() const
    {
        return m_nodeList.end();
    }

    /**
     * @brief リスト中の指し手の計算がすべて完了しているか調べます。
     */
    bool done(Depth depth, Value alpha, Value beta) const
    {
        return (undone_count(depth, alpha, beta) == 0);
    }

    /**
     * @brief 計算が完了していないノードの数を取得します。
     */
    int undone_count(Depth depth, Value alpha, Value beta) const
    {
        return (m_nodeList.size() - done_count(depth, alpha, beta));
    }

    std::vector<std::string> to_sfen_list() const;
    std::vector<std::string> to_kif_list() const;

    void clear();
    void add_move(Move move);

    int find(Move move) const;
    int find_undone(Depth depth, Value alpha, Value beta) const;
    int done_count(Depth depth, Value alpha, Value beta) const;

private:
#if defined(GODWHALE_SERVER)
    server::ServerClientPtr m_sclient;
#endif

    std::vector<MoveNode> m_nodeList;
};

} // namespace godwhale

#endif
