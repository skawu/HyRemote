#include "watermark_store.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace HyRemote::detail {
namespace {

constexpr auto kFileName = "watermark.policy";
constexpr auto kVersion = "1";

QString colorText(quint32 rgb)
{
    return QStringLiteral("#%1").arg(rgb & 0xFFFFFFU, 6, 16, QLatin1Char('0')).toUpper();
}

}  // namespace

QString watermarkStorePath(const HostPaths &paths)
{
    if (!paths.stateDir.isSet() || !paths.stateDir.ok())
        return {};
    return QDir(paths.stateDir.value).filePath(QString::fromLatin1(kFileName));
}

bool saveWatermarkPolicy(const HostPaths &paths, const WatermarkPolicy &policy)
{
    if (!policy.enabled)
        return clearWatermarkPolicy(paths);

    const QString path = watermarkStorePath(paths);
    if (path.isEmpty())
        return false;

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // The text is written verbatim: the policy already guarantees a single visible line, and escaping it would make the
    // file unreadable to the operator who has to support it.
    const QString body = QStringLiteral("version=%1\ntext=%2\nposition=%3\nopacity=%4\nfont-size=%5\ncolor=%6\n")
                             .arg(QString::fromLatin1(kVersion),
                                  policy.text,
                                  watermarkPositionName(policy.position),
                                  QString::number(policy.opacity, 'g', 6),
                                  QString::number(policy.fontPointSize),
                                  colorText(policy.colorRgb));
    const bool written = file.write(body.toUtf8()) == body.toUtf8().size();
    file.close();
    return written;
}

bool clearWatermarkPolicy(const HostPaths &paths)
{
    const QString path = watermarkStorePath(paths);
    if (path.isEmpty())
        return false;
    if (!QFile::exists(path))
        return true;
    return QFile::remove(path);
}

std::optional<WatermarkPolicyRequest> loadWatermarkRequest(const HostPaths &paths, QString *error)
{
    const QString path = watermarkStorePath(paths);
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("no writable state directory is selected");
        return std::nullopt;
    }

    QFile file(path);
    if (!file.exists())
        return std::nullopt;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("the stored watermark policy is not readable");
        return std::nullopt;
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    file.close();

    WatermarkPolicyRequest request;
    request.enabled = true;
    QString positionToken;
    for (const QString &rawLine : lines) {
        // A file that was copied through a Windows text editor (or written by a build whose newline translation differs)
        // carries CRLF. A stray CR is a control character to the policy, so without this the text would be rejected as
        // "not a single visible line" even though it is exactly what the operator stored.
        QString line = rawLine;
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);

        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1);
        if (key == QStringLiteral("version")) {
            if (value.trimmed() != QString::fromLatin1(kVersion)) {
                if (error)
                    *error = QStringLiteral("the stored watermark policy has an unsupported version");
                return std::nullopt;
            }
        } else if (key == QStringLiteral("text")) {
            request.text = value;
        } else if (key == QStringLiteral("position")) {
            positionToken = value.trimmed();
        } else if (key == QStringLiteral("opacity")) {
            bool ok = false;
            request.opacity = value.trimmed().toDouble(&ok);
            if (!ok)
                request.opacity = std::nullopt;
        } else if (key == QStringLiteral("font-size")) {
            bool ok = false;
            request.fontPointSize = value.trimmed().toInt(&ok);
            if (!ok)
                request.fontPointSize = std::nullopt;
        } else if (key == QStringLiteral("color")) {
            request.color = value.trimmed();
        }
    }

    if (!positionToken.isEmpty())
        request.positionToken = positionToken;
    if (request.text.isEmpty()) {
        if (error)
            *error = QStringLiteral("the stored watermark policy carries no text");
        return std::nullopt;
    }

    // Re-validate through the one shared resolver: a hand-edited file must not bypass the bounds.
    const WatermarkPolicyResult resolved = resolveWatermarkPolicy(request);
    if (!resolved.ok()) {
        if (error)
            *error = resolved.error;
        return std::nullopt;
    }
    return request;
}

}  // namespace HyRemote::detail
