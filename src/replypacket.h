#ifndef GODWHALE_REPLYPACKET_H
#define GODWHALE_REPLYPACKET_H

#include "saya_chan/position.h"

namespace godwhale {

///
/// @brief 応答コマンドの識別子です。
///
/// 応答コマンドとはクライアントからサーバーに送られる
/// インストラクションです。
///
enum ReplyType
{
    ///
    /// @brief 特になし。
    ///
    REPLY_NONE,
    /**
     * @brief ログイン用コマンド
     *
     * login <login_name> <bench_result> <hash_size>
     */
    REPLY_LOGIN,
    /**
     * @brief α値の更新の可能性があるとき、探索時間の調整などに使います。
     *
     * retryed <position_id> <itd> <pld> <move>
     */
    REPLY_RETRYED,
    /**
     * @brief α値の更新が行われたときに使います。
     *
     * updatevalue <position_id> <itd> <pld> <move> <value> <ule> <pv>
     */
    REPLY_UPDATEVALUE,
    /**
     * @brief 指定の段の探索が終わった時に呼ばれます。
     *
     * searchdone <position_id> <itd> <pld>
     */
    REPLY_SEARCHDONE,
};


///
/// @brief 応答コマンドデータを扱います。
///
class ReplyPacket
{
private:

public:
    explicit ReplyPacket(ReplyType type);
    ReplyPacket(ReplyPacket & other);
    ReplyPacket(ReplyPacket && other);

    ///
    /// @brief コマンドタイプを取得します。
    ///
    ReplyType type() const
    {
        return m_type;
    }

    ///
    /// @brief 局面IDを取得します。
    ///
    int position_id() const
    {
        return m_positionId;
    }

    ///
    /// @brief 反復深化の探索深さを取得します。
    ///
    int iteration_depth() const
    {
        return m_iterationDepth;
    }

    ///
    /// @brief ルート局面から思考局面までの手の深さを取得します。
    ///
    int ply_depth() const
    {
        return m_plyDepth;
    }

    ///
    /// @brief 評価値を取得します。
    ///
    int value() const
    {
        return m_value;
    }

    ///
    /// @brief 評価値を取得します。
    ///
    int alpha() const
    {
        return m_alpha;
    }

    ///
    /// @brief 評価値を取得します。
    ///
    int beta() const
    {
        return m_beta;
    }

    ///
    /// @brief 指し手の探索数を取得します。
    ///
    int64_t nodes() const
    {
        return m_nodes;
    }

    ///
    /// @brief 指し手を取得します。
    ///
    std::string const & move_sfen() const
    {
        return m_moveSFEN;
    }

    ///
    /// @brief 指し手のリスト(手の一覧やＰＶなど)を取得します。
    ///
    std::vector<std::string> const &move_sfen_list() const
    {
        return m_movesSFEN;
    }

public:
    ///
    /// @brief ログインＩＤを取得します。
    ///
    std::string const &login_name() const
    {
        return m_loginName;
    }

    ///
    /// @brief クライアントの探索ベンチマーク結果を取得します。
    ///
    int bench_result() const
    {
        return m_benchResult;
    }

    ///
    /// @brief クライアントのハッシュサイズを取得します。
    ///
    int hash_size() const
    {
        return m_hashSize;
    }

public:
    int priority() const;

    static ReplyPacketPtr parse(std::string const & rsi);
    std::string toRSI() const;

    static ReplyPacketPtr create_login(std::string const & name,
                                       int benchResult, int hashSize);
    static ReplyPacketPtr create_updatevalue(int positionId, int iterationDepth,
                                             int plyDepth, Move move, Value value,
                                             Value alpha, Value beta, int64_t nodes,
                                             std::vector<Move> pv);
    static ReplyPacketPtr create_searchdone(int positionId, int iterationDepth,
                                            int plyDepth);

private:
    // login
    static ReplyPacketPtr parse_login(std::string const & rsi,
                                      Tokenizer & tokens);
    std::string toRSI_login() const;

    // updatevalue
    static ReplyPacketPtr parse_updatevalue(std::string const & rsi,
                                            Tokenizer & tokens);
    std::string toRSI_updatevalue() const;

    // searchdone
    static ReplyPacketPtr parse_searchdone(std::string const & rsi,
                                           Tokenizer & tokens);
    std::string toRSI_searchdone() const;

private:
    ReplyType m_type;

    std::string m_loginName;
    int m_benchResult;
    int m_hashSize;

    int m_positionId;
    int m_iterationDepth;
    int m_plyDepth;
    Value m_value;
    Value m_alpha;
    Value m_beta;
    int64_t m_nodes;

    std::string m_moveSFEN;
    std::vector<std::string> m_movesSFEN;
};

} // namespace godwhale

#endif
