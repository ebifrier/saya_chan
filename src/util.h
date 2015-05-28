#ifndef GODWHALE_UTIL_H
#define GODWHALE_UTIL_H

#include "saya_chan/position.h"

namespace godwhale {

typedef boost::char_separator<char> CharSeparator;
typedef boost::tokenizer<CharSeparator> Tokenizer;

extern void init_search_godwhale(int threads, int hashSize);
extern void sleep(int milliseconds);

extern Tokenizer create_tokenizer(std::string const & text);
extern bool is_token(std::string const & str, std::string const & target);
extern Depth search_depth(int iterationDepth, int plyDepth);
extern ULEType detect_value_type(Value value, Value lower, Value upper);

/**
 * @brief SFEN形式の連続した指し手文字列を、実際の指し手に変換します。
 */
template<class Sequence>
std::vector<Move> move_list_from_uci(const Position& position,
                                     std::string const & sfen,
                                     Sequence & seq)
{
    auto begin = std::begin(seq);
    auto end = std::end(seq);

    if (sfen.empty()) {
        return move_list_from_uci(position, begin, end);
    }
    else {
        std::vector<std::string> list;

        list.push_back(sfen);
        list.insert(list.end(), begin, end);
        return move_list_from_uci(position, list.begin(), list.end());
    }
}

} // namespace godwhale

/**
 * @brief デフォルトでは指し手をKIF形式で出力します。
 */
template<typename Elem, typename Traits>
inline std::basic_ostream<Elem, Traits> &operator<<(std::basic_ostream<Elem, Traits> &os,
                                                    Move move)
{
    os << move_to_kif(move);
    return os;
}

/**
 * @brief デフォルトでは局面をKIF形式で出力します。
 */
template<typename Elem, typename Traits>
inline std::basic_ostream<Elem, Traits> &operator<<(std::basic_ostream<Elem, Traits> &os,
                                                    Position const & pos)
{
    os << position_to_kif(pos);
    return os;
}

#endif
