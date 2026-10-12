// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "file_logger.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStringList>

namespace FileLogger {
namespace {

QtMessageHandler g_previous = nullptr;
QMutex g_mutex;
QFile g_file;
qint64 g_maxBytes = 0;
qint64 g_size = 0;
bool g_configured = false;
QStringList g_pending; // lines logged before Configure()
constexpr int kMaxPending = 2000;

void RestartFile() {
    g_file.close();
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    const QByteArray note =
        QStringLiteral("%1   INFO  Log file reached its %2 MB limit and was started over\n")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
            .arg(g_maxBytes / (1024 * 1024))
            .toUtf8();
    g_size = g_file.write(note);
}

void WriteLine(const QString& line) {
    if (!g_file.isOpen())
        return;
    const QByteArray bytes = line.toUtf8() + '\n';
    if (g_maxBytes > 0 && g_size + bytes.size() > g_maxBytes)
        RestartFile();
    const qint64 written = g_file.write(bytes);
    if (written > 0)
        g_size += written;
    g_file.flush(); // keep the file current if the server stops abruptly
}

void Handler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    if (g_previous)
        g_previous(type, context, message);
    const QString line = qFormatLogMessage(type, context, message);
    {
        QMutexLocker lock(&g_mutex);
        if (!g_configured) {
            if (g_pending.size() < kMaxPending)
                g_pending.append(line);
        } else {
            WriteLine(line);
        }
    }
}

} // namespace

void Install() {
    g_previous = qInstallMessageHandler(Handler);
}

void Configure(const QString& path, qint64 maxBytes) {
    QString openError;
    {
        QMutexLocker lock(&g_mutex);
        g_configured = true;
        g_maxBytes = maxBytes > 0 ? maxBytes : 0;
        if (!path.isEmpty()) {
            g_file.setFileName(path);
            if (g_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                g_size = g_file.size();
                // An existing file already over the limit starts over now.
                if (g_maxBytes > 0 && g_size >= g_maxBytes)
                    RestartFile();
                for (const QString& line : g_pending)
                    WriteLine(line);
            } else {
                openError = g_file.errorString();
            }
        }
        g_pending.clear();
    }
    if (path.isEmpty())
        qInfo() << "Log file disabled (LogFile is empty)";
    else if (!openError.isEmpty())
        qWarning() << "Cannot open log file" << path << ":" << openError;
    else if (g_maxBytes > 0)
        qInfo().noquote() << "Logging to" << QFileInfo(path).absoluteFilePath() << "(limit"
                          << g_maxBytes / (1024 * 1024) << "MB, then started over)";
    else
        qInfo().noquote() << "Logging to" << QFileInfo(path).absoluteFilePath()
                          << "(no size limit)";
}

} // namespace FileLogger
