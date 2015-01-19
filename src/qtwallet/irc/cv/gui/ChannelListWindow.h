// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// ChannelListWindow is a Window that provides a user interface
// for fetching and filtering the list of channels on a server.

#pragma once

#include "cv/gui/Window.h"

class QVBoxLayout;
class QTreeView;
class QStandardItemModel;
class QModelIndex;
class QWidget;
class QPushButton;
class QCheckBox;
class QLabel;
class QGroupBox;
class QRadioButton;
class QSpinBox;

namespace cv {

class Session;

namespace gui {

class SearchBar;
class CLineEdit;
class ChannelTopicDelegate;

class ChannelListWindow : public Window
{
    Q_OBJECT

    Session *               m_pSession;

    // Main layout is vbox.
    QVBoxLayout *           m_pVLayout;

    QTreeView *             m_pView;
    QStandardItemModel *    m_pModel;
    ChannelTopicDelegate *  m_pDelegate;
    bool                    m_populatingList;

    // Upper section that contains the downloading groupbox,
    // filtering groupbox, save list button and channels label.
    QWidget *       m_pControlsSection;

    QGroupBox *     m_pDownloadingGroup;
    CLineEdit *     m_pCustomParameters;
    QCheckBox *     m_pTopicDisplay;
    QPushButton *   m_pDownloadListButton;
    QPushButton *   m_pStopDownloadButton;

    QLabel *        m_pChannelsLabel;
    int             m_currChannel;
    int             m_numVisible;

    QPushButton *   m_pSaveButton;

    QGroupBox *     m_pFilteringGroup;
    SearchBar *     m_pSearchBar;
    QTimer *        m_pSearchTimer;
    QString         m_searchStr;
    QRegExp         m_searchRegex;
    QCheckBox *     m_pCheckChanNames;
    QCheckBox *     m_pCheckChanTopics;
    QCheckBox *     m_pUseRegExp;
    QLabel *        m_pMinUsersLabel;
    QSpinBox *      m_pMinUsers;
    QLabel *        m_pMaxUsersLabel;
    QSpinBox *      m_pMaxUsers;
    QPushButton *   m_pApplyFilterButton;
    QPushButton *   m_pStopFilterButton;

    int             m_savedMinUsers;
    int             m_savedMaxUsers;
    bool            m_savedTopicDisplay;

public:
    ChannelListWindow(Session *pSession, const QSize &size = QSize(715, 300));

    void giveFocus();

    void beginPopulatingList();
    void addChannel(const QString &channel, const QString &numUsers, const QString &topic);
    void endPopulatingList();
    void clearList();

protected:
    void setupControls();

public slots:
    void handleConnect();
    void handleDisconnect();

    void downloadList();
    void stopDownload();
    void joinChannel(const QModelIndex &index);
    void startFilter();
    void performSearchIteration();
    void stopFilter();
    void saveList();
};

} } // End namespaces
