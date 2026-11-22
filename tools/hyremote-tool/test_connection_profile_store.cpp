#include "connection_profile_store.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>

namespace {
int failures = 0;
#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "PROFILE_TEST_FAILED line " << __LINE__ << ": " #expr << '\n'; \
    ++failures; } } while (false)

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

void testSaveEditOrderAndRestart()
{
    QTemporaryDir root;
    CHECK(root.isValid());
    if (!root.isValid())
        return;
    const auto path = root.filePath(QStringLiteral("profiles/connections.json"));
    QString error, id1, id2;
    ToolConnectionProfileStore store(path);
    CHECK(store.load(&error) == ToolConnectionProfileStore::LoadState::Missing);
    CHECK(error.isEmpty());
    CHECK(store.profiles().isEmpty());

    CHECK(store.add(QStringLiteral("Factory HMI"), QStringLiteral("hmi1.local"), 5901,
                    &id1, &error));
    CHECK(store.add(QStringLiteral("Field HMI"), QStringLiteral("192.168.1.6"), 5921,
                    &id2, &error));
    CHECK(id1 != id2 && id1.size() == 36 && id2.size() == 36);
    CHECK(store.profiles().size() == 2);
    CHECK(store.profiles().at(0).name == QStringLiteral("Factory HMI")); // alphabetical

    CHECK(!store.add(QStringLiteral("factory hmi"), QStringLiteral("other.local"), 5901,
                     nullptr, &error)); // duplicate name, case-insensitive
    CHECK(!store.add(QStringLiteral("Other"), QStringLiteral("HMI1.LOCAL"), 5901,
                     nullptr, &error)); // duplicate endpoint, case-insensitive
    CHECK(!store.add(QStringLiteral("Other"), QStringLiteral("user:pass@host"), 5901,
                     nullptr, &error)); // never persist embedded credentials
    CHECK(!store.add(QStringLiteral("Other"), QStringLiteral("bad host"), 5901,
                     nullptr, &error)); // invalid host
    CHECK(!store.add(QStringLiteral("Bad\nName"), QStringLiteral("good.local"), 5901,
                     nullptr, &error)); // control characters
    CHECK(!store.add(QStringLiteral("Other"), QStringLiteral("good.local"), 0,
                     nullptr, &error));
    CHECK(store.profiles().size() == 2);

    CHECK(store.markUsed(id2, 1700000000000LL, &error));
    CHECK(store.profiles().at(0).id == id2); // recently-used first
    CHECK(store.edit(id1, QStringLiteral("Factory Renamed"),
                     QStringLiteral("hmi-new.local"), 5903, &error));
    CHECK(!store.edit(id1, QStringLiteral("Field HMI"),
                      QStringLiteral("hmi-new.local"), 5903, &error));
    CHECK(store.profiles().size() == 2);
    {
        QFile file(path);
        CHECK(file.open(QIODevice::ReadOnly));
        const auto data = file.readAll();
        CHECK(!data.contains("password"));
        CHECK(!data.contains("token"));
        CHECK(!data.contains("secret"));
        CHECK(!data.contains("user:pass"));
        CHECK(QJsonDocument::fromJson(data).object()
                  .value(QStringLiteral("schemaVersion")).toInt(-1) == 1);
    }

    ToolConnectionProfileStore restarted(path);
    CHECK(restarted.load(&error) == ToolConnectionProfileStore::LoadState::Loaded);
    CHECK(restarted.profiles().size() == 2);
    CHECK(restarted.profiles().at(0).id == id2);
    CHECK(restarted.profiles().at(0).lastUsedUtcMs == 1700000000000LL);
    CHECK(restarted.profiles().at(1).name == QStringLiteral("Factory Renamed"));
    CHECK(restarted.remove(id1, &error));
    CHECK(restarted.profiles().size() == 1);
    CHECK(!restarted.remove(id1, &error));
    CHECK(restarted.profiles().size() == 1);

    ToolConnectionProfileStore last(path);
    CHECK(last.load(&error) == ToolConnectionProfileStore::LoadState::Loaded);
    CHECK(last.profiles().size() == 1 && last.profiles().at(0).id == id2);
}

void testStaleWriterAndFailedDisk()
{
    QTemporaryDir root;
    CHECK(root.isValid());
    if (!root.isValid())
        return;
    const QString path = root.filePath(QStringLiteral("profiles.json"));
    ToolConnectionProfileStore a(path), b(path);
    CHECK(a.load() == ToolConnectionProfileStore::LoadState::Missing);
    CHECK(b.load() == ToolConnectionProfileStore::LoadState::Missing);
    CHECK(a.add(QStringLiteral("A"), QStringLiteral("host-a"), 5901));
    QString error;
    CHECK(!b.add(QStringLiteral("B"), QStringLiteral("host-b"), 5902, nullptr, &error));
    CHECK(error.contains(QStringLiteral("another process")));
    CHECK(b.profiles().isEmpty());
    CHECK(b.load() == ToolConnectionProfileStore::LoadState::Loaded);
    CHECK(b.add(QStringLiteral("B"), QStringLiteral("host-b"), 5902));
    CHECK(!a.add(QStringLiteral("C"), QStringLiteral("host-c"), 5903));

    // This parent is a regular file: cannot create a directory below it,
    // deterministic even for CI runners running as Administrator/root.
    const QString blockingPath = root.filePath(QStringLiteral("blocked"));
    CHECK(writeFile(blockingPath, "not-a-directory"));
    ToolConnectionProfileStore blocked(blockingPath + QStringLiteral("/profiles.json"));
    CHECK(blocked.load() == ToolConnectionProfileStore::LoadState::Missing);
    CHECK(!blocked.add(QStringLiteral("Never persisted"), QStringLiteral("host"), 5900,
                       nullptr, &error));
    CHECK(blocked.profiles().isEmpty());
}

void testCorruptionAndExplicitRecovery()
{
    QTemporaryDir root;
    CHECK(root.isValid());
    if (!root.isValid())
        return;
    const QString path = root.filePath(QStringLiteral("profiles.json"));
    QString error;
    const QByteArray corrupt = "{not-json";
    CHECK(writeFile(path, corrupt));
    ToolConnectionProfileStore store(path);
    CHECK(store.load(&error) == ToolConnectionProfileStore::LoadState::Corrupt);
    CHECK(!store.add(QStringLiteral("X"), QStringLiteral("host"), 5900));
    CHECK(QFileInfo::exists(path)); // don't overwrite valuable broken source
    QString backup;
    CHECK(store.recoverCorrupt(&backup, &error));
    CHECK(!QFileInfo::exists(path));
    CHECK(QFileInfo::exists(backup));
    {
        QFile file(backup);
        CHECK(file.open(QIODevice::ReadOnly));
        CHECK(file.readAll() == corrupt);
    }
    CHECK(store.add(QStringLiteral("X"), QStringLiteral("host"), 5900));
    CHECK(QFileInfo::exists(path));

    const auto payloads = {
        QByteArray("{\"schemaVersion\":999,\"profiles\":[]}"),
        QByteArray("{\"schemaVersion\":1,\"profiles\":[{\"password\":\"leak\"}]}"),
        QByteArray("{\"schemaVersion\":1,\"profiles\":\"bad\"}"),
        QByteArray("{\"schemaVersion\":1,\"profiles\":[1]}"),
    };
    for (const auto &payload : payloads) {
        CHECK(writeFile(path, payload));
        ToolConnectionProfileStore rejected(path);
        CHECK(rejected.load(&error) == ToolConnectionProfileStore::LoadState::Corrupt);
        CHECK(!rejected.add(QStringLiteral("Z"), QStringLiteral("hostz"), 5901));
        CHECK(rejected.profiles().isEmpty());
    }

    QByteArray oversized(128 * 1024 + 1, 'x');
    CHECK(writeFile(path, oversized));
    ToolConnectionProfileStore huge(path);
    CHECK(huge.load(&error) == ToolConnectionProfileStore::LoadState::Corrupt);
    CHECK(!huge.add(QStringLiteral("Z"), QStringLiteral("hostz"), 5901));
    CHECK(!huge.recoverCorrupt(&backup, &error));
    CHECK(QFileInfo::exists(path)); // over-limit: no bounded snapshot to check
}

void testTwoCorruptionRecoveriesCannotClobberFreshData()
{
    QTemporaryDir root;
    CHECK(root.isValid());
    if (!root.isValid())
        return;
    const auto path = root.filePath(QStringLiteral("profiles.json"));
    CHECK(writeFile(path, "{bad-json"));
    ToolConnectionProfileStore first(path), second(path);
    QString error;
    CHECK(first.load(&error) == ToolConnectionProfileStore::LoadState::Corrupt);
    CHECK(second.load(&error) == ToolConnectionProfileStore::LoadState::Corrupt);
    CHECK(second.recoverCorrupt(nullptr, &error));
    CHECK(second.add(QStringLiteral("Recovered"),
                     QStringLiteral("new-hmi.local"), 5901, nullptr, &error));
    CHECK(!first.recoverCorrupt(nullptr, &error)); // original bad snapshot is stale
    CHECK(error.contains(QStringLiteral("changed")));
    ToolConnectionProfileStore restored(path);
    CHECK(restored.load(&error) == ToolConnectionProfileStore::LoadState::Loaded);
    CHECK(restored.profiles().size() == 1);
    CHECK(restored.profiles().at(0).name == QStringLiteral("Recovered"));

    // A file created as empty after a missing-file load is a real concurrent
    // write, not the same as no file. The stale instance may not erase it.
    const auto emptyPath = root.filePath(QStringLiteral("empty.json"));
    ToolConnectionProfileStore missing(emptyPath);
    CHECK(missing.load(&error) == ToolConnectionProfileStore::LoadState::Missing);
    CHECK(writeFile(emptyPath, ""));
    CHECK(!missing.add(QStringLiteral("A"), QStringLiteral("hmi.local"),
                       5901, nullptr, &error));
    CHECK(QFileInfo::exists(emptyPath));
}

void testLegacyMigration()
{
    QTemporaryDir root;
    CHECK(root.isValid());
    if (!root.isValid())
        return;
    const auto path = root.filePath(QStringLiteral("legacy.json"));
    CHECK(writeFile(path, "{\"version\":0,\"profiles\":["
                     "{\"name\":\"Legacy\",\"host\":\"legacy.local\",\"port\":5900}]}"));
    ToolConnectionProfileStore store(path);
    QString error;
    CHECK(store.load(&error) == ToolConnectionProfileStore::LoadState::MigratedV0);
    CHECK(store.profiles().size() == 1);
    CHECK(store.profiles().at(0).name == QStringLiteral("Legacy"));
    const auto id = store.profiles().at(0).id;
    CHECK(store.edit(id, QStringLiteral("Modern"), QStringLiteral("legacy.local"), 5900));
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    const auto migrated = QJsonDocument::fromJson(file.readAll()).object();
    CHECK(migrated.value(QStringLiteral("schemaVersion")).toInt(-1) == 1);
    CHECK(!migrated.contains(QStringLiteral("version")));
    CHECK(store.profiles().at(0).id == id);
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testSaveEditOrderAndRestart();
    testStaleWriterAndFailedDisk();
    testCorruptionAndExplicitRecovery();
    testTwoCorruptionRecoveriesCannotClobberFreshData();
    testLegacyMigration();
    if (failures) {
        std::cerr << "TOOL_PROFILES_FAILED=" << failures << '\n';
        return 1;
    }
    std::cout << "TOOL_PROFILES_PASS" << std::endl;
    return 0;
}
