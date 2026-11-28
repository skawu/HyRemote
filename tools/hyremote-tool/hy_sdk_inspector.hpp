#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVector>

// Read-only static inspection of an explicitly selected installed SDK and Qt
// Kit. It never scans a root, executes tools, opens arbitrary relative paths,
// or claims runtime/ABI qualification from file presence alone.
class HySdkInspector final
{
public:
    enum class Result {
        PassObserved,
        FailObserved,
        NotRun,
        NotApplicable,
        UnknownSource,
    };

    struct Check {
        QString id;
        QString source;
        QString method;
        Result result = Result::UnknownSource;
        QString reason;
        QString nextStep;
        QJsonObject toObject() const;
    };

    struct Report {
        QVector<Check> checks;
        QString sdkVersion;
        QString sdkArch;
        QString sdkBuildType;
        QString kitVersion;
        QString kitArch;
        QString sdkSourceSha;

        QJsonObject summaryObject() const;
        QVariantList checkList() const;
    };

    static Report inspect(const QString &sdkRoot, const QString &kitRoot);
    static QString resultToken(Result result);
    static QString localizedResult(Result result, bool chinese);
};
