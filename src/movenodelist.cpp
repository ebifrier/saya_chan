#include "precomp.h"
#if defined(GODWHALE_SERVER)
#include "server/stdinc_server.h"
#else
#include "stdinc.h"
#endif

#include "movenodelist.h"

namespace godwhale {

MoveNodeList::MoveNodeList(
#if defined(GODWHALE_SERVER)
    server::ServerClientPtr sclient)
        : m_sclient(sclient)
#else
    )
#endif
{
    // ちょっとした高速化
    m_nodeList.reserve(200);
}

MoveNodeList::~MoveNodeList()
{
}

/**
 * @brief 指し手のリストをSFEN形式の文字列リストに変換します。
 */
std::vector<std::string> MoveNodeList::to_sfen_list() const
{
    std::vector<std::string> result;

    for (auto node : m_nodeList) {
        result.push_back(move_to_uci(node.move()));
    }

    return result;
}

/**
* @brief 指し手のリストをKIF形式の文字列リストに変換します。
*/
std::vector<std::string> MoveNodeList::to_kif_list() const
{
    std::vector<std::string> result;

    for (auto node : m_nodeList) {
        result.push_back(move_to_kif(node.move()));
    }

    return result;
}

/**
 * @brief 設定された指し手一覧をすべてクリアします。
 */
void MoveNodeList::clear()
{
    m_nodeList.clear();
}

/**
 * @brief 新たな候補手を一手設定します。
 */
void MoveNodeList::add_move(Move move)
{
    m_nodeList.push_back(MoveNode(move));
}

/**
 * @brief 与えられた指し手を持つノードのインデックスを取得します。
 */
int MoveNodeList::find(Move move) const
{
    int size = m_nodeList.size();

    for (int i = 0; i < size; ++i) {
        if (m_nodeList[i].move() == move) {
            return i;
        }
    }

    return -1;
}

/**
 * @brief 計算が完了していない指し手のインデックスを取得します。
 */
int MoveNodeList::find_undone(Depth depth, Value alpha, Value beta) const
{
    int size = m_nodeList.size();

    for (int i = 0; i < size; ++i) {
        if (!m_nodeList[i].done(depth, alpha, beta)) {
            return i;
        }
    }

    return -1;
}

/**
 * @brief 計算が完了しているノードの数を取得します。
 */
int MoveNodeList::done_count(Depth depth, Value alpha, Value beta) const
{
    int size = m_nodeList.size();
    int count = 0;

    for (int i = 0; i < size; ++i) {
        auto & nodeList = m_nodeList[i];

        if (nodeList.done(depth, alpha, beta)) {
            ++count;
        }
    }

    return count;
}
   
} // namespace godwhale
