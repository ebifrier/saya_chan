#include "precomp.h"
#include "stdinc.h"

#include "commandpacket.h"
#include "replypacket.h"
#include "cmovenodetree.h"
#include "client.h"

namespace godwhale {
namespace client {

shared_ptr<Client> Client::ms_instance;

/**
 * @brief クライアントのシングルトンインスタンスを初期化します。
 */
void Client::init()
{
    shared_ptr<Client> client(new Client);
    client->initialize();

    ms_instance = client;
}

Client::Client()
    : m_available(true), m_tree(new CMoveNodeTree(*this))
    , m_logined(false), m_nthreads(0), m_nodes(0)
{
}

Client::~Client()
{
    close();
}

/**
 * @brief クライアントの開始処理を行います。
 */
void Client::initialize()
{
    m_rsiService.reset(new RSIService(m_service, shared_from_this()));
}

/**
 * @brief コネクションを切断します。
 */
void Client::close()
{
    if (m_rsiService != nullptr) {
        m_rsiService->close();
        m_rsiService = nullptr;
    }

    m_available = false;
}

/**
 * @brief コネクションの切断時に呼ばれます。
 */
void Client::on_disconnected()
{
    LOG_NOTIFICATION() << "Client[" << m_loginName << "] is disconnected.";

    m_available = false;
}

/**
 * @brief サーバーからコマンドを受信した時に呼ばれます。
 */
void Client::on_received(std::string const & rsi)
{
    add_command(rsi);
}

/**
 * @brief 受信したRSIコマンドを追加します。
 */
void Client::add_command(std::string const & rsi)
{
    auto command = CommandPacket::parse(rsi);
    if (command == nullptr) {
        LOG_ERROR() << rsi << ": RSIコマンドの解釈に失敗しました。";
        return;
    }

    add_command(command);
}

/**
 * @brief コマンドを追加します。
 */
void Client::add_command(CommandPacketPtr command)
{
    if (command == nullptr) {
        throw std::invalid_argument("command");
    }

    LOCK (m_commandGuard) {
        auto it = m_commandList.begin();

        // 挿入ソートで優先順位の高い順に並べます。
        for (; it != m_commandList.end(); ++it) {
            if (command->priority() > (*it)->priority()) {
                break;
            }
        }

        m_commandList.insert(it, command);
    }
}

/**
 * @brief コマンドを削除します。
 */
void Client::remove_command(CommandPacketPtr command)
{
    LOCK (m_commandGuard) {
        m_commandList.remove(command);
    }
}

/**
 * @brief 次のコマンドを取得します。
 */
CommandPacketPtr Client::get_next_command() const
{
    LOCK (m_commandGuard) {
        if (m_commandList.empty()) {
            return CommandPacketPtr();
        }

        return m_commandList.front();
    }
}

/**
 * @brief コマンドをサーバーに送信します。
 */
void Client::send_reply(ReplyPacketPtr reply, bool isOutLog/*=true*/)
{
    std::string rsi = reply->toRSI();
    if (rsi.empty()) {
        LOG_ERROR() << F("reply type %1%: toRSIに失敗しました。")
                     % reply->type();
        return;
    }

    m_rsiService->send(rsi, isOutLog);
}

/**
 * @brief サーバーに接続に行きます。
 */
void Client::connect(std::string const & address, std::string const & port)
{
    if (m_rsiService->is_open()) {
        throw std::logic_error("すでに接続しています。");
    }

    m_rsiService->connect(address, port);
    m_logined = false;
}

/**
 * @brief サーバーに接続に行きます。
 */
void Client::login(std::string const & loginName)
{
    if (loginName.empty()) {
        LOG_ERROR() << "ログイン名がありません。";
        return;
    }

    if (!m_rsiService->is_open()) {
        LOG_ERROR() << "クジラちゃんサーバーに接続できていません。";
        return;
    }
    
    // ログイン用のリプライコマンドをサーバーに送ります。
    ReplyPacketPtr reply = ReplyPacket::create_login(loginName, 1, 1);
    m_rsiService->send(reply->toRSI());

    m_loginName = loginName;
    m_logined = true;
}

/**
 * @brief 受信した各コマンドの処理を行います。
 */
int Client::proce(bool searching)
{
    int status = PROCE_CONTINUE;

    while (status == PROCE_CONTINUE) {
        // これでクライアントの受信処理を一斉に行います。
        m_service.poll();
        m_service.reset();

        CommandPacketPtr command = get_next_command();
        if (command == nullptr) {
            return PROCE_OK;
        }

        int type = command->type();
        if (searching) {
            if (type == COMMAND_SETPOSITION  ||
                type == COMMAND_MAKEMOVEROOT ||
                type == COMMAND_SETPV ||
                type == COMMAND_STOP ||
                type == COMMAND_QUIT) {
                return PROCE_ABORT;
            }
        }

        remove_command(command);
        
        switch (command->type()) {
        case COMMAND_SETPOSITION:
            status = proce_setposition(command);
            break;
        case COMMAND_MAKEMOVEROOT:
            status = proce_makemoveroot(command);
            break;
        case COMMAND_SETPV:
            status = proce_setpv(command);
            break;
        case COMMAND_SETMOVELIST:
            status = proce_setmovelist(command);
            break;
        case COMMAND_START:
            status = proce_start(command);
            break;
        case COMMAND_STOP:
            status = proce_stop(command);
            break;
        case COMMAND_NOTIFY:
            status = proce_notify(command);
            break;
        case COMMAND_COMMIT:
            status = proce_commit(command);
            break;
        case COMMAND_QUIT:
            status = proce_quit(command);
            break;
        default:
            unreachable();
            break;
        }
    }

    return (status == PROCE_CONTINUE ? PROCE_OK : status);
}

/**
 * @brief setpositionコマンドを処理します。
 *
 * setposition <position_id> <sfen>
 */
int Client::proce_setposition(CommandPacketPtr command)
{
    m_tree->set_position(
        command->position_sfen(),
        command->position_id());

    return PROCE_CONTINUE;
}

/**
 * @brief setmovelistコマンドを処理します。
 *
 * makerootmove <position_id> <old_position_id> <move>
 */
int Client::proce_makemoveroot(CommandPacketPtr command)
{
    auto prevPositionId = command->prev_position_id();
    auto positionId = command->position_id();
    auto moveSFEN = command->move_sfen();

    if (m_tree->position_id() != prevPositionId) {
        LOG_ERROR() << "position_idが一致しません。";
        return PROCE_ABORT;
    }

    // SFEN形式の指し手を指し手に変換します。
    auto move = move_from_uci(m_tree->position(), moveSFEN);
    if (move == MOVE_NONE) {
        LOG_ERROR() << moveSFEN << ": 指し手の変換に失敗しました。";
        return PROCE_ABORT;
    }

    m_tree->make_move_root(move, positionId);

    return PROCE_CONTINUE;
}

/**
 * @brief setpvコマンドを処理します。
 *
 * setpv <position_id> <itd> <move1> ... <moveN>
 */
int Client::proce_setpv(CommandPacketPtr command)
{
    auto positionId = command->position_id();
    auto iterationDepth = command->iteration_depth();
    auto & movesSFEN = command->move_sfen_list();

    assert(iterationDepth > 0);

    // positionIdの確認
    if (m_tree->position_id() != positionId) {
        LOG_ERROR() << "position_idが一致しません。";
        return PROCE_CONTINUE;
    }

    // PVを設定します。
    auto pv = move_list_from_uci(m_tree->position(),
                                 movesSFEN.begin(), movesSFEN.end());
    if (iterationDepth != pv.size()) {
        LOG_ERROR() << "pvが正しく解析できません。";
        return PROCE_CONTINUE;
    }

    m_tree->set_pv(iterationDepth, pv);

    return PROCE_CONTINUE;
}

/**
 * @brief setmovelistコマンドを処理します。
 *
 * setmovelist <position_id> <itd> <pld> <move1> ... <moven>
 */
int Client::proce_setmovelist(CommandPacketPtr command)
{
    auto positionId = command->position_id();
    auto iterationDepth = command->iteration_depth();
    auto plyDepth = command->ply_depth();
    auto & movesSFEN = command->move_sfen_list();

    // positionIdの確認を行います。
    if (positionId != m_tree->position_id()) {
        LOG_ERROR() << "position_idが一致しません。";
        return PROCE_ABORT;
    }

    if (iterationDepth != m_tree->iteration_depth()) {
        LOG_ERROR() << "事前にPVを設定する必要があります。";
        return PROCE_ABORT;
    }

    auto moves = m_tree->move_list_from_sfen(plyDepth, movesSFEN);
    m_tree->set_move_list(plyDepth, moves);

    return PROCE_CONTINUE;
}

/**
 * @brief startコマンドを処理します。
 *
 * start <position_id> <itd> <pld> <alpha> <beta>
 */
int Client::proce_start(CommandPacketPtr command)
{
    auto positionId = command->position_id();
    auto iterationDepth = command->iteration_depth();
    auto plyDepth = command->ply_depth();

    if (positionId != m_tree->position_id()) {
        LOG_ERROR() << "position_idが一致しません。";
        return PROCE_ABORT;
    }

    if (iterationDepth != m_tree->iteration_depth()) {
        LOG_ERROR() << "事前にPVを設定する必要があります。";
        return PROCE_ABORT;
    }

    auto alpha = (Value)command->alpha();
    auto beta = (Value)command->beta();
    m_tree->start(plyDepth, alpha, beta);

    return PROCE_CONTINUE;
}

/**
 * @brief stopコマンドを処理します。
 *
 * stop
 */
int Client::proce_stop(CommandPacketPtr command)
{
    // すべての探索タスクを破棄します。
    //clear_search_task();

    return PROCE_ABORT;
}

/**
 * @brief notifyコマンドを処理します。
 *
 * notify <position_id> <itd> <pld> <value>
 */
int Client::proce_notify(CommandPacketPtr command)
{
    auto positionId = command->position_id();
    auto iterationDepth = command->iteration_depth();
    auto plyDepth = command->ply_depth();
    auto value = (Value)command->alpha();

    if (positionId != m_tree->position_id()) {
        LOG_ERROR() << "position_idが一致しません。";
        return PROCE_ABORT;
    }

    if (iterationDepth != m_tree->iteration_depth()) {
        LOG_ERROR() << "事前にPVを設定する必要があります。";
        return PROCE_ABORT;
    }

    LOG_NOTIFICATION() << F("notify: pid=%1%, itd=%2%, pld=%3%, value=%4%")
                        % positionId % iterationDepth % plyDepth % value;

    m_tree->notify(plyDepth, value);

    // 指定の段の評価地を更新します。
    return PROCE_CONTINUE;
}

/**
 * @brief commitコマンドを処理します。
 *
 * commit <position_id> <itd> <pld>
 */
int Client::proce_commit(CommandPacketPtr command)
{
    auto positionId = command->position_id();
    auto iterationDepth = command->iteration_depth();
    auto plyDepth = command->ply_depth();
    auto value = (Value)command->alpha();

    if (positionId != m_tree->position_id()) {
        LOG_ERROR() << "position_idが一致しません。";
        return PROCE_ABORT;
    }

    if (iterationDepth != m_tree->iteration_depth()) {
        LOG_ERROR() << "事前にPVを設定する必要があります。";
        return PROCE_ABORT;
    }

    LOG_NOTIFICATION() << F("commit: pid=%1%, itd=%2%, pld=%3%")
                        % positionId % iterationDepth % plyDepth;

    // plyDepth段の計算が完了したことを通知します。
    m_tree->commit(plyDepth);

    // 指定の段の評価地を更新します。
    return PROCE_CONTINUE;
}

/**
 * @brief quitコマンドを処理します。
 *
 * quit
 */
int Client::proce_quit(CommandPacketPtr command)
{
    // クライアントを停止します。
    close();

    return PROCE_ABORT;
}

} // namespace client
} // namespace godwhale
