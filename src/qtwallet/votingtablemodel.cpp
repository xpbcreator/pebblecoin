// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QColor>

#include "include_base_utils.h"

#include "cryptonote_config.h"
#include "cryptonote_core/delegate_auto_vote.h"
#include "cryptonote_core/blockchain_storage.h"

#include "interface/main.h"
#include "interface/wallet.h"
#include "bitcoin/sync.h"

#include "bitcoinunits.h"
#include "votingtablemodel.h"
#include "votingrecord.h"

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {
    Qt::AlignLeft|Qt::AlignVCenter, /* Selected */
    Qt::AlignCenter|Qt::AlignVCenter, /* Rank */
    Qt::AlignRight|Qt::AlignVCenter, /* Votes */
    Qt::AlignLeft|Qt::AlignVCenter, /* ID */
    Qt::AlignLeft|Qt::AlignVCenter, /* Address */
    Qt::AlignRight|Qt::AlignVCenter, /* AutoselectScore */
};

/*// Comparison operator for sort/binary search of model tx list
struct VotingLessThan
{
    bool operator()(const VotingRecord &a, const VotingRecord &b) const
    {
        return a.info->delegate_id < b.info->delegate_id;
    }
    bool operator()(const VotingRecord &a, const cryptonote::delegate_id_t &b) const
    {
        return a.info->delegate_id < b;
    }
    bool operator()(const cryptonote::delegate_id_t &a, const VotingRecord &b) const
    {
        return a < b.info->delegate_id;
    }
};*/

// Private implementation
class VotingTablePriv
{
public:
    VotingTablePriv(CWallet *wallet, VotingTableModel *parent)
        : wallet(wallet)
        , parent(parent)
        , amountUnvoted(0)
    {
    }

    VotingTableModel *parent;
    CWallet *wallet;
    
    uint64_t amountUnvoted;

    /* Local cache of delegates. Sorted by delegate id.
     */
    QList<VotingRecord> cachedDelegates;

    /* Query entire delegates anew from core.
     */
    void refreshDelegates()
    {
        cachedDelegates.clear();
        
        auto infos = GetDelegateInfos();
        
        LOCK(wallet->cs_wallet);
        const auto& userDelegates = wallet->GetWallet2()->user_delegates();
        auto our_address = wallet->GetWallet2()->get_public_address();
        
        BOOST_FOREACH(const auto& info, infos)
        {
            cachedDelegates.append(VotingRecord(info, info.cached_vote_rank < cryptonote::config::dpos_num_delegates,
                                                info.cached_autoselect_rank < cryptonote::config::dpos_num_delegates,
                                                userDelegates.count(info.delegate_id) > 0,
                                                info.public_address == our_address));
        }
        amountUnvoted = wallet->GetWallet2()->get_amount_unvoted();
    }
    
    int size()
    {
        return cachedDelegates.size();
    }

    VotingRecord *index(int idx)
    {
        if(idx >= 0 && idx < cachedDelegates.size())
        {
            return &cachedDelegates[idx];
        }
        else
        {
            return 0;
        }
    }

    QString describe(VotingRecord *rec)
    {
        return QString("VotingRecord::describe not implemented yet");
    }
};

VotingTableModel::VotingTableModel(CWallet *wallet) :
        QAbstractTableModel(NULL),
        priv(new VotingTablePriv(wallet, this)),
        cachedNumDelegates(0)
{
    columns << QString() << tr("#") << tr("Votes") << tr("ID") << tr("Address") << tr("Score");

    priv->refreshDelegates();
}

VotingTableModel::~VotingTableModel() { };

int VotingTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int VotingTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QString VotingTableModel::formatTooltip(const VotingRecord *rec) const
{
    auto unit = BitcoinUnits::XPB;
    
    QString tooltip;
    tooltip += tr("Blocks Processed:") + " " + QString::number(rec->info->processed_blocks);
    tooltip += "\n" + tr("Blocks Missed:") + " " + QString::number(rec->info->missed_blocks);
    tooltip += "\n" + tr("Fees Received:") + " " + BitcoinUnits::formatWithUnit(unit, rec->info->fees_received);
    tooltip += "\n" + tr("Vote Rank:") + " " + QString::number(rec->info->cached_vote_rank + 1);
    tooltip += "\n" + tr("Autoselect Rank:") + " " + QString::number(rec->info->cached_autoselect_rank + 1);

    return tooltip;
}

QVariant VotingTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
    
    VotingRecord *rec = static_cast<VotingRecord*>(index.internalPointer());
    
    switch(role)
    {
    case Qt::DisplayRole:
        switch(index.column())
        {
        case Selected:
            return isVotingUserDelegates() ? rec->isUserSelected : rec->isAutoselected;
        case Rank:
            return (qint64)(rec->info->cached_vote_rank + 1);
        case Votes:
            return BitcoinUnits::formatWithUnit(BitcoinUnits::XPB, rec->info->total_votes);
        case ID:
            return QString::number(rec->info->delegate_id);
        case Address:
            return QString::fromStdString(rec->info->address_as_string);
        case AutoselectScore:
            return QString::number(cryptonote::get_delegate_rank(*rec->info), 'f', 4);
        }
        break;
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch(index.column())
        {
        case Selected:
            return isVotingUserDelegates() ? rec->isUserSelected : rec->isAutoselected;
        case Rank:
            return (qint64)(rec->info->cached_vote_rank);
        case Votes:
            return quint64(rec->info->total_votes);
        case ID:
            return quint16(rec->info->delegate_id);
        case Address:
            return QString::fromStdString(rec->info->address_as_string);
        case AutoselectScore:
            return cryptonote::get_delegate_rank(*rec->info);
        }
        break;
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
        if (rec->isThisWallet)
            return QColor(0, 0, 255);
        if (!rec->isTopDelegate)
            return QColor(128, 128, 128);
        break;
    case Qt::CheckStateRole:
        if (index.column() == Selected)
        {
            return (isVotingUserDelegates() ? rec->isUserSelected : rec->isAutoselected) ? Qt::Checked : Qt::Unchecked;
        }
        break;
    case LongDescriptionRole:
        return priv->describe(rec);
    case IsTopDelegateRole:
        return rec->isTopDelegate;
    case IsUserSelectedRole:
        return rec->isUserSelected;
    case IsAutoSelectedRole:
        return rec->isAutoselected;
    }
    
    return QVariant();
}

QVariant VotingTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if(role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        }
        else if(role == Qt::ToolTipRole)
        {
            switch(section)
            {
            case Selected:
                return tr("Whether you are voting for this delegate with your transactions.");
            case Rank:
                return tr("The delegate's vote rank.");
            case Votes:
                return tr("The total number of votes this delegate has received from the network.");
            case ID:
                return tr("The delegate's ID.");
            case Address:
                return tr("The delegate's public address.");
            case AutoselectScore:
                return tr("The automatically-calculated delegate score. It is the percentage of blocks the network is 95% sure the delegate will successfully process.");
            }
        }
    }
    return QVariant();
}

Qt::ItemFlags VotingTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;
    
    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    
    if (index.column() == Selected)
    {
        retval |= Qt::ItemIsUserCheckable;
        if (!isVotingUserDelegates())
            retval &= ~Qt::ItemIsEnabled;
    }
    
    return retval;
}

QModelIndex VotingTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    VotingRecord *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void VotingTableModel::updateDelegates()
{
    beginResetModel();
    priv->refreshDelegates();
    endResetModel();
    
    cachedNumDelegates = priv->cachedDelegates.size();
    emit amountUnvotedChanged();
}

bool VotingTableModel::isVotingUserDelegates() const
{
    //LOCK(priv->wallet->cs_wallet); // would cause too many locks/unlocks
    return priv->wallet->GetWallet2()->voting_user_delegates();
}

void VotingTableModel::setVotingUserDelegates(bool voting_user_delegates)
{
    {
        LOCK(priv->wallet->cs_wallet);
        priv->wallet->GetWallet2()->set_voting_user_delegates(voting_user_delegates);
    }
    emit dataChanged(index(0, Selected), index(priv->size() - 1, Selected));
}

void VotingTableModel::toggleUserDelegate(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    
    VotingRecord *rec = static_cast<VotingRecord*>(index.internalPointer());
    
    {
        LOCK(priv->wallet->cs_wallet);
        
        cryptonote::delegate_votes votes = priv->wallet->GetWallet2()->user_delegates();
        if (rec->isUserSelected)
        {
            votes.erase(rec->info->delegate_id);
            rec->isUserSelected = false;
        }
        else
        {
            votes.insert(rec->info->delegate_id);
            rec->isUserSelected = true;
        }
        priv->wallet->GetWallet2()->set_user_delegates(votes);
    }
    emit dataChanged(this->index(index.row(), Selected), this->index(index.row(), Selected));
}

uint64_t VotingTableModel::amountUnvoted() const
{
    return priv->amountUnvoted;
}
