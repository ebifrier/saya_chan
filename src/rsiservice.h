#ifndef GODWHALE_RSISERVICE_H
#define GODWHALE_RSISERVICE_H

namespace godwhale {

/**
 * @brief テキストコマンドの受信時に使われるリスナーインターフェースです。
 */
class IRSIListener
{
public:
    virtual ~IRSIListener() {}

    /**
     * @brief コマンド受信時に呼ばれ、コマンドの処理を行います。
     */
    virtual void on_received(std::string const & command) = 0;

    /**
     * @brief 接続の切断時に呼ばれます。
     */
    virtual void on_disconnected() = 0;
};

/**
 * @brief テキストコマンドのデータ送信用クラスです。(privateクラス)
 */
class RSISendData
{
public:
    explicit RSISendData();
    explicit RSISendData(std::string const & command, bool isOutLog);
    RSISendData(RSISendData const & other);
    RSISendData(RSISendData && other);

    RSISendData &operator =(RSISendData const & other);
    RSISendData &operator =(RSISendData && other);

    /**
     * @brief コマンド文字列を取得します。
     */
    std::string const &command() const
    {
        return m_command;
    }

    /**
     * @brief ログ表示を行うかどうかのフラグを取得します。
     */
    bool is_outlog() const
    {
        return m_isOutLog;
    }

    /**
     * @brief コマンドが空かどうかを取得します。
     */
    bool empty() const
    {
        return m_command.empty();
    }

    /**
     * @brief コマンドを初期化します。
     */
    void clear()
    {
        m_command.clear();
    }

    /**
     * @brief 必要な場合のみ、コマンドの区切り記号を追加します。
     */
    void add_delimiter()
    {
        if (m_command.empty() || m_command.back() != '\n') {
            m_command.append("\n");
        }
    }

private:
    std::string m_command;
    bool m_isOutLog;
};

/**
 * @brief テキストで表現されたコマンドの送受信を行うクラスです。
 */
class RSIService : public enable_shared_from_this<RSIService>
{
public:
    explicit RSIService(boost::asio::io_service & service,
                        weak_ptr<IRSIListener> listener);
    virtual ~RSIService();

    /**
     * @brief ソケットが接続中かどうかを調べます。
     */
    bool is_open() const
    {
        auto socket = m_socket;
        return (socket != nullptr && socket->is_open());
    }

    /**
     * @brief コマンドデータの送信を行います。
     */
    void send(boost::format const & fmt, bool isOutLog = true)
    {
        send(fmt.str(), isOutLog);
    }

    void connect(std::string const & address, std::string const & port);
    void set_socket(shared_ptr<tcp::socket> socket);

    void close();
    void send(std::string const & command, bool isOutLog = true);

private:
    void on_disconnected();
    
    void start_receive();
    void handle_async_receive(boost::system::error_code const & error,
                              std::size_t size);

    void add_send_data(RSISendData const & sendData);
    RSISendData get_send_data();

    void begin_async_send();
    void handle_async_send(boost::system::error_code const & error,
                           std::size_t size);

private:
    mutable Mutex m_guard;
    boost::asio::io_service & m_service;
    shared_ptr<tcp::socket> m_socket;
    weak_ptr<IRSIListener> m_listener;

    volatile bool m_isShutdown;

    boost::asio::streambuf m_streambuf;
    std::list<RSISendData> m_sendList;
    RSISendData m_sending;
};

} // namespace godwhale

#endif
