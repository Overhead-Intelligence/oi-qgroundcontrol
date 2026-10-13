#pragma once

#include "LinkConfiguration.h"
#include "LinkInterface.h"

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>

#include <atomic>

class QUdpSocket;
class QThread;

/*===========================================================================*/

struct UDPClient
{
    UDPClient(const QHostAddress &addr, quint16 portNum)
        : address(addr)
        , port(portNum)
    {}

    UDPClient(const QString &host, const QHostAddress &addr, quint16 portNum)
        : hostname(host)
        , address(addr)
        , port(portNum)
    {}

    explicit UDPClient(const UDPClient *other)
        : hostname(other->hostname)
        , address(other->address)
        , port(other->port)
    {}

    bool operator==(const UDPClient &other) const
    {
        return ((address == other.address) && (port == other.port));
    }

    UDPClient &operator=(const UDPClient &other)
    {
        hostname = other.hostname;
        address = other.address;
        port = other.port;

        return *this;
    }

    QString hostname;
    QHostAddress address;
    quint16 port = 0;
};

/*===========================================================================*/

class UDPConfiguration : public LinkConfiguration
{
    Q_OBJECT

    Q_PROPERTY(QStringList hostList READ hostList NOTIFY hostListChanged)
    Q_PROPERTY(quint16 localPort READ localPort WRITE setLocalPort NOTIFY localPortChanged)
    Q_PROPERTY(bool acceptAnySender READ acceptAnySender WRITE setAcceptAnySender NOTIFY acceptAnySenderChanged)

public:
    explicit UDPConfiguration(const QString &name, QObject *parent = nullptr);
    explicit UDPConfiguration(const UDPConfiguration *source, QObject *parent = nullptr);
    virtual ~UDPConfiguration();

    Q_INVOKABLE void addHost(const QString &host);
    Q_INVOKABLE void addHost(const QString &host, quint16 port);
    Q_INVOKABLE void removeHost(const QString &host);
    Q_INVOKABLE void removeHost(const QString &host, quint16 port);

    LinkType type() const override { return LinkConfiguration::TypeUdp; }
    void copyFrom(const LinkConfiguration *source) override;
    void loadSettings(QSettings &settings, const QString &root) override;
    void saveSettings(QSettings &settings, const QString &root) const override;
    QString settingsURL() const override { return QStringLiteral("UdpSettings.qml"); }
    QString settingsTitle() const override { return tr("UDP Link Settings"); }

    QStringList hostList() const { return _hostList; }
    QList<std::shared_ptr<UDPClient>> targetHosts() const { return _targetHosts; }

    /// Hostnames with no address yet. The worker resolves these; nothing on a GUI thread does,
    /// because a name lookup blocks for as long as the system resolver takes to give up.
    QStringList unresolvedHostnames() const;
    /// Record a lookup result against every target using that name.
    /// @return true if any target's address changed.
    bool applyResolvedAddress(const QString &hostname, const QHostAddress &address) const;
    quint16 localPort() const { return _localPort; }
    void setLocalPort(quint16 port) { if (port != _localPort) { _localPort = port; emit localPortChanged(); } }

    /// When false - the default - the link accepts datagrams only from its configured server
    /// addresses. The socket binds 0.0.0.0, so without this a link hears every aircraft that
    /// shares its port, which is how connecting one aircraft brings up another.
    bool acceptAnySender() const { return _acceptAnySender; }
    void setAcceptAnySender(bool accept) { if (accept != _acceptAnySender) { _acceptAnySender = accept; emit acceptAnySenderChanged(); } }

signals:
    void hostListChanged();
    void localPortChanged();
    void acceptAnySenderChanged();

private:
    void _updateHostList();

    /// Parses an address that is already a literal. Deliberately does **not** do a name
    /// lookup: this runs on the GUI thread from addHost(), and QHostInfo::fromName() blocks
    /// there until the resolver gives up - seconds, when a DNS server is configured but
    /// unreachable, which is exactly what losing internet on a working LAN looks like.
    static QHostAddress _literalAddress(const QString &address);

    QStringList _hostList;
    QList<std::shared_ptr<UDPClient>> _targetHosts;
    quint16 _localPort = 0;
    bool _acceptAnySender = false;
};

/*===========================================================================*/

class UDPWorker : public QObject
{
    Q_OBJECT

public:
    explicit UDPWorker(const UDPConfiguration *config, QObject *parent = nullptr);
    virtual ~UDPWorker();

    bool isConnected() const;

public slots:
    void setupSocket();
    void connectLink();
    void disconnectLink();
    void writeData(const QByteArray &data);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &errorString);
    void dataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);

private slots:
    void _onHostLookupFinished(const QHostInfo &info);
    void _resolvePendingHosts();
    void _onSocketConnected();
    void _onSocketDisconnected();
    void _onSocketReadyRead();
    void _onSocketBytesWritten(qint64 bytes);
    void _onSocketErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    /// True if @p sender is an address this link is allowed to hear from.
    bool _isAcceptedSender(const QHostAddress &sender) const;
    /// Resolves the configured hosts into _acceptedAddresses. Call on the worker thread.
    void _snapshotAcceptedAddresses();

    const UDPConfiguration *_udpConfig = nullptr;
    QUdpSocket *_socket = nullptr;
    QMutex _sessionTargetsMutex;
    QList<std::shared_ptr<UDPClient>> _sessionTargets;
    bool _isConnected = false;
    bool _errorEmitted = false;
    QSet<QHostAddress> _localAddresses;
    /// Taken once per connect rather than read per datagram: the configuration belongs to the
    /// GUI thread and is rewritten in place when a link is edited. Host changes made while
    /// connected therefore apply on the next connect, which is already true of the port.
    QSet<QHostAddress> _acceptedAddresses;
    bool _acceptAnySender = false;
    /// Senders already reported as rejected, so one misrouted aircraft cannot flood the log.
    QSet<QHostAddress> _rejectedSenders;

    /// Retries name lookups for as long as the link is up.
    ///
    /// A name that failed once used to stay failed for the life of the link: resolution ran at
    /// connect and nowhere else, and the send loop skips a target with no address without
    /// saying anything. Losing internet in the field therefore killed a hostname link until it
    /// was recreated by IP, even though the aircraft was reachable on the LAN the whole time.
    QTimer *_resolveTimer = nullptr;
    /// Lookups already in flight, so a slow resolver is not asked the same name every tick.
    QSet<QString> _lookupsInFlight;
    /// Names already reported to the operator, so the retry does not reopen the same error.
    QSet<QString> _reportedUnresolved;
};

/*===========================================================================*/

class UDPLink : public LinkInterface
{
    Q_OBJECT

public:
    explicit UDPLink(SharedLinkConfigurationPtr &config, QObject *parent = nullptr);
    virtual ~UDPLink();

    bool isConnected() const override;
    void disconnect() override;
    bool isSecureConnection() const override;

protected:
    bool _connect() override;

private slots:
    void _writeBytes(const QByteArray &data) override;
    void _onConnected();
    void _onDisconnected();
    void _onErrorOccurred(const QString &errorString);
    void _onDataReceived(const QByteArray &data);
    void _onDataSent(const QByteArray &data);

private:
    const UDPConfiguration *_udpConfig = nullptr;
    UDPWorker *_worker = nullptr;
    QThread *_workerThread = nullptr;
    std::atomic<bool> _disconnectedEmitted{false};
};
