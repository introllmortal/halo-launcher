// SPDX-License-Identifier: GPL-3.0-only

#include <QTest>

#include "diagnostics/CrashAnalyzer.h"

class CrashAnalyzerTest : public QObject {
    Q_OBJECT

   private slots:
    void testOutOfMemory()
    {
        const QStringList logs = {
            "java.lang.OutOfMemoryError: Java heap space",
            "\tat java.base/java.util.Arrays.copyOf(Arrays.java:3512)",
        };
        const auto res = CrashAnalyzer::analyze(nullptr, logs, {}, 1);
        QCOMPARE(res.category, CrashCategory::OutOfMemory);
        QCOMPARE(res.fixAction, RecommendedFix::IncreaseRam);
        QVERIFY(res.suggestedRamMb >= 8192);
    }

    void testCorruptedConfig()
    {
        const QStringList logs = {
            "com.electronwill.nightconfig.core.io.ParsingException: Not enough data available in create-client.toml",
        };
        const auto res = CrashAnalyzer::analyze(nullptr, logs, {}, 1);
        QCOMPARE(res.category, CrashCategory::CorruptedConfig);
        QVERIFY(res.technicalDetails.contains("create-client.toml"));
    }

    void testModConflictFromMixin()
    {
        const QStringList logs = {
            "org.spongepowered.asm.mixin.transformer.throwables.MixinTransformerError: An unexpected critical error was encountered",
            "Caused by: org.spongepowered.asm.mixin.injection.throwables.InjectionError: Critical injection failure: Callback handler onHandler in create_addon.mixins.json:ExampleMixin failed",
        };
        const auto res = CrashAnalyzer::analyze(nullptr, logs, {}, 1);
        QCOMPARE(res.category, CrashCategory::ModConflict);
        QVERIFY(!res.offendingModName.isEmpty());
    }

    void testGraphicsDriverGlfw()
    {
        const QStringList logs = {
            "GLFW error 65542: WGL: The driver does not appear to support OpenGL",
        };
        const auto res = CrashAnalyzer::analyze(nullptr, logs, {}, -1);
        QCOMPARE(res.category, CrashCategory::GraphicsDriver);
    }

    void testFallbackDeepestCausedBy()
    {
        const QStringList logs = {
            "java.lang.RuntimeException: Outer error",
            "Caused by: java.lang.IllegalStateException: Innermost root cause",
        };
        const auto res = CrashAnalyzer::analyze(nullptr, logs, {}, 1);
        QCOMPARE(res.category, CrashCategory::Unknown);
        QCOMPARE(res.fixAction, RecommendedFix::UploadLog);
        QVERIFY(res.technicalDetails.contains("Innermost root cause"));
    }
};

QTEST_GUILESS_MAIN(CrashAnalyzerTest)
#include "CrashAnalyzer_test.moc"
