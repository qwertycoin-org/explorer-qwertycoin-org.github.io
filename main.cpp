#define CROW_ENABLE_SSL
#define CROW_MAIN
#define CROW_USE_BOOST 1

#include "src/page.h"

#include "ext/crow_all.h"
#include "src/CmdLineOptions.h"
#include "src/MicroCore.h"
#include "src/SupplyIndex.h"
#include "src/wallet_rpc_policy.h"

#include <fstream>
#include <regex>
#include <atomic>

using boost::filesystem::path;
using xmreg::remove_bad_chars;

using namespace std;

namespace myxmr
{
struct htmlresponse: public crow::response
{
    htmlresponse(string&& _body)
            : crow::response {std::move(_body)}
    {
        add_header("Content-Type", "text/html; charset=utf-8");
        add_header("Cache-Control", "no-store");
        add_header("Referrer-Policy", "no-referrer");
        add_header("X-Content-Type-Options", "nosniff");
        add_header("X-Frame-Options", "DENY");
        add_header("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
        add_header("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'");
    }
};

struct jsonresponse: public crow::response
{
    jsonresponse(const nlohmann::json& _body)
            : crow::response {_body.dump()}
    {
        const auto status_it = _body.find("status");
        if (status_it != _body.end() && status_it->is_string())
        {
            const string status = status_it->get<string>();
            if (status == "fail")
                code = 400;
            else if (status == "error")
                code = 503;
        }
        add_header("Access-Control-Allow-Origin", "*");
        add_header("Access-Control-Allow-Headers", "Content-Type");
        add_header("Content-Type", "application/json");
        add_header("Cache-Control", "no-store");
        add_header("Referrer-Policy", "no-referrer");
        add_header("X-Content-Type-Options", "nosniff");
        add_header("X-Frame-Options", "DENY");
        add_header("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
    }
};
}

int
main(int ac, const char* av[])
{

    // get command line options
    xmreg::CmdLineOptions opts {ac, av};

    auto help_opt                      = opts.get_option<bool>("help");

    // if help was chosen, display help text and finish
    if (*help_opt)
    {
        return EXIT_SUCCESS;
    }

    auto port_opt                      = opts.get_option<string>("port");
    auto bindaddr_opt                  = opts.get_option<string>("bindaddr");
    auto bc_path_opt                   = opts.get_option<string>("bc-path");
    auto daemon_url_opt                = opts.get_option<string>("daemon-url");
    auto derived_data_path_opt         = opts.get_option<string>("derived-data-path");
    auto chain_reset_id_opt            = opts.get_option<string>("chain-reset-id");
    auto ssl_crt_file_opt              = opts.get_option<string>("ssl-crt-file");
    auto ssl_key_file_opt              = opts.get_option<string>("ssl-key-file");
    auto no_blocks_on_index_opt        = opts.get_option<string>("no-blocks-on-index");
    auto testnet_url                   = opts.get_option<string>("testnet-url");
    auto stagenet_url                  = opts.get_option<string>("stagenet-url");
    auto mainnet_url                   = opts.get_option<string>("mainnet-url");
    auto mempool_info_timeout_opt      = opts.get_option<string>("mempool-info-timeout");
    auto mempool_refresh_time_opt      = opts.get_option<string>("mempool-refresh-time");
    auto daemon_login_opt              = opts.get_option<string>("daemon-login");
    auto testnet_opt                   = opts.get_option<bool>("testnet");
    auto stagenet_opt                  = opts.get_option<bool>("stagenet");
    auto enable_autorefresh_option_opt = opts.get_option<bool>("enable-autorefresh-option");
    auto enable_pusher_opt             = opts.get_option<bool>("enable-pusher");
    auto enable_randomx_opt            = opts.get_option<bool>("enable-randomx");
    auto enable_mixin_details_opt      = opts.get_option<bool>("enable-mixin-details");
    auto enable_json_api_opt           = opts.get_option<bool>("enable-json-api");
    auto enable_as_hex_opt             = opts.get_option<bool>("enable-as-hex");
    auto enable_mixin_guess_opt        = opts.get_option<bool>("enable-mixin-guess");
    auto concurrency_opt               = opts.get_option<size_t>("concurrency");


    bool testnet                      {*testnet_opt};
    bool stagenet                     {*stagenet_opt};

    if (testnet && stagenet)
    {
        cerr << "testnet and stagenet cannot be specified at the same time!" << endl;
        return EXIT_FAILURE;
    }

    const cryptonote::network_type nettype = testnet ?
        cryptonote::network_type::TESTNET : stagenet ?
        cryptonote::network_type::STAGENET : cryptonote::network_type::MAINNET;

    bool enable_pusher                {*enable_pusher_opt};
    bool enable_randomx               {*enable_randomx_opt};
    bool enable_key_image_checker     {false};
    bool enable_autorefresh_option    {*enable_autorefresh_option_opt};
    bool enable_output_key_checker    {false};
    bool enable_mixin_details         {*enable_mixin_details_opt};
    bool enable_mixin_guess           {*enable_mixin_guess_opt};
    bool enable_json_api              {*enable_json_api_opt};
    bool enable_as_hex                {*enable_as_hex_opt};

    //temprorary disable randomx
    if (enable_randomx == true) {
        cout << "Support for randomx code is disabled due to issues with it"<< endl;
        enable_randomx = false;
    }

    // set  monero log output level
    uint32_t log_level = 0;
    mlog_configure("", true);

    (void) log_level;

    //cast port number in string to uint
    uint16_t app_port = boost::lexical_cast<uint16_t>(*port_opt);

    string bindaddr = *bindaddr_opt;

    // cast no_blocks_on_index_opt to uint
    uint64_t no_blocks_on_index = boost::lexical_cast<uint64_t>(*no_blocks_on_index_opt);

    bool use_ssl {false};

    string ssl_crt_file;
    string ssl_key_file;

    xmreg::rpccalls::login_opt daemon_rpc_login {};


    if (daemon_login_opt)
    {

       string user {};
       epee::wipeable_string pass {};

       string daemon_login = *daemon_login_opt;

       size_t colon_location = daemon_login.find_first_of(':');

       if (colon_location != std::string::npos)
       {
           // have colon for user:password
           user = daemon_login.substr(0, colon_location);
           pass  = daemon_login.substr(colon_location + 1);
       }
       else
       {
          user = *daemon_login_opt;
       }

       daemon_rpc_login = epee::net_utils::http::login {user, pass};

       //cout << "colon_location: " << colon_location << endl;
       // cout << "user: " << user << endl;
       // cout << "pass: " << std::string(pass.data(), pass.size()) << endl;
    }


    // check if ssl enabled and files exist

    if (ssl_crt_file_opt && ssl_key_file_opt)
    {
        if (!boost::filesystem::exists(boost::filesystem::path(*ssl_crt_file_opt)))
        {
            cerr << "ssl_crt_file path: " << *ssl_crt_file_opt
                 << "does not exist!" << endl;

            return EXIT_FAILURE;
        }

        if (!boost::filesystem::exists(boost::filesystem::path(*ssl_key_file_opt)))
        {
            cerr << "ssl_key_file path: " << *ssl_key_file_opt
                 << "does not exist!" << endl;

            return EXIT_FAILURE;
        }

        ssl_crt_file = *ssl_crt_file_opt;
        ssl_key_file = *ssl_key_file_opt;

        use_ssl = true;
    }



    // get blockchain path
    path blockchain_path;

    if (!xmreg::get_blockchain_path(bc_path_opt, blockchain_path, nettype))
    {
        cerr << "Error getting blockchain path." << endl;
        return EXIT_FAILURE;
    }

    // create instance of our MicroCore
    // and make pointer to the Blockchain
    xmreg::MicroCore mcore;
    cryptonote::Blockchain* core_storage;

    // initialize mcore and core_storage
    if (!xmreg::init_blockchain(blockchain_path.string(),
                               mcore, core_storage, nettype))
    {
        cerr << "Error accessing blockchain." << endl;
        return EXIT_FAILURE;
    }

    const bool valid_reset_id = chain_reset_id_opt && !chain_reset_id_opt->empty()
            && chain_reset_id_opt->size() <= 128
            && std::all_of(chain_reset_id_opt->begin(), chain_reset_id_opt->end(),
                    [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_' || c == '.'; });
    if (!daemon_url_opt || daemon_url_opt->empty()
        || !derived_data_path_opt || derived_data_path_opt->empty() || !valid_reset_id)
    {
        cerr << "--daemon-url, --derived-data-path and --chain-reset-id are required" << endl;
        return EXIT_FAILURE;
    }
    string daemon_url {*daemon_url_opt};

    xmreg::SupplyIndex supply_index {&mcore, core_storage,
                                     *derived_data_path_opt,
                                     *chain_reset_id_opt,
                                     nettype};
    if (!supply_index.start())
    {
        cerr << "Supply index failed to start" << endl;
        return EXIT_FAILURE;
    }

    uint64_t mempool_info_timeout {5000};

    try
    {
        mempool_info_timeout = boost::lexical_cast<uint64_t>(
                *mempool_info_timeout_opt);
    }
    catch (boost::bad_lexical_cast &e)
    {
        cout << "Cant cast " << (*mempool_info_timeout_opt)
             <<" into numbers. Using default values.\n";
    }

    uint64_t mempool_refresh_time {10};


    xmreg::MempoolStatus::blockchain_path
            = blockchain_path;
    xmreg::MempoolStatus::nettype
            = nettype;
    xmreg::MempoolStatus::daemon_url
            = daemon_url;
    xmreg::MempoolStatus::login
            = daemon_rpc_login;
    xmreg::MempoolStatus::set_blockchain_variables(
            &mcore, core_storage);

    xmreg::MempoolStatus::network_info initial_info;
    strcpy(initial_info.block_size_limit_str, "0.0");
    strcpy(initial_info.block_size_median_str, "0.0");
    xmreg::MempoolStatus::current_network_info = initial_info;

    try
    {
        mempool_refresh_time = boost::lexical_cast<uint64_t>(*mempool_refresh_time_opt);

    }
    catch (boost::bad_lexical_cast &e)
    {
        cout << "Cant cast " << (*mempool_refresh_time_opt)
             <<" into number. Using default value."
             << endl;
    }

    // launch the status monitoring thread so that it keeps track of blockchain
    // info, e.g., current height. Information from this thread is used
    // by tx searching threads that are launched for each user independently,
    // when they log back or create new account.
    xmreg::MempoolStatus::mempool_refresh_time = mempool_refresh_time;
    xmreg::MempoolStatus::start_mempool_status_thread();

    std::vector<std::unique_ptr<xmreg::rpccalls>> wallet_rpc_pool;
    for (unsigned i = 0; i < 4; ++i)
        wallet_rpc_pool.emplace_back(std::make_unique<xmreg::rpccalls>(
                daemon_url, daemon_rpc_login, 20000));
    std::atomic<unsigned> wallet_rpc_inflight {0};
    std::atomic<unsigned> wallet_rpc_next_slot {0};

    // create instance of page class which
    // contains logic for the website
    xmreg::page xmrblocks(&mcore,
                          core_storage,
                          &supply_index,
                          daemon_url,
                          nettype,
                          enable_pusher,
                          enable_randomx,
                          enable_as_hex,
                          enable_key_image_checker,
                          enable_output_key_checker,
                          enable_autorefresh_option,
                          enable_mixin_details,
                          enable_mixin_guess,
                          no_blocks_on_index,
                          mempool_info_timeout,
                          *testnet_url,
                          *stagenet_url,
                          *mainnet_url,
                          daemon_rpc_login);

    // crow instance
    crow::SimpleApp app;

    CROW_ROUTE(app, "/healthz")
    ([]() {
        return myxmr::jsonresponse{nlohmann::json{
                {"status", "success"}, {"data", {{"process", "running"}}}}};
    });

    CROW_ROUTE(app, "/readyz")
    ([&]() {
        const nlohmann::json identity = xmrblocks.json_identity();
        const nlohmann::json supply = xmrblocks.json_emission();
        if (identity.value("status", "error") != "success"
            || !identity.at("data").value("compatible", false)
            || supply.value("status", "error") != "success"
            || !supply.at("data").value("complete", false))
        {
            return myxmr::jsonresponse{nlohmann::json{
                    {"status", "error"},
                    {"message", "Observer identity, freshness, EPoSE v2 compatibility, or supply anchor is unverified"}}};
        }
        return myxmr::jsonresponse{nlohmann::json{
                {"status", "success"},
                {"data", {{"identity", identity.at("data")},
                          {"supply", supply.at("data")}}}}};
    });

    // get domian url based on the request
    auto get_domain = [&use_ssl](crow::request const& req) {
        return (use_ssl ? "https://" : "http://")
               + req.get_header_value("Host");
    };

    CROW_ROUTE(app, "/")
    ([&]() {
        return myxmr::htmlresponse(xmrblocks.index2());
    });

    CROW_ROUTE(app, "/page/<uint>")
    ([&](const crow::request& req, size_t page_no) {
        uint64_t block_count {0};
        const char* count = req.url_params.get("block_count");
        const char* hash = req.url_params.get("tip_hash");
        if (count && !xmreg::parse_uint64_strict(count, block_count))
            block_count = 0;
        return myxmr::htmlresponse(xmrblocks.index2(page_no, false, "overview",
                block_count, hash ? remove_bad_chars(hash) : string {}));
    });

    CROW_ROUTE(app, "/blocks")
    ([&]() { return myxmr::htmlresponse(xmrblocks.index2(0, false, "blocks")); });

    CROW_ROUTE(app, "/blocks/<uint>")
    ([&](const crow::request& req, size_t page_no) {
        uint64_t block_count {0};
        const char* count = req.url_params.get("block_count");
        const char* hash = req.url_params.get("tip_hash");
        if (count && !xmreg::parse_uint64_strict(count, block_count))
            block_count = 0;
        return myxmr::htmlresponse(xmrblocks.index2(page_no, false, "blocks",
                block_count, hash ? remove_bad_chars(hash) : string {}));
    });

    CROW_ROUTE(app, "/service-nodes")
    ([&]() { return myxmr::htmlresponse(xmrblocks.index2(0, false, "service-nodes")); });

    CROW_ROUTE(app, "/epochs")
    ([&]() { return myxmr::htmlresponse(xmrblocks.index2(0, false, "epochs")); });

    CROW_ROUTE(app, "/network")
    ([&]() { return myxmr::htmlresponse(xmrblocks.index2(0, false, "network")); });

    CROW_ROUTE(app, "/block/<uint>")
    ([&](size_t block_height) {
        return myxmr::htmlresponse(xmrblocks.show_block(block_height));
    });

    CROW_ROUTE(app, "/randomx/<uint>")
    ([&](size_t block_height) {
        return myxmr::htmlresponse(xmrblocks.show_randomx(block_height));
    });

    CROW_ROUTE(app, "/block/<string>")
    ([&](string block_hash) {
        return myxmr::htmlresponse(
                xmrblocks.show_block(remove_bad_chars(block_hash)));
    });

    CROW_ROUTE(app, "/tx/<string>")
    ([&](string tx_hash) {
        return myxmr::htmlresponse(
                xmrblocks.show_tx(remove_bad_chars(tx_hash)));
    });
    if (enable_autorefresh_option)
    {
        CROW_ROUTE(app, "/tx/<string>/autorefresh")
        ([&](string tx_hash) {
            bool refresh_page {true};
            uint16_t with_ring_signatures {0};
            return myxmr::htmlresponse(
                xmrblocks.show_tx(remove_bad_chars(tx_hash), with_ring_signatures, refresh_page));
        });
    }

    if (enable_as_hex)
    {
        CROW_ROUTE(app, "/txhex/<string>")
        ([&](string tx_hash) {
            return crow::response(
                    xmrblocks.show_tx_hex(remove_bad_chars(tx_hash)));
        });

        CROW_ROUTE(app, "/ringmembershex/<string>")
        ([&](string tx_hash) {
            return crow::response(
                    xmrblocks.show_ringmembers_hex(remove_bad_chars(tx_hash)));
        });

        CROW_ROUTE(app, "/blockhex/<uint>")
        ([&](size_t block_height) {
            return crow::response(
                    xmrblocks.show_block_hex(block_height, false));
        });

        CROW_ROUTE(app, "/blockhexcomplete/<uint>")
        ([&](size_t block_height) {
            return crow::response(
                    xmrblocks.show_block_hex(block_height, true));
        });

//        CROW_ROUTE(app, "/ringmemberstxhex/<string>")
//        ([&](string tx_hash) {
//            return crow::response(
//              xmrblocks.show_ringmemberstx_hex(remove_bad_chars(tx_hash)));
//        });

        CROW_ROUTE(app, "/ringmemberstxhex/<string>")
        ([&](string tx_hash) {
            return myxmr::jsonresponse {
                xmrblocks.show_ringmemberstx_jsonhex(
                        remove_bad_chars(tx_hash))};
        });

    }

    CROW_ROUTE(app, "/tx/<string>/<uint>")
    ([&](string tx_hash, uint16_t with_ring_signatures)
     {
        return myxmr::htmlresponse(
                xmrblocks.show_tx(remove_bad_chars(tx_hash),
                    with_ring_signatures));
    });
    if (enable_autorefresh_option)
    {
        CROW_ROUTE(app, "/tx/<string>/<uint>/autorefresh")
        ([&](string tx_hash, uint16_t with_ring_signature) {
            bool refresh_page {true};
            return myxmr::htmlresponse(
                xmrblocks.show_tx(remove_bad_chars(tx_hash), with_ring_signature, refresh_page));
        });
    }

    if (enable_pusher)
    {
        CROW_ROUTE(app, "/rawtx")
        ([&]() {
            return myxmr::htmlresponse(xmrblocks.show_rawtx());
        });

        CROW_ROUTE(app, "/checkandpush").methods("POST"_method)
        ([&](const crow::request& req) -> myxmr::htmlresponse
         {

            map<std::string, std::string> post_body
                    = xmreg::parse_crow_post_data(req.body);

            if (post_body.count("rawtxdata") == 0
                    || post_body.count("action") == 0)
            {
                return string("Raw tx data or action not provided");
            }

            string raw_tx_data = remove_bad_chars(post_body["rawtxdata"]);
            string action      = remove_bad_chars(post_body["action"]);

            if (action == "check")
                return myxmr::htmlresponse(
                        xmrblocks.show_checkrawtx(raw_tx_data, action));
            else if (action == "push")
                return myxmr::htmlresponse(
                        xmrblocks.show_pushrawtx(raw_tx_data, action));
            return string("Provided action is neither check nor push");

        });
    }

    CROW_ROUTE(app, "/search").methods("POST"_method)
    ([&](const crow::request& req) -> myxmr::htmlresponse {
        if (req.body.size() > 4096)
        {
            const vector<pair<string, vector<string>>> no_results;
            myxmr::htmlresponse response {
                    xmrblocks.show_search_results("request too large", no_results)};
            response.code = 413;
            return response;
        }
        const auto post_body = xmreg::parse_crow_post_data(req.body);
        const auto value_it = post_body.find("value");
        if (value_it == post_body.end() || value_it->second.empty() || value_it->second.size() > 128)
        {
            const string submitted = value_it == post_body.end()
                    ? string {} : remove_bad_chars(value_it->second.substr(0, 128));
            const vector<pair<string, vector<string>>> no_results;
            myxmr::htmlresponse response {
                    xmrblocks.show_search_results(submitted, no_results)};
            response.code = 400;
            return response;
        }
        return myxmr::htmlresponse(xmrblocks.search(remove_bad_chars(value_it->second)));
    });

    CROW_ROUTE(app, "/qwc-rpc/<path>").methods("POST"_method, "OPTIONS"_method)
    ([&](const crow::request& req, string requested_path) {
        crow::response response;
        const string origin = req.get_header_value("Origin");
        if (origin == "https://wallet.qwertycoin.org"
            || (origin.size() > 18 && origin.rfind("https://", 0) == 0
                && origin.find(".pages.dev") == origin.size() - 10))
        {
            response.add_header("Access-Control-Allow-Origin", origin);
            response.add_header("Vary", "Origin");
        }
        response.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        response.add_header("Access-Control-Allow-Headers", "Content-Type");
        response.add_header("X-Content-Type-Options", "nosniff");
        response.add_header("Cache-Control", "no-store");
        if (req.method == crow::HTTPMethod::Options)
        {
            response.code = 204;
            return response;
        }

        const string path = "/" + requested_path;
        if (!xmreg::wallet_rpc_path_allowed(path))
        {
            response.code = 403;
            response.set_header("Content-Type", "application/json");
            response.body = R"({"error":"RPC path not allowed"})";
            return response;
        }
        const bool binary = path.size() >= 4 && path.substr(path.size() - 4) == ".bin";
        const size_t maximum_request = binary ? 2 * 1024 * 1024 : 256 * 1024;
        if (req.body.size() > maximum_request)
        {
            response.code = 413;
            response.set_header("Content-Type", "application/json");
            response.body = R"({"error":"RPC request too large"})";
            return response;
        }
        if (path == "/json_rpc")
        {
            string method;
            const auto policy = xmreg::authorize_wallet_json_rpc(req.body, method);
            if (policy != xmreg::wallet_rpc_policy_result::allowed)
            {
                response.code = policy == xmreg::wallet_rpc_policy_result::malformed ? 400 : 403;
                response.set_header("Content-Type", "application/json");
                response.body = policy == xmreg::wallet_rpc_policy_result::malformed
                        ? R"({"error":"Malformed JSON-RPC envelope; batches and notifications are not accepted"})"
                        : R"({"error":"JSON-RPC method not allowed"})";
                return response;
            }
        }
        if (wallet_rpc_inflight.fetch_add(1) >= 16)
        {
            wallet_rpc_inflight.fetch_sub(1);
            response.code = 429;
            response.set_header("Retry-After", "1");
            response.set_header("Content-Type", "application/json");
            response.body = R"({"error":"Wallet RPC gateway busy"})";
            return response;
        }
        struct inflight_guard
        {
            std::atomic<unsigned>& value;
            ~inflight_guard() { value.fetch_sub(1); }
        } guard {wallet_rpc_inflight};

        xmreg::rpccalls::raw_response upstream;
        const string content_type = binary ? "application/octet-stream" : "application/json";
        const unsigned slot = wallet_rpc_next_slot.fetch_add(1) % wallet_rpc_pool.size();
        if (!wallet_rpc_pool[slot]->proxy_wallet_request(
                path, req.body, content_type, upstream))
        {
            response.code = 502;
            response.set_header("Content-Type", "application/json");
            response.body = R"({"error":"Restricted QWC node unavailable"})";
            return response;
        }
        response.code = upstream.status;
        response.set_header("Content-Type", binary ? "application/octet-stream"
                                                    : "application/json");
        response.body = std::move(upstream.body);
        return response;
    });

    CROW_ROUTE(app, "/mempool")
    ([&]() {
        return myxmr::htmlresponse(xmrblocks.mempool(true));
    });

    // alias to  "/mempool"
    CROW_ROUTE(app, "/txpool")
    ([&]() {
        return myxmr::htmlresponse(xmrblocks.mempool(true));
    });

    CROW_ROUTE(app, "/favicon.ico")
    ([&]() {
        crow::response response;
        response.set_header("Cache-Control", "public, max-age=86400");
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_static_file_info_unsafe("./templates/assets/favicon.ico");
        response.set_header("Content-Type", "image/x-icon");
        return response;
    });

    CROW_ROUTE(app, "/assets/<string>")
    ([&](const string& asset_name) {
        static const map<string, string> public_assets {
            {"qwertycoin-mark.svg", "image/svg+xml"},
            {"favicon.svg", "image/svg+xml"},
            {"favicon-16x16.png", "image/png"},
            {"favicon-32x32.png", "image/png"},
            {"favicon-192x192.png", "image/png"},
            {"apple-touch-icon.png", "image/png"},
        };
        const auto asset = public_assets.find(asset_name);
        if (asset == public_assets.end())
            return crow::response(404);
        crow::response response;
        response.set_header("Cache-Control", "public, max-age=86400");
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_static_file_info_unsafe("./templates/assets/" + asset_name);
        response.set_header("Content-Type", asset->second);
        return response;
    });

    CROW_ROUTE(app, "/assets/fonts/<string>")
    ([&](const string& font_name) {
        static const set<string> public_fonts {
            "archivo-latin-900.woff2",
            "inter-latin-400.woff2",
            "inter-latin-600.woff2",
        };
        if (public_fonts.count(font_name) == 0)
            return crow::response(404);
        crow::response response;
        response.set_header("Cache-Control", "public, max-age=86400");
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_static_file_info_unsafe("./templates/assets/fonts/" + font_name);
        response.set_header("Content-Type", "font/woff2");
        return response;
    });

//    CROW_ROUTE(app, "/altblocks")
//    ([&](const crow::request& req) {
//        return xmrblocks.altblocks();
//    });

    CROW_ROUTE(app, "/robots.txt")
    ([&]() {
        string text = "User-agent: *\n"
                      "Disallow: ";
        return text;
    });

    if (enable_json_api)
    {

        cout << "Enable JSON API\n";

        CROW_ROUTE(app, "/api/transaction/<string>")
        ([&](string tx_hash) {

            myxmr::jsonresponse r{xmrblocks.json_transaction(remove_bad_chars(tx_hash))};

            return r;
        });

        CROW_ROUTE(app, "/api/rawtransaction/<string>")
        ([&](string tx_hash) {

            myxmr::jsonresponse r{xmrblocks.json_rawtransaction(remove_bad_chars(tx_hash))};

            return r;
        });

        CROW_ROUTE(app, "/api/detailedtransaction/<string>")
        ([&](string tx_hash) {

            myxmr::jsonresponse r{xmrblocks.json_detailedtransaction(remove_bad_chars(tx_hash))};

            return r;
        });

        CROW_ROUTE(app, "/api/block/<string>")
        ([&](string block_no_or_hash) {

            myxmr::jsonresponse r{xmrblocks.json_block(remove_bad_chars(block_no_or_hash))};

            return r;
        });

        CROW_ROUTE(app, "/api/rawblock/<string>")
        ([&](string block_no_or_hash) {

            myxmr::jsonresponse r{xmrblocks.json_rawblock(remove_bad_chars(block_no_or_hash))};

            return r;
        });

        CROW_ROUTE(app, "/api/transactions").methods("GET"_method)
        ([&](const crow::request &req) {

            const char* page_param = req.url_params.get("page");
            const char* limit_param = req.url_params.get("limit");
            string page = page_param ? page_param : "0";
            string limit = limit_param ? limit_param : "25";

            myxmr::jsonresponse r{xmrblocks.json_transactions(
                    page, limit)};

            return r;
        });

        CROW_ROUTE(app, "/api/mempool").methods("GET"_method)
        ([&](const crow::request &req) {

            const char* page_param = req.url_params.get("page");
            const char* limit_param = req.url_params.get("limit");
            string page = page_param ? page_param : "0";
            string limit = limit_param ? limit_param : "25";

            myxmr::jsonresponse r{xmrblocks.json_mempool(
                    page, limit)};

            return r;
        });

        CROW_ROUTE(app, "/api/networkinfo")
        ([&]() {

            myxmr::jsonresponse r{xmrblocks.json_networkinfo()};

            return r;
        });

        CROW_ROUTE(app, "/api/epose")
        ([&]() {

            myxmr::jsonresponse r{xmrblocks.json_epose_info()};

            return r;
        });

        CROW_ROUTE(app, "/api/epose/service-nodes")
        ([&]() {

            myxmr::jsonresponse r{xmrblocks.json_epose_service_nodes()};

            return r;
        });

        CROW_ROUTE(app, "/api/epose/rewards")
        ([&]() {

            myxmr::jsonresponse r{xmrblocks.json_epose_rewards()};

            return r;
        });

        CROW_ROUTE(app, "/api/feeestimate").methods("GET"_method)
        ([&](const crow::request &req) {

            string grace_blocks = regex_search(
                    req.raw_url, regex {"grace_blocks=\\d+"}) ?
                                  req.url_params.get("grace_blocks") : "";

            myxmr::jsonresponse r{xmrblocks.json_feeestimate(
                    remove_bad_chars(grace_blocks))};

            return r;
        });

        CROW_ROUTE(app, "/api/version")
        ([&]() {

            myxmr::jsonresponse r{xmrblocks.json_version()};

            return r;
        });

        // Versioned, allowlisted browser adapter. Legacy read aliases above are
        // retained temporarily for API clients; no generic daemon proxy belongs
        // behind this application.
        CROW_ROUTE(app, "/api/v1/epose")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_epose_info()}; });

        CROW_ROUTE(app, "/api/v1/epose/service-nodes")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_epose_service_nodes()}; });

        CROW_ROUTE(app, "/api/v1/epose/rewards")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_epose_rewards()}; });

        CROW_ROUTE(app, "/api/v1/network")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_networkinfo()}; });

        CROW_ROUTE(app, "/api/v1/overview")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_overview()}; });

        CROW_ROUTE(app, "/api/v1/supply")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_emission()}; });

        CROW_ROUTE(app, "/api/v1/identity")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_identity()}; });

        CROW_ROUTE(app, "/api/v1/version")
        ([&]() { return myxmr::jsonresponse{xmrblocks.json_version()}; });

        CROW_ROUTE(app, "/api/v1/transactions").methods("GET"_method)
        ([&](const crow::request &req) {
            const char* page_param = req.url_params.get("page");
            const char* limit_param = req.url_params.get("limit");
            const char* block_count_param = req.url_params.get("block_count");
            const char* tip_hash_param = req.url_params.get("tip_hash");
            return myxmr::jsonresponse{xmrblocks.json_transactions(
                    page_param ? page_param : "0",
                    limit_param ? limit_param : "25",
                    block_count_param ? block_count_param : "",
                    tip_hash_param ? remove_bad_chars(tip_hash_param) : "")};
        });

        CROW_ROUTE(app, "/api/v1/mempool").methods("GET"_method)
        ([&](const crow::request &req) {
            const char* page_param = req.url_params.get("page");
            const char* limit_param = req.url_params.get("limit");
            return myxmr::jsonresponse{xmrblocks.json_mempool(
                    page_param ? page_param : "0",
                    limit_param ? limit_param : "25")};
        });

    } // if (enable_json_api)

    if (enable_autorefresh_option)
    {
        CROW_ROUTE(app, "/autorefresh")
        ([&]() {
            uint64_t page_no {0};
            bool refresh_page {true};
            return myxmr::htmlresponse(xmrblocks.index2(page_no, refresh_page));
        });
    }

    // run the crow http server

    if (use_ssl)
    {
        cout << "Staring in ssl mode" << endl;
        app.bindaddr(bindaddr).port(app_port).ssl_file(
                ssl_crt_file, ssl_key_file)
                .multithreaded().run();
    }
    else
    {
        cout << "Staring in non-ssl mode" << endl;
        if (*concurrency_opt == 0)
        {
            app.bindaddr(bindaddr).port(app_port).multithreaded().run();
        }
        else
        {
            app.bindaddr(bindaddr).port(app_port)
                .concurrency(*concurrency_opt).run();
        }
    }

    // finish mempool thread

    cout << "Waiting for mempool monitoring thread to finish." << endl;

    xmreg::MempoolStatus::m_thread.interrupt();
    xmreg::MempoolStatus::m_thread.join();

    cout << "Mempool monitoring thread finished." << endl;

    cout << "The explorer is terminating." << endl;

    supply_index.stop();

    return EXIT_SUCCESS;
}
