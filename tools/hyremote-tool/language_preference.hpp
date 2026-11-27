#pragma once

#include <QLocale>
#include <QSettings>
#include <QString>

#include <optional>

// S1 (#590): a single source of truth for the local Tool UI language.
// The preference belongs to THIS installation of HyRemoteTool, not to the
// remote RFB host and not to the Framework's runtime configuration.
enum class ToolLanguage {
    Chinese,
    English,
};

enum class ToolLanguagePreference {
    Auto,
    Chinese,
    English,
};

inline QString toolLanguagePreferenceToken(ToolLanguagePreference preference)
{
    switch (preference) {
    case ToolLanguagePreference::Auto: return QStringLiteral("auto");
    case ToolLanguagePreference::Chinese: return QStringLiteral("zh");
    case ToolLanguagePreference::English: return QStringLiteral("en");
    }
    return QStringLiteral("auto");
}

inline ToolLanguagePreference toolLanguagePreferenceFromToken(const QString &token)
{
    if (token == QStringLiteral("zh"))
        return ToolLanguagePreference::Chinese;
    if (token == QStringLiteral("en"))
        return ToolLanguagePreference::English;
    // Unknown/corrupt values are never interpreted as a hidden language
    // override. The documented default is to follow the local OS.
    return ToolLanguagePreference::Auto;
}

inline std::optional<ToolLanguage> parseToolCliLanguage(const QString &token)
{
    if (token.compare(QStringLiteral("zh"), Qt::CaseInsensitive) == 0)
        return ToolLanguage::Chinese;
    if (token.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0)
        return ToolLanguage::English;
    return std::nullopt;
}

inline ToolLanguagePreference loadToolLanguagePreference(QSettings &settings)
{
    return toolLanguagePreferenceFromToken(
        settings.value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString());
}

inline bool saveToolLanguagePreference(QSettings &settings,
                                       ToolLanguagePreference preference)
{
    settings.setValue(QStringLiteral("ui/language"),
                      toolLanguagePreferenceToken(preference));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

inline ToolLanguage resolveToolLanguage(ToolLanguagePreference preference,
                                       const QLocale &localSystemLocale,
                                       std::optional<ToolLanguage> cliOverride = std::nullopt)
{
    // A command-line override applies to this launch only. It never changes
    // QSettings; a later manual UI selection may explicitly save a preference.
    if (cliOverride)
        return *cliOverride;
    if (preference == ToolLanguagePreference::Chinese)
        return ToolLanguage::Chinese;
    if (preference == ToolLanguagePreference::English)
        return ToolLanguage::English;

    // All zh-* locales, including zh_TW, use the available Chinese UI.
    // Other languages fall back to English until translated.
    return localSystemLocale.language() == QLocale::Chinese
               ? ToolLanguage::Chinese
               : ToolLanguage::English;
}
