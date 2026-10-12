// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>

namespace FileLogger {
void Install();
void Configure(const QString& path, qint64 maxBytes);

} // namespace FileLogger
