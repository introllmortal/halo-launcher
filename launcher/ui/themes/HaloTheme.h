// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "FusionTheme.h"

enum class HaloAccent {
    Cyan,
    Emerald,
    Amethyst,
    Amber
};

class HaloTheme : public FusionTheme {
   public:
    explicit HaloTheme(HaloAccent accent = HaloAccent::Cyan);
    virtual ~HaloTheme() {}

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    QPalette colorScheme() override;
    double fadeAmount() override;
    QColor fadeColor() override;
    LogColors logColorScheme() override;

   private:
    HaloAccent m_accent = HaloAccent::Cyan;
    QColor accentColor() const;
    QColor accentLightColor() const;
    QString accentColorHex() const;
    QString accentLightHex() const;
};
