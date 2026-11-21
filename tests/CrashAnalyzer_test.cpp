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

    void testJavaVersionMismatch()
    {
        const QStringList logs21 = {
            "java.lang.UnsupportedClassVersionError: net/example/Mod has been compiled by a more recent version of the Java Runtime (class file version 65.0), this version of the Java Runtime only recognizes class file versions up to 61.0",
        };
        const auto res21 = CrashAnalyzer::analyze(nullptr, logs21, {}, 1);
        QCOMPARE(res21.category, CrashCategory::JavaVersion);
        QCOMPARE(res21.fixAction, RecommendedFix::SwitchJava);
        QCOMPARE(res21.requiredJavaVersion, QStringLiteral("21"));

        const QStringList logs17 = {
            "Caused by: java.lang.UnsupportedClassVersionError: com/foo/Bar has been compiled by a more recent version of the Java Runtime (class file version 61.0)",
        };
        const auto res17 = CrashAnalyzer::analyze(nullptr, logs17, {}, 1);
        QCOMPARE(res17.category, CrashCategory::JavaVersion);
        QCOMPARE(res17.requiredJavaVersion, QStringLiteral("17"));

        // Capture group 2: "class file version <N>.0 ... compiled by a more recent version of the Java Runtime"
        const QStringList logsGroup2 = {
            "java.lang.UnsupportedClassVersionError: class file version 61.0 cannot load class compiled by a more recent version of the Java Runtime",
        };
        const auto resGroup2 = CrashAnalyzer::analyze(nullptr, logsGroup2, {}, 1);
        QCOMPARE(resGroup2.category, CrashCategory::JavaVersion);
        QCOMPARE(resGroup2.requiredJavaVersion, QStringLiteral("17"));

        const QStringList logsFallback = {
            "java.lang.UnsupportedClassVersionError: incompatible class format",
        };
        const auto resFallback = CrashAnalyzer::analyze(nullptr, logsFallback, {}, 1);
        QCOMPARE(resFallback.category, CrashCategory::JavaVersion);
        QCOMPARE(resFallback.requiredJavaVersion, QStringLiteral("21"));
    }

    void testSourceAndWhyPopulatedAcrossCategories()
    {
        const auto oom = CrashAnalyzer::analyze(nullptr, { "java.lang.OutOfMemoryError: Java heap space" }, {}, 1);
        QVERIFY(!oom.sourceSummary.isEmpty());
        QVERIFY(!oom.whySummary.isEmpty());

        const auto cfg = CrashAnalyzer::analyze(
            nullptr, { "com.electronwill.nightconfig.core.io.ParsingException: Not enough data available in sodium-options.toml" }, {}, 1);
        QVERIFY(cfg.sourceSummary.contains(QStringLiteral("sodium-options.toml")));
        QVERIFY(!cfg.whySummary.isEmpty());

        const auto mixin = CrashAnalyzer::analyze(
            nullptr,
            { "Caused by: org.spongepowered.asm.mixin.injection.throwables.InjectionError: Critical injection failure: Callback handler "
              "onTick in example_mod.mixins.json:TickMixin failed" },
            {}, 1);
        QCOMPARE(mixin.category, CrashCategory::ModConflict);
        QVERIFY(mixin.sourceSummary.contains(QStringLiteral("example_mod")));
        QVERIFY(mixin.whySummary.contains(QStringLiteral("Mixin")));

        const auto dep = CrashAnalyzer::analyze(
            nullptr,
            { "Unsupported installed optional dependencies:\nMod ID: 'jei', Requested by: 'ftbxmodcompat', Expected range: '[19.0,)', "
              "Actual version: '18.2'" },
            {}, 1);
        QCOMPARE(dep.category, CrashCategory::MissingDependency);
        QVERIFY(dep.sourceSummary.contains(QStringLiteral("ftbxmodcompat")));
        QVERIFY(dep.sourceSummary.contains(QStringLiteral("jei")));
        QVERIFY(!dep.whySummary.isEmpty());

        const auto noSuchMethod = CrashAnalyzer::analyze(
            nullptr,
            { "java.lang.NoSuchMethodError: 'void net.minecraft.world.level.Level.foo()'",
              "Caused by: java.lang.NoSuchMethodError: 'void net.minecraft.world.level.Level.foo()'" },
            {}, 1);
        QCOMPARE(noSuchMethod.category, CrashCategory::Unknown);
        QVERIFY(!noSuchMethod.sourceSummary.isEmpty());
        QVERIFY(noSuchMethod.whySummary.contains(QStringLiteral("method or class")));

        const auto dup = CrashAnalyzer::analyze(nullptr, { "Duplicate mod id 'create'" }, {}, 1);
        QCOMPARE(dup.category, CrashCategory::DuplicateMods);
        QVERIFY(dup.sourceSummary.contains(QStringLiteral("create")));
        QVERIFY(!dup.whySummary.isEmpty());

        const auto modLoad = CrashAnalyzer::analyze(nullptr, { "ModLoadingException: Mod 'flywheel' failed to load" }, {}, 1);
        QCOMPARE(modLoad.category, CrashCategory::ModConflict);
        QVERIFY(modLoad.sourceSummary.contains(QStringLiteral("flywheel")));
        QVERIFY(modLoad.whySummary.contains(QStringLiteral("ModLoadingException")));

        const auto shader = CrashAnalyzer::analyze(nullptr, { "Failed to compile fragment shader: gbuffers_terrain.fsh" }, {}, 1);
        QCOMPARE(shader.category, CrashCategory::GraphicsDriver);
        QVERIFY(!shader.sourceSummary.isEmpty());
        QVERIFY(!shader.whySummary.isEmpty());

        const auto ticking = CrashAnalyzer::analyze(
            nullptr, {}, QStringLiteral("---- Minecraft Crash Report ----\nDescription: Ticking entity\n"), 1);
        QCOMPARE(ticking.category, CrashCategory::Unknown);
        QVERIFY(ticking.sourceSummary.contains(QStringLiteral("Ticking entity")));
        QVERIFY(ticking.whySummary.contains(QStringLiteral("Ticking entity")));
    }
};

QTEST_GUILESS_MAIN(CrashAnalyzerTest)
#include "CrashAnalyzer_test.moc"
