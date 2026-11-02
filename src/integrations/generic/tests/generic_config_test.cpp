// #174 mapping, Generic Plugin: the launch syntax onto the one frontend-neutral runtime contract. This is mapping
// only - the parser never resolves an interface and never invents an address - so a real adapter is not needed here.

#include "../generic_config.hpp"
#include "detail/windows_unicode_launch.hpp"

#include <QHostAddress>
#include <QString>

#include <iostream>

namespace {

int failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr << '\n';       \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)

}  // namespace

int main()
{
    using HyRemote::Generic::parseSpecification;
    using HyRemote::Runtime::Automatic::AccessConfig;

    // An empty specification is the runtime default: the IPv4 wildcard, no interface, the default port.
    {
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(QString(), config, error));
        CHECK(error.isEmpty());
        CHECK(config.listenAddress == QHostAddress(QHostAddress::AnyIPv4));
        CHECK(config.listenInterface.isEmpty());
        CHECK(config.port == HYREMOTE_DEFAULT_PORT);
    }

    // interface=<QNetworkInterface::name()> selects interface mode and leaves the address alone.
    {
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(QStringLiteral("interface=eth-test"), config, error));
        CHECK(error.isEmpty());
        CHECK(config.listenInterface == QStringLiteral("eth-test"));
        CHECK(config.listenAddress == QHostAddress(QHostAddress::AnyIPv4));
    }

    // address=<ipv4> still selects exactly one address, with no interface.
    {
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(QStringLiteral("address=192.168.1.4"), config, error));
        CHECK(config.listenAddress == QHostAddress(QStringLiteral("192.168.1.4")));
        CHECK(config.listenInterface.isEmpty());
    }

    // Both keys describe the same decision, so the parser refuses rather than silently choosing one - in either order.
    {
        AccessConfig config;
        QString error;
        CHECK(!parseSpecification(QStringLiteral("address=192.168.1.4;interface=eth-test"), config, error));
        CHECK(error.contains(QStringLiteral("mutually exclusive")));
    }
    {
        AccessConfig config;
        QString error;
        CHECK(!parseSpecification(QStringLiteral("interface=eth-test;address=192.168.1.4"), config, error));
        CHECK(error.contains(QStringLiteral("mutually exclusive")));
    }

    // #589: the watermark is stated in the launch syntax and validated by the shared policy, not by the parser.
    {
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(QStringLiteral("watermark=内部资料;watermark-position=tiled;"
                                                "watermark-opacity=0.5;watermark-font-size=24;"
                                                "watermark-color=#00FF00"),
                                 config, error));
        CHECK(error.isEmpty());
        CHECK(config.watermarkText == QStringLiteral("内部资料"));
        CHECK(config.watermarkPosition == QStringLiteral("tiled"));
        CHECK(config.watermarkOpacity.has_value() && *config.watermarkOpacity == 0.5);
        CHECK(config.watermarkFontPointSize.has_value() && *config.watermarkFontPointSize == 24);
        CHECK(config.watermarkColor.has_value() && *config.watermarkColor == QStringLiteral("#00FF00"));
    }
    // #761: Once Qt has delivered a valid QString, the Generic adapter must preserve CJK
    // characters AND internal spaces through the semicolon-separated specification. This
    // intentionally does not assert that every Windows shell/argv path produces that QString.
    {
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(QStringLiteral("port=5921;watermark=水印候选"), config, error));
        CHECK(error.isEmpty());
        CHECK(config.watermarkText == QStringLiteral("水印候选"));
    }
    {
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(QStringLiteral("watermark=持久 候选;watermark-position=top-left"),
                                 config, error));
        CHECK(error.isEmpty());
        CHECK(config.watermarkText == QStringLiteral("持久 候选"));
        CHECK(config.watermarkPosition == QStringLiteral("top-left"));
    }
    {
        AccessConfig config;
        QString error;
        CHECK(!parseSpecification(QStringLiteral("watermark-position=centre"), config, error));
        CHECK(error.contains(QStringLiteral("watermark-position")));
    }
    {
        AccessConfig config;
        QString error;
        CHECK(!parseSpecification(QStringLiteral("watermark-opacity=high"), config, error));
        CHECK(error.contains(QStringLiteral("watermark-opacity")));
    }

#ifdef Q_OS_WIN
    // #761: Simulate the exact ANSI-936 byte->Latin-1 widening observed in
    // the frozen Windows candidate. The original wide -plugin token must attest
    // the recovery. Both ordinary Latin-1 and ambiguous/unrelated options stay untouched.
    {
        const QString original = QStringLiteral("port=5921;watermark=水印候选");
        const QString widened = QStringLiteral("port=5921;watermark=")
                                + QString::fromLatin1(QByteArray::fromHex("cbaed3a1baf2d1a1"));
        CHECK(HyRemote::detail::oneByteWidening(original, 936) == widened);
        const QString recovered = HyRemote::detail::recoverWindowsGenericSpecification(
            widened, {QStringLiteral("app.exe"), QStringLiteral("-plugin"),
                      QStringLiteral("hyremote:") + original}, 936);
        CHECK(recovered == original);
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(recovered, config, error));
        CHECK(config.watermarkText == QStringLiteral("水印候选"));
        CHECK(HyRemote::detail::recoverWindowsGenericSpecification(
                  widened, {QStringLiteral("-plugin"), QStringLiteral("another:") + original}, 936)
              == widened);
        CHECK(HyRemote::detail::recoverWindowsGenericSpecification(
                  widened, {QStringLiteral("-plugin"), QStringLiteral("hyremote:watermark=Ë®Ó¡")}, 936)
              == widened);
    }
    {
        const QString original = QStringLiteral("watermark=持久 候选;watermark-position=top-left");
        const QString widened = QStringLiteral("watermark=")
                                + QString::fromLatin1(QByteArray::fromHex("b3d6bec320baf2d1a1"))
                                + QStringLiteral(";watermark-position=top-left");
        CHECK(HyRemote::detail::oneByteWidening(original, 936) == widened);
        const QString recovered = HyRemote::detail::recoverWindowsGenericSpecification(
            widened, {QStringLiteral("-plugin"), QStringLiteral("hyremote:") + original}, 936);
        CHECK(recovered == original);
        AccessConfig config;
        QString error;
        CHECK(parseSpecification(recovered, config, error));
        CHECK(config.watermarkText == QStringLiteral("持久 候选"));
        CHECK(HyRemote::detail::recoverWindowsGenericSpecification(
                  original, {QStringLiteral("-plugin"), QStringLiteral("hyremote:") + original}, 936)
              == original);
    }
#endif

    if (failures != 0)
        std::cerr << failures << " generic config checks failed\n";
    return failures == 0 ? 0 : 1;
}
