#ifndef FILETRANSFERCLIENT_H
#define FILETRANSFERCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QMap>
#include "TransferSession.h"

namespace Witra {

class FileTransferClient : public QObject {
    Q_OBJECT
    
public:
    explicit FileTransferClient(QObject* parent = nullptr);
    ~FileTransferClient();
    
    TransferSession* connectToPeer(const QHostAddress& address, quint16 port);
    TransferSession* session(const QString& sessionId) const;
    TransferSession* sessionByPeerId(const QString& peerId) const;
    QList<TransferSession*> sessions() const { return m_sessions.values(); }
    
    void setDownloadPath(const QString& path) { m_downloadPath = path; }
    QString downloadPath() const { return m_downloadPath; }
    
signals:
    void connected(TransferSession* session);
    void connectionFailed(const QString& error);
    void sessionClosed(const QString& sessionId);
    
private slots:
    void onSessionDisconnected();
    
private:
    QMap<QString, TransferSession*> m_sessions;
    QString m_downloadPath;
};

} // namespace Witra

#endif // FILETRANSFERCLIENT_H
