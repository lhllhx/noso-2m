#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <regex>
#include <thread>

#include "misc.hpp"
#include "output.hpp"

extern char g_miner_address[];
extern std::uint32_t g_threads_count;
extern std::vector<pool_specs_t> g_mining_pools;

inline
bool is_valid_address( std::string const & address ) {
    if ( address.length() < 30 || address.length() > 31 ) return false;
    return true;
}

inline
bool is_valid_threads( std::uint32_t count ) {
    if ( count < 2 ) return false;
    return true;
}

inline
std::vector<pool_specs_t> parse_pools_argv( std::string const & poolstr ) {
    const std::regex re_pool1 { ";|[[:space:]]" };
    const std::regex re_pool2 {
        "^"
        "("
            "[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9]"
        ")"
        "(\\:"
            "("
                "("
                    "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\."
                    "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\."
                    "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])\\."
                    "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9][0-9]|[0-9])"
                ")"
                "|"
                "("
                    "([a-zA-Z0-9]+(\\-[a-zA-Z0-9]+)*\\.)*[a-zA-Z]{2,}"
                ")"
            ")"
            "(\\:"
                "("
                    "6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|[1-5][0-9]{4}|[0-5]{0,5}|[0-9]{1,4}"
                ")$"
            ")?"
        ")?"
        "$"
    };
    std::vector<pool_specs_t> mining_pools;
    std::for_each(
            std::sregex_token_iterator { poolstr.begin(), poolstr.end(), re_pool1 , -1 },
            std::sregex_token_iterator {}, [&]( const auto &tok ) {
                std::string pls { tok.str() };
                std::for_each(
                        std::sregex_iterator { pls.begin(), pls.end(), re_pool2 },
                        std::sregex_iterator {}, [&]( const auto &sm0 ) {
                            std::string name { sm0[1].str() };
                            std::string host { sm0[3].str() };
                            std::string port { sm0[13].str() };
                            if ( name.length() <= 0 ) return;
                            if ( host.length() <= 0 ) return;
                            if ( port.length() <= 0 ) port = std::string { "8082" };
                            mining_pools.push_back( { name, host, port } );
                        }
                    );
            }
        );
    return mining_pools;
}

mining_options_t
    g_arg_options = {
        .threads = 2,
    },
    g_cfg_options = {
        .threads = 2,
    };

inline
void process_arg_options( cxxopts::ParseResult const & parsed_options ) {
    try {
        g_arg_options.address = parsed_options["address"].as<std::string>();
        if ( !is_valid_address( g_arg_options.address ) )
            throw std::invalid_argument( "Invalid miner address option" );
        g_arg_options.threads = parsed_options["threads"].as<std::uint32_t>();
        if ( !is_valid_threads( g_arg_options.threads ) )
            throw std::invalid_argument( "Invalid threads count option" );
        g_arg_options.pools = parsed_options["pools"].as<std::string>();
    } catch( const std::invalid_argument& e ) {
        std::string msg { e.what() };
        NOSO_LOG_FATAL << msg << std::endl;
        NOSO_TUI_OutputHistPad( msg.c_str() );
        throw std::bad_exception();
    }
}

inline
void process_cfg_options( cxxopts::ParseResult const & parsed_options ) {
    g_cfg_options.filename = parsed_options["config"].as<std::string>();
    std::ifstream cfg_istream( g_cfg_options.filename );
    if( !cfg_istream.good() ) {
        std::string msg { "Config file '" + g_cfg_options.filename + "' not found!" };
        NOSO_LOG_WARN << msg << std::endl;
        NOSO_TUI_OutputHistPad( msg.c_str() );
        if ( g_cfg_options.filename != DEFAULT_CONFIG_FILENAME ) throw std::bad_exception();
        msg = "Use default options";
        NOSO_LOG_INFO << msg << std::endl;
        NOSO_TUI_OutputHistPad( msg.c_str() );
        NOSO_TUI_OutputHistWin();
    } else {
        std::string msg { "Load config file '" + g_cfg_options.filename + "'" };
        NOSO_LOG_INFO << msg << std::endl;
        NOSO_TUI_OutputHistPad( msg.c_str() );
        NOSO_TUI_OutputHistWin();
        int line_no { 0 };
        std::string line_str;
        try {
            while ( std::getline( cfg_istream, line_str ) ) {
                line_no++;
                if        ( line_str.rfind( "address ", 0 ) == 0 ) {
                    g_cfg_options.address = line_str.substr( 8 );
                    if ( !is_valid_address( g_cfg_options.address ) )
                        throw std::invalid_argument( "Invalid address config" );
                } else if ( line_str.rfind( "threads ", 0 ) == 0 ) {
                    g_cfg_options.threads = std::stoi( line_str.substr( 8 ) );
                    if ( !is_valid_threads( g_cfg_options.threads ) )
                        throw std::invalid_argument( "Invalid threads count config" );
                } else if ( line_str.rfind( "pools ",   0 ) == 0 ) {
                    if ( g_cfg_options.pools.size() > 0 ) g_cfg_options.pools += ";";
                    g_cfg_options.pools += line_str.substr( 6 );
                }
            }
        } catch( const std::invalid_argument& e ) {
            std::string msg { std::string( e.what() ) + " in file '" + g_cfg_options.filename + "'" };
            NOSO_LOG_FATAL << msg << " line#" << line_no << "[" << line_str << "]" << std::endl;
            NOSO_TUI_OutputHistPad( msg.c_str() );
            throw std::bad_exception();
        }
    }
}

void process_options( cxxopts::ParseResult const & parsed_options ) {
    process_cfg_options( parsed_options );
    process_arg_options( parsed_options );
    std::string sel_address {
        g_arg_options.address != DEFAULT_MINER_ADDRESS ? g_arg_options.address
            : g_cfg_options.address.length() > 0 ? g_cfg_options.address : DEFAULT_MINER_ADDRESS };
    std::string sel_pools {
        g_arg_options.pools != DEFAULT_POOL_URL_LIST ? g_arg_options.pools
            : g_cfg_options.pools.length() > 0 ? g_cfg_options.pools : DEFAULT_POOL_URL_LIST };
    std::strcpy( g_miner_address, sel_address.c_str() );
    g_threads_count = g_arg_options.threads > DEFAULT_THREADS_COUNT ? g_arg_options.threads
        : g_cfg_options.threads > DEFAULT_THREADS_COUNT ? g_cfg_options.threads : DEFAULT_THREADS_COUNT;
    g_mining_pools = parse_pools_argv( sel_pools );
}

std::mutex _g_awaiting_threads_mutex;
std::unordered_map<std::thread::id, std::shared_ptr<std::condition_variable>> _g_awaiting_threads_uomap {};

inline
bool _awaiting_threads_append( std::shared_ptr<std::condition_variable> const & wait ) {
    assert( wait );
    std::unique_lock lock_awaiting_threads( _g_awaiting_threads_mutex );
    auto await_itor { _g_awaiting_threads_uomap.find( std::this_thread::get_id() ) };
    assert ( await_itor == _g_awaiting_threads_uomap.end() );
    if ( await_itor == _g_awaiting_threads_uomap.end() ) {
        _g_awaiting_threads_uomap[std::this_thread::get_id()] = wait;
        return true;
    }
    return false;
}

inline
bool _awaiting_threads_remove( ) {
    std::unique_lock lock_awaiting_threads( _g_awaiting_threads_mutex );
    auto await_itor { _g_awaiting_threads_uomap.find( std::this_thread::get_id() ) };
    assert ( await_itor != _g_awaiting_threads_uomap.end() );
    if ( await_itor != _g_awaiting_threads_uomap.end() ) {
        _g_awaiting_threads_uomap.erase(await_itor);
        return true;
    }
    return false;
}

void awaiting_threads_notify( ) {
    std::unique_lock lock_awaiting_threads( _g_awaiting_threads_mutex );
    std::for_each( _g_awaiting_threads_uomap.begin(), _g_awaiting_threads_uomap.end(),
            []( std::pair<std::thread::id, std::shared_ptr<std::condition_variable>> element ){
                    element.second->notify_all(); } );
}

void awaiting_threads_wait( std::mutex & mutex_wait, bool ( * wake_up )() ) {
    auto condv_wait = std::make_shared<std::condition_variable>();
    std::unique_lock<std::mutex> lock_wait( mutex_wait );
    _awaiting_threads_append( condv_wait );
    condv_wait->wait( lock_wait, wake_up );
    _awaiting_threads_remove( );
}

void awaiting_threads_wait_for( int sec, std::mutex & mutex_wait, bool ( * wake_up )() ) {
    if ( sec < 0 ) return;
    auto condv_wait = std::make_shared<std::condition_variable>();
    std::unique_lock<std::mutex> lock_wait( mutex_wait );
    _awaiting_threads_append( condv_wait );
    condv_wait->wait_for( lock_wait, std::chrono::milliseconds( 1'000 * sec ), wake_up );
    _awaiting_threads_remove( );
}
