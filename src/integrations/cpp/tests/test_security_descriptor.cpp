#include "detail/security_descriptor.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

namespace {

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(content) == content.size();
}

bool check(bool condition, const char *message)
{
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main()
{
    using HyRemote::Runtime::SecurityProfile;
    using HyRemote::detail::loadSecurityDescriptor;

    QTemporaryDir temp;
    if (!check(temp.isValid(), "temporary directory is available"))
        return 1;

    const QString passwordPath = temp.filePath(QStringLiteral("password.txt"));
    if (!check(writeFile(passwordPath, QByteArray("secret7\n")), "password fixture created"))
        return 2;

    const QString authenticatedPath = temp.filePath(QStringLiteral("authenticated.conf"));
    const QByteArray authenticated =
        "version=1\n"
        "credentialId=maintenance-console\n"
        "passwordFile=password.txt\n";
    if (!check(writeFile(authenticatedPath, authenticated), "authenticated descriptor created"))
        return 3;

    QString error;
    auto descriptor = loadSecurityDescriptor(authenticatedPath,
                                             SecurityProfile::Authenticated,
                                             error);
    if (!check(descriptor.has_value(), "valid authenticated descriptor loads")
        || !check(error.isEmpty(), "valid descriptor has no error")
        || !check(descriptor->credentialId == QStringLiteral("maintenance-console"),
                  "credential id is preserved")
        || !check(descriptor->password == QByteArray("secret7"),
                  "one trailing newline is removed without truncating secret bytes")
        || !check(descriptor->certificateFile.isEmpty(),
                  "authenticated-only profile does not require TLS files")) {
        return 4;
    }
    descriptor.reset();

    const QString longPasswordPath = temp.filePath(QStringLiteral("long-password.txt"));
    if (!check(writeFile(longPasswordPath, QByteArray("123456789\n")), "long password fixture created"))
        return 5;
    const QString longDescriptorPath = temp.filePath(QStringLiteral("long.conf"));
    const QByteArray longDescriptor =
        "version=1\n"
        "credentialId=operator\n"
        "passwordFile=long-password.txt\n";
    if (!check(writeFile(longDescriptorPath, longDescriptor), "long descriptor created"))
        return 6;
    error.clear();
    if (!check(!loadSecurityDescriptor(longDescriptorPath,
                                       SecurityProfile::Authenticated,
                                       error),
               "VNC password longer than eight bytes is rejected")
        || !check(error.contains(QStringLiteral("8-byte")),
                  "password rejection is actionable without echoing the password")) {
        return 7;
    }

    const QString unknownPath = temp.filePath(QStringLiteral("unknown.conf"));
    const QByteArray unknown =
        "version=1\n"
        "credentialId=operator\n"
        "passwordFile=password.txt\n"
        "password=must-never-be-accepted-inline\n";
    if (!check(writeFile(unknownPath, unknown), "unknown-key descriptor created"))
        return 8;
    error.clear();
    if (!check(!loadSecurityDescriptor(unknownPath,
                                       SecurityProfile::Authenticated,
                                       error),
               "inline password/unknown keys are rejected")
        || !check(!error.contains(QStringLiteral("must-never-be-accepted-inline")),
                  "unknown-key failure never echoes secret-like values")) {
        return 9;
    }

    const QString encryptedPath = temp.filePath(QStringLiteral("encrypted.conf"));
    const QByteArray encrypted =
        "version=1\n"
        "credentialId=operator\n"
        "passwordFile=password.txt\n";
    if (!check(writeFile(encryptedPath, encrypted), "encrypted descriptor fixture created"))
        return 10;
    error.clear();
    if (!check(!loadSecurityDescriptor(encryptedPath,
                                       SecurityProfile::AuthenticatedEncrypted,
                                       error),
               "encrypted profile without certificate/private key fails closed")
        || !check(error.contains(QStringLiteral("certificateFile"))
                      && error.contains(QStringLiteral("privateKeyFile")),
                  "encrypted-profile failure names missing fields without secret values")) {
        return 11;
    }

    const QString duplicatePath = temp.filePath(QStringLiteral("duplicate.conf"));
    const QByteArray duplicate =
        "version=1\n"
        "credentialId=operator\n"
        "credentialId=other\n"
        "passwordFile=password.txt\n";
    if (!check(writeFile(duplicatePath, duplicate), "duplicate-key descriptor created"))
        return 12;
    error.clear();
    if (!check(!loadSecurityDescriptor(duplicatePath,
                                       SecurityProfile::Authenticated,
                                       error),
               "duplicate keys are rejected deterministically")) {
        return 13;
    }

    return 0;
}
