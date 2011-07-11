/***************************************************************************
 *   Copyright (C) 2011 by Vishesh Yadav <vishesh3y@gmail.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "syncdialogbase.h"
#include "hgconfig.h"
#include "fileviewhgpluginsettings.h"

#include <QtGui/QApplication>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <QtCore/QStringList>
#include <QtCore/QTextCodec>
#include <QtGui/QHeaderView>
#include <klocale.h>
#include <kurl.h>
#include <kmessagebox.h>

HgSyncBaseDialog::HgSyncBaseDialog(DialogType dialogType, QWidget *parent):
    KDialog(parent, Qt::Dialog),
    m_haveChanges(false),
    m_dialogType(dialogType)
{
    m_hgw = HgWrapper::instance();
}

void HgSyncBaseDialog::setup()
{
    createChangesGroup();
    setupUI();
    slotChangeEditUrl(0);
    
    connect(m_selectPathAlias, SIGNAL(currentIndexChanged(int)), 
            this, SLOT(slotChangeEditUrl(int)));
    connect(m_selectPathAlias, SIGNAL(highlighted(int)), 
            this, SLOT(slotChangeEditUrl(int)));
    connect(m_changesButton, SIGNAL(clicked()),
            this, SLOT(slotGetChanges()));
    connect(&m_process, SIGNAL(stateChanged(QProcess::ProcessState)), 
            this, SLOT(slotUpdateBusy(QProcess::ProcessState)));
    connect(&m_main_process, SIGNAL(stateChanged(QProcess::ProcessState)), 
            this, SLOT(slotUpdateBusy(QProcess::ProcessState)));
    connect(&m_main_process, SIGNAL(finished(int, QProcess::ExitStatus)), 
            this, SLOT(slotOperationComplete(int, QProcess::ExitStatus)));
    connect(&m_main_process, SIGNAL(error(QProcess::ProcessError)), 
            this, SLOT(slotOperationError()));
    connect(&m_process, SIGNAL(error(QProcess::ProcessError)), 
            this, SLOT(slotChangesProcessError()));
    connect(&m_process, SIGNAL(finished(int, QProcess::ExitStatus)), 
            this, SLOT(slotChangesProcessComplete(int, QProcess::ExitStatus)));
}

void HgSyncBaseDialog::createOptionGroup()
{
    setOptions();
    QVBoxLayout *layout = new QVBoxLayout;

    foreach (QCheckBox *cb, m_options) {
        layout->addWidget(cb);
    }

    m_optionGroup = new QGroupBox;
    m_optionGroup->setLayout(layout);
    setDetailsWidget(m_optionGroup);
}

void HgSyncBaseDialog::setupUI()
{
    HgConfig hgc(HgConfig::RepoConfig);
    m_pathList = hgc.repoRemotePathList();

    // top url bar
    QHBoxLayout *urlLayout = new QHBoxLayout;
    m_selectPathAlias = new KComboBox;
    m_urlEdit = new KLineEdit;
    m_urlEdit->setReadOnly(true);
    QMutableMapIterator<QString, QString> it(m_pathList);
    while (it.hasNext()) {
        it.next();
        m_selectPathAlias->addItem(it.key());
    }
    m_selectPathAlias->addItem(i18nc("@label:combobox", 
                "<edit>"));
    urlLayout->addWidget(m_selectPathAlias);
    urlLayout->addWidget(m_urlEdit);

    // changes button
    //FIXME not very good idea. Bad HACK 
    if (m_dialogType == PullDialog) {
        m_changesButton = new KPushButton(i18nc("@label:button", 
                "Show Incoming Changes"));
    }
    else {
        m_changesButton = new KPushButton(i18nc("@label:button", 
                "Show Outgoing Changes"));
    }
    m_changesButton->setSizePolicy(QSizePolicy::Fixed,
            QSizePolicy::Fixed);

    // dialog's main widget
    QWidget *widget = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(urlLayout);

    // changes
    // createChangesGroup();
    mainLayout->addWidget(m_changesGroup);

    // bottom bar
    QHBoxLayout *bottomLayout = new QHBoxLayout;
    m_statusProg = new QProgressBar;
    m_statusProg->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    bottomLayout->addWidget(m_changesButton, Qt::AlignLeft);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_statusProg);
    
    //
    mainLayout->addLayout(bottomLayout);
    mainLayout->addStretch();
    widget->setLayout(mainLayout);

    createOptionGroup();
    setMainWidget(widget);
}

void HgSyncBaseDialog::slotChangeEditUrl(int index)
{
    if (index == m_selectPathAlias->count() - 1) {
        m_urlEdit->setReadOnly(false);
        m_urlEdit->clear();
        m_urlEdit->setFocus();
    }
    else {
        QString url = m_pathList[m_selectPathAlias->itemText(index)];
        m_urlEdit->setText(url);
        m_urlEdit->setReadOnly(true);
    }
}

void HgSyncBaseDialog::slotGetChanges()
{
    if (m_haveChanges) {
        m_changesGroup->setVisible(!m_changesGroup->isVisible());
        return;
    }
    if (m_process.state() == QProcess::Running) {
        return;
    }
    m_changesButton->setEnabled(false);
    
    QStringList args;
    getHgChangesArguments(args);
    m_process.setWorkingDirectory(m_hgw->getBaseDir());
    m_process.start(QLatin1String("hg"), args);
}

QString HgSyncBaseDialog::remoteUrl() const
{
    return (m_selectPathAlias->currentIndex() == m_selectPathAlias->count()-1)?m_urlEdit->text():m_selectPathAlias->currentText();
}

void HgSyncBaseDialog::slotChangesProcessError()
{
    kDebug() << "Cant get changes";
    KMessageBox::error(this, i18n("Error!"));
    m_changesButton->setEnabled(true);
}

void HgSyncBaseDialog::slotChangesProcessComplete(int exitCode, QProcess::ExitStatus status)
{
    m_changesButton->setEnabled(true);

    if (exitCode != 0 || status != QProcess::NormalExit) {
        return;
    }

    char buffer[512];
    bool unwantedRead = false;

    while (m_process.readLine(buffer, sizeof(buffer)) > 0) {
        QString line(QTextCodec::codecForLocale()->toUnicode(buffer));
        if (unwantedRead ) {
            line.remove(QLatin1String("Commit: "));
            parseUpdateChanges(line.trimmed());
        }
        else if (line.startsWith(QLatin1String("Commit: "))) {
            unwantedRead = true;
            line.remove(QLatin1String("Commit: "));
            parseUpdateChanges(line.trimmed());
        }
    }

    m_changesGroup->setVisible(true);
    m_haveChanges = true; 
}

void HgSyncBaseDialog::done(int r)
{
    if (r == KDialog::Accepted) {
        if (m_main_process.state() == QProcess::Running ||
                m_main_process.state() == QProcess::Starting) {
            kDebug() << "HgWrapper already busy";
            return;
        }

        QStringList args;
        QString command = (m_dialogType==PullDialog)?"pull":"push";
        args << command;
        args << remoteUrl();
        appendOptionArguments(args);

        enableButtonOk(false);
        
        m_main_process.setWorkingDirectory(m_hgw->getBaseDir());
        m_main_process.start(QLatin1String("hg"), args);
    }
    else {
        if (m_process.state() == QProcess::Running || 
            m_process.state() == QProcess::Starting ||
            m_main_process.state() == QProcess::Running ||
            m_main_process.state() == QProcess::Starting) 
        {
            if (m_process.state() == QProcess::Running ||
                    m_process.state() == QProcess::Starting) {
                m_process.terminate();
            }
            if (m_main_process.state() == QProcess::Running ||
                     m_main_process.state() == QProcess::Starting) {
                kDebug() << "terminating HgWrapper process";
                m_main_process.terminate();
            }
        }
        else {
            KDialog::done(r);
        }
    }
}

void HgSyncBaseDialog::slotOperationComplete(int exitCode, QProcess::ExitStatus status)
{
    if (exitCode == 0 && status == QProcess::NormalExit) {
        KDialog::done(KDialog::Accepted);
    }
    else {
        enableButtonOk(true);
        KMessageBox::error(this, i18n("Error!"));
    }
}

void HgSyncBaseDialog::slotOperationError()
{
    enableButtonOk(true);
    KMessageBox::error(this, i18n("Error!"));
}

void HgSyncBaseDialog::slotUpdateBusy(QProcess::ProcessState state)
{
    if (state == QProcess::Running || state == QProcess::Starting) {
        m_statusProg->setRange(0, 0);
    }
    else {
        m_statusProg->setRange(0, 100);
    }
    m_statusProg->repaint();
    QApplication::processEvents();
}

#include "syncdialogbase.moc"

