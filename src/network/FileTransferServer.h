#ifndef FILETRANSFERSERVER_H
#define FILETRANSFERSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QMap>
#include "TransferSession.h"

namespace Witra {

class FileTransferServer : public QObject {
    Q_OBJECT
    
public:
    explicit FileTransferServer(QObject* parent = nullptr);
    ~FileTransferServer();
    
    bool start(quint16 port = TRANSFER_PORT);
    void stop();
    bool isListening() const;
    quint16 port() const;
    
    void setDownloadPath(const QString& path) { m_downloadPath = path; }
    QString downloadPath() const { return m_downloadPath; }
    
    TransferSession* session(const QString& sessionId) const;
    QList<TransferSession*> sessions() const { return m_sessions.values(); }
    
signals:
    void newConnection(TransferSession* session);
    void connectionRequestReceived(TransferSession* session, const QString& senderName);
    void sessionClosed(const QString& sessionId);
    void error(const QString& errorMessage);
    
private slots:
    void onNewConnection();
    void onSessionDisconnected();
    
private:
    QTcpServer* m_server;
    QMap<QString, TransferSession*> m_sessions;
    QString m_downloadPath;
};

} // namespace Witra

#endif // FILETRANSFERSERVER_H
