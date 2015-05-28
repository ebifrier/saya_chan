#ifndef GODWHALE_STDINC_H
#define GODWHALE_STDINC_H

#include "precomp.h"
#include "saya_chan/types.h"

#define LOCK_IMPL(obj, lock, flag)       \
    if (bool flag = false) {} else       \
    for (ScopedLock lock(obj); !flag; flag = true)

/** オリジナルのロック定義 */
#define LOCK(obj) \
    LOCK_IMPL(obj, BOOST_PP_CAT(lock_, __LINE__), BOOST_PP_CAT(flag_, __LINE__))

#if defined(_MSC_VER)
#define unreachable()  __assume(false); assert(false)
#elif defined(__GNUC__)
#define unreachable()  __builtin_unreachable(); assert(false)
#else
#define unreachable()  assert(false)
#endif

#define F(fmt)         ::boost::format(fmt)
#define INITIAL_SFEN   "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
#define SQ             make_square

namespace godwhale {

using std::shared_ptr;
using std::make_shared;
using std::weak_ptr;
using std::enable_shared_from_this;

typedef std::recursive_mutex Mutex;
typedef std::lock_guard<Mutex> ScopedLock;

typedef boost::asio::ip::tcp tcp;

class CommandPacket;
typedef shared_ptr<CommandPacket> CommandPacketPtr;

class ReplyPacket;
typedef shared_ptr<ReplyPacket> ReplyPacketPtr;

///
/// @brief 何もしないdeleterです。
///
struct empty_deleter
{
    typedef void result_type;
    void operator() (const volatile void*) const {}
};

/**
 * @brief bonanzaの探索時に使います。
 */
enum
{
    PROCE_OK = 0,
    PROCE_ABORT = 1,
    PROCE_CONTINUE = 2,
};

/**
 * @brief 評価値の種類を示します。
 */
enum ULEType
{
    /// 特になし？
    ULE_NONE  = 0,
    /// 数字が評価値の下限値であることを示す。
    ULE_LOWER = 1,
    /// 数字が評価値の上限値であることを示す。
    ULE_UPPER = 2,
    /// 数字が評価値そのものであることを示す。
    ULE_EXACT = 3,
};

/**
 * @brief 値の種類を示します。
 */
enum ValueType
{
    VALUETYPE_ALPHA,
    VALUETYPE_BETA,
    VALUETYPE_GAMMA,
};

} // namespace godwhale

#include "logger.h"
#include "util.h"
#include "exceptions.h"

#endif
