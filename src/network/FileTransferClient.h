#ifndef FILETRANSFERCLIENT_H
#define FILETRANSFERCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QSslSocket>
#include <QMap>
#include "TransferSession.h"
#include "TlsIdentity.h"

namespace Witra {

class FileTransferClient : public QObject {
    Q_OBJECT
    
public:
    explicit FileTransferClient(QObject* parent = nullptr);
    ~FileTransferClient();
    
    TransferSession* connectToPeer(const QHostAddress& address, quint16 port,
                                  const QString& expectedPeerId = QString());
    void setTlsIdentity(TlsIdentity* identity);
    TransferSession* session(const QString& sessionId) const;
    TransferSession* sessionByPeerId(const QString& peerId) const;
    QList<TransferSession*> sessions() const { return m_sessions.values(); }
    
    void setDownloadPath(const QString& path) { m_downloadPath = path; }
    void setMaxFileSize(qint64 size) { m_maxFileSize = size; }
    QString downloadPath() const { return m_downloadPath; }
    
signals:
    void connected(TransferSession* session);
    void connectionFailed(TransferSession* session, const QString& error);
    void sessionClosed(const QString& sessionId);
    
private slots:
    void onSessionDisconnected();
    
private:
    TlsIdentity* identity() const;

    QMap<QString, TransferSession*> m_sessions;
    QString m_downloadPath;
    qint64 m_maxFileSize;
    TlsIdentity* m_identity;
};

} // namespace Witra

#endif // FILETRANSFERCLIENT_H
