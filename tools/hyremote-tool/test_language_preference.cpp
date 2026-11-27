#include "language_preference.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <iostream>

namespace {
int failures = 0;
#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "LANGUAGE_PREFERENCE_FAIL line " << __LINE__ << ": " #expr << '\n'; \
    ++failures; } } while (false)

void testSystemAutoAndFallback()
{
    using P = ToolLanguagePreference;
    using L = ToolLanguage;
    CHECK(resolveToolLanguage(P::Auto, QLocale(QStringLiteral("zh_CN"))) == L::Chinese);
    CHECK(resolveToolLanguage(P::Auto, QLocale(QStringLiteral("zh_TW"))) == L::Chinese);
    CHECK(resolveToolLanguage(P::Auto, QLocale(QStringLiteral("en_US"))) == L::English);
    CHECK(resolveToolLanguage(P::Auto, QLocale(QStringLiteral("fr_FR"))) == L::English);
    CHECK(resolveToolLanguage(P::Chinese, QLocale(QStringLiteral("en_US"))) == L::Chinese);
    CHECK(resolveToolLanguage(P::English, QLocale(QStringLiteral("zh_CN"))) == L::English);
    CHECK(toolLanguagePreferenceToken(P::Auto) == QStringLiteral("auto"));
    CHECK(toolLanguagePreferenceToken(P::Chinese) == QStringLiteral("zh"));
    CHECK(toolLanguagePreferenceToken(P::English) == QStringLiteral("en"));
    CHECK(toolLanguagePreferenceFromToken(QStringLiteral("unexpected")) == P::Auto);
    CHECK(toolLanguagePreferenceFromToken(QStringLiteral("EN")) == P::Auto);
}

void testLocalPreferenceAcrossRestartAndCliIsolation()
{
    using P = ToolLanguagePreference;
    using L = ToolLanguage;
    QTemporaryDir temp;
    CHECK(temp.isValid());
    if (!temp.isValid())
        return;
    const QString path = temp.filePath(QStringLiteral("hyremote-tool.ini"));
    {
        QSettings settings(path, QSettings::IniFormat);
        CHECK(loadToolLanguagePreference(settings) == P::Auto);
        CHECK(saveToolLanguagePreference(settings, P::Chinese));
        CHECK(settings.value(QStringLiteral("ui/language")).toString()
              == QStringLiteral("zh"));
    }
    {
        // Simulates process restart, even when the system locale is English.
        QSettings settings(path, QSettings::IniFormat);
        const auto saved = loadToolLanguagePreference(settings);
        CHECK(saved == P::Chinese);
        CHECK(resolveToolLanguage(saved, QLocale(QStringLiteral("en_US")))
              == L::Chinese);
        const auto cli = parseToolCliLanguage(QStringLiteral("EN"));
        CHECK(cli && *cli == L::English);
        CHECK(resolveToolLanguage(saved, QLocale(QStringLiteral("en_US")), cli)
              == L::English);
        // CLI override must not call the storage mutation path.
        CHECK(loadToolLanguagePreference(settings) == P::Chinese);
        CHECK(saveToolLanguagePreference(settings, P::Auto));
    }
    {
        QSettings settings(path, QSettings::IniFormat);
        const auto preference = loadToolLanguagePreference(settings);
        CHECK(preference == P::Auto);
        CHECK(resolveToolLanguage(preference, QLocale(QStringLiteral("fr_FR")))
              == L::English);
        CHECK(resolveToolLanguage(preference, QLocale(QStringLiteral("zh_CN")))
              == L::Chinese);
        CHECK(saveToolLanguagePreference(settings, P::English));
    }
    {
        QSettings settings(path, QSettings::IniFormat);
        CHECK(loadToolLanguagePreference(settings) == P::English);
        CHECK(resolveToolLanguage(loadToolLanguagePreference(settings),
                                  QLocale(QStringLiteral("zh_TW"))) == L::English);
        settings.setValue(QStringLiteral("ui/language"), QStringLiteral("bad-value"));
        settings.sync();
        CHECK(loadToolLanguagePreference(settings) == P::Auto);
    }
}

void testStrictCommandLine()
{
    CHECK(parseToolCliLanguage(QStringLiteral("zh")) == ToolLanguage::Chinese);
    CHECK(parseToolCliLanguage(QStringLiteral("en")) == ToolLanguage::English);
    CHECK(parseToolCliLanguage(QStringLiteral("ZH")) == ToolLanguage::Chinese);
    CHECK(!parseToolCliLanguage(QStringLiteral("auto")));
    CHECK(!parseToolCliLanguage(QStringLiteral("zh_TW")));
    CHECK(!parseToolCliLanguage(QString()));
    CHECK(!parseToolCliLanguage(QStringLiteral("unsupported")));
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testSystemAutoAndFallback();
    testLocalPreferenceAcrossRestartAndCliIsolation();
    testStrictCommandLine();
    if (failures)
        std::cerr << "TOOL_LANGUAGE_PREFERENCE_FAILED=" << failures << '\n';
    else
        std::cout << "TOOL_LANGUAGE_PREFERENCE_PASS" << std::endl;
    return failures ? 1 : 0;
}
