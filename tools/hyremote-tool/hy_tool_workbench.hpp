#pragma once

#include "hy_sdk_inspector.hpp"
#include "language_preference.hpp"
#include "rfb_client.h"
#include "tool_diagnostic_session.hpp"

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariantList>

// Qt 6.8 developer workbench model. It owns exactly one RfbClient and one
// ToolDiagnosticSession; QML only holds lightweight view state and commands.
class HyToolWorkbench final : public QObject
{
    Q_OBJECT

public:
    explicit HyToolWorkbench(QSettings &settings, std::optional<ToolLanguage> cliLanguage,
                             bool cliOverride, QObject *parent = nullptr);

    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QString languagePreference READ languagePreference NOTIFY languageChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString sdkRoot READ sdkRoot WRITE setSdkRoot NOTIFY inspectionChanged)
    Q_PROPERTY(QString kitRoot READ kitRoot WRITE setKitRoot NOTIFY inspectionChanged)
    Q_PROPERTY(QVariantList checks READ checks NOTIFY inspectionChanged)
    Q_PROPERTY(QVariantList routes READ routes NOTIFY inspectionChanged)
    Q_PROPERTY(QVariantMap routeStatuses READ routeStatuses NOTIFY inspectionChanged)
    Q_PROPERTY(QString sdkSummary READ sdkSummary NOTIFY inspectionChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString stageText READ stageText NOTIFY connectionChanged)
    Q_PROPERTY(QString failureText READ failureText NOTIFY connectionChanged)
    Q_PROPERTY(QString nextStep READ nextStep NOTIFY connectionChanged)
    Q_PROPERTY(QString eventsText READ eventsText NOTIFY connectionChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY connectionChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY connectionChanged)
    Q_PROPERTY(quint64 frames READ frames NOTIFY connectionChanged)
    Q_PROPERTY(quint64 bytes READ bytes NOTIFY connectionChanged)
    Q_PROPERTY(double fps READ fps NOTIFY connectionChanged)
    Q_PROPERTY(double mbps READ mbps NOTIFY connectionChanged)
    Q_PROPERTY(QString sampleImage READ sampleImage NOTIFY sampleChanged)
    Q_PROPERTY(QString diagnosticsJson READ diagnosticsJson NOTIFY connectionChanged)

    QString language() const { return m_language == ToolLanguage::Chinese ? QStringLiteral("zh") : QStringLiteral("en"); }
    QString languagePreference() const { return toolLanguagePreferenceToken(m_preference); }
    QString version() const;
    QString sdkRoot() const { return m_sdkRoot; }
    QString kitRoot() const { return m_kitRoot; }
    QVariantList checks() const { return m_report.checkList(m_language == ToolLanguage::Chinese); }
    QVariantList routes() const;
    QVariantMap routeStatuses() const;
    QString sdkSummary() const;
    QString connectionState() const;
    QString stageText() const;
    QString failureText() const;
    QString nextStep() const;
    QString eventsText() const;
    int frameWidth() const { return m_lastFrame.width(); }
    int frameHeight() const { return m_lastFrame.height(); }
    quint64 frames() const { return m_stats.frames; }
    quint64 bytes() const { return m_stats.bytes; }
    double fps() const { return m_stats.fps; }
    double mbps() const { return m_stats.mbitPerSecond; }
    QString sampleImage() const { return m_sampleImage; }
    QString diagnosticsJson() const;

    Q_INVOKABLE void setSdkRoot(const QString &root);
    Q_INVOKABLE void setKitRoot(const QString &root);
    Q_INVOKABLE void inspectSelectedPaths();
    Q_INVOKABLE void setLanguagePreference(const QString &token);
    Q_INVOKABLE QString guidance(const QString &route) const;
    Q_INVOKABLE QString routeStatus(const QString &route) const;
    Q_INVOKABLE void copyGuidance(const QString &route);
    Q_INVOKABLE void connectHost(const QString &host, int port);
    Q_INVOKABLE void cancelConnection();
    Q_INVOKABLE void endSession();
    Q_INVOKABLE void sampleFrame();
    Q_INVOKABLE void copyDiagnostics();
    Q_INVOKABLE void saveDiagnostics();
    Q_INVOKABLE void requestLegacySwitch();
    Q_INVOKABLE bool verifyWorkbench(QObject *root, const QString &sdkRoot,
                                     const QString &kitRoot, const QString &languageToken);

signals:
    void languageChanged();
    void inspectionChanged();
    void connectionChanged();
    void sampleChanged();
    void legacySwitchRequested();

private:
    struct RouteInfo {
        const char *id;
        const char *title;
        const char *prerequisites;
        const char *call;
        const char *deploy;
        const char *launch;
        const char *verify;
        const char *zhTitle;
        const char *zhPrerequisites;
        const char *zhCall;
        const char *zhDeploy;
        const char *zhLaunch;
        const char *zhVerify;
    };

    static const RouteInfo &routeInfo(const QString &route);
    QString mismatchWarningText() const;
    void refreshConnectionText();
    void clearFrame();

    QSettings &m_settings;
    std::optional<ToolLanguage> m_cliLanguage;
    bool m_cliOverride = false;
    ToolLanguagePreference m_preference = ToolLanguagePreference::Auto;
    ToolLanguage m_language = ToolLanguage::English;

    RfbClient m_client;
    ToolDiagnosticSession m_diagnostics;
    RfbClient::Stats m_stats;
    QImage m_lastFrame;
    QString m_sampleImage;
    QString m_connectionState;
    QString m_stageText;
    QString m_failureText;
    QString m_nextStep;
    QString m_eventsText;

    QString m_sdkRoot;
    QString m_kitRoot;
    HySdkInspector::Report m_report;
};
