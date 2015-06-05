// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QLabel>
#include <QVBoxLayout>

#include "version.h"
#include "common/util.h"

#include "interface/init.h"

#include "bitcoingui.h"
#include "clientmodel.h"
#include "guiutil.h"

#include "ui_aboutdialog.h"
#include "ui_helpmessagedialog.h"
#include "utilitydialog.h"


/** "About" dialog box */
AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Set current copyright year
    ui->copyrightLabel->setText(tr("Copyright &copy; %1-%2 %3").arg(2013).arg(COPYRIGHT_YEAR).arg(tr("The Pebblecoin developers")) + "<br>" +
                                tr("Copyright &copy; %1-%2 %3").arg(2012).arg(COPYRIGHT_YEAR).arg(tr("The Cryptonote developers")) + "<br>" +
                                tr("Copyright &copy; %1-%2 %3").arg(2009).arg(COPYRIGHT_YEAR).arg(tr("The Bitcoin Core developers")));
}

void AboutDialog::setModel(ClientModel *model)
{
    if(model)
    {
        QString version = model->formatFullVersion();
        /* On x86 add a bit specifier to the version so that users can distinguish between
         * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambigious.
         */
#if defined(__x86_64__)
        version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
        version += " " + tr("(%1-bit)").arg(32);
#endif
        ui->versionLabel->setText(version);
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_buttonBox_accepted()
{
    close();
}

/** "Help message" dialog box */
HelpMessageDialog::HelpMessageDialog(boost::program_options::options_description descOptionsIn, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HelpMessageDialog),
    descOptions(descOptionsIn)
{
    ui->setupUi(this);
    GUIUtil::restoreWindowGeometry("nHelpMessageDialogWindow", this->size(), this);

    header = QString() + QString::fromStdString(tools::get_project_description("Qt"));
  
    std::ostringstream stream;
    stream << descOptions;
    coreOptions = QString::fromStdString(stream.str());
  
    ui->helpMessageLabel->setFont(GUIUtil::bitcoinAddressFont());

    // Set help message text
    ui->helpMessageLabel->setText(header + "\n" + coreOptions + "\n");
}

HelpMessageDialog::~HelpMessageDialog()
{
    GUIUtil::saveWindowGeometry("nHelpMessageDialogWindow", this);
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    QString strUsage = header + "\n" + coreOptions + "\n" + uiOptions + "\n";
    fprintf(stdout, "%s", strUsage.toStdString().c_str());
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
        // On Windows, show a message box, as there is no stderr/stdout in windowed applications
        exec();
#else
        // On other operating systems, print help text to console
        printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    close();
}


/** "Shutdown" window */
void ShutdownWindow::showShutdownWindow(BitcoinGUI *window)
{
    if (!window)
        return;

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(
        tr("Pebblecoin Qt is shutting down...") + "<br /><br />" +
        tr("Do not shut down the computer until this window disappears.")));
    shutdownWindow->setLayout(layout);

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
}
