#include "connection_profile_store.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int kSchemaVersion = 1;
constexpr int kMaxProfiles = 100;
constexpr qint64 kMaxFileBytes = 128 * 1024;
constexpr qint64 kMaxStoredUtcMs = 4102444800000LL; // 2100-01-01

void fail(QString *error, const QString &reason)
{
    if (error)
        *error = reason;
}

bool allowedKeys(const QJsonObject &object, const QSet<QString> &allowed)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!allowed.contains(it.key()))
            return false; // avoid silently importing credentials/unknown data
    }
    return true;
}

bool validUuid(const QString &id)
{
    if (id.size() != 36)
        return false;
    for (int i = 0; i < id.size(); ++i) {
        const auto ch = id.at(i).toLatin1();
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (ch != '-')
                return false;
        } else if (!((ch >= '0' && ch <= '9')
                     || (ch >= 'a' && ch <= 'f')
                     || (ch >= 'A' && ch <= 'F'))) {
            return false;
        }
    }
    return true;
}

bool printable(const QString &text)
{
    for (const auto ch : text) {
        if (ch.isNull() || ch.category() == QChar::Other_Control
            || ch == QChar::LineSeparator || ch == QChar::ParagraphSeparator)
            return false;
    }
    return true;
}

void sortProfiles(QVector<ToolConnectionProfile> &items)
{
    std::stable_sort(items.begin(), items.end(),
                     [](const ToolConnectionProfile &a, const ToolConnectionProfile &b) {
        if (a.lastUsedUtcMs != b.lastUsedUtcMs)
            return a.lastUsedUtcMs > b.lastUsedUtcMs;
        const auto name = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        return name == 0 ? a.id < b.id : name < 0;
    });
}

QByteArray digest(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

bool safeRead(const QString &path, QByteArray &content, QString *error)
{
    QFile file(path);
    if (!file.exists()) {
        content.clear();
        return true;
    }
    if (file.size() > kMaxFileBytes) {
        fail(error, QStringLiteral("connection profile file exceeds 128 KiB"));
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("cannot read connection profile file"));
        return false;
    }
    content = file.read(kMaxFileBytes + 1);
    if (content.size() > kMaxFileBytes || file.error() != QFile::NoError) {
        fail(error, QStringLiteral("cannot safely read connection profile file"));
        return false;
    }
    return true;
}
} // namespace

ToolConnectionProfileStore::ToolConnectionProfileStore(QString filename)
    : m_filename(std::move(filename))
{
}

bool ToolConnectionProfileStore::valid(const ToolConnectionProfile &profile,
                                       QString *error) const
{
    if (!validUuid(profile.id)) {
        fail(error, QStringLiteral("invalid local profile identity"));
        return false;
    }
    if (profile.name.isEmpty() || profile.name != profile.name.trimmed()
        || profile.name.size() > 80 || !printable(profile.name)) {
        fail(error, QStringLiteral("invalid or oversized display name"));
        return false;
    }
    if (profile.host.isEmpty() || profile.host != profile.host.trimmed()
        || profile.host.size() > 255 || !printable(profile.host)
        || profile.host.contains(QLatin1Char('@'))
        || profile.host.contains(QLatin1Char('/'))
        || profile.host.contains(QLatin1Char('\\'))
        || profile.host.contains(QLatin1Char('?'))
        || profile.host.contains(QLatin1Char('#'))) {
        fail(error, QStringLiteral("invalid host; credentials and URLs are forbidden"));
        return false;
    }
    for (const auto ch : profile.host) {
        if (ch.isSpace()) {
            fail(error, QStringLiteral("host may not contain whitespace"));
            return false;
        }
    }
    if (profile.port == 0 || profile.lastUsedUtcMs < 0
        || profile.lastUsedUtcMs > kMaxStoredUtcMs) {
        fail(error, QStringLiteral("port or recent timestamp is out of range"));
        return false;
    }
    return true;
}

bool ToolConnectionProfileStore::validSet(const QVector<ToolConnectionProfile> &items,
                                          QString *error) const
{
    if (items.size() > kMaxProfiles) {
        fail(error, QStringLiteral("too many saved connections"));
        return false;
    }
    QSet<QString> ids;
    QSet<QString> names;
    QSet<QString> endpoints;
    for (const auto &item : items) {
        if (!valid(item, error))
            return false;
        const QString name = item.name.toCaseFolded();
        const QString endpoint = item.host.toCaseFolded()
                                 + QLatin1Char('/') + QString::number(item.port);
        const QString foldedId = item.id.toCaseFolded();
        if (ids.contains(foldedId) || names.contains(name)
            || endpoints.contains(endpoint)) {
            fail(error, QStringLiteral("duplicate profile identity, name or endpoint"));
            return false;
        }
        ids.insert(foldedId);
        names.insert(name);
        endpoints.insert(endpoint);
    }
    return true;
}

ToolConnectionProfileStore::LoadState
ToolConnectionProfileStore::load(QString *error)
{
    if (error)
        error->clear();
    m_loaded = false;
    m_corrupt = false;
    m_profiles.clear();
    m_diskDigest.clear();
    m_corruptDigest.clear();

    const QFileInfo info(m_filename);
    if (!info.exists()) {
        m_loaded = true;
        return LoadState::Missing;
    }
    if (info.size() > kMaxFileBytes) {
        m_corrupt = true;
        fail(error, QStringLiteral("connection profile file exceeds 128 KiB"));
        return LoadState::Corrupt; // explicit quarantine recovery is possible
    }
    QByteArray raw;
    if (!safeRead(m_filename, raw, error))
        return LoadState::IoError;
    // An untrusted profile file can only be quarantined after a second
    // byte-for-byte digest check under the same writer lock. Retain this
    // digest for bounded data even when JSON parsing or validation fails.
    m_corruptDigest = digest(raw);
    if (raw.size() == 0) {
        m_corrupt = true;
        fail(error, QStringLiteral("connection profile file is empty"));
        return LoadState::Corrupt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_corrupt = true;
        fail(error, QStringLiteral("connection profile JSON is malformed"));
        return LoadState::Corrupt;
    }

    const auto root = document.object();
    const bool legacy = root.value(QStringLiteral("schemaVersion")).isUndefined()
                        && root.value(QStringLiteral("version")).toInt(-1) == 0;
    const int version = legacy ? 0
                        : root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if ((version != 0 && version != kSchemaVersion)
        || !root.value(QStringLiteral("profiles")).isArray()
        || !allowedKeys(root, legacy
                           ? QSet<QString>{QStringLiteral("version"), QStringLiteral("profiles")}
                           : QSet<QString>{QStringLiteral("schemaVersion"), QStringLiteral("profiles")})) {
        m_corrupt = true;
        fail(error, QStringLiteral("unsupported or unknown connection profile schema"));
        return LoadState::Corrupt;
    }

    const auto array = root.value(QStringLiteral("profiles")).toArray();
    if (array.size() > kMaxProfiles) {
        m_corrupt = true;
        fail(error, QStringLiteral("too many profile entries in file"));
        return LoadState::Corrupt;
    }
    QVector<ToolConnectionProfile> next;
    next.reserve(array.size());
    for (const auto &entry : array) {
        if (!entry.isObject()) {
            m_corrupt = true;
            fail(error, QStringLiteral("profile entry is not an object"));
            return LoadState::Corrupt;
        }
        const auto item = entry.toObject();
        if (!allowedKeys(item, legacy
                              ? QSet<QString>{QStringLiteral("name"), QStringLiteral("host"),
                                              QStringLiteral("port")}
                              : QSet<QString>{QStringLiteral("id"), QStringLiteral("name"),
                                              QStringLiteral("host"), QStringLiteral("port"),
                                              QStringLiteral("lastUsedUtcMs")})) {
            m_corrupt = true;
            fail(error, QStringLiteral("unexpected profile fields (secrets forbidden)"));
            return LoadState::Corrupt;
        }
        const auto portValue = item.value(QStringLiteral("port"));
        if (!portValue.isDouble() || portValue.toDouble() < 1
            || portValue.toDouble() > 65535
            || std::floor(portValue.toDouble()) != portValue.toDouble()) {
            m_corrupt = true;
            fail(error, QStringLiteral("invalid profile port"));
            return LoadState::Corrupt;
        }
        const auto timeValue = item.value(QStringLiteral("lastUsedUtcMs"));
        if (!legacy && (!timeValue.isDouble()
                        || !std::isfinite(timeValue.toDouble())
                        || timeValue.toDouble() < 0
                        || timeValue.toDouble() > kMaxStoredUtcMs
                        || std::floor(timeValue.toDouble()) != timeValue.toDouble())) {
            m_corrupt = true;
            fail(error, QStringLiteral("invalid profile recent timestamp"));
            return LoadState::Corrupt;
        }
        if (!item.value(QStringLiteral("name")).isString()
            || !item.value(QStringLiteral("host")).isString()
            || (!legacy && !item.value(QStringLiteral("id")).isString())) {
            m_corrupt = true;
            fail(error, QStringLiteral("malformed profile identity/name/host"));
            return LoadState::Corrupt;
        }

        ToolConnectionProfile profile;
        profile.id = legacy ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                            : item.value(QStringLiteral("id")).toString();
        profile.name = item.value(QStringLiteral("name")).toString();
        profile.host = item.value(QStringLiteral("host")).toString();
        profile.port = static_cast<quint16>(portValue.toInt());
        profile.lastUsedUtcMs = legacy ? 0 : static_cast<qint64>(timeValue.toDouble());
        next.append(std::move(profile));
    }
    if (!validSet(next, error)) {
        m_corrupt = true;
        return LoadState::Corrupt;
    }
    sortProfiles(next);
    m_profiles = std::move(next);
    m_loaded = true;
    m_diskDigest = m_corruptDigest;
    m_corruptDigest.clear();
    return legacy ? LoadState::MigratedV0 : LoadState::Loaded;
}

bool ToolConnectionProfileStore::editable(QString *error) const
{
    if (m_loaded && !m_corrupt)
        return true;
    fail(error, QStringLiteral("profile store not loaded or needs explicit recovery"));
    return false;
}

bool ToolConnectionProfileStore::persist(const QVector<ToolConnectionProfile> &updated,
                                         QString *error)
{
    if (!editable(error) || !validSet(updated, error))
        return false;
    QFileInfo info(m_filename);
    if (!QDir().mkpath(info.absolutePath())) {
        fail(error, QStringLiteral("cannot create connection profile directory"));
        return false;
    }
    QLockFile lock(m_filename + QStringLiteral(".lock"));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(1000)) {
        fail(error, QStringLiteral("connection profile file is locked"));
        return false;
    }

    QByteArray current;
    if (!safeRead(m_filename, current, error))
        return false;
    // Existing zero-byte files are NOT equivalent to a missing path. A
    // second process may have created a corrupt file since our load().
    if ((QFileInfo::exists(m_filename) ? digest(current) : QByteArray())
        != m_diskDigest) {
        fail(error, QStringLiteral("connection profiles changed in another process; reload first"));
        return false;
    }

    QJsonArray array;
    for (const auto &profile : updated) {
        QJsonObject item;
        item.insert(QStringLiteral("id"), profile.id);
        item.insert(QStringLiteral("name"), profile.name);
        item.insert(QStringLiteral("host"), profile.host);
        item.insert(QStringLiteral("port"), static_cast<int>(profile.port));
        item.insert(QStringLiteral("lastUsedUtcMs"),
                    static_cast<double>(profile.lastUsedUtcMs));
        array.append(item);
    }
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    root.insert(QStringLiteral("profiles"), array);
    const auto encoded = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (encoded.size() > kMaxFileBytes) {
        fail(error, QStringLiteral("connection profile save exceeds 128 KiB"));
        return false;
    }
    QSaveFile writer(m_filename);
    if (!writer.open(QIODevice::WriteOnly) || writer.write(encoded) != encoded.size()
        || !writer.commit()) {
        fail(error, QStringLiteral("cannot atomically save connection profiles"));
        return false;
    }

    m_diskDigest = digest(encoded);
    m_profiles = updated;
    sortProfiles(m_profiles);
    return true;
}

bool ToolConnectionProfileStore::add(QString name, QString host, quint16 port,
                                     QString *idOut, QString *error)
{
    ToolConnectionProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = std::move(name);
    profile.host = std::move(host);
    profile.port = port;
    auto next = m_profiles;
    next.append(profile);
    if (!persist(next, error))
        return false;
    if (idOut)
        *idOut = profile.id;
    return true;
}

bool ToolConnectionProfileStore::edit(const QString &id, QString name, QString host,
                                      quint16 port, QString *error)
{
    auto next = m_profiles;
    for (auto &item : next) {
        if (item.id != id)
            continue;
        item.name = std::move(name);
        item.host = std::move(host);
        item.port = port;
        return persist(next, error);
    }
    fail(error, QStringLiteral("saved profile not found"));
    return false;
}

bool ToolConnectionProfileStore::remove(const QString &id, QString *error)
{
    auto next = m_profiles;
    for (auto it = next.begin(); it != next.end(); ++it) {
        if (it->id == id) {
            next.erase(it);
            return persist(next, error);
        }
    }
    fail(error, QStringLiteral("saved profile not found"));
    return false;
}

bool ToolConnectionProfileStore::markUsed(const QString &id, qint64 nowUtcMs,
                                          QString *error)
{
    if (nowUtcMs < 0 || nowUtcMs > kMaxStoredUtcMs) {
        fail(error, QStringLiteral("invalid recent timestamp"));
        return false;
    }
    auto next = m_profiles;
    for (auto &item : next) {
        if (item.id == id) {
            item.lastUsedUtcMs = nowUtcMs;
            return persist(next, error);
        }
    }
    fail(error, QStringLiteral("saved profile not found"));
    return false;
}

bool ToolConnectionProfileStore::recoverCorrupt(QString *backupPath,
                                                QString *error)
{
    if (!m_corrupt || m_loaded || !QFileInfo::exists(m_filename)) {
        fail(error, QStringLiteral("no corrupt profile file requiring recovery"));
        return false;
    }
    if (m_corruptDigest.isEmpty()) {
        // A file above 128 KiB was deliberately not ingested into memory.
        // It has no defensible original fingerprint; keep it untouched
        // rather than moving a possibly new file based on old metadata.
        fail(error, QStringLiteral("oversized corrupt profile requires manual recovery"));
        return false;
    }
    QLockFile lock(m_filename + QStringLiteral(".lock"));
    if (!lock.tryLock(1000)) {
        fail(error, QStringLiteral("connection profile file is locked"));
        return false;
    }
    QByteArray current;
    if (!QFileInfo::exists(m_filename)
        || !safeRead(m_filename, current, error)
        || digest(current) != m_corruptDigest) {
        fail(error, QStringLiteral("corrupt profile changed since load; reload before recovery"));
        return false;
    }
    const auto target = m_filename + QStringLiteral(".corrupt-")
                        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QFile::rename(m_filename, target)) {
        fail(error, QStringLiteral("cannot quarantine corrupt profile file"));
        return false;
    }
    if (backupPath)
        *backupPath = target;
    m_profiles.clear();
    m_loaded = true;
    m_corrupt = false;
    m_diskDigest.clear();
    m_corruptDigest.clear();
    return true;
}
