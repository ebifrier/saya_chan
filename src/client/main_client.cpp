
#include "precomp.h"
#include "stdinc.h"
#include <boost/program_options.hpp>

#include "saya_chan/search.h"
#include "saya_chan/thread.h"
#include "client.h"

using namespace godwhale;
using namespace godwhale::client;
namespace opt = boost::program_options;

static std::string server_address;
static std::string server_port;
static std::string login_name;
static int thread_count;
static int hash_size;

/**
 * @brief プログラム引数のパースを行います。
 */
static bool parse_options(int argc, char *argv[])
{
    opt::options_description desc(
        "godwhale_child [options] <address> <port> <name>");
    desc.add_options()
        ("help,h", "ヘルプを表示")
        /*("addr,d", opt::value<std::string>(), "サーバーアドレス")
        ("port,p", opt::value<std::string>(), "サーバーポート")
        ("name,n", opt::value<std::string>(), "ログイン名")*/
        ("threads,t", opt::value<int>(&thread_count)->default_value(2), "探索スレッド数")
        ("hash,h",    opt::value<int>(&hash_size)->default_value(100),  "ハッシュサイズ[MB]");

    try {
        opt::variables_map values;
        auto parsed = opt::parse_command_line(argc, argv, desc);
        opt::store(parsed, values);

        // valuesに値を格納
        opt::notify(values);

        // helpの表示
        if (values.empty() || values.count("help") != 0) {
            std::cout << desc << std::endl;
            return false;
        }

        // 必須オプションが足りない場合
        auto unrecognized = opt::collect_unrecognized(parsed.options,
                                                      opt::include_positional);
        if (unrecognized.size() < 3) {
            std::cout << desc << std::endl;
            return false;
        }

        server_address = unrecognized[0];
        server_port = unrecognized[1];
        login_name = unrecognized[2];
        return true;
    }
    catch (std::exception & ex) {
        std::cout << ex.what() << std::endl;
        return false;
    }
}

int main(int argc, char *argv[])
{
    if (!parse_options(argc, argv)) {
        return 1;
    }

    // Startup initializations
    init_log();
    init_application_once();
    Position::init();
    init_search();
    Threads.init();

    // スレッド数とハッシュサイズを初期化します。
    init_search_godwhale(thread_count, hash_size);

    try {
        Client::init();

        // クジラちゃんサーバーへの接続処理を行います。
        Client::instance()->connect(server_address, server_port);
        Client::instance()->login(login_name/*, thread_count, hash_size*/);

        // 接続処理が終わったら、思考を開始します。
        return Client::instance()->run();
    }
    catch (std::exception & ex) {
        LOG_CRITICAL() << "throw exception: " << ex.what();
    }
}
