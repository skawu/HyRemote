#include "hy_sdk_inspector.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QRegularExpression>

namespace {

constexpr qint64 kMaxInspectedBytes = 256 * 1024;

QString resolvedUnderRoot(const QString &root, const QString &relative)
{
    if (root.isEmpty() || relative.isEmpty())
        return {};
    QDir dir(root);
    const QString canonicalRoot = dir.canonicalPath();
    if (canonicalRoot.isEmpty())
        return {};
    const QString candidate = dir.absoluteFilePath(relative);
    QFileInfo info(candidate);
    if (!info.exists() || !info.isFile())
        return {};
    const QString canonical = info.canonicalFilePath();
    const QString prefix = canonicalRoot.endsWith(QLatin1Char('/'))
        ? canonicalRoot : canonicalRoot + QLatin1Char('/');
    if (canonical.isEmpty() || !canonical.startsWith(prefix, Qt::CaseSensitive))
        return {};
    return canonical;
}

QString readBounded(const QString &root, const QString &relative)
{
    const QString path = resolvedUnderRoot(root, relative);
    if (path.isEmpty())
        return {};
    QFileInfo info(path);
    if (!info.isFile() || info.size() > kMaxInspectedBytes)
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool fileExists(const QString &root, const QString &relative)
{
    return !resolvedUnderRoot(root, relative).isEmpty();
}

bool allowedFormat(const QString &value, const QString &kind)
{
    const QRegularExpression version(QStringLiteral("^\\d+(?:\\.\\d+){1,3}(?:[-+][A-Za-z0-9._-]+)?$"));
    const QRegularExpression arch(QStringLiteral("^[A-Za-z0-9_-]{1,32}$"));
    const QRegularExpression sha(QStringLiteral("^[0-9a-fA-F]{7,64}$"));
    const QRegularExpression build(QStringLiteral("^(Release|Debug|RelWithDebInfo|MinSizeRel)$"));
    if (kind == QStringLiteral("version"))
        return version.match(value).hasMatch();
    if (kind == QStringLiteral("arch"))
        return arch.match(value).hasMatch();
    if (kind == QStringLiteral("sha"))
        return sha.match(value).hasMatch();
    if (kind == QStringLiteral("build"))
        return build.match(value).hasMatch();
    return false;
}

QVariantMap manifestValues(const QString &root)
{
    QVariantMap values;
    const QString text = readBounded(root, QStringLiteral("HYREMOTE-MANIFEST.txt"));
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;
        const int separator = trimmed.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;
        values.insert(trimmed.left(separator).trimmed(),
                      trimmed.mid(separator + 1).trimmed());
    }
    return values;
}

QString kitValue(const QString &root, const QString &key)
{
    const QString text = readBounded(root, QStringLiteral("mkspecs/qconfig.pri"));
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int separator = line.indexOf(QLatin1Char('='));
        if (separator > 0 && line.left(separator).trimmed() == key)
            return line.mid(separator + 1).trimmed();
    }
    return {};
}

bool archCompatible(const QString &manifestArch, const QString &kitArch)
{
    if (manifestArch.isEmpty() || kitArch.isEmpty())
        return false;
    const auto normalize = [](QString value) {
        return value.trimmed().toLower().remove(QLatin1Char('_')).remove(QLatin1Char('-'));
    };
    const QString a = normalize(manifestArch);
    const QString b = normalize(kitArch);
    return a == b || (a == QStringLiteral("amd64") && b == QStringLiteral("x8664"));
}

QString majorVersion(const QString &version)
{
    const int dot = version.indexOf(QLatin1Char('.'));
    return dot <= 0 ? version : version.left(dot);
}

enum class FlagState { True, False, Unknown };

FlagState flagState(const QString &raw)
{
    const QString value = raw.trimmed();
    if (value.compare(QStringLiteral("ON"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("TRUE"), Qt::CaseInsensitive) == 0
        || value == QStringLiteral("1"))
        return FlagState::True;
    if (value.compare(QStringLiteral("OFF"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("FALSE"), Qt::CaseInsensitive) == 0
        || value == QStringLiteral("0"))
        return FlagState::False;
    return FlagState::Unknown;
}

QString packageVersion(const QString &configText)
{
    const QRegularExpression expression(
        QStringLiteral("set\\s*\\(\\s*PACKAGE_VERSION\\s+\"([^\"]+)\"\\s*\\)"));
    const QRegularExpressionMatch match = expression.match(configText);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

HySdkInspector::Check makeCheck(const QString &id, const QString &source,
                                const QString &method, HySdkInspector::Result result,
                                const QString &reason, const QString &nextStep)
{
    HySdkInspector::Check check;
    check.id = id;
    check.source = source;
    check.method = method;
    check.result = result;
    check.reason = reason;
    check.nextStep = nextStep;
    return check;
}

void addRouteChecks(HySdkInspector::Report &report, const QString &sdkRoot,
                    const QVariantMap &values)
{
    struct Route {
        const char *id;
        const char *flag;
        QStringList required;
        const char *source;
        const char *next;
    };
    const QVector<Route> routes{
        { "cpp", "CPP",
          { QStringLiteral("include/HyRemote/RemoteAccess.h"),
            QStringLiteral("lib/cmake/HyRemote/HyRemoteConfig.cmake"),
            QStringLiteral("lib/cmake/HyRemote/HyRemoteTargets.cmake"),
            QStringLiteral("bin/HyRemoteRemoteAccess.dll") },
          "public-header+cmake+runtime", "Open the C++ getting-started guide and verify the selected SDK with a real consumer." },
        { "qml", "QML",
          { QStringLiteral("lib/qml/HyRemote/qmldir"),
            QStringLiteral("lib/qml/HyRemote/hyremote-qmlplugin.dll") },
          "qml-payload", "Open the QML getting-started guide and verify imports from a real consumer." },
        { "generic", "GENERIC",
          { QStringLiteral("lib/HyRemote/plugins/generic/qhyremote.dll") },
          "generic-plugin", "Open the Generic getting-started guide and verify the zero-code launch." },
        { "qpa", "QPA",
          { QStringLiteral("lib/HyRemote/plugins/platforms/qhyremote.dll") },
          "qpa-plugin", "Open the QPA guide and verify exact Qt 6.8.3 private ABI with a real consumer." },
    };
    const bool recognizedLayout = fileExists(sdkRoot,
        QStringLiteral("lib/cmake/HyRemote/HyRemoteConfig.cmake"));
    for (const Route &route : routes) {
        if (!recognizedLayout) {
            report.checks.push_back(makeCheck(QString::fromLatin1(route.id),
                QString::fromLatin1(route.source), QStringLiteral("canonical SDK layout"),
                HySdkInspector::Result::UnknownSource,
                QStringLiteral("SDK root is not a recognized installed HyRemote package layout."),
                QStringLiteral("Select a canonical installed SDK with lib/cmake/HyRemote/HyRemoteConfig.cmake.")));
            continue;
        }
        const FlagState state = flagState(values.value(QString::fromLatin1(route.flag)).toString());
        if (state == FlagState::Unknown) {
            report.checks.push_back(makeCheck(QString::fromLatin1(route.id),
                QString::fromLatin1(route.source), QStringLiteral("manifest flag"),
                HySdkInspector::Result::UnknownSource,
                QStringLiteral("Route flag is absent or not a recognized boolean."),
                QStringLiteral("Select an SDK manifest with a recognized boolean route flag.")));
            continue;
        }
        if (state == FlagState::False) {
            report.checks.push_back(makeCheck(QString::fromLatin1(route.id),
                QString::fromLatin1(route.source), QStringLiteral("manifest flag"),
                HySdkInspector::Result::NotApplicable,
                QStringLiteral("Manifest declares the route disabled."),
                QStringLiteral("Select an SDK built with this route enabled.")));
            continue;
        }
        QStringList missing;
        for (const QString &relative : route.required) {
            if (!fileExists(sdkRoot, relative))
                missing.push_back(relative.section(QLatin1Char('/'), -1));
        }
        report.checks.push_back(makeCheck(QString::fromLatin1(route.id),
            QString::fromLatin1(route.source), QStringLiteral("fixed relative payload paths"),
            missing.isEmpty() ? HySdkInspector::Result::PassObserved
                              : HySdkInspector::Result::FailObserved,
            missing.isEmpty() ? QStringLiteral("Declared route payload files are present.")
                              : QStringLiteral("Missing payload: %1.").arg(missing.join(QStringLiteral(", "))),
            QString::fromLatin1(route.next)));
    }
}

} // namespace

QString localizedCheckText(const QString &text, bool chinese)
{
    if (!chinese)
        return text;
    if (text == QStringLiteral("No readable manifest at the explicitly selected SDK root."))
        return QStringLiteral("显式选择的 SDK 根没有可读 manifest。");
    if (text == QStringLiteral("Manifest fields are readable."))
        return QStringLiteral("manifest 必填字段可读。");
    if (text.startsWith(QStringLiteral("Missing fields:")))
        return QStringLiteral("缺少字段：%1。").arg(text.section(QLatin1Char(' '), 2));
    if (text == QStringLiteral("Use a canonical installed SDK manifest."))
        return QStringLiteral("请选择规范安装的 SDK manifest。");
    if (text == QStringLiteral("SDK root is not a recognized installed HyRemote package layout."))
        return QStringLiteral("SDK 根不是可识别的 HyRemote 安装布局。");
    if (text == QStringLiteral("Select a canonical installed SDK with lib/cmake/HyRemote/HyRemoteConfig.cmake."))
        return QStringLiteral("请选择包含 lib/cmake/HyRemote/HyRemoteConfig.cmake 的规范安装 SDK。");
    if (text == QStringLiteral("Route flag is absent or not a recognized boolean."))
        return QStringLiteral("路线开关缺失或不是可识别布尔值。");
    if (text == QStringLiteral("Select an SDK manifest with a recognized boolean route flag."))
        return QStringLiteral("请选择路线开关为可识别布尔值的 SDK manifest。");
    if (text == QStringLiteral("Manifest declares the route disabled."))
        return QStringLiteral("manifest 明确声明该路线已禁用（NOT_APPLICABLE）。");
    if (text == QStringLiteral("Select an SDK built with this route enabled."))
        return QStringLiteral("请选择启用该路线构建的 SDK。");
    if (text == QStringLiteral("Declared route payload files are present."))
        return QStringLiteral("该路线声明的 payload 文件存在（仅静态核对，不代表可运行）。");
    if (text.startsWith(QStringLiteral("Missing payload:")))
        return QStringLiteral("缺少 payload：%1。").arg(text.section(QLatin1Char(' '), 2));
    if (text == QStringLiteral("No readable Qt Kit metadata at the selected root."))
        return QStringLiteral("所选 Qt Kit 根没有可读元数据。");
    if (text == QStringLiteral("Select the Qt Kit root containing mkspecs/qconfig.pri."))
        return QStringLiteral("请选择包含 mkspecs/qconfig.pri 的 Qt Kit 根。");
    if (text == QStringLiteral("Kit version and architecture fields are readable."))
        return QStringLiteral("Kit 版本与架构字段可读。");
    if (text == QStringLiteral("Compare them with the SDK manifest; presence alone is not ABI proof."))
        return QStringLiteral("请与 SDK manifest 对比；文件存在不等于 ABI 或运行资格证明。");
    if (text == QStringLiteral("Expected qmake and QtCore files are present."))
        return QStringLiteral("qmake 与匹配 major 的 QtCore 文件存在。");
    if (text == QStringLiteral("Expected Kit binary is missing."))
        return QStringLiteral("Kit 二进制文件缺失。");
    if (text == QStringLiteral("Expected Kit metadata is unavailable; no binary verdict is inferred."))
        return QStringLiteral("Kit 元数据不可用，不对二进制状态作判定。");
    if (text == QStringLiteral("Select a complete Qt Kit for the declared major version."))
        return QStringLiteral("请选择与声明 major 匹配的完整 Qt Kit。");
    if (text == QStringLiteral("Declared Qt major versions match."))
        return QStringLiteral("SDK 构建 Qt 与 Kit 的声明 major 匹配。");
    if (text == QStringLiteral("Declared Qt major versions differ."))
        return QStringLiteral("SDK 构建 Qt 与 Kit 的声明 major 不匹配。");
    if (text == QStringLiteral("At least one version source is unavailable."))
        return QStringLiteral("至少一个版本来源不可用。");
    if (text == QStringLiteral("Select both an SDK manifest root and a Qt Kit root."))
        return QStringLiteral("请同时选择 SDK manifest 根与 Qt Kit 根。");
    if (text == QStringLiteral("QPA exact reference version is observed."))
        return QStringLiteral("QPA 要求的精确 Qt 6.8.3 版本已观察到。");
    if (text == QStringLiteral("QPA is disabled by the SDK manifest; exact-version check is not applicable."))
        return QStringLiteral("SDK manifest 已禁用 QPA；精确版本检查不适用。");
    if (text == QStringLiteral("QPA flag is unknown; exact-version check is not claimed."))
        return QStringLiteral("QPA 开关未知，不宣称精确版本检查通过。");
    if (text == QStringLiteral("QPA requires exact Qt 6.8.3; observed values do not match."))
        return QStringLiteral("QPA 要求精确 Qt 6.8.3；观察值不匹配。");
    if (text == QStringLiteral("Do not claim QPA eligibility without exact Qt 6.8.3."))
        return QStringLiteral("没有精确 Qt 6.8.3 时不得宣称具备 QPA 资格。");
    if (text == QStringLiteral("Declared architectures are compatible."))
        return QStringLiteral("声明的架构兼容。");
    if (text == QStringLiteral("Declared architectures differ."))
        return QStringLiteral("声明的架构不匹配。");
    if (text == QStringLiteral("Select a Kit matching the SDK manifest architecture."))
        return QStringLiteral("请选择与 SDK manifest 架构匹配的 Kit。");
    if (text == QStringLiteral("Package version file is not readable."))
        return QStringLiteral("包版本文件不可读。");
    if (text == QStringLiteral("set(PACKAGE_VERSION ...) was not found."))
        return QStringLiteral("未找到 set(PACKAGE_VERSION ...) 声明。");
    if (text == QStringLiteral("0.0.0 is the develop sentinel, not a released version."))
        return QStringLiteral("0.0.0 是开发哨兵值，不是已发布版本。");
    if (text == QStringLiteral("Package version is readable."))
        return QStringLiteral("包版本可读。");
    if (text == QStringLiteral("Treat 0.0.0 as an explicit development sentinel; do not infer a release version."))
        return QStringLiteral("将 0.0.0 视为明确的开发哨兵，不得推断为发布版本。");
    if (text == QStringLiteral("CMake package entry files are present."))
        return QStringLiteral("CMake package 入口文件存在。");
    if (text == QStringLiteral("CMake package entry file is missing."))
        return QStringLiteral("CMake package 入口文件缺失。");
    if (text == QStringLiteral("Install a complete SDK package before using find_package(HyRemote)."))
        return QStringLiteral("使用 find_package(HyRemote) 前请安装完整 SDK 包。");
    if (text.startsWith(QStringLiteral("Open the QML")))
        return QStringLiteral("打开 QML 入门指南，并用真实消费者验证导入。");
    if (text.startsWith(QStringLiteral("Open the Generic")))
        return QStringLiteral("打开 Generic 入门指南，并用真实消费者验证零代码启动。");
    if (text.startsWith(QStringLiteral("Open the QPA")))
        return QStringLiteral("打开 QPA 指南，并用真实消费者验证精确 Qt 6.8.3 私有 ABI。");
    if (text.startsWith(QStringLiteral("Open the C++")))
        return QStringLiteral("打开 C++ 入门指南，并用真实消费者验证启动与帧投递。");
    return text;
}

QJsonObject HySdkInspector::Check::toObject(bool chinese) const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("source"), source);
    object.insert(QStringLiteral("method"), method);
    object.insert(QStringLiteral("result"), resultToken(result));
    object.insert(QStringLiteral("reason"), localizedCheckText(reason, chinese));
    object.insert(QStringLiteral("nextStep"), localizedCheckText(nextStep, chinese));
    return object;
}

QString HySdkInspector::localizedText(const QString &text, bool chinese)
{
    return localizedCheckText(text, chinese);
}

QString HySdkInspector::resultToken(Result result)
{
    switch (result) {
    case Result::PassObserved: return QStringLiteral("PASS_OBSERVED");
    case Result::FailObserved: return QStringLiteral("FAIL_OBSERVED");
    case Result::NotRun: return QStringLiteral("NOT_RUN");
    case Result::NotApplicable: return QStringLiteral("NOT_APPLICABLE");
    case Result::UnknownSource: return QStringLiteral("UNKNOWN_SOURCE");
    }
    return QStringLiteral("UNKNOWN_SOURCE");
}

QString HySdkInspector::localizedResult(Result result, bool chinese)
{
    if (!chinese)
        return resultToken(result);
    switch (result) {
    case Result::PassObserved: return QStringLiteral("静态核对通过");
    case Result::FailObserved: return QStringLiteral("不匹配/缺项");
    case Result::NotRun: return QStringLiteral("未运行");
    case Result::NotApplicable: return QStringLiteral("不适用");
    case Result::UnknownSource: return QStringLiteral("未知来源");
    }
    return QStringLiteral("未知来源");
}

QJsonObject HySdkInspector::Report::summaryObject() const
{
    QJsonObject object;
    if (allowedFormat(sdkVersion, QStringLiteral("version")))
        object.insert(QStringLiteral("sdkPackageVersion"), sdkVersion);
    if (allowedFormat(sdkQtVersion, QStringLiteral("version")))
        object.insert(QStringLiteral("sdkBuildQtVersion"), sdkQtVersion);
    if (allowedFormat(sdkArch, QStringLiteral("arch")))
        object.insert(QStringLiteral("sdkArch"), sdkArch);
    if (allowedFormat(sdkBuildType, QStringLiteral("build")))
        object.insert(QStringLiteral("buildType"), sdkBuildType);
    if (allowedFormat(kitVersion, QStringLiteral("version")))
        object.insert(QStringLiteral("kitVersion"), kitVersion);
    if (allowedFormat(kitArch, QStringLiteral("arch")))
        object.insert(QStringLiteral("kitArch"), kitArch);
    if (allowedFormat(sdkSourceSha, QStringLiteral("sha")))
        object.insert(QStringLiteral("sourceSha"), sdkSourceSha);
    QJsonArray routes;
    for (const Check &check : checks) {
        if (check.id == QStringLiteral("cpp") || check.id == QStringLiteral("qml")
            || check.id == QStringLiteral("generic") || check.id == QStringLiteral("qpa")) {
            QJsonObject route;
            route.insert(QStringLiteral("route"), check.id);
            route.insert(QStringLiteral("status"), resultToken(check.result));
            route.insert(QStringLiteral("source"), check.source);
            routes.append(route);
        }
    }
    object.insert(QStringLiteral("routes"), routes);
    return object;
}

QVariantList HySdkInspector::Report::checkList(bool chinese) const
{
    QVariantList list;
    for (const Check &check : checks)
        list.append(check.toObject(chinese));
    return list;
}

HySdkInspector::Report HySdkInspector::inspect(const QString &sdkRoot, const QString &kitRoot)
{
    Report report;
    const QString manifest = readBounded(sdkRoot, QStringLiteral("HYREMOTE-MANIFEST.txt"));
    if (manifest.isEmpty()) {
        report.checks.push_back(makeCheck(QStringLiteral("manifest"), QStringLiteral("manifest"),
            QStringLiteral("HYREMOTE-MANIFEST.txt"), Result::UnknownSource,
            QStringLiteral("No readable manifest at the explicitly selected SDK root."),
            QStringLiteral("Select the installed SDK root containing HYREMOTE-MANIFEST.txt.")));
    } else {
        const QVariantMap values = manifestValues(sdkRoot);
        report.sdkQtVersion = values.value(QStringLiteral("QT_VERSION")).toString().trimmed();
        report.sdkArch = values.value(QStringLiteral("ARCH")).toString().trimmed();
        report.sdkBuildType = values.value(QStringLiteral("BUILD_TYPE")).toString().trimmed();
        report.sdkSourceSha = values.value(QStringLiteral("SOURCE_SHA")).toString().trimmed();
        const QStringList required{
            QStringLiteral("SOURCE_SHA"), QStringLiteral("OS"), QStringLiteral("ARCH"),
            QStringLiteral("QT_VERSION"), QStringLiteral("BUILD_TYPE"),
            QStringLiteral("CPP"), QStringLiteral("QML"), QStringLiteral("GENERIC"),
            QStringLiteral("QPA"),
        };
        QStringList missing;
        for (const QString &key : required) {
            if (values.value(key).toString().trimmed().isEmpty())
                missing.push_back(key);
        }
        report.checks.push_back(makeCheck(QStringLiteral("manifest"), QStringLiteral("manifest"),
            QStringLiteral("key=value manifest"), missing.isEmpty() ? Result::PassObserved : Result::FailObserved,
            missing.isEmpty() ? QStringLiteral("Manifest fields are readable.")
                              : QStringLiteral("Missing fields: %1.").arg(missing.join(QStringLiteral(", "))),
            QStringLiteral("Use a canonical installed SDK manifest.")));
        addRouteChecks(report, sdkRoot, values);
    }

    const QString kitConfig = readBounded(kitRoot, QStringLiteral("mkspecs/qconfig.pri"));
    report.kitVersion = kitValue(kitRoot, QStringLiteral("QT_VERSION"));
    report.kitArch = kitValue(kitRoot, QStringLiteral("QT_ARCH"));
    if (kitConfig.isEmpty()) {
        report.checks.push_back(makeCheck(QStringLiteral("kit"), QStringLiteral("kit-metadata"),
            QStringLiteral("mkspecs/qconfig.pri"), Result::UnknownSource,
            QStringLiteral("No readable Qt Kit metadata at the selected root."),
            QStringLiteral("Select the Qt Kit root containing mkspecs/qconfig.pri.")));
    } else {
        report.checks.push_back(makeCheck(QStringLiteral("kit"), QStringLiteral("kit-metadata"),
            QStringLiteral("mkspecs/qconfig.pri"), Result::PassObserved,
            QStringLiteral("Kit version and architecture fields are readable."),
            QStringLiteral("Compare them with the SDK manifest; presence alone is not ABI proof.")));
    }

    const bool qmake = fileExists(kitRoot, QStringLiteral("bin/qmake.exe"))
        || fileExists(kitRoot, QStringLiteral("bin/qmake"));
    const QString kitMajor = majorVersion(report.kitVersion);
    const bool kitMajorKnown = kitMajor == QStringLiteral("5") || kitMajor == QStringLiteral("6");
    bool core = false;
    if (kitMajor == QStringLiteral("6")) {
        core = fileExists(kitRoot, QStringLiteral("lib/Qt6Core.lib"))
            || fileExists(kitRoot, QStringLiteral("lib/libQt6Core.so"))
            || fileExists(kitRoot, QStringLiteral("lib/Qt6Core.dll"));
    } else if (kitMajor == QStringLiteral("5")) {
        core = fileExists(kitRoot, QStringLiteral("lib/Qt5Core.lib"))
            || fileExists(kitRoot, QStringLiteral("lib/libQt5Core.so"))
            || fileExists(kitRoot, QStringLiteral("lib/Qt5Core.dll"));
    }
    const HySdkInspector::Result binariesResult = !kitMajorKnown
        ? HySdkInspector::Result::UnknownSource
        : (qmake && core ? HySdkInspector::Result::PassObserved : HySdkInspector::Result::FailObserved);
    report.checks.push_back(makeCheck(QStringLiteral("kit-binaries"), QStringLiteral("kit-binary"),
        QStringLiteral("fixed qmake/QtCore paths by kit major"), binariesResult,
        !kitMajorKnown ? QStringLiteral("Expected Kit metadata is unavailable; no binary verdict is inferred.")
        : qmake && core ? QStringLiteral("Expected qmake and QtCore files are present.")
                        : QStringLiteral("Expected Kit binary is missing."),
        QStringLiteral("Select a complete Qt Kit for the declared major version.")));

    if (!report.sdkQtVersion.isEmpty() && !report.kitVersion.isEmpty()) {
        const bool majorOk = majorVersion(report.sdkQtVersion) == majorVersion(report.kitVersion);
        report.checks.push_back(makeCheck(QStringLiteral("version-match"), QStringLiteral("manifest+kit-metadata"),
            QStringLiteral("SDK build Qt / Kit declared major version compare"),
            majorOk ? Result::PassObserved : Result::FailObserved,
            majorOk ? QStringLiteral("Declared Qt major versions match.")
                    : QStringLiteral("Declared Qt major versions differ."),
            QStringLiteral("Select a Kit matching the SDK manifest Qt major version.")));
    } else {
        report.checks.push_back(makeCheck(QStringLiteral("version-match"), QStringLiteral("manifest+kit-metadata"),
            QStringLiteral("declared major version compare"), Result::UnknownSource,
            QStringLiteral("At least one version source is unavailable."),
            QStringLiteral("Select both an SDK manifest root and a Qt Kit root.")));
    }

    const FlagState qpaFlag = flagState(manifestValues(sdkRoot).value(QStringLiteral("QPA")).toString());
    HySdkInspector::Result qpaExact = HySdkInspector::Result::UnknownSource;
    QString qpaReason = QStringLiteral("QPA flag is unknown; exact-version check is not claimed.");
    if (qpaFlag == FlagState::False) {
        qpaExact = HySdkInspector::Result::NotApplicable;
        qpaReason = QStringLiteral("QPA is disabled by the SDK manifest; exact-version check is not applicable.");
    } else if (qpaFlag == FlagState::True && !report.sdkQtVersion.isEmpty() && !report.kitVersion.isEmpty()) {
        const bool exact = report.sdkQtVersion == QStringLiteral("6.8.3")
            && report.kitVersion == QStringLiteral("6.8.3");
        qpaExact = exact ? Result::PassObserved : Result::FailObserved;
        qpaReason = exact ? QStringLiteral("QPA exact reference version is observed.")
                          : QStringLiteral("QPA requires exact Qt 6.8.3; observed values do not match.");
    }
    report.checks.push_back(makeCheck(QStringLiteral("qpa-exact"), QStringLiteral("manifest+kit-metadata"),
        QStringLiteral("QPA exact reference version check"), qpaExact, qpaReason,
        QStringLiteral("Do not claim QPA eligibility without exact Qt 6.8.3.")));

    if (!report.sdkArch.isEmpty() && !report.kitArch.isEmpty()) {
        report.checks.push_back(makeCheck(QStringLiteral("arch-match"), QStringLiteral("manifest+kit-metadata"),
            QStringLiteral("declared architecture compare"),
            archCompatible(report.sdkArch, report.kitArch) ? Result::PassObserved : Result::FailObserved,
            archCompatible(report.sdkArch, report.kitArch)
                ? QStringLiteral("Declared architectures are compatible.")
                : QStringLiteral("Declared architectures differ."),
            QStringLiteral("Select a Kit matching the SDK manifest architecture.")));
    }

    const QString configVersion = readBounded(sdkRoot,
        QStringLiteral("lib/cmake/HyRemote/HyRemoteConfigVersion.cmake"));
    report.sdkVersion = packageVersion(configVersion);
    const bool sentinel = report.sdkVersion == QStringLiteral("0.0.0");
    report.checks.push_back(makeCheck(QStringLiteral("sdk-version"), QStringLiteral("cmake-config"),
        QStringLiteral("HyRemoteConfigVersion.cmake set(PACKAGE_VERSION ...)"),
        configVersion.isEmpty() || report.sdkVersion.isEmpty() || sentinel
            ? Result::UnknownSource : Result::PassObserved,
        configVersion.isEmpty() ? QStringLiteral("Package version file is not readable.")
            : report.sdkVersion.isEmpty() ? QStringLiteral("set(PACKAGE_VERSION ...) was not found.")
            : sentinel ? QStringLiteral("0.0.0 is the develop sentinel, not a released version.")
                       : QStringLiteral("Package version is readable."),
        QStringLiteral("Treat 0.0.0 as an explicit development sentinel; do not infer a release version.")));

    const bool cmakeFiles = fileExists(sdkRoot, QStringLiteral("lib/cmake/HyRemote/HyRemoteConfig.cmake"))
        && fileExists(sdkRoot, QStringLiteral("lib/cmake/HyRemote/HyRemoteTargets.cmake"));
    report.checks.push_back(makeCheck(QStringLiteral("cmake-package"), QStringLiteral("cmake-config"),
        QStringLiteral("fixed HyRemoteConfig/Targets paths"),
        cmakeFiles ? Result::PassObserved : Result::FailObserved,
        cmakeFiles ? QStringLiteral("CMake package entry files are present.")
                   : QStringLiteral("CMake package entry file is missing."),
        QStringLiteral("Install a complete SDK package before using find_package(HyRemote).")));

    return report;
}
