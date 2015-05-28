#ifndef GODWHALE_CLIENT_CLIENT_H
#define GODWHALE_CLIENT_CLIENT_H

#include "saya_chan/position.h"
#include "rsiservice.h" // IRSIListener

namespace godwhale {
namespace client {

class SearchTask;
class CMoveNodeTree;

/**
 * @brief クライアントの実行・管理を行います。
 *
 * シングルトンクラスです。
 */
class Client : public enable_shared_from_this<Client>,
               public IRSIListener,
               private boost::noncopyable
{
public:
    /**
     * @brief 初期化処理を行います。
     */
    static void init();

    /**
     * @brief シングルトンインスタンスを取得します。
     */
    static shared_ptr<Client> instance()
    {
        return ms_instance;
    }

public:
    ~Client();

    /**
     * @brief 正常にログイン処理が行われたかどうかを取得します。
     */
    bool logined() const
    {
        return m_logined;
    }

    /**
     * @brief クライアントのログイン名を取得します。
     */
    std::string const &login_name() const
    {
        return m_loginName;
    }

    /**
     * @brief 各クライアントの使用スレッド数を取得します。
     */
    int thread_count() const
    {
        return m_nthreads;
    }

    /**
     * @brief 探索ノード数を取得します。
     */
    long node_searched() const
    {
        return m_nodes;
    }

    void connect(std::string const & address, std::string const & port);
    void login(std::string const & loginName);
    void close();

    void send_reply(ReplyPacketPtr reply, bool isOutLog = true);
    /*void update_value(Move move, Value value, Value alpha, Value beta,
                      int64_t nodes, std::vector<Move> const & pv);*/

    int run();

private:
    friend class CMoveNodeTree;
    static shared_ptr<Client> ms_instance;

    explicit Client();

    virtual void on_disconnected();
    virtual void on_received(std::string const & command);

    void add_command(CommandPacketPtr command);
    void add_command(std::string const & rsi);
    void remove_command(CommandPacketPtr command);
    CommandPacketPtr get_next_command() const;

private:
    void initialize();
    void search_main(SearchTask & stask);

private:
    int proce(bool searching);
    int proce_login(CommandPacketPtr command);
    int proce_setposition(CommandPacketPtr command);
    int proce_makemoveroot(CommandPacketPtr command);
    int proce_setpv(CommandPacketPtr command);
    int proce_setmovelist(CommandPacketPtr command);
    int proce_start(CommandPacketPtr command);
    int proce_stop(CommandPacketPtr command);
    int proce_notify(CommandPacketPtr command);
    int proce_commit(CommandPacketPtr command);
    int proce_quit(CommandPacketPtr command);

private:
    boost::asio::io_service m_service;
    shared_ptr<RSIService> m_rsiService;
    volatile bool m_available;

    mutable Mutex m_commandGuard;
    std::list<CommandPacketPtr> m_commandList;

    shared_ptr<CMoveNodeTree> m_tree;

    bool m_logined;
    std::string m_loginName;
    int m_nthreads;
    long m_nodes;
};

} // namespace client
} // namespace godwhale

#endif
