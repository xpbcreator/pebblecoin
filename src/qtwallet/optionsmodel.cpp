// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

#include "cryptonote_config.h"

#include "bitcoin/util.h"
#include "interface/init.h"
#include "interface/main.h"
#ifdef ENABLE_WALLET
#include "interface/wallet.h"
#endif

#include "qt_options.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"


OptionsModel::OptionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
    Init();
}

// Writes all missing QSettings with their default values
void OptionsModel::Init()
{
    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool();

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::XPB);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("bDisplayAddresses"))
        settings.setValue("bDisplayAddresses", false);
    bDisplayAddresses = settings.value("bDisplayAddresses", false).toBool();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //
    // If setting doesn't exist create it with defaults.
    //
    // If SoftSetArg() or SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.

    // Main
    /*if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");*/

    /*if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");*/

    // Wallet
#ifdef ENABLE_WALLET
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_FEE); //DEFAULT_TRANSACTION_FEE);
    nTransactionFee = settings.value("nTransactionFee").toLongLong(); // if -paytxfee is set, this will be overridden later in init.cpp
    /*if (mapArgs.count("-paytxfee"))
        addOverriddenOption("-paytxfee");

    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");*/
#endif

    /*// Network
    if (!settings.contains("fUseUPnP"))
#ifdef USE_UPNP
        settings.setValue("fUseUPnP", true);
#else
        settings.setValue("fUseUPnP", false);
#endif
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", "127.0.0.1:9050");
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    if (!settings.contains("nSocksVersion"))
        settings.setValue("nSocksVersion", 5);
    // Only try to set -socks, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-socks", settings.value("nSocksVersion").toString().toStdString()))
        addOverriddenOption("-socks");*/

    // Display
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!SoftSetArg(qt_opt::arg_lang, settings.value("language").toString().toStdString()))
        addOverriddenOption(qt_opt::arg_lang);

    language = settings.value("language").toString();
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Remove all entries from our QSettings object
    settings.clear();

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            LOG_PRINT_L0("StartAtStartup");
            return GUIUtil::GetStartOnSystemStartup();
        case MinimizeToTray:
            LOG_PRINT_L0("MinimizeToTray");
            return fMinimizeToTray;
        case MapPortUPnP:
            LOG_PRINT_L0("MapPortUPnP");
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            LOG_PRINT_L0("MinimizeOnClose");
            return fMinimizeOnClose;

        // default proxy
        case ProxyUse:
            LOG_PRINT_L0("ProxyUse");
            return settings.value("fUseProxy", false);
        /*case ProxyIP: {
            LOG_PRINT_L0("ProxyIP");
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {
            LOG_PRINT_L0("ProxyPort");
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }
        case ProxySocksVersion:
            LOG_PRINT_L0("ProxySocksVersion");
            return settings.value("nSocksVersion", 5);*/

#ifdef ENABLE_WALLET
        case Fee:
            LOG_PRINT_L0("Fee");
            // Attention: Init() is called before nTransactionFee is set in AppInit2()!
            // To ensure we can change the fee on-the-fly update our QSetting when
            // opening OptionsDialog, which queries Fee via the mapper.
            if (nTransactionFee != settings.value("nTransactionFee").toLongLong())
                settings.setValue("nTransactionFee", (qint64)nTransactionFee);
            // Todo: Consider to revert back to use just nTransactionFee here, if we don't want
            // -paytxfee to update our QSettings!
            return settings.value("nTransactionFee");
        case SpendZeroConfChange:
            LOG_PRINT_L0("SpendZeroConfChange");
            return settings.value("bSpendZeroConfChange");
#endif
        case DisplayUnit:
            LOG_PRINT_L0("DisplayUnit");
            return nDisplayUnit;
        case DisplayAddresses:
            LOG_PRINT_L0("DisplayAddresses");
            return bDisplayAddresses;
        case ThirdPartyTxUrls:
            LOG_PRINT_L0("ThirdPartyTxUrls");
            return strThirdPartyTxUrls;
        case Language:
            LOG_PRINT_L0("Language");
            return settings.value("language");
        case CoinControlFeatures:
            LOG_PRINT_L0("CoinControlFeatures");
            return fCoinControlFeatures;
        //case DatabaseCache:
        //    return settings.value("nDatabaseCache");
        //case ThreadsScriptVerif:
        //    return settings.value("nThreadsScriptVerif");
         
        default:
            LOG_PRINT_L0("StartAtStartup");
            return QVariant();
        }
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            //MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        // default proxy
        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        /*case ProxyIP: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
        case ProxyPort: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
        case ProxySocksVersion: {
            if (settings.value("nSocksVersion") != value) {
                settings.setValue("nSocksVersion", value.toInt());
                setRestartRequired(true);
            }
        }
        break;*/
#ifdef ENABLE_WALLET
        case Fee: // core option - can be changed on-the-fly
            // Todo: Add is valid check  and warn via message, if not
            nTransactionFee = value.toLongLong();
            settings.setValue("nTransactionFee", (qint64)nTransactionFee);
            emit transactionFeeChanged(nTransactionFee);
            break;
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
#endif
        case DisplayUnit:
            nDisplayUnit = value.toInt();
            settings.setValue("nDisplayUnit", nDisplayUnit);
            emit displayUnitChanged(nDisplayUnit);
            break;
        case DisplayAddresses:
            bDisplayAddresses = value.toBool();
            settings.setValue("bDisplayAddresses", bDisplayAddresses);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            emit coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        /*case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;*/
        default:
            break;
        }
    }
    emit dataChanged(index, index);

    return successful;
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    /*proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        if (curProxy.second == 5) {
            proxy.setType(QNetworkProxy::Socks5Proxy);
            proxy.setHostName(QString::fromStdString(curProxy.first.ToStringIP()));
            proxy.setPort(curProxy.first.GetPort());

            return true;
        }
        else
            return false;
    }
    else
        proxy.setType(QNetworkProxy::NoProxy);

    return true;*/
    proxy.setType(QNetworkProxy::NoProxy);
    return true;  
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}
