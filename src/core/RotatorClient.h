#pragma once

#include "core/RotatorProtocol.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

namespace AetherSDR {

// One rotator controller over TCP (GS-232A or Hamlib rotctld). Keeps the
// connection up (reconnecting every few seconds while enabled), reads the
// heading once a second, and sends turn and stop commands.
//
// Safety posture, the same as the Green Heron applet's: nothing here turns a
// rotator on its own. A turn is only ever sent by moveTo()/nudge(), which the
// GUI calls from an explicit button or menu action.
class RotatorClient : public QObject {
    Q_OBJECT

public:
    explicit RotatorClient(const Rotator::Config& config, QObject* parent = nullptr);
    ~RotatorClient() override;

    const Rotator::Config& config() const { return m_config; }

    bool isConnected() const { return m_connected; }
    bool hasHeading() const { return m_hasHeading; }
    int  heading() const { return m_heading; }
    // The last turn this client sent, -1 when none since connecting.
    int  target() const { return m_target; }
    QString lastError() const { return m_lastError; }

    void start();   // connect (and keep reconnecting)
    void stop();    // disconnect, no reconnect

    void moveTo(int azimuthDeg);
    // Turn by `deltaDeg` from the current heading (or the last target while
    // a turn is in progress, so repeated nudges add up).
    void nudge(int deltaDeg);
    void halt();

signals:
    void connectionChanged(bool connected);
    void headingChanged(int azimuthDeg);
    void targetChanged(int azimuthDeg);
    void errorChanged(const QString& message);

private:
    void openSocket();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void poll();
    void send(const QByteArray& bytes);
    void setError(const QString& message);
    void scheduleReconnect();

    Rotator::Config m_config;
    QTcpSocket* m_socket{nullptr};
    QTimer* m_pollTimer{nullptr};
    QTimer* m_reconnectTimer{nullptr};
    QTimer* m_connectTimeout{nullptr};
    QByteArray m_buffer;
    bool m_running{false};
    bool m_connected{false};
    bool m_hasHeading{false};
    int  m_heading{0};
    int  m_target{-1};
    QString m_lastError;
};

}  // namespace AetherSDR
