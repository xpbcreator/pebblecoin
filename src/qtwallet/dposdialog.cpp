// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dposdialog.h"
#include "ui_dposdialog.h"

#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QMenu>

#include "include_base_utils.h"

#include "cryptonote_config.h"
#include "cryptonote_core/blockchain_storage.h"

#include "interface/main.h"
#include "guiutil.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "votingtablemodel.h"

DposDialog::DposDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DposDialog),
    model(0),
    proxyModel(NULL),
    cachedDelegateId(0)
{
    ui->setupUi(this);
  
#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->registerButton->setIcon(QIcon());
#endif
    
    ui->labelUnvoted->setText("N/A");
    
    // Actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);

    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    
    connect(ui->votingTableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
}

void DposDialog::setModel(WalletModel *model)
{
    this->model = model;
    
    if (!model)
        return;
  
    connect(model, SIGNAL(delegateInfoChanged(const cryptonote::bs_delegate_info&)),
            this, SLOT(setDelegateInfo(const cryptonote::bs_delegate_info&)));

    proxyModel = new QSortFilterProxyModel(this);
    
    proxyModel->setSourceModel(model->getVotingTableModel());
    proxyModel->setDynamicSortFilter(true);
    
    proxyModel->setSortRole(Qt::EditRole);
    
    ui->votingTableView->setModel(proxyModel);
    ui->votingTableView->sortByColumn(VotingTableModel::Votes, Qt::DescendingOrder);
    
    ui->votingTableView->setColumnWidth(VotingTableModel::Selected, SELECTED_COLUMN_WIDTH);
    ui->votingTableView->setColumnWidth(VotingTableModel::Rank, RANK_COLUMN_WIDTH);
    ui->votingTableView->setColumnWidth(VotingTableModel::Votes, VOTES_COLUMN_WIDTH);
    ui->votingTableView->setColumnWidth(VotingTableModel::ID, ID_COLUMN_WIDTH);
    ui->votingTableView->setColumnWidth(VotingTableModel::Address, ADDRESS_MINIMUM_COLUMN_WIDTH);
    ui->votingTableView->setColumnWidth(VotingTableModel::AutoselectScore, AUTOSELECT_SCORE_COLUMN_WIDTH);
    
    ui->votingTableView->horizontalHeader()->setResizeMode(VotingTableModel::Address, QHeaderView::Stretch);

    ui->autoselectDelegates->setChecked(!model->getVotingTableModel()->isVotingUserDelegates());
    
    connect(ui->autoselectDelegates, SIGNAL(toggled(bool)),
            this, SLOT(autoSelectDelegatesToggled(bool)));
    
    connect(ui->votingTableView, SIGNAL(clicked(const QModelIndex&)),
            this, SLOT(onVoteTableClicked(const QModelIndex&)));
    
    connect(model->getVotingTableModel(), SIGNAL(amountUnvotedChanged()),
            this, SLOT(amountUnvotedChanged()));
    amountUnvotedChanged();
}

DposDialog::~DposDialog()
{
    delete ui;
    if (proxyModel)
        delete proxyModel;
}

void DposDialog::clear()
{
}

void DposDialog::reject()
{
    clear();
}

void DposDialog::accept()
{
    clear();
}

void DposDialog::autoSelectDelegatesToggled(bool checked)
{
    model->getVotingTableModel()->setVotingUserDelegates(!checked);
}

void DposDialog::onVoteTableClicked(const QModelIndex& proxyIndex)
{
    QModelIndex voteIndex = proxyModel->mapToSource(proxyIndex);
    
    if (voteIndex.column() == VotingTableModel::Selected
        && model->getVotingTableModel()->isVotingUserDelegates())
    {
        model->getVotingTableModel()->toggleUserDelegate(voteIndex);
    }
}

void DposDialog::amountUnvotedChanged()
{
    if (!model)
        return;
    
    int unit = model->getOptionsModel()->getDisplayUnit();
    
    ui->labelUnvoted->setText(BitcoinUnits::formatWithUnit(unit, model->getVotingTableModel()->amountUnvoted()));
    
    // always fired on each block update, so just cheat and update here
    if (model->getCachedNumBlocks() < (int)cryptonote::config::dpos_registration_start_block)
    {
        ui->labelDelegateId->setText(tr("Delegate registration starts in %1 blocks").arg(
            cryptonote::config::dpos_registration_start_block - model->getCachedNumBlocks()));
        ui->registerButton->setVisible(false);
    }
    else {
        if (cachedDelegateId == 0) {
            ui->labelDelegateId->setText(tr("Not a delegate - register below"));
            ui->registerButton->setVisible(true);
        }
        else {
            ui->labelDelegateId->setText(QString::number(cachedDelegateId));
            ui->registerButton->setVisible(false);
        }
    }
}

void DposDialog::setDelegateInfo(const cryptonote::bs_delegate_info& info)
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    
    cachedDelegateId = info.delegate_id;
    
    if (info.delegate_id == 0)
    {
        ui->labelDelegateId->setText(tr("Not a delegate - register below"));
        ui->registerButton->setVisible(true);
        
        ui->labelTotalVotes->setText(tr("N/A"));
        ui->labelBlocksProcessed->setText(tr("N/A"));
        ui->labelBlocksMissed->setText(tr("N/A"));
        ui->labelFeesReceived->setText(tr("N/A"));
        ui->labelVoteRank->setText(tr("N/A"));
        ui->labelAutoselectRank->setText(tr("N/A"));
    }
    else
    {
        ui->labelDelegateId->setText(QString::number(info.delegate_id));
        ui->registerButton->setVisible(false);
        
        ui->labelTotalVotes->setText(BitcoinUnits::formatWithUnit(unit, info.total_votes));
        ui->labelBlocksProcessed->setText(QString::number(info.processed_blocks));
        ui->labelBlocksMissed->setText(QString::number(info.missed_blocks));
        ui->labelFeesReceived->setText(BitcoinUnits::formatWithUnit(unit, info.fees_received));
        ui->labelVoteRank->setText(QString::number(info.cached_vote_rank + 1));
        ui->labelAutoselectRank->setText(QString::number(info.cached_autoselect_rank + 1));
    }
}

void DposDialog::on_registerButton_clicked()
{
    if(!model || !model->getOptionsModel())
        return;

    QList<SendCoinsRecipient> recipients;
    cryptonote::delegate_id_t delegateId;
    uint64_t registrationFee;
    
    if (!GetDposRegisterInfo(delegateId, registrationFee))
    {
        return;
    }

    // Format confirmation message
    QStringList formatted;
    formatted.append(tr("Register to be delegate #%1 for %2").arg(delegateId).arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), registrationFee)));

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        return;
    }

    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    currentTransaction.setRegisteringDelegate(delegateId, registrationFee);
    WalletModel::SendCoinsReturn prepareStatus;
    prepareStatus = model->prepareTransaction(currentTransaction);

    // process prepareStatus and on error generate message shown to user
    model->processSendCoinsReturn(prepareStatus, BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));

    if(prepareStatus.status != WalletModel::OK) {
        return;
    }

    currentTransaction.setFakeOuts(0, 5);
    
    qint64 txFee = currentTransaction.getTransactionFee();
    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><br />%1");

    if(txFee > 0)
    {
        // append fee string if a fee is required
        questionString.append("<hr /><span style='color:#aa0000;'>");
        questionString.append(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span> ");
        questionString.append(tr("added as transaction fee"));
    }

    // add total amount in all subdivision units
    questionString.append("<hr />");
    qint64 totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
    QStringList alternativeUnits;
    foreach(BitcoinUnits::Unit u, BitcoinUnits::availableUnits())
    {
        if(u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcoinUnits::formatWithUnit(u, totalAmount));
    }
    questionString.append(tr("Total Amount %1 (= %2)")
        .arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount))
        .arg(alternativeUnits.join(" " + tr("or") + " ")));

    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm registration"),
        questionString.arg(formatted.join("<br />")),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        return;
    }
  
    // now send the prepared transaction
    WalletModel::SendCoinsReturn sendStatus = model->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    model->processSendCoinsReturn(sendStatus);

    if (sendStatus.status == WalletModel::OK)
    {
        accept();
    }
}

void DposDialog::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->votingTableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void DposDialog::copyAddress()
{
    GUIUtil::copyEntryData(ui->votingTableView, VotingTableModel::Address, Qt::EditRole);
}

/*void TransactionView::showDetails()
{
    if(!transactionView->selectionModel())
        return;
    QModelIndexList selection = transactionView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        TransactionDescDialog dlg(selection.at(0));
        dlg.exec();
    }
}
void TransactionView::focusTransaction(const QModelIndex &idx)
{
    if(!transactionProxyModel)
        return;
    QModelIndex targetIdx = transactionProxyModel->mapFromSource(idx);
    transactionView->scrollTo(targetIdx);
    transactionView->setCurrentIndex(targetIdx);
    transactionView->setFocus();
}*/
