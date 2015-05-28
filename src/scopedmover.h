#ifndef GODWHALE_SCOPEDMOVER_H
#define GODWHALE_SCOPEDMOVER_H

#include "saya_chan/position.h"

namespace godwhale {

/**
 * @brief スコープ内でのみ局面を変化させます。
 *
 * このオブジェクトの破棄時に局面は元に戻されます。
 */
template<int MaxMoves = 128>
class ScopedMover
{
public:
    explicit ScopedMover(Position & pos)
        : m_position(pos), m_index(0) {
    }

    ~ScopedMover() {
        rewind();
    }

    /**
     * @brief 局面を元に戻します。
     */
    void rewind() {
        while (m_index > 0) {
            undo_move();
        }
    }

    /**
     * @brief 指し手を一つ進めます。
     */
    bool do_move(Move move) {
        if (m_index >= MaxMoves) {
            LOG_ERROR() << "move count exceeds " << MaxMoves;
            return false;
        }

        if (!m_position.pl_move_is_legal(move)) {
            LOG_ERROR() << "move is illegal '" << move_to_kif(move) << "'";
            return false;
        }

        // 念のためStateInfoは初期化してから渡す。
        memset(&m_ss[m_index], 0, sizeof(StateInfo));

        m_position.do_move(move, m_ss[m_index]);
        m_moves[m_index] = move;
        ++m_index;
        return true;
    }

    /**
     * @brief SFEN形式で指定された指し手を一つ進めます。
     */
    bool do_move_sfen(std::string const & sfen) {
        if (sfen.empty()) {
            return false;
        }

        auto move = move_from_uci(m_position, sfen);
        if (move == MOVE_NONE) {
            return false;
        }

        return do_move(move);
    }

    /**
     * @brief 指し手を一つ戻します。
     */
    void undo_move() {
        if (m_index <= 0) {
            LOG_ERROR() << "couldn't undo move";
            return;
        }

        --m_index;
        m_position.undo_move(m_moves[m_index]);
        m_moves[m_index] = MOVE_NONE;
    }

private:
    Position & m_position;
    Move m_moves[MaxMoves];
    StateInfo m_ss[MaxMoves];
    int m_index;
};

} // namespace godwhale

#endif
