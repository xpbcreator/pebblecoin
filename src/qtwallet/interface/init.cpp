#include <atomic>
#include <string>

#include <boost/thread.hpp>
#include <boost/filesystem/path.hpp>

#include <QMessageBox>

#include "include_base_utils.h"
#include "console_handler.h"

#include "common/types.h"
#include "common/ntp_time.h"
#include "common/util.h"
#include "crypto/hash.h"
#include "cryptonote_core/checkpoints_create.h"
#include "cryptonote_core/miner.h"
#include "rpc/core_rpc_server.h"
#include "daemon/daemon_options.h"
#include "daemon/daemon_commands_handler.h"
#include "cryptonote_core/blockchain_storage.h"

#include "bitcoin/util.h"
#include "init.h"
#include "wallet.h"
#include "main.h"

using namespace epee;

std::atomic<bool> fRequestShutdown(false);

void StartShutdown()
{
    fRequestShutdown = true;
    LOG_PRINT_YELLOW("Requested shutdown.", LOG_LEVEL_0);
}

bool ShutdownRequested()
{
    return fRequestShutdown;
}

static boost::thread *pdaemonThread = NULL;

void Shutdown()
{
    LOG_PRINT_L0("Shutdown() called");
    
    {
        LOCK(cs_main);
        if (pnodeSrv)
        {
            pnodeSrv->send_stop_signal();
        }
    }
    
    if (pdaemonThread)
    {
        LOG_PRINT_L0("Waiting for daemon thread to finish...");
        pdaemonThread->interrupt();
        pdaemonThread->join();
        delete pdaemonThread;
        LOG_PRINT_L0("... daemon shutdown successfully");
    }
    
    LOG_PRINT_L0("Stopping boulderhash threadpool...");
    crypto::pc_stop_threadpool();
    
    if (crypto::g_boulderhash_state)
    {
        LOG_PRINT_L0("Deinitializing shared boulderhash state...");
        crypto::pc_free_state(crypto::g_boulderhash_state);
        crypto::g_boulderhash_state = NULL;
    }
}


void WalletRefreshThread(rpc_server_t *prpcServer)
{
    bool rapidChecks = true;
    while (!ShutdownRequested())
    {
        if (prpcServer->check_core_ready())
        {
            if (pwalletMain->RefreshOnce())
                rapidChecks = false;
        }
      
        if (DaemonProcessedHeight() + 5 < NumBlocksOfPeers())
        {
            rapidChecks = false;
        }
        
        try {
            boost::this_thread::sleep(boost::posix_time::milliseconds(rapidChecks ? 500 : 30000));
        }
        catch (boost::thread_interrupted& e) {
            rapidChecks = true;
        }
      
        if (!rapidChecks && pcore) {
            pcore->get_blockchain_storage().store_blockchain();
        }
    }
}


bool DaemonThreadInit(node_server_t &p2psrv, protocol_handler_t &cprotocol,
                      rpc_server_t &rpc_server, core_t &ccore,
                      daemon_cmmands_handler &dch)
{
    TRY_ENTRY();
    
    bool res = true;
    
    //initialize objects
    LOG_PRINT_L0("Initializing global hash cache, testnet=" << ((int)cryptonote::config::testnet)
                 << ", data dir=" << command_line::get_data_dir(vmapArgs) << "...");
    res = crypto::g_hash_cache.init(command_line::get_data_dir(vmapArgs));
    CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize global hash cache.");
    LOG_PRINT_L0("Global hash cache initialized OK...");
    
    LOG_PRINT_L0("Initializing p2p server...");
    res = p2psrv.init(vmapArgs);
    CHECK_AND_ASSERT_MES(res, false, "Failed to initialize p2p server.");
    LOG_PRINT_L0("P2p server initialized OK");
    
    LOG_PRINT_L0("Initializing cryptonote protocol...");
    res = cprotocol.init(vmapArgs);
    CHECK_AND_ASSERT_MES(res, false, "Failed to initialize cryptonote protocol.");
    LOG_PRINT_L0("Cryptonote protocol initialized OK");
    
    LOG_PRINT_L0("Initializing core rpc server...");
    res = rpc_server.init(vmapArgs);
    CHECK_AND_ASSERT_MES(res, false, "Failed to initialize core rpc server.");
    LOG_PRINT_GREEN("Core rpc server initialized OK on port: " << rpc_server.get_binded_port(), LOG_LEVEL_0);
    
    //initialize core here
    LOG_PRINT_L0("Initializing core...");
    res = ccore.init(vmapArgs);
    CHECK_AND_ASSERT_MES(res, false, "Failed to initialize core");
    LOG_PRINT_L0("Core initialized OK");
    
    // start components
    if(!command_line::has_arg(vmapArgs, daemon_opt::arg_console))
    {
        dch.start_handling();
    }
    
    LOG_PRINT_L0("Starting core rpc server...");
    res = rpc_server.run(2, false);
    CHECK_AND_ASSERT_MES(res, false, "Failed to initialize core rpc server.");
    LOG_PRINT_L0("Core rpc server started ok");
    
    tools::signal_handler::install([&dch, &p2psrv] {
        dch.stop_handling();
        p2psrv.send_stop_signal();
    });
    
    return true;
    
    CATCH_ENTRY_L0("DaemonThreadInit", false);
}

class thread_interrupter : public cryptonote::i_core_callback {
public:
    mutable CCriticalSection cs_wt;
    
    thread_interrupter(boost::thread* th) : walletThread(th) {}
    
    void on_new_block_added(uint64_t height, const cryptonote::block& bl)
    {
        LOCK(cs_wt);
        if (walletThread)
            walletThread->interrupt();
    }
    
    void disable()
    {
        LOCK(cs_wt);
        walletThread = NULL;
    }
private:
    boost::thread *walletThread;
};

struct DaemonInitState {
    boost::mutex mutex;
    boost::condition_variable cond;
    bool fDone;
    bool fError;
    QString errorMessage;
    
    DaemonInitState() : fDone(false), fError(false) {}
    
    void success()
    {
        {
            boost::mutex::scoped_lock lock(mutex);
            fDone = true;
            fError = false;
        }
        cond.notify_all();
    }
    
    void error(const QString& message)
    {
        {
            boost::mutex::scoped_lock lock(mutex);
            fDone = true;
            fError = true;
            errorMessage = message;
        }
        cond.notify_all();
    }
    
    void waitUntilDone()
    {
        boost::mutex::scoped_lock lock(mutex);

        while (!fDone)
        {
            cond.wait(lock);
        }
    }
    
};

bool DaemonThread(DaemonInitState& initState)
{
    TRY_ENTRY();
    
    cryptonote::checkpoints checkpoints;
    bool res = cryptonote::create_checkpoints(checkpoints);
    CHECK_AND_ASSERT_MES(res, false, "Failed to initialize checkpoints");
    
    //create objects and link them
    tools::ntp_time ntp(60*60);
    core_t ccore(NULL, ntp);
    ccore.set_checkpoints(std::move(checkpoints));
    protocol_handler_t cprotocol(ccore, NULL);
    node_server_t p2psrv(cprotocol);
    rpc_server_t rpc_server(ccore, p2psrv);
    cprotocol.set_p2p_endpoint(&p2psrv);
    ccore.set_cryptonote_protocol(&cprotocol);
    daemon_cmmands_handler dch(p2psrv);
    
    if (!DaemonThreadInit(p2psrv, cprotocol, rpc_server, ccore, dch))
    {
        LOG_PRINT_RED_L0("Daemon failed to initialize");
        
        initState.error(QObject::tr("Error: The daemon failed to initialize."));
        
        /*if (!ShutdownRequested())
            StartShutdown();*/
 
        return false;
    }
    
    //
    if (!pwalletMain->GetWallet2()->check_connection())
    {
        LOG_PRINT_RED_L0("Wallet failed to connect to daemon");

        initState.error(QObject::tr("Error: The wallet failed to initialize."));
        
        /*if (!ShutdownRequested())
            StartShutdown();*/
        
        return false;
    }
    LOG_PRINT_L0("Wallet connected to daemon!");
    
    initState.success();
    
    // set up hooks for wallet refreshing
    
    boost::thread *walletRefreshThread = new boost::thread(boost::bind(WalletRefreshThread, &rpc_server));;
    thread_interrupter wti(walletRefreshThread);
    ccore.callback(&wti);
    
    // allow global accessing
    {
        LOCK(cs_main);
        pnodeSrv = &p2psrv;
        pcore = &ccore;
        pwalletMain->SetTxMemoryPool(ccore.get_memory_pool());
    }
    
    try
    {
        boost::this_thread::interruption_point();
        LOG_PRINT_L0("Starting p2p net loop...");
        p2psrv.run();
        LOG_PRINT_L0("p2p net loop stopped");
        boost::this_thread::interruption_point();
    }
    catch (boost::thread_interrupted const& )
    {
        LOG_PRINT_L0("Daemon thread was interrupted!");
    }
    
    if (!ShutdownRequested()) // given "exit" command or hit CTRL + C
        StartShutdown();
    
    {
        LOCK(cs_main);
        pcore = NULL;
        pnodeSrv = NULL;
    }
    
    LOG_PRINT_L0("Shutting down wallet...");
    pwalletMain->Shutdown();
    
    LOG_PRINT_L0("Stopping wallet refresh thread...");
    ccore.callback(NULL);
    wti.disable();
    walletRefreshThread->interrupt();
    walletRefreshThread->join();
    
    //stop components
    LOG_PRINT_L0("Stopping core rpc server...");
    rpc_server.send_stop_signal();
    rpc_server.timed_wait_server_stop(5000);
    
    //deinitialize components
    LOG_PRINT_L0("Deinitializing core...");
    ccore.deinit();
    LOG_PRINT_L0("Deinitializing rpc server...");
    rpc_server.deinit();
    LOG_PRINT_L0("Deinitializing cryptonote_protocol...");
    cprotocol.deinit();
    LOG_PRINT_L0("Deinitializing p2p...");
    p2psrv.deinit();
    
    ccore.set_cryptonote_protocol(NULL);
    cprotocol.set_p2p_endpoint(NULL);
    
    LOG_PRINT_L0("Deinitializing global hash cache...");
    crypto::g_hash_cache.deinit();
    
    LOG_PRINT("Node stopped.", LOG_LEVEL_0);
    
    return true;
    
    CATCH_ENTRY_L0("DaemonThread", false);
}


bool CommandLinePreprocessor()
{
    bool fExit = false;
    if (GetArg(command_line::arg_version))
    {
        std::cout << tools::get_project_description("Qt") << ENDL;
        fExit = true;
    }
    if (GetArg(daemon_opt::arg_os_version))
    {
        std::cout << "OS: " << tools::get_os_version_string() << ENDL;
        fExit = true;
    }
    
    if (fExit)
    {
        return true;
    }
    
    int new_log_level = GetArg(daemon_opt::arg_log_level);
    if(new_log_level < LOG_LEVEL_MIN || new_log_level > LOG_LEVEL_MAX)
    {
        LOG_PRINT_L0("Wrong log level value: " << new_log_level);
    }
    else if (log_space::get_set_log_detalisation_level(false) != new_log_level)
    {
        log_space::get_set_log_detalisation_level(true, new_log_level);
        LOG_PRINT_L0("LOG_LEVEL set to " << new_log_level);
    }
    
    return false;
}

bool AppInit2(boost::thread_group& threadGroup, QString& error)
{
    TRY_ENTRY();
    
    if (pdaemonThread != NULL)
    {
        error = QObject::tr("App is already initialized");
        return false;
    }
    
    LOG_PRINT_L0("Initializing shared boulderhash state...");
    crypto::g_boulderhash_state = crypto::pc_malloc_state();
    LOG_PRINT_L0("Shared boulderhash state initialized OK");
    
    LOG_PRINT_L0("Initializing boulderhash threadpool...");
    crypto::pc_init_threadpool(vmapArgs);

    //set up logging options
    LOG_PRINT_L0("Setting up logging options...");
    boost::filesystem::path log_file_path(GetArg(daemon_opt::arg_log_file));
    if (log_file_path.empty())
        log_file_path = log_space::log_singletone::get_default_log_file();
    std::string log_dir;
    log_dir = log_file_path.has_parent_path() ? log_file_path.parent_path().string() : log_space::log_singletone::get_default_log_folder();
    
    log_space::log_singletone::add_logger(LOGGER_FILE, log_file_path.filename().string().c_str(), log_dir.c_str());
    LOG_PRINT_L0(tools::get_project_description("Qt"));
    
    LOG_PRINT_L0("Running command line preprocessor...");
    if (CommandLinePreprocessor())
    {
        error = "";
        return false;
    }
    
    // Load the wallet
    LOG_PRINT_L0("Loading wallet2...");
    tools::wallet2 *pwallet2 = new tools::wallet2();
    
    pwallet2->set_conn_timeout(250); // local daemon should always be instant
    
    try
    {
        pwallet2->load(GetWalletFile().string(), "");
    }
    catch (const std::exception &e)
    {
        LOG_PRINT_L0("Failed to load wallet: " << e.what());
        error = QObject::tr("Failed to load wallet: %1").arg(e.what());
        return false;
    }
  
    std::string rpc_bind_port = GetArg(core_rpc_opt::arg_rpc_bind_port);
    if (rpc_bind_port.empty())
    {
        rpc_bind_port = std::to_string(cryptonote::config::rpc_default_port());
    }
    pwallet2->init("http://localhost:" + rpc_bind_port);
    
    LOG_PRINT_L0("Creating CWallet...");
    pwalletMain = new CWallet(GetWalletFile().string() + ".cwallet");
    pwalletMain->SetWallet2(pwallet2);
    
    // Run daemon in a thread. Daemon will start the wallet once it is loaded.
    LOG_PRINT_L0("Running daemon in a thread");
    {
        DaemonInitState initState;
        pdaemonThread = new boost::thread(boost::bind(DaemonThread, boost::ref(initState)));
        
        LOG_PRINT_L0("Waiting until daemon thread initialization done...");
        initState.waitUntilDone();
        LOG_PRINT_L0("done, fError is: " << initState.fError);
        
        if (initState.fError)
        {
            error = initState.errorMessage;
            return false;
        }
    }
    
    LOG_PRINT_L0("AppInit2() done");
    return true;

    CATCH_ENTRY_L0("AppInit2", false);
}