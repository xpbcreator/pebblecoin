// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DPOSDIALOG_H
#define DPOSDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QModelIndex;
class QSortFilterProxyModel;
class QMenu;
QT_END_NAMESPACE

namespace cryptonote {
    struct bs_delegate_info;
}

namespace GUIUtil {
    class TableViewLastColumnResizingFixer;
}
namespace Ui {
    class DposDialog;
}
class OptionsModel;
class WalletModel;

/** Dialog for requesting payment of bitcoins */
class DposDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DposDialog(QWidget *parent = 0);
    ~DposDialog();

    void setModel(WalletModel *model);

    enum ColumnWidths {
        SELECTED_COLUMN_WIDTH = 23,
        RANK_COLUMN_WIDTH = 30,
        VOTES_COLUMN_WIDTH = 130,
        ID_COLUMN_WIDTH = 60,
        ADDRESS_MINIMUM_COLUMN_WIDTH = 200,
        AUTOSELECT_SCORE_COLUMN_WIDTH = 70,
    };
    
public slots:
    void clear();
    void reject();
    void accept();
    void setDelegateInfo(const cryptonote::bs_delegate_info& info);
    void autoSelectDelegatesToggled(bool checked);
    void onVoteTableClicked(const QModelIndex& index);
    void amountUnvotedChanged();
    
private:
    Ui::DposDialog *ui;
    WalletModel *model;
    QSortFilterProxyModel *proxyModel;
    
    QMenu *contextMenu;
    
    int cachedDelegateId;
    
private slots:
    void on_registerButton_clicked();
    void contextualMenu(const QPoint &);
    void copyAddress();

signals:
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);
};

#endif // DPOSDIALOG_H
