#pragma once

#include "core/MqttSettings.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTextEdit;
class QGridLayout;

namespace AetherSDR {

class MqttClient;

// Applet for MQTT station device integration (#699).
// Live MQTT operation only; durable configuration lives in Settings -> MQTT.
class MqttApplet : public QWidget {
    Q_OBJECT

public:
    explicit MqttApplet(QWidget* parent = nullptr);

    void setMqttClient(MqttClient* client);
    void refreshSettings();
    void setCachedPassword(const QString& password);
    void restoreConnectionState();
    QVector<MqttTopicDef> topicConfig() const { return m_topicDefs; }
    MqttOverlayLayout overlayLayout() const { return m_overlayLayout; }

signals:
    void connectRequested(const QString& host, quint16 port,
                          const QString& user, const QString& pass,
                          const QStringList& topics,
                          bool useTls, const QString& caFile);
    void disconnectRequested();
    // A display-on-pan subscription received a value (#699).  `topic` is the
    // full topic and is the overlay's key, so two topics that share a last
    // path segment no longer overwrite each other (#7136).  `order` is the
    // subscription's position in Settings -> MQTT -> Subscriptions, and
    // `label` is what the overlay draws in front of the value.
    void displayValueChanged(const QString& topic, int order,
                             const QString& label, const QString& value);
    void displayCleared();
    void antennaAliasRequested(const QString& token, const QString& alias);
    void settingsRequested();

private:
    void buildUI();
    void updateStatus(const QString& text, bool ok);
    void appendMessageLog(const QString& line);
    void onMessageReceived(const QString& topic, const QByteArray& payload);
    void rebuildButtons();
    void requestConnectFromSettings();
    void finishPasswordLoad();

    // MQTT broker password lives in QKeychain rather than AppSettings
    // (GHSA-mmqp-cm4w-cvpp).  Both calls are async and no-op on builds
    // without HAVE_KEYCHAIN defined.
    void loadPasswordFromKeychain();

    MqttClient* m_client{nullptr};
    QPushButton* m_enableBtn{nullptr};
    QLabel*      m_statusLabel{nullptr};
    QTextEdit*   m_messageLog{nullptr};
    QString      m_password;
    bool         m_passwordLoaded{false};
    bool         m_restoreConnectPending{false};

    // Publish buttons
    QGridLayout*          m_buttonGrid{nullptr};
    QVector<MqttButtonDef> m_buttonDefs;
    QVector<QPushButton*>  m_buttons;

    // Per-topic display config
    QVector<MqttTopicDef> m_topicDefs;
    MqttOverlayLayout     m_overlayLayout{MqttOverlayLayout::SingleLine};
};

} // namespace AetherSDR
