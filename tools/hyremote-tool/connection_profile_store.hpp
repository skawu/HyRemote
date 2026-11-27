#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>

// #583 V0.3.6 S1: non-secret local connection bookmarks only.
// No runtime, credential, View/Edge device trust or automatic network work.
struct ToolConnectionProfile {
    QString id;
    QString name;
    QString host;
    quint16 port = 0;
    qint64 lastUsedUtcMs = 0;
};

class ToolConnectionProfileStore final {
public:
    enum class LoadState { Missing, Loaded, MigratedV0, Corrupt, IoError };

    explicit ToolConnectionProfileStore(QString filename);

    LoadState load(QString *error = nullptr);
    const QVector<ToolConnectionProfile> &profiles() const noexcept { return m_profiles; }
    const QString &filePath() const noexcept { return m_filename; }
    bool add(QString name, QString host, quint16 port,
             QString *idOut = nullptr, QString *error = nullptr);
    bool edit(const QString &id, QString name, QString host, quint16 port,
              QString *error = nullptr);
    bool remove(const QString &id, QString *error = nullptr);
    bool markUsed(const QString &id, qint64 nowUtcMs, QString *error = nullptr);

    // Caller must first obtain user consent. A corrupt file is renamed
    // to a unique adjacent .corrupt-<UUID> path; never silently overwritten.
    bool recoverCorrupt(QString *backupPath = nullptr, QString *error = nullptr);

private:
    bool persist(const QVector<ToolConnectionProfile> &updated,
                 QString *error);
    bool valid(const ToolConnectionProfile &profile, QString *error) const;
    bool validSet(const QVector<ToolConnectionProfile> &profiles, QString *error) const;
    bool editable(QString *error) const;

    QString m_filename;
    QVector<ToolConnectionProfile> m_profiles;
    bool m_loaded = false;
    bool m_corrupt = false;
    QByteArray m_diskDigest; // optimistic stale-writer detection under QLockFile
    QByteArray m_corruptDigest; // exact bounded bad-file snapshot for explicit recovery
};
