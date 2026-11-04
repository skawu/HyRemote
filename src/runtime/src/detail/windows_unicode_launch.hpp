#pragma once

// #761: Qt's Windows startup can widen CRT ANSI bytes as Latin-1 before a zero-code
// plugin receives its QString parameters. Recover ONLY when the original UTF-16
// Windows command line independently attests the exact intended HyRemote option.
// Never blindly reinterpret legitimate Latin-1 application/plugin input as GBK.

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <string>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#endif

namespace HyRemote::detail {

#ifdef Q_OS_WIN
inline QString oneByteWidening(const QString &wide, unsigned int codePage)
{
    if (wide.isEmpty())
        return {};
    const std::wstring original = wide.toStdWString();
    const bool utf8 = codePage == CP_UTF8;
    BOOL substituted = FALSE;
    const int flags = utf8 ? 0 : WC_NO_BEST_FIT_CHARS;
    const int length = WideCharToMultiByte(codePage, flags, original.data(),
                                           static_cast<int>(original.size()), nullptr, 0,
                                           nullptr, utf8 ? nullptr : &substituted);
    if (length <= 0 || substituted)
        return {};
    QByteArray bytes(length, '\0');
    substituted = FALSE;
    if (WideCharToMultiByte(codePage, flags, original.data(),
                            static_cast<int>(original.size()), bytes.data(), length,
                            nullptr, utf8 ? nullptr : &substituted) != length
        || substituted)
        return {};
    return QString::fromLatin1(bytes);
}

inline QStringList originalWindowsArguments()
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    QStringList result;
    if (!argv)
        return result;
    for (int i = 0; i < argc; ++i)
        result.append(QString::fromWCharArray(argv[i]));
    LocalFree(argv);
    return result;
}

inline QStringList withOriginalWindowsEnvironment(QStringList args)
{
    // Qt also supports these two environment launch paths. Read the already-wide
    // Windows environment through Qt; they are never treated as ANSI byte strings.
    const QString generic = qEnvironmentVariable("QT_QPA_GENERIC_PLUGINS");
    if (!generic.isEmpty()) {
        args.append(QStringLiteral("-plugin"));
        args.append(generic);
    }
    const QString qpa = qEnvironmentVariable("QT_QPA_PLATFORM");
    if (!qpa.isEmpty()) {
        args.append(QStringLiteral("-platform"));
        args.append(qpa);
    }
    return args;
}

inline QString recoverWindowsGenericSpecification(const QString &received,
                                                  const QStringList &originalArguments,
                                                  unsigned int codePage)
{
    QString match;
    int matches = 0;
    for (int i = 0; i + 1 < originalArguments.size(); ++i) {
        if (originalArguments.at(i).compare(QStringLiteral("-plugin"), Qt::CaseInsensitive) != 0)
            continue;
        const QString token = originalArguments.at(i + 1);
        if (!token.startsWith(QStringLiteral("hyremote:"), Qt::CaseInsensitive))
            continue;
        const QString original = token.mid(QStringLiteral("hyremote:").size());
        if (original == received || oneByteWidening(original, codePage) == received) {
            match = original;
            ++matches;
        }
        ++i;
    }
    // More than one matching plugin invocation is ambiguous: preserve existing
    // parsing instead of choosing an unrelated / later argument.
    return matches == 1 ? match : received;
}

inline QStringList recoverWindowsQpaWatermark(const QStringList &received,
                                             const QStringList &originalArguments,
                                             unsigned int codePage)
{
    QStringList result = received;
    for (int index = 0; index < received.size(); ++index) {
        constexpr char key[] = "hyremote-watermark=";
        if (!received.at(index).startsWith(QLatin1String(key), Qt::CaseInsensitive))
            continue;
        const QString current = received.at(index).mid(static_cast<int>(sizeof(key) - 1));
        QString correct;
        int matches = 0;
        for (int i = 0; i + 1 < originalArguments.size(); ++i) {
            if (originalArguments.at(i).compare(QStringLiteral("-platform"), Qt::CaseInsensitive) != 0)
                continue;
            const QString token = originalArguments.at(i + 1);
            if (!token.startsWith(QStringLiteral("hyremote:"), Qt::CaseInsensitive))
                continue;
            const QStringList fields = token.mid(QStringLiteral("hyremote:").size())
                                           .split(QLatin1Char(':'), Qt::KeepEmptyParts);
            for (const QString &field : fields) {
                if (!field.startsWith(QLatin1String(key), Qt::CaseInsensitive))
                    continue;
                const QString original = field.mid(static_cast<int>(sizeof(key) - 1));
                if (original == current || oneByteWidening(original, codePage) == current) {
                    correct = original;
                    ++matches;
                }
            }
            ++i;
        }
        if (matches == 1)
            result[index] = QString::fromLatin1(key) + correct;
    }
    return result;
}
#else
inline QString recoverWindowsGenericSpecification(const QString &received,
                                                  const QStringList &, unsigned int)
{
    return received;
}
inline QStringList recoverWindowsQpaWatermark(const QStringList &received,
                                             const QStringList &, unsigned int)
{
    return received;
}
#endif

inline QString recoverActualWindowsGenericSpecification(const QString &received)
{
#ifdef Q_OS_WIN
    return recoverWindowsGenericSpecification(received,
                                              withOriginalWindowsEnvironment(originalWindowsArguments()),
                                              GetACP());
#else
    return received;
#endif
}

inline QStringList recoverActualWindowsQpaWatermark(const QStringList &received)
{
#ifdef Q_OS_WIN
    return recoverWindowsQpaWatermark(received,
                                     withOriginalWindowsEnvironment(originalWindowsArguments()),
                                     GetACP());
#else
    return received;
#endif
}

} // namespace HyRemote::detail
