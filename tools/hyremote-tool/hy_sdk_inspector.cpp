#include "hy_sdk_inspector.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QLatin1StringView>

namespace {

constexpr qint64 kMaxInspectedBytes = 256 * 1024;

QString readBounded(const QString &root, const QString &relative)
{
    if (root.isEmpty() || relative.isEmpty())
        return {};
    QDir dir(root);
    const QString canonicalRoot = dir.canonicalPath();
    const QString candidate = dir.absoluteFilePath(relative);
    QFileInfo info(candidate);
    if (!info.exists() || !info.isFile() || info.size() > kMaxInspectedBytes)
        return {};
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty())
        return {};
    const QString prefix = canonicalRoot.endsWith(QLatin1Char('/'))
        ? canonicalRoot : canonicalRoot + QLatin1Char('/');
    if (!canonical.startsWith(prefix, Qt::CaseSensitive))
        return {};
    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool fileExists(const QString &root, const QString &relative)
{
    return !readBounded(root, relative).isEmpty()
        || QFileInfo(QDir(root).absoluteFilePath(relative)).isFile();
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
          { QStringLiteral("qml/HyRemote/qmldir") },
          "qml-payload", "Open the QML getting-started guide and verify imports from a real consumer." },
        { "generic", "GENERIC",
          { QStringLiteral("plugins/generic/qhyremote.dll") },
          "generic-plugin", "Open the Generic getting-started guide and verify the zero-code launch." },
        { "qpa", "QPA",
          { QStringLiteral("plugins/platforms/qhyremote.dll") },
          "qpa-plugin", "Open the QPA guide and verify exact Qt 6.8.3 private ABI with a real consumer." },
    };
    for (const Route &route : routes) {
        const QString flag = values.value(QString::fromLatin1(route.flag)).toString().trimmed();
        const bool declared = flag.compare(QStringLiteral("ON"), Qt::CaseInsensitive) == 0
            || flag.compare(QStringLiteral("TRUE"), Qt::CaseInsensitive) == 0
            || flag.compare(QStringLiteral("1")) == 0;
        if (flag.isEmpty()) {
            report.checks.push_back(makeCheck(QString::fromLatin1(route.id),
                QString::fromLatin1(route.source), QStringLiteral("manifest flag"),
                HySdkInspector::Result::UnknownSource,
                QStringLiteral("Route flag is absent from the manifest."),
                QString::fromLatin1(route.next)));
            continue;
        }
        if (!declared) {
            report.checks.push_back(makeCheck(QString::fromLatin1(route.id),
                QString::fromLatin1(route.source), QStringLiteral("manifest flag"),
                HySdkInspector::Result::NotApplicable,
                QStringLiteral("Manifest declares the route unavailable."),
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

QJsonObject HySdkInspector::Check::toObject() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("source"), source);
    object.insert(QStringLiteral("method"), method);
    object.insert(QStringLiteral("result"), resultToken(result));
    object.insert(QStringLiteral("reason"), reason);
    object.insert(QStringLiteral("nextStep"), nextStep);
    return object;
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
    object.insert(QStringLiteral("sdkVersion"), sdkVersion);
    object.insert(QStringLiteral("sdkArch"), sdkArch);
    object.insert(QStringLiteral("buildType"), sdkBuildType);
    object.insert(QStringLiteral("kitVersion"), kitVersion);
    object.insert(QStringLiteral("kitArch"), kitArch);
    if (!sdkSourceSha.isEmpty())
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

QVariantList HySdkInspector::Report::checkList() const
{
    QVariantList list;
    for (const Check &check : checks)
        list.append(check.toObject());
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
        report.sdkVersion = values.value(QStringLiteral("QT_VERSION")).toString().trimmed();
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
    const bool core = fileExists(kitRoot, QStringLiteral("lib/Qt6Core.lib"))
        || fileExists(kitRoot, QStringLiteral("lib/libQt6Core.so"))
        || fileExists(kitRoot, QStringLiteral("lib/Qt6Core.dll"));
    report.checks.push_back(makeCheck(QStringLiteral("kit-binaries"), QStringLiteral("kit-binary"),
        QStringLiteral("fixed qmake/QtCore paths"), qmake && core ? Result::PassObserved : Result::FailObserved,
        qmake && core ? QStringLiteral("Expected qmake and QtCore files are present.")
                      : QStringLiteral("Expected Kit binary is missing."),
        QStringLiteral("Select a complete Qt Kit for the declared major version.")));

    if (!report.sdkVersion.isEmpty() && !report.kitVersion.isEmpty()) {
        const bool majorOk = majorVersion(report.sdkVersion) == majorVersion(report.kitVersion);
        report.checks.push_back(makeCheck(QStringLiteral("version-match"), QStringLiteral("manifest+kit-metadata"),
            QStringLiteral("declared major version compare"),
            majorOk ? Result::PassObserved : Result::FailObserved,
            majorOk ? QStringLiteral("Declared Qt major versions match.")
                    : QStringLiteral("Declared Qt major versions differ."),
            QStringLiteral("Select a Kit matching the SDK manifest Qt major version.")));
        const bool qpaExact = report.sdkVersion == QStringLiteral("6.8.3")
            && report.kitVersion == QStringLiteral("6.8.3");
        report.checks.push_back(makeCheck(QStringLiteral("qpa-exact"), QStringLiteral("manifest+kit-metadata"),
            QStringLiteral("QPA exact reference version check"),
            qpaExact ? Result::PassObserved : Result::FailObserved,
            qpaExact ? QStringLiteral("QPA exact reference version is observed.")
                     : QStringLiteral("QPA requires exact Qt 6.8.3; observed values do not match."),
            QStringLiteral("Do not claim QPA eligibility without exact Qt 6.8.3.")));
    } else {
        report.checks.push_back(makeCheck(QStringLiteral("version-match"), QStringLiteral("manifest+kit-metadata"),
            QStringLiteral("declared major version compare"), Result::UnknownSource,
            QStringLiteral("At least one version source is unavailable."),
            QStringLiteral("Select both an SDK manifest root and a Qt Kit root.")));
    }

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
    const bool sentinel = configVersion.contains(QStringLiteral("0.0.0"));
    report.checks.push_back(makeCheck(QStringLiteral("sdk-version"), QStringLiteral("cmake-config"),
        QStringLiteral("HyRemoteConfigVersion.cmake PACKAGE_VERSION"),
        configVersion.isEmpty() ? Result::UnknownSource
            : sentinel ? Result::UnknownSource : Result::PassObserved,
        configVersion.isEmpty() ? QStringLiteral("Package version file is not readable.")
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
