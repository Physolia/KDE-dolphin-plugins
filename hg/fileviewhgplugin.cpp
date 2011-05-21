/***************************************************************************
 *   Copyright (C) 2009-2010 by Vishesh Yadav <vishesh3y@gmail.com>        *
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

#include "fileviewhgplugin.h"

#include <QProcess>
#include <QString>
#include <QDebug>

#include <KPluginFactory>
#include <KPluginLoader>
K_PLUGIN_FACTORY(FileViewHgPluginFactory, registerPlugin<FileViewHgPlugin>();)
K_EXPORT_PLUGIN(FileViewHgPluginFactory("fileviewhgplugin"))

FileViewHgPlugin::FileViewHgPlugin(QObject* parent, const QList<QVariant>& args) :
    KVersionControlPlugin(parent)
{
    Q_UNUSED(args);
}

FileViewHgPlugin::~FileViewHgPlugin()
{
}

QString FileViewHgPlugin::fileName() const
{
    return QLatin1String(".hg");
}

bool FileViewHgPlugin::beginRetrieval(const QString& directory)
{
    int nTrimOutLeft = 0;
    QProcess process;
    process.setWorkingDirectory(directory);

    // Get repo root directory
    process.start(QLatin1String("hg root"));
    while (process.waitForReadyRead()) {
        char buffer[512];
        while (process.readLine(buffer, sizeof(buffer)) > 0)  {
            nTrimOutLeft = QString(buffer).trimmed().length(); 
        }
    }

    QString relativePrefix = directory.right(directory.length() - nTrimOutLeft - 1);
    QString hgBaseDir = directory.left(directory.length() - relativePrefix.length());
    
    // Clear all entries for this directory including the entries
    QMutableHashIterator<QString, VersionState> it(m_versionInfoHash);
    while (it.hasNext()) {
        it.next();
        if (it.key().startsWith(directory)) {
            it.remove();
        }
    }

    // Get status of files
    process.start(QLatin1String("hg status"));
    while (process.waitForReadyRead()) {
        char buffer[1024];
        while (process.readLine(buffer, sizeof(buffer)) > 0)  {
            const QString currentLine(buffer);
            char currentStatus = buffer[0];
            QString currentFile = currentLine.mid(2);
            if (currentFile.startsWith(relativePrefix)) {
                VersionState vs = NormalVersion;
                switch (currentStatus) {
                case 'A':  vs = AddedVersion; break;
                case 'M': vs = LocallyModifiedVersion; break;
                case '?': vs = UnversionedVersion; break;
                case 'D': vs = RemovedVersion; break;
                default: vs = NormalVersion; break;
                }
                if (vs != NormalVersion) {
                    QString filePath = hgBaseDir + currentFile;
                    filePath.remove(QChar('\n'));
                    qDebug() << filePath;
                    m_versionInfoHash.insert(filePath, vs);
                }
            }
        }
    }
    return true;
}

void FileViewHgPlugin::endRetrieval()
{
}

KVersionControlPlugin::VersionState FileViewHgPlugin::versionState(const KFileItem& item)
{
    const QString itemUrl = item.localPath();
    qDebug() << itemUrl;
    if (m_versionInfoHash.contains(itemUrl)) {
        return m_versionInfoHash.value(itemUrl);
    } 
    if (item.isDir()) {
        QHash<QString, VersionState>::const_iterator it = m_versionInfoHash.constBegin();
        while (it != m_versionInfoHash.constEnd()) {
            if (it.key().startsWith(itemUrl)) {
                const VersionState state = m_versionInfoHash.value(it.key());
                if (state == LocallyModifiedVersion) {
                    return LocallyModifiedVersion;
                }
            }
            ++it;
        }
        return NormalVersion;
    }
    return NormalVersion;
}

QList<QAction*> FileViewHgPlugin::contextMenuActions(const KFileItemList& items)
{
    return QList<QAction*>();
}

QList<QAction*> FileViewHgPlugin::contextMenuActions(const QString& directory)
{
    return QList<QAction*>();
}

