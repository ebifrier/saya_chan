#ifndef GODWHALE_COMMANDPACKET_H
#define GODWHALE_COMMANDPACKET_H

#include "saya_chan/position.h"

namespace godwhale {

/**
 * @brief コマンド識別子です。
 *
 * コマンドとは
 * 1, サーバーからクライアントに送られた
 * 2, stdinから受信された
 * 命令インストラクションです。
 */
enum CommandType
{
    /**
     * @brief 特になし
     */
    COMMAND_NONE,
    /**
     * @brief ログイン処理の結果をもらいます。
     */
    COMMAND_LOGINRESULT,
    /**
     * @brief ルート局面を設定します。
     */
    COMMAND_SETPOSITION,
    /**
     * @brief ルート局面から指し手を一つ進めます。
     */
    COMMAND_MAKEMOVEROOT,
    /**
     * @brief PVを設定します。
     */
    COMMAND_SETPV,
    /**
     * @brief 担当する指し手を設定します。
     */
    COMMAND_SETMOVELIST,
    /**
     * @brief 指定の局面の探索を開始します。
     */
    COMMAND_START,
    /**
     * クライアントを停止します。
     */
    COMMAND_STOP,

    /**
     * 評価値の更新を通知します。
     */
    COMMAND_NOTIFY,
    COMMAND_CANCEL,
    COMMAND_COMMIT,
    /**
     * @brief サーバーとクライアントの状態が一致しているか確認します。
     */
    COMMAND_VERIFY,
    /**
     * @brief クライアントを終了させます。
     */
    COMMAND_QUIT,
};


/**
 * @brief 
 */
struct ValueSet
{
    explicit ValueSet()
    {
    }

    ValueSet(Value value_, Value alpha_, Value beta_, Value gamma_)
        : value(value_), alpha(alpha_), beta(beta_), gamma(gamma_)
    {
    }

    Value value;
    Value alpha;
    Value beta;
    Value gamma;
};


/**
 * @brief コマンドデータをまとめて扱います。
 */
class CommandPacket
{
public:
    explicit CommandPacket(CommandType type);
    CommandPacket(CommandPacket const & other);
    CommandPacket(CommandPacket && other);

    ///
    /// @brief コマンドタイプを取得します。
    ///
    CommandType type() const
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
    /// @brief 更新前の局面IDを取得します。
    ///
    int prev_position_id() const
    {
        return m_prevPositionId;
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
    /// @brief 探索のα値を取得します。
    ///
    int alpha() const
    {
        return m_alpha;
    }

    ///
    /// @brief 探索のβ値を取得します。
    ///
    int beta() const
    {
        return m_beta;
    }

    ///
    /// @brief 局面を取得します。
    ///
    std::string const &position_sfen() const
    {
        return m_positionSFEN;
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

    ///
    /// @brief 値のペア配列を取得します。
    ///
    std::vector<ValueSet> const &values_set() const
    {
        return m_valuesSet;
    }

public:
    /* for Login */

    /**
     * @brief ログイン結果を取得します。
     */
    /*int login_result() const
    {
        return m_loginResult;
    }*/

public:
    int priority() const;
    std::string toRSI() const;

    static CommandPacketPtr create_setposition(int positionId,
                                               std::string const & positionSFEN);
    static CommandPacketPtr create_makemoveroot(int nextPositionId,
                                                int prevPositionId,
                                                std::string const & moveSFEN);
    static CommandPacketPtr create_setpv(int positionId, int iterationDepth,
                                         std::vector<std::string> const & pvSFEN);
    static CommandPacketPtr create_setmovelist(int positionId, int iterationDepth,
                                               int plyDepth,
                                               std::vector<std::string> const & movesSFEN);
    static CommandPacketPtr create_start(int positionId, int iterationDepth,
                                         int plyDepth, Value alpha, Value beta);
    static CommandPacketPtr create_stop();

    static CommandPacketPtr create_notify(int positionId, int iterationDepth,
                                          int plyDepth, Value value);
    static CommandPacketPtr create_cancel(int positionId, int iterationDepth,
                                          int plyDepth);
    static CommandPacketPtr create_commit(int positionId, int iterationDepth,
                                          int plyDepth);

    static CommandPacketPtr create_verify(int positionId, int iterationDepth,
                                          int plyDepth,
                                          std::vector<ValueSet> const & values);
    static CommandPacketPtr create_quit();

    static CommandPacketPtr parse(std::string const & rsi);

private:    
    // loginresult
    /*static CommandPacketPtr parse_loginresult(std::string const & rsi,
                                                Tokenizer & tokens);
    std::string toRSI_loginresult() const;*/

    // setposition
    static CommandPacketPtr parse_setposition(std::string const & rsi,
                                              Tokenizer & tokens);
    std::string toRSI_setposition() const;

    // makerootmove
    static CommandPacketPtr parse_makemoveroot(std::string const & rsi,
                                               Tokenizer & tokens);
    std::string toRSI_makemoveroot() const;

    // setpv
    static CommandPacketPtr parse_setpv(std::string const & rsi,
                                        Tokenizer & tokens);
    std::string toRSI_setpv() const;

    // setmovelist
    static CommandPacketPtr parse_setmovelist(std::string const & rsi,
                                              Tokenizer & tokens);
    std::string toRSI_setmovelist() const;

    // start
    static CommandPacketPtr parse_start(std::string const & rsi,
                                        Tokenizer & tokens);
    std::string toRSI_start() const;

    // stop
    static CommandPacketPtr parse_stop(std::string const & rsi,
                                       Tokenizer & tokens);
    std::string toRSI_stop() const;

    // notify
    static CommandPacketPtr parse_notify(std::string const & rsi,
                                         Tokenizer & tokens);
    std::string toRSI_notify() const;

    // cancel
    static CommandPacketPtr parse_cancel(std::string const & rsi,
                                         Tokenizer & tokens);
    std::string toRSI_cancel() const;

    // commit
    static CommandPacketPtr parse_commit(std::string const & rsi,
                                         Tokenizer & tokens);
    std::string toRSI_commit() const;

    // verify
    static CommandPacketPtr parse_verify(std::string const & rsi,
                                         Tokenizer & tokens);
    std::string toRSI_verify() const;

    // quit
    static CommandPacketPtr parse_quit(std::string const & rsi,
                                       Tokenizer & tokens);
    std::string toRSI_quit() const;

private:
    CommandType m_type;

    int m_positionId;
    int m_prevPositionId;
    int m_iterationDepth;
    int m_plyDepth;

    Value m_alpha;
    Value m_beta;

    std::string m_positionSFEN;
    std::string m_moveSFEN;
    std::vector<std::string> m_movesSFEN;
    std::vector<ValueSet> m_valuesSet;
};

} // namespace godwhale

#endif
