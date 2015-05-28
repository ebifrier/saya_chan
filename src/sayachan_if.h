
namespace godwhale {

#if defined(GODWHALE_SERVER)
namespace server {
    /*extern int CONV detect_signals_server();
    extern void CONV set_position_hook(min_posi_t const * posi);
    extern void CONV init_game_hook();
    extern void CONV quit_game_hook();
    extern void CONV make_move_root_hook(move_t move);
    extern void CONV unmake_move_root_hook();
    extern void CONV adjust_time_hook(int turn);
    extern int CONV server_iterate(tree_t *restrict ptree, int *value,
    move_t *pvseq, int *pvseq_length);*/
}
#endif

#if defined(GODWHALE_CLIENT)
namespace client {
    extern void poll_client();
    //extern void update_value(Move move, Value value, Value alpha, Value beta,
    //                         int64_t nodes, std::vector<Move> const & pv);
}
#endif

}
