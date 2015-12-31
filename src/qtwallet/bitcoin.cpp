// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "bitcoin-config.h"
#endif

#ifndef Q_MOC_RUN
#include "bitcoingui.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "intro.h"
#include "optionsmodel.h"
#include "splashscreen.h"
#include "utilitydialog.h"
#include "winshutdownmonitor.h"
#ifdef ENABLE_WALLET
#include "walletmodel.h"
#endif
#include "qt_options.h"

#include "interface/init.h"
#include "interface/main.h"
//#include "rpcserver.h"
#include "bitcoin/util.h"
#ifdef ENABLE_WALLET
#include "interface/wallet.h"
#endif

#include "include_base_utils.h"

using namespace epee; // needed for epee util imports to work

#include "common/ui_interface.h"
#include "common/command_line.h"
#include "common/util.h" // Pebblecoin utils
#include "daemon/daemon_options.h"
#include "p2p/net_node.h"
#include "crypto/hash_options.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/miner_opt.h"
#include "rpc/core_rpc_server.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "common/types.h"
#include "wallet/wallet2.h"
#include "cryptonote_genesis_config.h"

#include <stdint.h>

#include <boost/filesystem/operations.hpp>
#endif

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <QThread>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#else
Q_IMPORT_PLUGIN(AccessibleFactory)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif
#endif

#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif

namespace po = boost::program_options;

// Declare meta types used for QMetaObject::invokeMethod
Q_DECLARE_METATYPE(bool*)

static void InitMessage(const std::string &message)
{
    LogPrintf("init message: %s\n", message);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("bitcoin-core", psz).toStdString();
}

/** Set up translations */
static void initTranslations(QTranslator &qtTranslatorBase, QTranslator &qtTranslator, QTranslator &translatorBase, QTranslator &translator)
{
    QSettings settings;

    // Remove old translators
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    QString lang_territory = QLocale::system().name();
    // 2) Language from QSettings
    QString lang_territory_qsettings = settings.value("language", "").toString();
    if(!lang_territory_qsettings.isEmpty())
        lang_territory = lang_territory_qsettings;
    // 3) -lang command line argument
    if (HasArg(qt_opt::arg_lang))
    {
        lang_territory = QString::fromStdString(GetArg(qt_opt::arg_lang));
    }

    // Convert to "de" only by truncating "_DE"
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);

    // Load e.g. bitcoin_de.qm (shortcut "de" needs to be defined in bitcoin.qrc)
    if (translatorBase.load(lang, ":/translations/"))
        QApplication::installTranslator(&translatorBase);

    // Load e.g. bitcoin_de_DE.qm (shortcut "de_DE" needs to be defined in bitcoin.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        QApplication::installTranslator(&translator);
}

/* qDebug() message handler --> debug.log */
#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char *msg)
{
    Q_UNUSED(type);
    LogPrint("qt", "GUI: %s\n", msg);
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    LogPrint("qt", "GUI: %s\n", qPrintable(msg));
}
#endif

/** Class encapsulating Bitcoin Core startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class BitcoinCore: public QObject
{
    Q_OBJECT
public:
    explicit BitcoinCore();

public slots:
    void initialize();
    void shutdown();

signals:
    void initializeResult(int retval, QString error);
    void shutdownResult(int retval);
    void runawayException(const QString &message);

private:
    boost::thread_group threadGroup;

    /// Pass fatal exception message to UI thread
    void handleRunawayException(std::exception *e);
};

/** Main Bitcoin application object */
class BitcoinApplication: public QApplication
{
    Q_OBJECT
public:
    explicit BitcoinApplication(int &argc, char **argv, boost::program_options::options_description descOptionsIn);
    ~BitcoinApplication();

    /// Create options model
    void createOptionsModel();
    /// Create main window
    void createWindow(bool isaTestNet);
    /// Create splash screen
    void createSplashScreen(bool isaTestNet);

    /// Request core initialization
    void requestInitialize();
    /// Request core shutdown
    void requestShutdown();

    /// Get process return value
    int getReturnValue() { return returnValue; }

    /// Get window identifier of QMainWindow (BitcoinGUI)
    WId getMainWinId() const;

public slots:
    void initializeResult(int retval, QString error);
    void shutdownResult(int retval);
    /// Handle runaway exceptions. Shows a message box with the problem and quits the program.
    void handleRunawayException(const QString &message);

signals:
    void requestedInitialize();
    void requestedShutdown();
    void stopThread();
    void splashFinished(QWidget *window);

private:
    QThread *coreThread;
    OptionsModel *optionsModel;
    ClientModel *clientModel;
    BitcoinGUI *window;
    QTimer *pollShutdownTimer;
#ifdef ENABLE_WALLET
    WalletModel *walletModel;
#endif
    int returnValue;
    boost::program_options::options_description descOptions;

    void startThread();
};

#include "bitcoin.moc"

BitcoinCore::BitcoinCore():
    QObject()
{
}

void BitcoinCore::handleRunawayException(std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    emit runawayException(QString::fromStdString(strMiscWarning));
}

void BitcoinCore::initialize()
{
    try
    {
        QString error;
        LOG_PRINT_L0("Running AppInit2 in thread\n");
        int rv = AppInit2(threadGroup, error);
        LOG_PRINT_L0("AppInit2 done, rv=" << rv);
        /*if(rv)
        {
            // Start a dummy RPC thread if no RPC thread is active yet
            // to handle timeouts.
            StartDummyRPCThread();
        }*/
        emit initializeResult(rv, error);
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
}

void BitcoinCore::shutdown()
{
    try
    {
        LogPrintf("Running Shutdown in thread\n");
        threadGroup.interrupt_all();
        threadGroup.join_all();
        Shutdown();
        LogPrintf("Shutdown finished\n");
        emit shutdownResult(1);
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
}

BitcoinApplication::BitcoinApplication(int &argc, char **argv, boost::program_options::options_description descOptionsIn):
    QApplication(argc, argv),
    coreThread(0),
    optionsModel(0),
    clientModel(0),
    window(0),
    pollShutdownTimer(0),
#ifdef ENABLE_WALLET
    walletModel(0),
#endif
    returnValue(0),
    descOptions(descOptionsIn)
{
    setQuitOnLastWindowClosed(false);
    startThread();
}

BitcoinApplication::~BitcoinApplication()
{
    LogPrintf("Stopping thread\n");
    emit stopThread();
    coreThread->wait();
    LogPrintf("Stopped thread\n");

    //delete window; // don't delete, for now this causes a segfault because of the IRC window
    window = 0;
    delete optionsModel;
    optionsModel = 0;
}

void BitcoinApplication::createOptionsModel()
{
    optionsModel = new OptionsModel();
}

void BitcoinApplication::createWindow(bool isaTestNet)
{
    window = new BitcoinGUI(descOptions, isaTestNet, 0);

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, SIGNAL(timeout()), window, SLOT(detectShutdown()));
    pollShutdownTimer->start(200);
}

void BitcoinApplication::createSplashScreen(bool isaTestNet)
{
    SplashScreen *splash = new SplashScreen(QPixmap(), 0, isaTestNet);
    splash->setAttribute(Qt::WA_DeleteOnClose);
    splash->show();
    connect(this, SIGNAL(splashFinished(QWidget*)), splash, SLOT(slotFinish(QWidget*)));
}

void BitcoinApplication::startThread()
{
    coreThread = new QThread(this);
    BitcoinCore *executor = new BitcoinCore();
    executor->moveToThread(coreThread);

    /*  communication to and from thread */
    connect(executor, SIGNAL(initializeResult(int,QString)), this, SLOT(initializeResult(int,QString)));
    connect(executor, SIGNAL(shutdownResult(int)), this, SLOT(shutdownResult(int)));
    connect(executor, SIGNAL(runawayException(QString)), this, SLOT(handleRunawayException(QString)));
    connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
    connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

    coreThread->start();
}

void BitcoinApplication::requestInitialize()
{
    LogPrintf("Requesting initialize\n");
    emit requestedInitialize();
}

void BitcoinApplication::requestShutdown()
{
    LogPrintf("Requesting shutdown\n");
    window->hide();
    window->setClientModel(0);
    pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
    window->removeAllWallets();
    delete walletModel;
    walletModel = 0;
#endif
    delete clientModel;
    clientModel = 0;

    // Show a simple window indicating shutdown status
    ShutdownWindow::showShutdownWindow(window);

    // Request shutdown from core thread
    emit requestedShutdown();
}

void BitcoinApplication::initializeResult(int retval, QString error)
{
    LOG_PRINT_L0("Initialization result: " << retval << ", error=" << error.toStdString());
    // Set exit result: 0 if successful, 1 if failure
    returnValue = retval ? 0 : 1;
    if(retval)
    {
        emit splashFinished(window);

        clientModel = new ClientModel(optionsModel);
        window->setClientModel(clientModel);

#ifdef ENABLE_WALLET
        if(pwalletMain)
        {
            walletModel = new WalletModel(pwalletMain, optionsModel);

            window->addWallet("~Default", walletModel);
            window->setCurrentWallet("~Default");
        }
#endif

        // If -min option passed, start window minimized.
        if(GetArg(qt_opt::arg_min))
        {
            window->showMinimized();
        }
        else
        {
            window->show();
        }
    } else {
        if (!error.isEmpty())
        {
            QMessageBox::critical(0, tr("Pebblecoin"), error);
        }
        quit(); // Exit main loop
    }
}

void BitcoinApplication::shutdownResult(int retval)
{
    LogPrintf("Shutdown result: %i\n", retval);
    quit(); // Exit main loop after shutdown finished
}

void BitcoinApplication::handleRunawayException(const QString &message)
{
    QMessageBox::critical(0, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. Bitcoin can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(1);
}

WId BitcoinApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

static po::options_description InitParameters()
{
    po::options_description desc_cmd_only("Command line options");
    command_line::add_arg(desc_cmd_only, command_line::arg_help);
    command_line::add_arg(desc_cmd_only, command_line::arg_version);
    command_line::add_arg(desc_cmd_only, daemon_opt::arg_os_version);
    command_line::add_arg(desc_cmd_only, command_line::arg_data_dir);
    command_line::add_arg(desc_cmd_only, daemon_opt::arg_config_file);
    command_line::add_arg(desc_cmd_only, qt_opt::arg_wallet_file);

    po::options_description desc_cmd_sett("Command line options and settings options");
    command_line::add_arg(desc_cmd_sett, daemon_opt::arg_log_file);
    command_line::add_arg(desc_cmd_sett, daemon_opt::arg_log_level);
    command_line::add_arg(desc_cmd_sett, daemon_opt::arg_console);
    command_line::add_arg(desc_cmd_sett, daemon_opt::arg_testnet_on);

    cryptonote_opt::init_options(desc_cmd_sett);
    core_t::init_options(desc_cmd_sett);
    rpc_server_t::init_options(desc_cmd_sett);
    node_server_t::init_options(desc_cmd_sett);
    cryptonote::miner::init_options(desc_cmd_sett);
    crypto::init_options(desc_cmd_sett);

    po::options_description desc_cmd_qt("QT-specific options");
    QString lang_territory = QLocale::system().name();
    command_line::add_arg(desc_cmd_qt, qt_opt::arg_lang);
    command_line::add_arg(desc_cmd_qt, qt_opt::arg_min);
    command_line::add_arg(desc_cmd_qt, qt_opt::arg_no_splash);
    command_line::add_arg(desc_cmd_qt, qt_opt::arg_disable_wallet);
    command_line::add_arg(desc_cmd_qt, qt_opt::arg_choose_data_dir);
    command_line::add_arg(desc_cmd_qt, qt_opt::arg_root_certificates);
  
    po::options_description desc_options("Allowed options");
    desc_options.add(desc_cmd_only).add(desc_cmd_sett).add(desc_cmd_qt);
    return desc_options;
}

#ifndef BITCOIN_QT_TEST

int main(int argc, char *argv[])
{
    epee::string_tools::set_module_name_and_folder(argv[0]);
  
    SetupEnvironment();
    log_space::get_set_log_detalisation_level(true, LOG_LEVEL_0);
    log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL);
    LOG_PRINT_L0("Starting...");
  
    /// 1. Parse command-line options. These take precedence over anything else.
    po::options_description desc_options = InitParameters();
    // Command-line options take precedence:
    if (!ParseParameters(argc, argv, desc_options))
    {
        // Failed to parse parameters
        QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                              QObject::tr("Error: Failed to parse command-line parameters."));
        return 1;
    }
    
    /// Enable testnet from command line if necessary
    if (GetArg(daemon_opt::arg_testnet_on) || cryptonote::config::testnet_only)
    {
        cryptonote::config::enable_testnet();
    }
    
    // Do not refer to data directory yet, this can be overridden by Intro::pickDataDirectory

    /// 2. Basic Qt initialization (not dependent on parameters or configuration)
#if QT_VERSION < 0x050000
    // Internal string conversion is all UTF-8
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());
#endif

    Q_INIT_RESOURCE(bitcoin);
    Q_INIT_RESOURCE(bitcoin_trans);
    BitcoinApplication app(argc, argv, desc_options);
#if QT_VERSION > 0x050100
    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    // Register meta types used for QMetaObject::invokeMethod
    qRegisterMetaType< bool* >();

    /// 3. Application identification
    // must be set before OptionsModel is initialized or translations are loaded,
    // as it is used to locate QSettings
    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(cryptonote::config::testnet ? QAPP_APP_NAME_TESTNET : QAPP_APP_NAME_DEFAULT);

    /// 4. Initialization of translations, so that intro dialog is in user's language
    // Now that QSettings are accessible, initialize translations
    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);
    uiInterface.Translate.connect(Translate);

    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (HasArg(command_line::arg_help))
    {
        HelpMessageDialog help(desc_options, NULL);
        help.showOrPrint();
        return 1;
    }

    /// 5. Now that settings and translations are available, ask user for data directory
    // User language is set up: pick a data directory
    Intro::pickDataDirectory();

    /// 6. Determine availability of data directory and parse pebblecoin.conf
    /// - Do not call GetDataDir(true) before this step finishes
    if (!boost::filesystem::is_directory(GetDataDir(false)))
    {
        QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                              QObject::tr("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(GetDataDir(false).string())));
        return 1;
    }
  
    if (!ReadConfigFile(desc_options)) {
        QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                              QObject::tr("Error: Cannot parse configuration file. Only use key=value syntax."));
        return 1;
    }
    
    /// Enable testnet from config file if necessary
    if (GetArg(daemon_opt::arg_testnet_on) || cryptonote::config::testnet_only)
    {
        cryptonote::config::enable_testnet();
    }
    
    // set app name for testnet settings from now on
    QApplication::setApplicationName(cryptonote::config::testnet ? QAPP_APP_NAME_TESTNET : QAPP_APP_NAME_DEFAULT);
    
    if (!cryptonote_opt::handle_command_line(vmapArgs))
    {
        QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                              QObject::tr("Error: Must provide the correct genesis block hashes. Copy the config file from the BTCTalk thread and put it in %1 . Make sure the hashes are the same.").arg(QString::fromStdString(GetConfigFile().string())));
        return 1;
    }
    
    /// 6b. Generate the wallet if it doesn't exist
    
#ifdef ENABLE_WALLET
    std::string wallet_path = GetWalletFile().string();
    bool keys_file_exists;
    bool wallet_file_exists;
    tools::wallet2::wallet_exists(wallet_path, keys_file_exists, wallet_file_exists);
    
    if (!keys_file_exists && !wallet_file_exists)
    {
        try {
            tools::wallet2().generate(wallet_path, "");
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                                  QObject::tr("Error: Could not generate wallet file:\n%1.").arg(e.what()));
            return 1;
        }
    }
    else if (keys_file_exists)
    {
    }
    else if (!keys_file_exists && wallet_file_exists)
    {
        QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                              QObject::tr("Error: Found wallet file but not keys file."));
        return 1;
    }
    
    /// 6c. Set mining options & finish processing options
    
    {
        tools::wallet2 w;
        
        try
        {
            w.load(wallet_path, "");
        }
        catch (const std::exception &e)
        {
            QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                                  QObject::tr("Error: Failed to load wallet file: %1").arg(e.what()));
            return 1;
        }
        
        bool enable_boulder = GetArg(hashing_opt::arg_enable_boulderhash) || cryptonote::config::testnet;
        if (enable_boulder)
        {
            SoftSetArg(miner_opt::arg_start_mining, cryptonote::get_account_address_as_str(w.get_public_address()));
        }
        SoftSetArg(miner_opt::arg_delegate_wallet_file, wallet_path);
    }

#endif
    
    if (!crypto::process_options(vmapArgs, HasArg(miner_opt::arg_start_mining)))
    {
        QMessageBox::critical(0, QObject::tr("Pebblecoin"),
                              QObject::tr("Error: Invalid hash-related options (see debug messages)."));
        return 1;
    }
    
    // Re-initialize translations after changing application name (language in network-specific settings can be different)
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

    /*/// 7. Determine network (and switch to network specific options)
     // - Do not call Params() before this step
     // - Do this after parsing the configuration file, as the network can be switched there
     // - QSettings() will use the new application name after this, resulting in network-specific settings
     // - Needs to be done before createOptionsModel
     
     // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
     if (!SelectParamsFromCommandLine()) {
     QMessageBox::critical(0, QObject::tr("Pebblecoin"), QObject::tr("Error: Invalid combination of -regtest and -testnet."));
     return 1;
     }*/
    
    /// 9. Main GUI initialization
    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if QT_VERSION < 0x050000
    // Install qDebug() message handler to route to debug.log
    qInstallMsgHandler(DebugMessageHandler);
#else
#if defined(Q_OS_WIN)
    // Install global event filter for processing Windows session related Windows messages (WM_QUERYENDSESSION and WM_ENDSESSION)
    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif
    // Install qDebug() message handler to route to debug.log
    qInstallMessageHandler(DebugMessageHandler);
#endif
    // Load GUI settings from QSettings
    app.createOptionsModel();

    // Subscribe to global signals from core
    uiInterface.InitMessage.connect(InitMessage);

    if (!GetArg(qt_opt::arg_no_splash) && !GetArg(qt_opt::arg_min))
        app.createSplashScreen(cryptonote::config::testnet);

    try
    {
        app.createWindow(cryptonote::config::testnet);
        app.requestInitialize();
#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
        WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("Pebblecoin Qt didn't yet exit safely..."), (HWND)app.getMainWinId());
#endif
        app.exec();
        app.requestShutdown();
        app.exec();
    } catch (std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    } catch (...) {
        PrintExceptionContinue(NULL, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    }
    return app.getReturnValue();
}
#endif // BITCOIN_QT_TEST
