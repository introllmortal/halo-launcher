// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QTranslator>

class CrashAssistantTranslator : public QTranslator {
    Q_OBJECT
public:
    explicit CrashAssistantTranslator(QObject* parent = nullptr);
    ~CrashAssistantTranslator() override = default;

    QString translate(const char* context, const char* sourceText,
                      const char* disambiguation = nullptr, int n = -1) const override;

    static void install();
    static bool isRussianLanguage();
};
