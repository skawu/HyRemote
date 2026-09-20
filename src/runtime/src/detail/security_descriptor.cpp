#include "detail/security_descriptor.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <algorithm>
#include <utility>

namespace HyRemote::detail {
namespace {

constexpr qint64 kMaxDescriptorBytes = 64 * 1024;
constexpr qint64 kMaxSecretFileBytes = 1024;
constexpr qsizetype kMaxCredentialIdBytes = 128;
constexpr qsizetype kMaxVncPasswordBytes = 8;

void wipe(QByteArray &bytes) noexcept
{
    if (!bytes.isEmpty())
        std::fill(bytes.begin(), bytes.end(), '\0');
    bytes.clear();
}

bool readableRegularFile(const QString &path, const char *label, QString &error)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        error = QStringLiteral("%1 is missing or is not a regular file").arg(QString::fromLatin1(label));
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("%1 is not readable").arg(QString::fromLatin1(label));
        return false;
    }
    return true;
}

QString resolvedPath(const QDir &base, const QString &value)
{
    const QFileInfo candidate(value);
    return candidate.isAbsolute() ? QDir::cleanPath(candidate.absoluteFilePath())
                                  : QDir::cleanPath(base.absoluteFilePath(value));
}

bool loadPassword(const QString &path, QByteArray &password, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("passwordFile is not readable");
        return false;
    }
    if (file.size() > kMaxSecretFileBytes) {
        error = QStringLiteral("passwordFile exceeds the bounded secret-file size");
        return false;
    }

    QByteArray bytes = file.read(kMaxSecretFileBytes + 1);
    if (bytes.endsWith("\r\n"))
        bytes.chop(2);
    else if (bytes.endsWith('\n'))
        bytes.chop(1);

    if (bytes.isEmpty()) {
        wipe(bytes);
        error = QStringLiteral("passwordFile contains an empty VNC password");
        return false;
    }
    if (bytes.contains('\r') || bytes.contains('\n')) {
        wipe(bytes);
        error = QStringLiteral("passwordFile must contain exactly one password line");
        return false;
    }
    if (bytes.size() > kMaxVncPasswordBytes) {
        wipe(bytes);
        error = QStringLiteral("passwordFile exceeds the 8-byte VNC authentication limit; truncation is forbidden");
        return false;
    }

    password = std::move(bytes);
    return true;
}

}  // namespace

SecurityDescriptor::SecurityDescriptor(SecurityDescriptor &&other) noexcept
    : credentialId(std::move(other.credentialId))
    , password(std::move(other.password))
    , certificateFile(std::move(other.certificateFile))
    , privateKeyFile(std::move(other.privateKeyFile))
{
}

SecurityDescriptor &SecurityDescriptor::operator=(SecurityDescriptor &&other) noexcept
{
    if (this == &other)
        return *this;
    clearSecret();
    credentialId = std::move(other.credentialId);
    password = std::move(other.password);
    certificateFile = std::move(other.certificateFile);
    privateKeyFile = std::move(other.privateKeyFile);
    return *this;
}

SecurityDescriptor::~SecurityDescriptor()
{
    clearSecret();
}

void SecurityDescriptor::clearSecret() noexcept
{
    wipe(password);
}

std::optional<SecurityDescriptor> loadSecurityDescriptor(const QString &descriptorFile,
                                                         RemoteSecurityProfile profile,
                                                         QString &error)
{
    error.clear();
    if (profile == RemoteSecurityProfile::Insecure) {
        error = QStringLiteral("the Insecure profile does not consume a security descriptor");
        return std::nullopt;
    }

    const QString descriptorPath = QDir::cleanPath(QFileInfo(descriptorFile).absoluteFilePath());
    QFile file(descriptorPath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("security descriptor is missing or unreadable");
        return std::nullopt;
    }
    if (file.size() < 0 || file.size() > kMaxDescriptorBytes) {
        error = QStringLiteral("security descriptor exceeds the bounded descriptor size");
        return std::nullopt;
    }

    const QByteArray raw = file.read(kMaxDescriptorBytes + 1);
    if (raw.size() > kMaxDescriptorBytes) {
        error = QStringLiteral("security descriptor exceeds the bounded descriptor size");
        return std::nullopt;
    }

    QHash<QString, QString> values;
    const QString text = QString::fromUtf8(raw);
    const QStringList lines = text.split('\n');
    for (qsizetype index = 0; index < lines.size(); ++index) {
        QString line = lines.at(index);
        if (line.endsWith('\r'))
            line.chop(1);
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const qsizetype separator = line.indexOf('=');
        if (separator <= 0) {
            error = QStringLiteral("security descriptor line %1 is malformed").arg(index + 1);
            return std::nullopt;
        }
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (key != QStringLiteral("version") && key != QStringLiteral("credentialId")
            && key != QStringLiteral("passwordFile") && key != QStringLiteral("certificateFile")
            && key != QStringLiteral("privateKeyFile")) {
            error = QStringLiteral("security descriptor contains an unknown key on line %1").arg(index + 1);
            return std::nullopt;
        }
        if (values.contains(key)) {
            error = QStringLiteral("security descriptor contains duplicate key '%1'").arg(key);
            return std::nullopt;
        }
        values.insert(key, value);
    }

    if (values.value(QStringLiteral("version")) != QStringLiteral("1")) {
        error = QStringLiteral("security descriptor version must be exactly 1");
        return std::nullopt;
    }

    SecurityDescriptor descriptor;
    descriptor.credentialId = values.value(QStringLiteral("credentialId"));
    if (descriptor.credentialId.isEmpty()
        || descriptor.credentialId.toUtf8().size() > kMaxCredentialIdBytes) {
        error = QStringLiteral("credentialId must be non-empty and at most 128 UTF-8 bytes");
        return std::nullopt;
    }

    const QString passwordValue = values.value(QStringLiteral("passwordFile"));
    if (passwordValue.isEmpty()) {
        error = QStringLiteral("passwordFile is required for authenticated security profiles");
        return std::nullopt;
    }

    const QDir descriptorDir(QFileInfo(descriptorPath).absolutePath());
    const QString passwordPath = resolvedPath(descriptorDir, passwordValue);
    if (!loadPassword(passwordPath, descriptor.password, error))
        return std::nullopt;

    if (profile == RemoteSecurityProfile::AuthenticatedEncrypted) {
        const QString certificateValue = values.value(QStringLiteral("certificateFile"));
        const QString privateKeyValue = values.value(QStringLiteral("privateKeyFile"));
        if (certificateValue.isEmpty() || privateKeyValue.isEmpty()) {
            error = QStringLiteral("certificateFile and privateKeyFile are required for AuthenticatedEncrypted");
            return std::nullopt;
        }

        descriptor.certificateFile = resolvedPath(descriptorDir, certificateValue);
        descriptor.privateKeyFile = resolvedPath(descriptorDir, privateKeyValue);
        if (!readableRegularFile(descriptor.certificateFile, "certificateFile", error)
            || !readableRegularFile(descriptor.privateKeyFile, "privateKeyFile", error)) {
            return std::nullopt;
        }
    }

    return descriptor;
}

}  // namespace HyRemote::detail
