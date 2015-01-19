// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QGroupBox>
#include <QTimer>
#include <QDate>
#include <QFileDialog>
#include <QTextStream>
#include <QTextCursor>
#include <QMessageBox>
#include "cv/Parser.h"
#include "cv/Session.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/SearchBar.h"
#include "cv/gui/ChannelListWindow.h"
#include "cv/gui/ChannelTopicDelegate.h"

namespace cv { namespace gui {

ChannelListWindow::ChannelListWindow(Session *pSession, const QSize &size/* = QSize(715, 300)*/)
    : Window("Channel List", size),
      m_searchStr(),
      m_searchRegex(),
      m_savedMinUsers(0),
      m_savedMaxUsers(0)
{
    m_pSession = pSession;

    m_pVLayout = new QVBoxLayout;
    m_pView = new QTreeView(this);
    m_pModel = new QStandardItemModel(this);
    m_pDelegate = new ChannelTopicDelegate(m_pView);

    m_pVLayout->addWidget(m_pView);
    m_pView->setModel(m_pModel);

    QStringList list;
    list.append("Name");
    list.append("# Users");
    list.append("Topic");
    m_pModel->setHorizontalHeaderLabels(list);

    m_pView->setColumnWidth(0, 175);
    m_pView->setColumnWidth(1, 50);
    m_pView->setItemDelegateForColumn(2, m_pDelegate);
    m_pView->setRootIsDecorated(false);
    m_pView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QObject::connect(m_pSession, SIGNAL(connected()), this, SLOT(handleConnect()));
    QObject::connect(m_pSession, SIGNAL(disconnected()), this, SLOT(handleDisconnect()));
    QObject::connect(m_pView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(joinChannel(const QModelIndex &)));

    setupControls();
    setLayout(m_pVLayout);
}

//-----------------------------------//

void ChannelListWindow::giveFocus()
{
    if(m_pView)
        m_pView->setFocus();
}

//-----------------------------------//

// Performs anything that needs to be done before
// channels are added.
void ChannelListWindow::beginPopulatingList()
{
    m_populatingList = true;
    m_pFilteringGroup->setEnabled(false);

    // Disable controls in the download group.
    m_pCustomParameters->setEnabled(false);
    m_pTopicDisplay->setEnabled(false);
    m_pDownloadListButton->setEnabled(false);
    m_pStopDownloadButton->setEnabled(true);

    m_pSaveButton->setEnabled(false);

    // Reset for saving to the text file.
    m_numVisible = 0;
    m_savedTopicDisplay = (m_pTopicDisplay->checkState() == Qt::Checked);
}

//-----------------------------------//

// Adds a channel to the QTreeWidget.
void ChannelListWindow::addChannel(const QString &channel, const QString &numUsers, const QString &topic)
{
    if(m_populatingList)
    {
        QList<QStandardItem *> list;
        list.append(new QStandardItem(channel));
        list.append(new QStandardItem(numUsers));

        QStandardItem *pItem = new QStandardItem;

        QString strippedTopic = stripCodes(topic);
        if(m_pTopicDisplay->checkState() == Qt::Checked)
        {
            pItem->setData(topic, Qt::DisplayRole);
        }
        else
        {
            pItem->setData(Qt::escape(strippedTopic), Qt::DisplayRole);
        }
        pItem->setSizeHint(QFontMetrics(m_pView->font()).size(Qt::TextSingleLine, strippedTopic));
        list.append(pItem);

        m_pModel->appendRow(list);
        /*
        if(m_pModel->rowCount() > 0)
        {
            QStandardItem *pCurrItem;

            // find the position to insert
            int rowToInsertBefore = 0;
            while(true)
            {
                pCurrItem = m_pModel->item(rowToInsertBefore, 0);
                if(!pCurrItem)
                {
                    m_pModel->appendRow(list);
                    break;
                }

                if(channel.compare(pCurrItem->text(), Qt::CaseInsensitive) < 0)
                {
                    m_pModel->insertRow(rowToInsertBefore, list);
                    break;
                }

                ++rowToInsertBefore;
            }
        }
        else
        {
            m_pModel->appendRow(list);
        }
        */
        m_pChannelsLabel->setText(QString("%1 / %1 Channels").arg(m_pModel->rowCount()));
        ++m_numVisible;
    }
}

//-----------------------------------//

// Performs anything that needs to be done when all
// channels have been added.
void ChannelListWindow::endPopulatingList()
{
    m_populatingList = false;
    m_pFilteringGroup->setEnabled(true);

    // Re-enable controls in the download group.
    m_pCustomParameters->setEnabled(true);
    m_pTopicDisplay->setEnabled(true);
    m_pDownloadListButton->setEnabled(true);
    m_pStopDownloadButton->setEnabled(false);

    m_pSaveButton->setEnabled(true);

    if(m_pView)
        m_pView->resizeColumnToContents(2);
}

//-----------------------------------//

// Clears the list of all channels.
void ChannelListWindow::clearList()
{
    m_pModel->clear();

    QStringList list;
    list.append("Name");
    list.append("# Users");
    list.append("Topic");
    m_pModel->setHorizontalHeaderLabels(list);

    m_pChannelsLabel->setText("0 / 0 Channels");
    m_pSaveButton->setEnabled(false);
}

//-----------------------------------//

// Sets up all the controls that will be used to interact
// with the channel list.
void ChannelListWindow::setupControls()
{
    m_pControlsSection = new QWidget(this);
    m_pControlsSection->setMinimumSize(0, 155);
    m_pControlsSection->setFocusPolicy(Qt::StrongFocus);
    m_pVLayout->insertWidget(0, m_pControlsSection);

    m_pDownloadingGroup = new QGroupBox("Downloading Channels", m_pControlsSection);
    m_pDownloadingGroup->setGeometry(QRect(0, 0, 301, 111));
    m_pDownloadingGroup->setFocusPolicy(Qt::StrongFocus);

    m_pCustomParameters = new CLineEdit("List Parameters (optional)", m_pDownloadingGroup);
    m_pCustomParameters->setGeometry(QRect(10, 20, 281, 21));
    QObject::connect(m_pCustomParameters, SIGNAL(returnPressed()), this, SLOT(downloadList()));

    m_pTopicDisplay = new QCheckBox("Display control codes in topics", m_pDownloadingGroup);
    m_pTopicDisplay->setGeometry(QRect(20, 50, 171, 16));
    m_pTopicDisplay->setCheckState(Qt::Checked);
    m_savedTopicDisplay = true;

    m_pDownloadListButton = new QPushButton("Download List", m_pDownloadingGroup);
    m_pDownloadListButton->setGeometry(QRect(120, 70, 101, 31));
    QObject::connect(m_pDownloadListButton, SIGNAL(clicked()), this, SLOT(downloadList()));

    m_pStopDownloadButton = new QPushButton("Stop", m_pDownloadingGroup);
    m_pStopDownloadButton->setGeometry(QRect(230, 70, 61, 31));
    m_pStopDownloadButton->setEnabled(false);
    QObject::connect(m_pStopDownloadButton, SIGNAL(clicked()), this, SLOT(stopDownload()));

    m_pChannelsLabel = new QLabel("0 / 0 Channels", m_pControlsSection);
    m_pChannelsLabel->setGeometry(QRect(0, 135, 201, 20));
    QFont newFont = m_pChannelsLabel->font();
    newFont.setBold(true);
    m_pChannelsLabel->setFont(newFont);

    m_pSaveButton = new QPushButton("Save List...", m_pControlsSection);
    m_pSaveButton->setGeometry(QRect(210, 120, 91, 31));
    m_pSaveButton->setEnabled(false);
    QObject::connect(m_pSaveButton, SIGNAL(clicked()), this, SLOT(saveList()));

    m_pFilteringGroup = new QGroupBox("Filtering Channels", m_pControlsSection);
    m_pFilteringGroup->setGeometry(QRect(316, 0, 381, 151));
    m_pFilteringGroup->setFocusPolicy(Qt::StrongFocus);

    m_pSearchBar = new SearchBar(m_pFilteringGroup);
    m_pSearchBar->setGeometry(QRect(10, 20, 361, 20));
    QObject::connect(m_pSearchBar, SIGNAL(returnPressed()), this, SLOT(startFilter()));

    m_pSearchTimer = new QTimer;
    m_pSearchTimer->setSingleShot(false);
    QObject::connect(m_pSearchTimer, SIGNAL(timeout()), this, SLOT(performSearchIteration()));

    m_pCheckChanNames = new QCheckBox("Check channel names", m_pFilteringGroup);
    m_pCheckChanNames->setGeometry(QRect(10, 40, 131, 21));
    m_pCheckChanNames->setChecked(true);

    m_pUseRegExp = new QCheckBox("Use Regular Expressions", m_pFilteringGroup);
    m_pUseRegExp->setGeometry(QRect(230, 40, 141, 21));

    m_pCheckChanTopics = new QCheckBox("Check channel topics", m_pFilteringGroup);
    m_pCheckChanTopics->setGeometry(QRect(10, 60, 131, 21));

    m_pMinUsersLabel = new QLabel("Min # Users:", m_pFilteringGroup);
    m_pMinUsersLabel->setGeometry(QRect(30, 90, 71, 21));

    m_pMinUsers = new QSpinBox(m_pFilteringGroup);
    m_pMinUsers->setGeometry(QRect(100, 90, 61, 20));
    m_pMinUsers->setMaximum(99999);

    m_pMaxUsersLabel = new QLabel("Max # Users:", m_pFilteringGroup);
    m_pMaxUsersLabel->setGeometry(QRect(30, 110, 71, 21));

    m_pMaxUsers = new QSpinBox(m_pFilteringGroup);
    m_pMaxUsers->setGeometry(QRect(100, 112, 61, 20));
    m_pMaxUsers->setMaximum(99999);

    m_pApplyFilterButton = new QPushButton("Apply Filter", m_pFilteringGroup);
    m_pApplyFilterButton->setGeometry(QRect(220, 110, 81, 31));
    QObject::connect(m_pApplyFilterButton, SIGNAL(clicked()), this, SLOT(startFilter()));

    m_pStopFilterButton = new QPushButton("Stop", m_pFilteringGroup);
    m_pStopFilterButton->setGeometry(QRect(310, 110, 61, 31));
    m_pStopFilterButton->setEnabled(false);
    QObject::connect(m_pStopFilterButton, SIGNAL(clicked()), this, SLOT(stopFilter()));
}

//-----------------------------------//

// Handles a connection fired from the Connection object.
void ChannelListWindow::handleConnect()
{
    clearList();
    m_pDownloadingGroup->setEnabled(true);
}

//-----------------------------------//

// Handles a disconnection fired from the Connection object.
void ChannelListWindow::handleDisconnect()
{
    m_pDownloadingGroup->setEnabled(false);
    m_pFilteringGroup->setEnabled(true);
    m_pSaveButton->setEnabled(true);
    if(m_pView)
    {
        m_pView->resizeColumnToContents(2);
    }
}

//-----------------------------------//

// Requests a new list of channels from the server.
void ChannelListWindow::downloadList()
{
    // If we're currently in a search, cancel it.
    if(m_pStopFilterButton->isEnabled())
    {
        m_pSearchTimer->stop();
        m_pSearchBar->clearProgress();
        m_pStopFilterButton->setEnabled(false);
    }

    QString textToSend = "LIST";
    if(m_pCustomParameters->text().size() > 0)
    {
        textToSend += QString(" %1").arg(m_pCustomParameters->text());
    }
    m_pSession->sendData(textToSend);
}

//-----------------------------------//

// Requests to stop the download of the channels list from the server.
void ChannelListWindow::stopDownload()
{
    m_pSession->sendData("LIST STOP");
}

//-----------------------------------//

// Joins the channel which is found with the given index.
void ChannelListWindow::joinChannel(const QModelIndex &index)
{
    // TODO (seand): Be able to disconnect when the connection is lost;
    // this isn't sufficient.
    if(index.isValid())
    {
        m_pSession->sendData(QString("JOIN :%1").arg(index.sibling(index.row(), 0).data().toString()));
    }
}

//-----------------------------------//

// Searches for the given string within the list of channels.
void ChannelListWindow::startFilter()
{
    // Cover the case where the user enters a new query mid-search.
    if(m_pSearchTimer->isActive())
    {
        m_pSearchTimer->stop();
        m_pSearchBar->clearProgress();
    }

    // Reset the string variables.
    m_searchStr = "";
    m_searchRegex = QRegExp();

    // Set the min/max variables.
    m_savedMinUsers = m_pMinUsers->value();
    m_savedMaxUsers = m_pMaxUsers->value();

    // If there are no constraints, display all the channels.
    if((m_pSearchBar->text().isEmpty()
            || (m_pCheckChanNames->checkState() == Qt::Unchecked
            && m_pCheckChanTopics->checkState() == Qt::Unchecked))
        && m_savedMinUsers == 0
        && m_savedMaxUsers == 0)
    {
        for(int i = 0; i < m_pModel->rowCount(); ++i)
        {
            m_pView->setRowHidden(i, QModelIndex(), false);
        }

        m_pChannelsLabel->setText(QString("%1 / %1 Channels").arg(m_pModel->rowCount()));
        m_numVisible = m_pModel->rowCount();
        return;
    }

    // Enable & disable controls.
    m_pStopFilterButton->setEnabled(true);
    m_pApplyFilterButton->setEnabled(false);
    m_pSearchBar->setEnabled(false);
    m_pCheckChanNames->setEnabled(false);
    m_pCheckChanTopics->setEnabled(false);
    m_pUseRegExp->setEnabled(false);
    m_pMinUsersLabel->setEnabled(false);
    m_pMinUsers->setEnabled(false);
    m_pMaxUsersLabel->setEnabled(false);
    m_pMaxUsers->setEnabled(false);
    if(m_pSession->isConnected())
        m_pDownloadingGroup->setEnabled(false);
    m_pSaveButton->setEnabled(false);

    // Hide all the channels.
    for(int i = 0; i < m_pModel->rowCount(); ++i)
        m_pView->setRowHidden(i, QModelIndex(), true);

    // Set the search string & regex.
    if(m_pUseRegExp->checkState() == Qt::Checked)
        m_searchRegex = QRegExp(m_pSearchBar->text(), Qt::CaseInsensitive);
    else
        m_searchStr = m_pSearchBar->text();

    // Initialize other variables.
    m_currChannel = 0;
    m_numVisible = 0;

    // Start the timer.
    m_pSearchTimer->setInterval(0);
    m_pSearchTimer->start();
}

//-----------------------------------//

// Performs one iteration of a search from the search bar.
void ChannelListWindow::performSearchIteration()
{
    int i;
    for(i = m_currChannel; i < m_currChannel + 30 && i < m_pModel->rowCount(); ++i)
    {
        m_pChannelsLabel->setText(QString("%1 / %2 Channels").arg(m_numVisible).arg(m_pModel->rowCount()));

        // Make sure the number of users is within the range of the min and max
        // users first; if it isn't, we continue so the result isn't included
        // in the display.
        int numUsers = m_pModel->item(i, 1)->text().toInt();
        if(m_savedMinUsers > 0)
        {
            if(numUsers < m_savedMinUsers)
                continue;
        }

        if(m_savedMaxUsers > 0)
        {
            if(numUsers > m_savedMaxUsers)
                continue;
        }

        // Then check the actual name or topic for a string match.
        if(!m_pSearchBar->text().isEmpty())
        {
            bool containsStr = false;
            if(m_pCheckChanNames->isChecked())
            {
                QStandardItem *pNameItem = m_pModel->item(i, 0);

                if(m_pUseRegExp->checkState() == Qt::Checked)
                {
                    containsStr = pNameItem->text().contains(m_searchRegex);
                }
                else
                {
                    containsStr = pNameItem->text().contains(m_searchStr, Qt::CaseInsensitive);
                }
            }

            // If [containsStr] is true, then it was already found in the name,
            // so this short-circuits and fails.
            if(!containsStr && m_pCheckChanTopics->isChecked())
            {
                QTextDocument doc;
                QStandardItem *pTopicItem = m_pModel->item(i, 2);
                doc.setHtml(pTopicItem->data(Qt::DisplayRole).toString());

                if(m_pUseRegExp->checkState() == Qt::Checked)
                {
                    containsStr = doc.toPlainText().contains(m_searchRegex);
                }
                else
                {
                    containsStr = doc.toPlainText().contains(m_searchStr, Qt::CaseInsensitive);
                }
            }

            if(!containsStr)
                continue;
        }

        m_pView->setRowHidden(i, QModelIndex(), false);
        ++m_numVisible;
    }

    m_pSearchBar->updateProgress((double) m_currChannel / m_pModel->rowCount());

    // Stop the timer if we've completed the search.
    if(i == m_pModel->rowCount())
        stopFilter();
    else
        m_currChannel = i;

    m_pChannelsLabel->setText(QString("%1 / %2 Channels").arg(m_numVisible).arg(m_pModel->rowCount()));
}

//-----------------------------------//

void ChannelListWindow::stopFilter()
{
    m_pSearchTimer->stop();
    m_pSearchBar->clearProgress();

    // Enable & disable controls.
    m_pStopFilterButton->setEnabled(false);
    m_pApplyFilterButton->setEnabled(true);
    m_pSearchBar->setEnabled(true);
    m_pCheckChanNames->setEnabled(true);
    m_pCheckChanTopics->setEnabled(true);
    m_pUseRegExp->setEnabled(true);
    m_pMinUsersLabel->setEnabled(true);
    m_pMinUsers->setEnabled(true);
    m_pMaxUsersLabel->setEnabled(true);
    m_pMaxUsers->setEnabled(true);
    if(m_pSession->isConnected())
        m_pDownloadingGroup->setEnabled(true);
    m_pSaveButton->setEnabled(true);
}

//-----------------------------------//

// Saves the entire list to a file.
void ChannelListWindow::saveList()
{
    if(m_pModel->rowCount() == 0)
        return;

    // Initialize the file name to be the server name with the current date.
    QString fileName = QString("/chanlist-%1-%2.txt")
                    .arg(m_pManager->getParentWindow(this)->getWindowName())
                    .arg(QDate::currentDate().toString("MM-dd-yyyy"));
    fileName = QFileDialog::getSaveFileName(this, "Save Channel List", fileName);
    if(fileName.isNull())
        return;

    QFile file(fileName);
    QMessageBox msgBox(QMessageBox::Critical, "File Operation Failed", "");
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        if(file.error() == QFile::PermissionsError)
        {
            msgBox.setText("The file could not be accessed.");
        }
        else
        {
            msgBox.setText("The file could not be opened.");
        }

        msgBox.exec();
        return;
    }

    // Write the filter information.
    QTextStream out(&file);

    if(!m_searchStr.isEmpty())
    {
        out << "[Filter] Search string: " << m_searchStr << '\n';
    }
    else if(!m_searchRegex.isEmpty())
    {
        out << "[Filter] Regular expression: " << m_searchRegex.pattern() << '\n';
    }

    if(m_savedMinUsers > 0)
    {
        out << "[Filter] Minimum # users: " << m_savedMinUsers << '\n';
    }

    if(m_savedMaxUsers > 0)
    {
        out << "[Filter] Maximum # users: " << m_savedMaxUsers << '\n';
    }

    out << "[Filter] " << m_numVisible << " shown / " << m_pModel->rowCount() << " total channels\n\n";

    // Write every visible channel to the file.
    //
    // Branch at the beginning to save time, even though it means some repetitive code.
    if(m_savedTopicDisplay)
    {
        QTextDocument doc;
        for(int i = 0; i < m_pModel->rowCount(); ++i)
        {
            if(!m_pView->isRowHidden(i, QModelIndex()))
            {
                doc.setHtml(m_pModel->item(i, 2)->data(Qt::DisplayRole).toString());
                out << m_pModel->item(i, 0)->text() << ' '
                    << m_pModel->item(i, 1)->text() << ' '
                    << doc.toPlainText() << '\n';
            }
        }
    }
    else
    {
        for(int i = 0; i < m_pModel->rowCount(); ++i)
        {
            if(!m_pView->isRowHidden(i, QModelIndex()))
            {
                out << m_pModel->item(i, 0)->text() << ' '
                    << m_pModel->item(i, 1)->text() << ' '
                    << m_pModel->item(i, 2)->data(Qt::DisplayRole).toString() << '\n';
            }
        }
    }

    out.flush();
}

} } // End namespaces
