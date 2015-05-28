#include "precomp.h"
#include "stdinc.h"
#include "rsiservice.h"

namespace godwhale {

RSISendData::RSISendData()
{
}

RSISendData::RSISendData(std::string const & command, bool isOutLog)
    : m_command(command), m_isOutLog(isOutLog)
{
}

RSISendData::RSISendData(RSISendData const & other)
    : m_command(other.m_command), m_isOutLog(other.m_isOutLog)
{
}

RSISendData::RSISendData(RSISendData && other)
    : m_command(std::move(other.m_command)), m_isOutLog(other.m_isOutLog)
{
}

RSISendData &RSISendData::operator =(RSISendData const & other)
{
    if (this != &other) {
        m_command = other.m_command;
        m_isOutLog = other.m_isOutLog;
    }

    return *this;
}

RSISendData &RSISendData::operator =(RSISendData && other)
{
    if (this != &other) {
        m_command = std::move(other.m_command);
        m_isOutLog = std::move(other.m_isOutLog);
    }

    return *this;
}


/////////////////////////////////////////////////////////////////////
RSIService::RSIService(boost::asio::io_service & service,
                       weak_ptr<IRSIListener> listener)
    : m_service(service), m_listener(listener)
    , m_isShutdown(false)
{
}

RSIService::~RSIService()
{
    close();
}

/**
 * @brief サーバーへの接続処理を行います。
 */
void RSIService::connect(std::string const & address, std::string const & port)
{
    if (address.empty()) {
        throw std::invalid_argument("address");
    }

    if (port.empty()) {
        throw std::invalid_argument("port");
    }

    tcp::resolver resolver(m_service);
    tcp::resolver::query query(address, port);
    shared_ptr<tcp::socket> sock(new tcp::socket(m_service));

    LOG_NOTIFICATION() << F("Begin to connect server: %1%:%2%") % address % port;

    while (true) {
        boost::system::error_code error;
        tcp::resolver::iterator endpoint = resolver.resolve(query, error);
        tcp::resolver::iterator end;

        if (error) {
            LOG_ERROR() << F("ネットワークエラーです。接続などを確認してください。(%1%)")
                         % error.message();
            return;
        }

        error = boost::asio::error::host_not_found;
        while (error && endpoint != end) {
            sock->close(error);
            sock->connect(*endpoint++, error);
            if (!error) break;
        }

        if (!error) break;

        LOG_NOTIFICATION() << "Connect retry...";
        sleep(5 * 1000);
    }

    LOG_NOTIFICATION() << "Connected !";

    set_socket(sock);
}

/**
 * @brief 通信用のソケットを設定します。
 */
void RSIService::set_socket(shared_ptr<tcp::socket> socket)
{
    close();

    // shared_ptrはスレッドセーフなので
    m_socket = socket;

    // 受信処理を開始します。
    if (m_socket != nullptr && m_socket->is_open()) {
        start_receive();
    }
}

/**
 * @brief ソケットの接続を閉じます。
 */
void RSIService::close()
{
    LOCK (m_guard) {
        m_sendList.clear();
    }

    auto socket = m_socket;
    if (socket != nullptr && socket->is_open()) {
        boost::system::error_code error;
        socket->shutdown(tcp::socket::shutdown_both, error);

        // シャットダウンフラグを立てておきます。
        m_isShutdown = true;
    }
}

/**
 * @brief ソケットが閉じられた後に呼ばれます。
 *
 * 自分からソケットを閉じた場合も、接続相手から切断された場合でも
 * どちらの場合でもこのメソッドが呼ばれます。
 */
void RSIService::on_disconnected()
{
    shared_ptr<IRSIListener> listener = m_listener.lock();
    if (listener != nullptr) {
        listener->on_disconnected();
    }

    LOCK(m_guard) {
        m_sendList.clear();
    }

    auto socket = m_socket;
    if (socket != nullptr && socket->is_open()) {
        boost::system::error_code error;
        socket->close(error);

        // ソケットを閉じたので、インスタンスにはnullptrを設定します。
        m_socket = nullptr;
    }
}

/**
 * @brief ソケットのコマンド受信処理を開始します。
 */
void RSIService::start_receive()
{
    auto socket = m_socket;
    if (socket == nullptr || !socket->is_open()) {
        LOG_ERROR() << "RSIService::start_receiveに失敗しました。";
        return;
    }

    weak_ptr<RSIService> weak = shared_from_this();
    boost::asio::async_read_until(
        *socket, m_streambuf, "\n",
        [=](boost::system::error_code const & error, std::size_t size) {
            auto self = weak.lock();
            if (self != nullptr) self->handle_async_receive(error, size);
        });
}

/**
 * @brief コマンド受信後に呼ばれ、コマンドのパースやその処理などを行います。
 */
void RSIService::handle_async_receive(boost::system::error_code const & error,
                                      std::size_t size)
{
    if (error) {
        if (m_isShutdown) {
            LOG_WARNING() << "ソケットはすでにshutdownされています。";
        }
        else if (error != boost::asio::error::eof &&
                 error != boost::asio::error::would_block) {
            LOG_ERROR() << error << ": 通信エラーが発生しました。"
                        << "(" << error.message() << ")";
        }

        on_disconnected();
        return;
    }

    if (size <= 0) {
        // 切断されたとみる
        on_disconnected();
        return;
    }

    // データの受信処理
    auto pcommand = boost::asio::buffer_cast<char const *>(m_streambuf.data());
    m_streambuf.consume(size);

    // 受信したコマンドを文字列化します。
    std::string command(&pcommand[0], &pcommand[size-1]);
    LOG_DEBUG() << "RSI received: " << command;

    // lock外で各コマンドを処理します。
    shared_ptr<IRSIListener> listener = m_listener.lock();
    if (listener != nullptr) {
        listener->on_received(command);
    }

    start_receive();
}

/**
 * @brief 送信用のコマンドリストに送信用データを追加します。
 */
void RSIService::add_send_data(RSISendData const & sendData)
{
    if (sendData.empty()) {
        return;
    }

    LOCK(m_guard) {
        m_sendList.push_back(sendData);
    }
}

/**
 * @brief 未送信の送信用データを取得します。
 */
RSISendData RSIService::get_send_data()
{
    LOCK(m_guard) {
        if (m_sendList.empty()) {
            return RSISendData();
        }

        auto sendData = m_sendList.front();
        m_sendList.pop_front();
        return sendData;
    }
}

/**
 * @brief コマンドの送信処理を行います。
 */
void RSIService::send(std::string const & command, bool isOutLog/*=true*/)
{
    add_send_data(RSISendData(command, isOutLog));

    // 次の送信処理を開始します。
    begin_async_send();
}

/**
 * @brief 非同期的な送信処理を開始します。
 */
void RSIService::begin_async_send()
{
    auto socket = m_socket;
    if (socket == nullptr || !socket->is_open()) {
        LOG_ERROR() << "The socket is not opened or connected.";
        return;
    }

    LOCK(m_guard) {
        if (m_isShutdown) {
            LOG_ERROR() << "The socket is already shutdown.";
            return;
        }

        // データ送信中の場合
        if (!m_sending.empty()) {
            return;
        }

        // 未送信のデータを取得
        m_sending = get_send_data();
        if (m_sending.empty()) {
            return;
        }

        if (m_sending.is_outlog()) {
            LOG_DEBUG() << "Send: " << m_sending.command();
        }

        // データ送信前に区切り記号を付加します。
        m_sending.add_delimiter();
    }

    // コールバックが即座に呼ばれることがあるため、
    // async_writeはロックを解除した状態で呼びます。
    weak_ptr<RSIService> weak = shared_from_this();
    boost::asio::async_write(
        *socket, boost::asio::buffer(m_sending.command()),
        [=](boost::system::error_code const & error, std::size_t size) {
            auto self = weak.lock();
            if (self != nullptr) self->handle_async_send(error, size);
        });
}

/**
 * @brief 非同期送信の完了後に呼ばれます。
 */
void RSIService::handle_async_send(boost::system::error_code const & error,
                                   std::size_t size)
{
    if (error) {
        LOG_ERROR() << "Failed to send a command (" << error.message() << ")";
        on_disconnected();
        return;
    }

    LOCK(m_guard) {
        m_sending.clear();
    }

    // 次の送信処理に移ります。
    begin_async_send();
}

} // namespace godwhale
