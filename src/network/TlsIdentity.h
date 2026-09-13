#ifndef TLSIDENTITY_H
#define TLSIDENTITY_H

#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QSslSocket>
#include <QString>

namespace Witra {

class TlsIdentity {
public:
    explicit TlsIdentity(const QString& storageDir = QString());

    static TlsIdentity& application();

    bool ensure();
    bool isValid() const;
    QSslCertificate certificate() const { return m_certificate; }
    QSslKey privateKey() const { return m_privateKey; }
    QByteArray fingerprintSha256() const;
    QSslConfiguration socketConfiguration() const;

    static QString pairingCode(const QSslCertificate& local, const QSslCertificate& peer);
    static QByteArray fingerprintOf(const QSslCertificate& certificate);
    static bool evaluateHandshake(QSslSocket* socket,
                                  const QString& expectedPeerId,
                                  const QList<QSslError>& errors,
                                  QString* reason = nullptr);

private:
    bool loadFromDisk();
    bool generateAndSave();
    bool generateWithOpenSsl();
#ifdef Q_OS_WIN
    bool generateWithWindows();
#endif
    QString certPath() const;
    QString keyPath() const;

    QString m_storageDir;
    QSslCertificate m_certificate;
    QSslKey m_privateKey;
};

class CertificatePinStore {
public:
    static QByteArray pinFor(const QString& peerId);
    static bool hasPin(const QString& peerId);
    static bool matches(const QString& peerId, const QByteArray& fingerprint);
    static void pin(const QString& peerId, const QByteArray& fingerprint);
    static void clear(const QString& peerId);
    static void clearAll();
};

} // namespace Witra

#endif // TLSIDENTITY_H
