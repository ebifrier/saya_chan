#include "precomp.h"
#include "stdinc.h"
#include "commandpacket.h"

namespace godwhale {

CommandPacket::CommandPacket(CommandType type)
    : m_type(type), m_positionId(-1)
{
}

CommandPacket::CommandPacket(CommandPacket const & other)
    : m_type(other.m_type)
    , m_positionId(other.m_positionId)
    , m_prevPositionId(other.m_prevPositionId)
    , m_iterationDepth(other.m_iterationDepth)
    , m_plyDepth(other.m_plyDepth)
    , m_alpha(other.m_alpha)
    , m_beta(other.m_beta)
    , m_positionSFEN(other.m_positionSFEN)
    , m_moveSFEN(other.m_moveSFEN)
    , m_movesSFEN(other.m_movesSFEN)
{
}

CommandPacket::CommandPacket(CommandPacket && other)
    : m_type(std::move(other.m_type))
    , m_positionId(std::move(other.m_positionId))
    , m_prevPositionId(std::move(other.m_prevPositionId))
    , m_iterationDepth(std::move(other.m_iterationDepth))
    , m_plyDepth(std::move(other.m_plyDepth))
    , m_alpha(std::move(other.m_alpha))
    , m_beta(std::move(other.m_beta))
    , m_positionSFEN(std::move(other.m_positionSFEN))
    , m_moveSFEN(std::move(other.m_moveSFEN))
    , m_movesSFEN(std::move(other.m_movesSFEN))
{
}

/**
 * @brief コマンドの実行優先順位を取得します。
 *
 * 値が大きい方が、優先順位は高いです。
 */
int CommandPacket::priority() const
{
    switch (m_type) {
    // 終了系のコマンドはすぐに実行
    case COMMAND_QUIT:
    case COMMAND_STOP:
        return 100;

    // 通常のコマンドはそのままの順で
    case COMMAND_SETPOSITION:
    case COMMAND_MAKEMOVEROOT:
    case COMMAND_SETPV:
    case COMMAND_SETMOVELIST:
    case COMMAND_START:
    case COMMAND_NOTIFY:
    case COMMAND_CANCEL:
    case COMMAND_COMMIT:
    case COMMAND_VERIFY:
    case COMMAND_LOGINRESULT:
        return 50;

    // エラーみたいなもの
    case COMMAND_NONE:
        return 0;
    }

    unreachable();
    return -1;
}

/**
 * @brief RSI(remote shogi interface)をパースし、コマンドに直します。
 */
CommandPacketPtr CommandPacket::parse(std::string const & rsi)
{
    if (rsi.empty()) {
        throw new std::invalid_argument("rsi");
    }

    auto tokens = create_tokenizer(rsi);
    auto token = *tokens.begin();

    if (is_token(token, "setposition")) {
        return parse_setposition(rsi, tokens);
    }
    else if (is_token(token, "makemoveroot")) {
        return parse_makemoveroot(rsi, tokens);
    }
    else if (is_token(token, "setpv")) {
        return parse_setpv(rsi, tokens);
    }
    else if (is_token(token, "setmovelist")) {
        return parse_setmovelist(rsi, tokens);
    }
    else if (is_token(token, "start")) {
        return parse_start(rsi, tokens);
    }
    else if (is_token(token, "stop")) {
        return parse_stop(rsi, tokens);
    }
    else if (is_token(token, "notify")) {
        return parse_notify(rsi, tokens);
    }
    else if (is_token(token, "cancel")) {
        return parse_cancel(rsi, tokens);
    }
    else if (is_token(token, "commit")) {
        return parse_commit(rsi, tokens);
    }
    else if (is_token(token, "quit")) {
        return parse_quit(rsi, tokens);
    }

    return CommandPacketPtr();
}

/**
 * @brief コマンドをRSI(remote shogi interface)に変換します。
 */
std::string CommandPacket::toRSI() const
{
    assert(m_type != COMMAND_NONE);

    switch (m_type) {
    case COMMAND_SETPOSITION:
        return toRSI_setposition();
    case COMMAND_MAKEMOVEROOT:
        return toRSI_makemoveroot();
    case COMMAND_SETPV:
        return toRSI_setpv();
    case COMMAND_SETMOVELIST:
        return toRSI_setmovelist();
    case COMMAND_START:
        return toRSI_start();
    case COMMAND_STOP:
        return toRSI_stop();
    case COMMAND_NOTIFY:
        return toRSI_notify();
    case COMMAND_CANCEL:
        return toRSI_cancel();
    case COMMAND_COMMIT:
        return toRSI_commit();
    case COMMAND_QUIT:
        return toRSI_quit();
    default:
        unreachable();
        break;
    }

    return std::string();
}


#pragma region SetPosition
/**
 * @brief setpositionコマンドをパースします。
 *
 * setposition <position_id> [sfen <sfen> | startpos] moves <move1> ... <moveN>
 * <sfen> = <board_sfen> <turn_sfen> <hand_sfen> <nmoves>
 */
CommandPacketPtr CommandPacket::parse_setposition(std::string const & rsi,
                                                  Tokenizer & tokens)
{
    auto begin = ++tokens.begin();
    std::string positionSFEN;
    std::vector<std::string> movesSFEN;
    int positionId = std::stoi(*begin++);

    std::string token = *begin++;
    if (token == "sfen") {
        std::string sfen;
        sfen += *begin++ + " "; // board
        sfen += *begin++ + " "; // turn
        sfen += *begin++;       // hand
        positionSFEN = sfen;
    }
    else if (token == "startpos") {
        positionSFEN = INITIAL_SFEN;
    }

    // movesはないことがあります。
    if (begin == tokens.end()) {
        return create_setposition(positionId, positionSFEN);
    }

    if (*begin++ != "moves") {
        throw ParseException(F("%1%: 指し手が正しくありません。") % rsi);
    }

    for (; begin != tokens.end(); ++begin) {
        movesSFEN.push_back(*begin);
    }

    return create_setposition(positionId, positionSFEN);
}

/**
 * @brief setpositionコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_setposition(int positionId,
                                                   std::string const & positionSFEN/*,
                                                   std::vector<std::string> const & movesSFEN*/)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_SETPOSITION));

    result->m_positionId = positionId;
    result->m_positionSFEN = positionSFEN;
    //result->m_movesSFEN = movesSFEN;

    return result;
}

/**
 * @brief setpositionコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_setposition() const
{
    std::string posStr;
    if (m_positionSFEN == "startpos" ||
        m_positionSFEN == INITIAL_SFEN) {
        posStr = "startpos";
    }
    else {
        posStr  = "sfen ";
        posStr += m_positionSFEN;
    }

    std::string movesStr =
        m_movesSFEN.empty() ?
        std::string() :
        " moves " + boost::join(m_movesSFEN, " ");

    return (F("setposition %1% %2%%3%")
        % m_positionId % posStr % movesStr)
        .str();
}
#pragma endregion


#pragma region MakeRootMove
/**
 * @brief makemoverootコマンドをパースします。
 *
 * makemoveroot <position_id> <old_position_id> <move>
 */
CommandPacketPtr CommandPacket::parse_makemoveroot(std::string const & rsi,
                                                   Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int oldPositionId = std::stoi(*begin++);
    std::string moveSFEN = *begin++;

    return create_makemoveroot(positionId, oldPositionId, moveSFEN);
}

/**
 * @brief makemoverootコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_makemoveroot(int nextPositionId,
                                                    int prevPositionId,
                                                    std::string const & moveSFEN)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_MAKEMOVEROOT));

    result->m_positionId = nextPositionId;
    result->m_prevPositionId = prevPositionId;
    result->m_moveSFEN = moveSFEN;
    return result;
}

/**
 * @brief makerootmoveコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_makemoveroot() const
{
    return (F("makemoveroot %1% %2% %3%")
        % m_positionId % m_prevPositionId % m_moveSFEN)
        .str();
}
#pragma endregion


#pragma region SetPV
/**
 * @brief setpvコマンドをパースします。
 *
 * setpv <position_id> <itd> <move1> ... <moveN>
 */
CommandPacketPtr CommandPacket::parse_setpv(std::string const & rsi,
                                            Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    std::vector<std::string> movesSFEN(begin, tokens.end());

    return create_setpv(positionId, iterationDepth, movesSFEN);
}

/**
 * @brief setpvコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_setpv(int positionId, int iterationDepth,
                                             std::vector<std::string> const & pvSFEN)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_SETPV));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_movesSFEN = pvSFEN;
    return result;
}

/**
 * @brief setpvコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_setpv() const
{
    std::string movesStr = boost::join(m_movesSFEN, " ");

    return (F("setpv %1% %2% %3%")
        % m_positionId % m_iterationDepth % movesStr)
        .str();
}
#pragma endregion


#pragma region SetMoveList
/**
 * @brief setmovelistコマンドをパースします。
 *
 * setmovelist <position_id> <itd> <pld> <move1> ... <moveN>
 */
CommandPacketPtr CommandPacket::parse_setmovelist(std::string const & rsi,
                                                  Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    int plyDepth = std::stoi(*begin++);
    std::vector<std::string> movesSFEN(begin, tokens.end());

    return create_setmovelist(positionId, iterationDepth, plyDepth, movesSFEN);
}

/**
 * @brief setmovelistコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_setmovelist(int positionId,
                                                   int iterationDepth,
                                                   int plyDepth,
                                                   std::vector<std::string> const & movesSFEN)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_SETMOVELIST));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;
    result->m_movesSFEN = movesSFEN;

    return result;
}

/**
 * @brief setmovelistコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_setmovelist() const
{
    std::string movesStr = boost::join(m_movesSFEN, " ");

    return (F("setmovelist %1% %2% %3% %4%")
        % m_positionId % m_iterationDepth % m_plyDepth % movesStr)
        .str();
}
#pragma endregion


#pragma region Start
/**
 * @brief startコマンドをパースします。
 *
 * start <position_id> <itd> <pld> <alpha> <beta>
 */
CommandPacketPtr CommandPacket::parse_start(std::string const & rsi,
                                            Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    int plyDepth = std::stoi(*begin++);
    Value alpha = (Value)std::stoi(*begin++);
    Value beta = (Value)std::stoi(*begin++);

    return create_start(positionId, iterationDepth, plyDepth, alpha, beta);
}

/**
 * @brief startコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_start(int positionId, int iterationDepth,
                                             int plyDepth, Value alpha, Value beta)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_START));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;
    result->m_alpha = alpha;
    result->m_beta = beta;

    return result;
}

/**
 * @brief startコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_start() const
{
    return (F("start %1% %2% %3% %4% %5%")
        % m_positionId %m_iterationDepth % m_plyDepth % m_alpha % m_beta)
        .str();
}
#pragma endregion


#pragma region Stop
/**
 * @brief stopコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_stop()
{
    return CommandPacketPtr(new CommandPacket(COMMAND_STOP));
}

/**
 * @brief stopコマンドをパースします。
 */
CommandPacketPtr CommandPacket::parse_stop(std::string const & rsi,
                                           Tokenizer & tokens)
{
    return create_stop();
}

/**
 * @brief stopコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_stop() const
{
    return "stop";
}
#pragma endregion


#pragma region Notify
/**
 * @brief notifyコマンドをパースします。
 *
 * notify <position_id> <itd> <pld> <value>
 */
CommandPacketPtr CommandPacket::parse_notify(std::string const & rsi,
                                             Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    int plyDepth = std::stoi(*begin++);
    Value value = (Value)std::stoi(*begin++);

    return create_notify(positionId, iterationDepth, plyDepth, value);
}

/**
 * @brief notifyコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_notify(int positionId, int iterationDepth,
                                              int plyDepth, Value value)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_NOTIFY));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;
    result->m_alpha = value;

    return result;
}

/**
 * @brief notifyコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_notify() const
{
    return (F("notify %1% %2% %3% %4%")
        % m_positionId % m_iterationDepth % m_plyDepth % m_alpha)
        .str();
}
#pragma endregion


#pragma region Cancel
/**
 * @brief cancelコマンドをパースします。
 */
CommandPacketPtr CommandPacket::parse_cancel(std::string const & rsi,
                                             Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    int plyDepth = std::stoi(*begin++);

    return create_cancel(positionId, iterationDepth, plyDepth);
}

/**
 * @brief cancelコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_cancel(int positionId, int iterationDepth,
                                              int plyDepth)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_CANCEL));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;

    return result;
}

/**
 * @brief cancelコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_cancel() const
{
    return (F("cancel %1% %2% %3%")
        % m_positionId %m_iterationDepth % m_plyDepth)
        .str();
}
#pragma endregion


#pragma region Commit
/**
 * @brief commitコマンドをパースします。
 */
CommandPacketPtr CommandPacket::parse_commit(std::string const & rsi,
                                             Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    int plyDepth = std::stoi(*begin++);

    return create_commit(positionId, iterationDepth, plyDepth);
}

/**
 * @brief commitコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_commit(int positionId, int iterationDepth,
                                              int plyDepth)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_COMMIT));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;

    return result;
}

/**
 * @brief commitコマンドを作成します。
 */
std::string CommandPacket::toRSI_commit() const
{
    return (F("commit %1% %2% %3%")
        % m_positionId %m_iterationDepth % m_plyDepth)
        .str();
}
#pragma endregion


#pragma region Verify
/**
 * @brief verifyコマンドをパースします。
 */
CommandPacketPtr CommandPacket::parse_verify(std::string const & rsi,
                                             Tokenizer & tokens)
{
    auto begin = ++tokens.begin(); // 最初のトークンは飛ばします。

    int positionId = std::stoi(*begin++);
    int iterationDepth = std::stoi(*begin++);
    int plyDepth = std::stoi(*begin++);

    std::vector<ValueSet> values;
    while (begin != tokens.end()) {
        ValueSet vset;
        vset.value = (Value)std::stoi(*begin++);
        vset.alpha = (Value)std::stoi(*begin++);
        vset.beta = (Value)std::stoi(*begin++);
        vset.gamma = (Value)std::stoi(*begin++);
        values.push_back(vset);
    }

    return create_verify(positionId, iterationDepth, plyDepth, values);
}

/**
 * @brief verifyコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_verify(int positionId, int iterationDepth,
                                              int plyDepth,
                                              std::vector<ValueSet> const & values)
{
    CommandPacketPtr result(new CommandPacket(COMMAND_VERIFY));

    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;
    result->m_valuesSet = values;

    return result;
}

/**
 * @brief verifyコマンドを作成します。
 */
std::string CommandPacket::toRSI_verify() const
{
    std::vector<std::string> strs;

    for (auto & vset : m_valuesSet) {
        strs.push_back((F("%1% %2% %3% %4%")
                        % vset.value % vset.alpha % vset.beta % vset.gamma)
                       .str());
    }

    return (F("verify %1% %2% %3%%4%")
        % m_positionId %m_iterationDepth % m_plyDepth
        % boost::join(strs, ""))
        .str();
}
#pragma endregion


#pragma region Quit
/**
 * @brief quitコマンドをパースします。
 */
CommandPacketPtr CommandPacket::parse_quit(std::string const & rsi,
                                           Tokenizer & tokens)
{
    return create_quit();
}

/**
 * @brief quitコマンドを作成します。
 */
CommandPacketPtr CommandPacket::create_quit()
{
    return CommandPacketPtr(new CommandPacket(COMMAND_QUIT));
}

/**
 * @brief quitコマンドをRSIに変換します。
 */
std::string CommandPacket::toRSI_quit() const
{
    return "quit";
}
#pragma endregion

} // namespace godwhale
