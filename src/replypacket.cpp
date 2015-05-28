#include "precomp.h"
#include "stdinc.h"
#include "replypacket.h"

namespace godwhale {

ReplyPacket::ReplyPacket(ReplyType type)
    : m_type(type), m_positionId(-1), m_iterationDepth(-1), m_plyDepth(-1)
    , m_value(-VALUE_INFINITE), m_alpha(-VALUE_INFINITE)
    , m_beta(VALUE_INFINITE), m_nodes(0), m_benchResult(0), m_hashSize(0)
{
}

ReplyPacket::ReplyPacket(ReplyPacket & other)
    : m_type(other.m_type)
    , m_positionId(other.m_positionId)
    , m_iterationDepth(other.m_iterationDepth)
    , m_plyDepth(other.m_plyDepth)
    , m_value(other.m_value)
    , m_alpha(other.m_alpha)
    , m_beta(other.m_beta)
    , m_nodes(other.m_nodes)
    , m_loginName(other.m_loginName)
    , m_benchResult(other.m_benchResult)
    , m_hashSize(other.m_hashSize)
    , m_moveSFEN(other.m_moveSFEN)
    , m_movesSFEN(other.m_movesSFEN)
{
}

ReplyPacket::ReplyPacket(ReplyPacket && other)
    : m_type(std::move(other.m_type))
    , m_positionId(std::move(other.m_positionId))
    , m_iterationDepth(std::move(other.m_iterationDepth))
    , m_plyDepth(std::move(other.m_plyDepth))
    , m_value(std::move(other.m_value))
    , m_alpha(std::move(other.m_alpha))
    , m_beta(std::move(other.m_beta))
    , m_nodes(std::move(other.m_nodes))
    , m_loginName(std::move(other.m_loginName))
    , m_benchResult(std::move(other.m_benchResult))
    , m_hashSize(std::move(other.m_hashSize))
    , m_moveSFEN(std::move(other.m_moveSFEN))
    , m_movesSFEN(std::move(other.m_movesSFEN))
{
}

///
/// @brief コマンドの実行優先順位を取得します。
///
/// 値が大きい方が、優先順位は高いです。
///
int ReplyPacket::priority() const
{
    switch (m_type) {
    // 終了系のコマンドはすぐに実行
    //case COMMAND_QUIT:
    //    return 100;

    // 通常のコマンドはそのままの順で
    case REPLY_LOGIN:
    case REPLY_UPDATEVALUE:
    case REPLY_SEARCHDONE:
        return 50;

    // エラーみたいなもの
    case REPLY_NONE:
        return 0;
    }

    unreachable();
    return -1;
}

///
/// @brief RSI(remote shogi interface)をパースし、コマンドに直します。
///
ReplyPacketPtr ReplyPacket::parse(std::string const & rsi)
{
    if (rsi.empty()) {
        throw new std::invalid_argument("rsi");
    }

    auto tokens = create_tokenizer(rsi);
    auto token = *tokens.begin();

    if (is_token(token, "login")) {
        return parse_login(rsi, tokens);
    }
    else if (is_token(token, "updatevalue")) {
        return parse_updatevalue(rsi, tokens);
    }
    else if (is_token(token, "searchdone")) {
        return parse_searchdone(rsi, tokens);
    }

    unreachable();
    return ReplyPacketPtr();
}

///
/// @brief コマンドをRSI(remote shogi interface)に変換します。
///
std::string ReplyPacket::toRSI() const
{
    assert(m_type != REPLY_NONE);

    switch (m_type) {
    case REPLY_LOGIN:
        return toRSI_login();
    case REPLY_UPDATEVALUE:
        return toRSI_updatevalue();
    case REPLY_SEARCHDONE:
        return toRSI_searchdone();
    }

    unreachable();
    return std::string();
}


#pragma region Login
///
/// @brief loginコマンドをパースします。
///
/// login <login_name> <bench_result> <hash_size>
///
ReplyPacketPtr ReplyPacket::parse_login(std::string const & rsi,
                                        Tokenizer & tokens)
{
    ReplyPacketPtr result(new ReplyPacket(REPLY_LOGIN));
    Tokenizer::iterator begin = ++tokens.begin();

    result->m_loginName = *begin++;
    result->m_benchResult = std::stoi(*begin++);
    result->m_hashSize = std::stoi(*begin++);
    return result;
}

///
/// @brief loginコマンドを作成します。
///
ReplyPacketPtr ReplyPacket::create_login(std::string const & name,
                                         int benchResult, int hashSize)
{
    ReplyPacketPtr result(new ReplyPacket(REPLY_LOGIN));

    result->m_loginName = name;
    result->m_benchResult = benchResult;
    result->m_hashSize = hashSize;
    return result;
}

///
/// @brief loginコマンドをRSIに変換します。
///
std::string ReplyPacket::toRSI_login() const
{
    return (F("login %1% %2% %3%")
        % m_loginName % m_benchResult % m_hashSize)
        .str();
}
#pragma endregion


#pragma region UpdateValue
/**
 * @brief updatevalue応答をパースします。
 *
 * updatevalue <position_id> <itd> <pld> <alpha> <beta> <pv>
 */
ReplyPacketPtr ReplyPacket::parse_updatevalue(std::string const & rsi,
                                              Tokenizer & tokens)
{
    ReplyPacketPtr result(new ReplyPacket(REPLY_UPDATEVALUE));
    auto begin = ++tokens.begin();

    result->m_positionId = std::stoi(*begin++);
    result->m_iterationDepth = std::stoi(*begin++);
    result->m_plyDepth = std::stoi(*begin++);
    result->m_moveSFEN = *begin++;
    result->m_value = (Value)std::stoi(*begin++);
    result->m_alpha = (Value)std::stoi(*begin++);
    result->m_beta = (Value)std::stoi(*begin++);
    result->m_nodes = std::stoll(*begin++);
    result->m_movesSFEN.assign(begin, tokens.end());
    return result;
}

/**
 * @brief updatevalue応答を作成します。
 */
ReplyPacketPtr ReplyPacket::create_updatevalue(int positionId, int iterationDepth,
                                               int plyDepth, Move move, Value value,
                                               Value alpha, Value beta, int64_t nodes,
                                               std::vector<Move> pv)
{
    if (move == MOVE_NONE) {
        LOG_ERROR() << "指し手が設定されていません。";
        return ReplyPacketPtr();
    }

    ReplyPacketPtr result(new ReplyPacket(REPLY_UPDATEVALUE));
    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;
    result->m_moveSFEN = move_to_uci(move);
    result->m_value = value;
    result->m_alpha = alpha;
    result->m_beta = beta;
    result->m_nodes = nodes;
    result->m_movesSFEN = move_list_to_sfen(pv.begin(), pv.end());
    return result;
}

/**
 * @brief updatevalue応答をRSIに変換します。
 */
std::string ReplyPacket::toRSI_updatevalue() const
{
    auto movesStr = boost::join(m_movesSFEN, " ");

    return (F("updatevalue %1% %2% %3% %4% %5% %6% %7% %8% %9%")
        % m_positionId % m_iterationDepth % m_plyDepth
        % m_moveSFEN % m_value % m_alpha % m_beta
        % m_nodes % movesStr)
        .str();
}
#pragma endregion


#pragma region searchdone
/**
 * @brief searchdone応答をパースします。
 *
 * searchdone <position_id> <itd> <pld>
 */
ReplyPacketPtr ReplyPacket::parse_searchdone(std::string const & rsi,
                                             Tokenizer & tokens)
{
    ReplyPacketPtr result(new ReplyPacket(REPLY_SEARCHDONE));
    auto begin = ++tokens.begin();

    result->m_positionId = std::stoi(*begin++);
    result->m_iterationDepth = std::stoi(*begin++);
    result->m_plyDepth = std::stoi(*begin++);
    return result;
}

/**
 * @brief searchdone応答を作成します。
 */
ReplyPacketPtr ReplyPacket::create_searchdone(int positionId, int iterationDepth,
                                              int plyDepth)
{
    ReplyPacketPtr result(new ReplyPacket(REPLY_SEARCHDONE));
    result->m_positionId = positionId;
    result->m_iterationDepth = iterationDepth;
    result->m_plyDepth = plyDepth;
    return result;
}

/**
 * @brief searchdone応答をRSIに変換します。
 */
std::string ReplyPacket::toRSI_searchdone() const
{
    return (F("searchdone %1% %2% %3%")
        % m_positionId % m_iterationDepth % m_plyDepth)
        .str();
}
#pragma endregion

} // namespace godwhale
