// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTINGTABLEMODEL_H
#define VOTINGTABLEMODEL_H

#include <memory>

#include <QAbstractTableModel>
#include <QStringList>

#include "common/types.h"

class CWallet;
class VotingRecord;
class VotingTablePriv;
class WalletModel;

/** UI model for the transaction table of a wallet.
 */
class VotingTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VotingTableModel(CWallet *wallet);
    ~VotingTableModel();

    enum ColumnIndex {
        Selected = 0,
        Rank = 1,
        Votes = 2,
        ID = 3,
        Address = 4,
        AutoselectScore = 5,
    };

    /** Roles to get specific information from a voting row.
        These are independent of column.
    */
    enum RoleIndex {
      /** Long description (HTML format) */
        LongDescriptionRole = Qt::UserRole,
        /** Is a top delegate? */
        IsTopDelegateRole,
        /** Is user-selected? */
        IsUserSelectedRole,
        /** Is auto-selected? */
        IsAutoSelectedRole,
    };

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    
    bool isVotingUserDelegates() const;
    void setVotingUserDelegates(bool voting_user_delegates);
    uint64_t amountUnvoted() const;
    
private:
    QStringList columns;
    std::unique_ptr<VotingTablePriv> priv;
    
    size_t cachedNumDelegates;

    QString formatTooltip(const VotingRecord *rec) const;
    
    /*QString lookupAddress(const std::string &address, bool tooltip) const;
    QVariant addressColor(const TransactionRecord *wtx) const;
    QString formatTxStatus(const TransactionRecord *wtx) const;
    QString formatTxDate(const TransactionRecord *wtx) const;
    QString formatTxType(const TransactionRecord *wtx) const;
    QString formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const;
    QString formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed=true) const;
    QVariant txStatusDecoration(const TransactionRecord *wtx) const;
    QVariant txAddressDecoration(const TransactionRecord *wtx) const;*/

public slots:
    void updateDelegates();
    void toggleUserDelegate(const QModelIndex &index);

    friend class VotingTablePriv;

signals:
    void amountUnvotedChanged();
};

#endif // VOTINGTABLEMODEL_H
