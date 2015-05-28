
#include "precomp.h"
#include "stdinc.h"

#include "saya_chan/thread.h"
#include "saya_chan/ucioption.h"
#include "saya_chan/tt.h"
#include "util.h"

namespace godwhale {

static const CharSeparator s_separator(" ", "");

void init_search_godwhale(int threads, int hashSize)
{
    // スレッド数やハッシュ値を設定します。
    Options["Threads"].set_value(std::to_string(threads));
    Options["Hash"].set_value(std::to_string(hashSize));

    Options["OwnBook"].set_value(
#if defined(GODWHALE_SERVER)
        "true"
#else
        "false"
#endif
        );

    // Read the thread options
    //Threads.read_uci_options();

    // Set a new TT size if changed
    //TT.set_size(Options["Hash"].value<int>());
}

/**
 * @brief 指定の㍉秒だけスリープします。
 */ 
void sleep(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

Tokenizer create_tokenizer(std::string const & text)
{
    return Tokenizer(text, s_separator);
}

///
/// @brief strがtargetが示すトークンであるか調べます。
///
bool is_token(std::string const & str, std::string const & target)
{
    return (str.compare(target) == 0);
}

/**
 * @brief 探索深さを計算します。
 *
 * イテレーション深さと与えられたルートからの手の数によって、
 * 探索深さを決定します。
 */
Depth search_depth(int iterationDepth, int plyDepth)
{
    return (iterationDepth * 2 - plyDepth) * ONE_PLY / 2;
}

/**
 * @brief 評価値の更新を行います。
 */
ULEType detect_value_type(Value value, Value lower, Value upper)
{    
    if (value <= lower) {
        return ULE_UPPER;
    }
    else if (value >= upper) {
        return ULE_LOWER;
        //value = upper;
    }
    else { // lower < value && value < upper
        return ULE_EXACT;
    }
}

} // namespace godwhale
