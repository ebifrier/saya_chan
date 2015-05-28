
#include <vector>
#include <iostream>
#include <sstream>

#include "position.h"

namespace {

    static const std::vector<std::string> PieceStrTable = {
        "〇", "歩", "香", "桂", "銀", "金", "角", "飛",
        "玉", "と", "杏", "圭", "全", "×", "馬", "龍"
    };

    static const std::vector<std::string> PieceGaijiStrTable = {
        "\uE000", "\uE001", "\uE002", "\uE003",
        "\uE004", "\uE005", "\uE006", "\uE007",
        "\uE008", "\uE009", "\uE00A", "\uE00B",
        "\uE00C", "\uE00D", "\uE00E", "\uE00F"
    };

    static const std::vector<std::string> KanjiNumberTable =
    {
        "零", "一", "二", "三", "四",
        "五", "六", "七", "八", "九"
    };

    static void print_piece(std::ostream &os, Piece piece, Square to, bool useGaiji);
    static void print_hand(std::ostream &os, Hand const & hand);
    static void print_hand0(std::ostream &os, int n, const std::string &str);

    /**
     * @brief 局面を出力します。
     */
    static std::string position_to_kif_internal(Position const & pos, Move last,
                                                bool useGaiji)
    {
        const Square to = move_to(last);
        const Square from = move_from(last);
        const bool promote = is_promotion(last);
        std::ostringstream os;

        os << "後手の持駒：";
        print_hand(os, pos.hand_of(WHITE));
        os << std::endl;
        os << "  ９ ８ ７ ６ ５ ４ ３ ２ １" << std::endl;
        os << "+---------------------------+" << std::endl;

        for (Rank rank = RANK_1; rank <= RANK_9; rank++) {
            os << "|";

            for (File file = FILE_9; file >= FILE_1; file--) {
                Square sq = make_square(file, rank);

                print_piece(os, pos.piece_on(sq), to, useGaiji);
            }

            os << "|" << KanjiNumberTable[rank] << std::endl;
        }

        os << "+---------------------------+" << std::endl;
        os << "先手の持駒：";
        print_hand(os, pos.hand_of(BLACK));

        if (pos.side_to_move() == WHITE) os << std::endl << "後手番";

        return os.str();
    }

    /**
     * @brief 駒を一つ出力します。
     */
    static void print_piece(std::ostream &os, Piece piece, Square to, bool useGaiji)
    {
        if (piece != EMP) {
            if (useGaiji) {
                auto str = (
                    color_of(piece) == BLACK ?
                    PieceStrTable[type_of(piece)] :
                    PieceGaijiStrTable[type_of(piece)]);
                os << " " << str;
            }
            else {
                char ch = (color_of(piece) == BLACK ? ' ' : 'v');
                os << ch << PieceStrTable[type_of(piece)];
            }
        }
        else {
            os << " ・";
        }
    }

    /**
     * @brief 持ち駒を出力します。
     */
    static void print_hand(std::ostream &os, Hand const & hand)
    {
        if (hand.Empty()) {
            os << "なし";
        }
        else {
            print_hand0(os, hand.getHI(), "飛");
            print_hand0(os, hand.getKA(), "角");
            print_hand0(os, hand.getKI(), "金");
            print_hand0(os, hand.getGI(), "銀");
            print_hand0(os, hand.getKE(), "桂");
            print_hand0(os, hand.getKY(), "香");
            print_hand0(os, hand.getFU(), "歩");
        }
    }

    /**
     * @brief 持ち駒を１つだけ出力します。
     */
    static void print_hand0(std::ostream &os, int n, const std::string &str)
    {
        if (n > 0) {
            os << str;

            if (n > 1) {
                if (n >= 10) {
                    os << "十";
                    n %= 10;
                }

                if (n > 0) {
                    os << KanjiNumberTable[n];
                }
            }

            os << "　";
        }
    }
}

/**
 * @brief KIF形式の局面を出力します。
 */
std::string position_to_kif(Position const & pos)
{
    return position_to_kif_internal(pos, MOVE_NONE, false);
}

/**
 * @brief 外字を使った局面を出力します。
 */
std::string position_to_kif_ex(Position const & pos, Move last/*= MOVE_NONE*/)
{
    return position_to_kif_internal(pos, last, true);
}
