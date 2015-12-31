// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>

#include <list>

#include <QDateTime>
#include <QDebug>
#include <QTimer>

#include "version.h"
#include "common/ui_interface.h"
#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_core.h"
#include "p2p/net_node.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"

#include "bitcoin/util.h"
#include "bitcoin/sync.h"
#include "interface/main.h"
#include "interface/init.h"
#include "interface/wallet.h"
#include "interface/qtversion.h"

#include "clientmodel.h"
#include "guiconstants.h"

static const int64_t nClientStartupTime = GetTime();

ClientModel::ClientModel(OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), optionsModel(optionsModel),
    cachedNumWalletBlocks(0),
    cachedNumDaemonBlocks(0),
    cachedNumBlocksOfPeers(0),
    cachedReindexing(0), cachedImporting(0),
    numBlocksAtStartup(-1), pollTimer(0)
{
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

ClientModel::~ClientModel()
{
    unsubscribeFromCoreSignals();
}

int ClientModel::getNumConnections(unsigned int flags) const
{
    LOCK(cs_main);
    if (!pnodeSrv)
    {
        LOG_PRINT_L0("getNumConnections: no pnodeSrv");
        return 0;
    }
    
    uint64_t nTotal = pnodeSrv->get_connections_count();
    uint64_t nOut = pnodeSrv->get_outgoing_connections_count();
    uint64_t nIn = nTotal - nOut;
    
    int res = 0;
    
    if (flags & CONNECTIONS_IN)
        res += nIn;
    if (flags & CONNECTIONS_OUT)
        res += nOut;
    
    // LOG_PRINT_L0("getNumConnections: flag is " << flags << ", in=" << nIn << ", out=" << nOut << ", res=" << res);
    
    return res;
}

int64_t ClientModel::getWalletProcessedHeight() const
{
    return WalletProcessedHeight();
}

int64_t ClientModel::getDaemonProcessedHeight() const
{
    return DaemonProcessedHeight();
}

/*int ClientModel::getNumBlocksAtStartup()
{
    if (numBlocksAtStartup == -1) numBlocksAtStartup = getNumBlocks();
    return numBlocksAtStartup;
}*/

quint64 ClientModel::getTotalBytesRecv() const
{
    return 1023; //CNode::GetTotalBytesRecv();
}

quint64 ClientModel::getTotalBytesSent() const
{
    return 1023; //CNode::GetTotalBytesSent();
}

QDateTime ClientModel::getLastDaemonBlockDate() const
{
    LOCK(cs_main);
    if (!pcore)
        return QDateTime::fromTime_t(1400884814); // ~ May 2015
    
    std::list<cryptonote::block> blocks;
    pcore->get_blocks(getDaemonProcessedHeight()-1, 1, blocks);
    return QDateTime::fromTime_t(blocks.front().timestamp);
}

QDateTime ClientModel::getLastWalletBlockDate() const
{
    LOCK(cs_main);
    if (!pcore)
        return QDateTime::fromTime_t(1400884814); // ~ May 2015
    
    std::list<cryptonote::block> blocks;
    pcore->get_blocks(getWalletProcessedHeight()-1, 1, blocks);
    return QDateTime::fromTime_t(blocks.front().timestamp);
}

double ClientModel::getVerificationProgress() const
{
    int64_t wallet = getWalletProcessedHeight();
    int64_t daemon = getDaemonProcessedHeight();
    int64_t total = getNumBlocksOfPeers();
    
    total = std::max(std::max(wallet, daemon), total);
    
    if (total == 0)
        return 0.0;
    
    double res = (double)(daemon * 0.75 + wallet * 0.25) / (double)total;
    
    LOG_PRINT_L0("Daemon=" << daemon << ", wallet=" << wallet << ", total=" << total << ", res=" << res);
    
    return res;
}

void ClientModel::updateTimer()
{
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    // Some quantities (such as number of blocks) change so fast that we don't want to be notified for each change.
    // Periodically check and update with a timer.
    int64_t newWalletBlocks = getWalletProcessedHeight();
    int64_t newDaemonBlocks = getDaemonProcessedHeight();
    int64_t newNumBlocksOfPeers = getNumBlocksOfPeers();

    // check for changed number of blocks we have, number of blocks peers claim to have, reindexing state and importing state
    if (cachedNumBlocksOfPeers != newNumBlocksOfPeers ||
        cachedReindexing != fReindex || cachedImporting != fImporting ||
        cachedNumWalletBlocks != newWalletBlocks || cachedNumDaemonBlocks != newDaemonBlocks)
    {
        cachedNumBlocksOfPeers = newNumBlocksOfPeers;
        cachedReindexing = fReindex;
        cachedImporting = fImporting;
        cachedNumWalletBlocks = newWalletBlocks;
        cachedNumDaemonBlocks = newDaemonBlocks;

        // ensure we return the maximum of newNumBlocksOfPeers and newNumBlocks to not create weird displays in the GUI
        emit numBlocksChanged(newDaemonBlocks, newWalletBlocks, std::max(std::max(newWalletBlocks, newDaemonBlocks), newNumBlocksOfPeers));
    }

    emit bytesChanged(getTotalBytesRecv(), getTotalBytesSent());
}

void ClientModel::updateNumConnections(int numConnections)
{
    emit numConnectionsChanged(numConnections);
}

void ClientModel::updateAlert(const QString &hash, int status)
{
    /*// Show error message notification for new alert
    if(status == CT_NEW)
    {
        uint256 hash_256;
        hash_256.SetHex(hash.toStdString());
        CAlert alert = CAlert::getAlertByHash(hash_256);
        if(!alert.IsNull())
        {
            emit message(tr("Network Alert"), QString::fromStdString(alert.strStatusBar), CClientUIInterface::ICON_ERROR);
        }
    }*/

    emit alertsChanged(getStatusBarWarnings());
}

QString ClientModel::getNetworkName() const
{
    return "main";
}

bool ClientModel::inInitialBlockDownload() const
{
    //return IsInitialBlockDownload()
    return true;
}

enum BlockSource ClientModel::getBlockSource() const
{
    if (fReindex)
        return BLOCK_SOURCE_REINDEX;
    else if (fImporting)
        return BLOCK_SOURCE_DISK;
    else if (getNumConnections() > 0)
        return BLOCK_SOURCE_NETWORK;

    return BLOCK_SOURCE_NONE;
}

int ClientModel::getNumBlocksOfPeers() const
{
    return NumBlocksOfPeers();
}

QString ClientModel::getStatusBarWarnings() const
{
    //return QString("ClientModel::getStatusBarWarnings() NYI");
    return "";
}

OptionsModel *ClientModel::getOptionsModel()
{
    return optionsModel;
}

QString ClientModel::formatFullVersion() const
{
    return QString::fromStdString(FormatFullVersion());
}

QString ClientModel::formatBuildDate() const
{
    return QString::fromStdString(CLIENT_DATE);
}

bool ClientModel::isReleaseVersion() const
{
    return PROJECT_VERSION_IS_RELEASE;
}

QString ClientModel::clientName() const
{
    return QString::fromStdString(CLIENT_NAME);
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromTime_t(nClientStartupTime).toString();
}

// Handlers for core signals
static void NotifyBlocksChanged(ClientModel *clientmodel)
{
    // This notification is too frequent. Don't trigger a signal.
    // Don't remove it, though, as it might be useful later.
}

static void NotifyNumConnectionsChanged(ClientModel *clientmodel, int newNumConnections)
{
    // Too noisy: qDebug() << "NotifyNumConnectionsChanged : " + QString::number(newNumConnections);
    QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
                              Q_ARG(int, newNumConnections));
}

/*static void NotifyAlertChanged(ClientModel *clientmodel, const uint256 &hash, ChangeType status)
{
    qDebug() << "NotifyAlertChanged : " + QString::fromStdString(hash.GetHex()) + " status=" + QString::number(status);
    QMetaObject::invokeMethod(clientmodel, "updateAlert", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(hash.GetHex())),
                              Q_ARG(int, status));
}*/

void ClientModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.NotifyBlocksChanged.connect(boost::bind(NotifyBlocksChanged, this));
    uiInterface.NotifyNumConnectionsChanged.connect(boost::bind(NotifyNumConnectionsChanged, this, _1));
    //uiInterface.NotifyAlertChanged.connect(boost::bind(NotifyAlertChanged, this, _1, _2));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.NotifyBlocksChanged.disconnect(boost::bind(NotifyBlocksChanged, this));
    uiInterface.NotifyNumConnectionsChanged.disconnect(boost::bind(NotifyNumConnectionsChanged, this, _1));
    //uiInterface.NotifyAlertChanged.disconnect(boost::bind(NotifyAlertChanged, this, _1, _2));
}
