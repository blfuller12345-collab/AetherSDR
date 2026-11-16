#include "core/RotatorClient.h"

#include "core/LogManager.h"

#include <QTcpSocket>
#include <QTimer>

#include <algorithm>
#include <cstdlib>

namespace AetherSDR {

namespace {

constexpr int kPollIntervalMs = 1000;
constexpr int kReconnectMs = 5000;
constexpr int kConnectTimeoutMs = 5000;
constexpr int kArrivedToleranceDeg = 2;

}  // namespace

RotatorClient::RotatorClient(const Rotator::Config& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_socket(new QTcpSocket(this))
    , m_pollTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_connectTimeout(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(kReconnectMs);
    m_connectTimeout->setSingleShot(true);
    m_connectTimeout->setInterval(kConnectTimeoutMs);

    connect(m_socket, &QTcpSocket::connected, this, &RotatorClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &RotatorClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RotatorClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        m_connectTimeout->stop();
        setError(m_socket->errorString());
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            onDisconnected();
        }
    });
    connect(m_pollTimer, &QTimer::timeout, this, &RotatorClient::poll);
    connect(m_reconnectTimer, &QTimer::timeout, this, &RotatorClient::openSocket);
    connect(m_connectTimeout, &QTimer::timeout, this, [this] {
        setError(tr("No answer from %1:%2").arg(m_config.host).arg(m_config.port));
        m_socket->abort();
        onDisconnected();
    });
}

RotatorClient::~RotatorClient()
{
    m_running = false;
    m_socket->abort();
}

void RotatorClient::start()
{
    if (m_running) return;
    m_running = true;
    openSocket();
}

void RotatorClient::stop()
{
    m_running = false;
    m_reconnectTimer->stop();
    m_connectTimeout->stop();
    m_pollTimer->stop();
    m_socket->abort();
    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false);
    }
}

void RotatorClient::openSocket()
{
    if (!m_running || m_socket->state() != QAbstractSocket::UnconnectedState) return;
    m_buffer.clear();
    m_socket->connectToHost(m_config.host, m_config.port);
    m_connectTimeout->start();
}

void RotatorClient::onConnected()
{
    m_connectTimeout->stop();
    m_connected = true;
    m_target = -1;
    setError(QString());
    qCInfo(lcTuner).noquote() << "Rotator" << m_config.name << "connected to"
                              << m_config.host << m_config.port;
    emit connectionChanged(true);
    poll();
    m_pollTimer->start();
}

void RotatorClient::onDisconnected()
{
    m_pollTimer->stop();
    const bool was = m_connected;
    m_connected = false;
    m_hasHeading = false;
    if (was) emit connectionChanged(false);
    scheduleReconnect();
}

void RotatorClient::scheduleReconnect()
{
    if (m_running && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void RotatorClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    while (const auto az = Rotator::takeHeading(m_config.protocol, m_buffer)) {
        const bool changed = !m_hasHeading || *az != m_heading;
        m_hasHeading = true;
        m_heading = *az;
        if (changed) emit headingChanged(m_heading);
        // Arrived: the turn is done, so the next nudge starts from here.
        if (m_target >= 0) {
            int diff = std::abs(m_heading - m_target) % 360;
            diff = std::min(diff, 360 - diff);
            if (diff <= kArrivedToleranceDeg) {
                m_target = -1;
                emit targetChanged(-1);
            }
        }
    }
}

void RotatorClient::poll()
{
    send(Rotator::readCommand(m_config.protocol));
}

void RotatorClient::send(const QByteArray& bytes)
{
    if (!m_connected) return;
    m_socket->write(bytes);
}

void RotatorClient::moveTo(int azimuthDeg)
{
    if (!m_connected) return;
    m_target = Rotator::normalizedAzimuth(azimuthDeg);
    qCInfo(lcTuner).noquote() << "Rotator" << m_config.name << "turn to" << m_target;
    send(Rotator::moveCommand(m_config.protocol, m_target));
    emit targetChanged(m_target);
}

void RotatorClient::nudge(int deltaDeg)
{
    if (!m_connected || !m_hasHeading) return;
    const int base = m_target >= 0 ? m_target : m_heading;
    moveTo(base + deltaDeg);
}

void RotatorClient::halt()
{
    if (!m_connected) return;
    send(Rotator::stopCommand(m_config.protocol));
    m_target = -1;
    emit targetChanged(-1);
}

void RotatorClient::setError(const QString& message)
{
    if (m_lastError == message) return;
    m_lastError = message;
    emit errorChanged(message);
}

}  // namespace AetherSDR
