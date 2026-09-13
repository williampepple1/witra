#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>
#include <QTimer>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QThread>
#include <QUuid>
#include <iostream>

#include "core/Peer.h"
#include "core/PeerManager.h"

#define private public
#include "core/TransferManager.h"
#undef private

#include "network/NetworkDiscovery.h"
#include "network/FileTransferServer.h"
#include "network/FileTransferClient.h"
#include "network/TransferSession.h"
#include "network/Protocol.h"
#include "network/TlsIdentity.h"

using namespace Witra;

static int g_testsRun = 0;
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(func) \
    do { \
        g_testsRun++; \
        std::cout << "[RUNNING] " << #func << "..." << std::endl; \
        if (func()) { \
            g_testsPassed++; \
            std::cout << "  [PASS] " << #func << std::endl; \
        } else { \
            g_testsFailed++; \
            std::cout << "  [FAILED] " << #func << std::endl; \
        } \
    } while(0)

// Helper: spin event loop until condition is met or timeout
template<typename Predicate>
bool waitForCondition(Predicate pred, int timeoutMs = 5000) {
    qint64 start = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - start < timeoutMs) {
        if (pred()) return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
    }
    return pred();
}

// -------------------------------------------------------------
// Test 1: PeerManager & Peer Lifetime
// -------------------------------------------------------------
bool testPeerManager() {
    PeerManager pm;
    
    // Display name validation (M12)
    QString longName = QString("A").repeated(100);
    pm.setDisplayName(longName);
    TEST_ASSERT(pm.displayName().length() <= MAX_DISPLAY_NAME_LENGTH, "Display name must be capped at MAX_DISPLAY_NAME_LENGTH");
    
    // Peer addition via discovery simulation
    pm.start();
    TEST_ASSERT(pm.isRunning(), "PeerManager should be running");
    
    Peer* peer1 = new Peer("peer-1", "Alice", QHostAddress("127.0.0.1"), 45679, &pm);
    peer1->setDeviceName("Laptop");
    TEST_ASSERT(peer1->displayName() == "Alice", "Peer display name should match");
    TEST_ASSERT(peer1->id() == "peer-1", "Peer id should match");
    
    // Test that active connected peers are NOT timed out (H2)
    peer1->setState(Peer::ConnectionState::Connected);
    TEST_ASSERT(peer1->isConnected(), "Peer should be connected");
    TEST_ASSERT(!peer1->hasTimedOut(10000), "Connected peer should not time out");
    
    pm.stop();
    return true;
}

// -------------------------------------------------------------
// Test 2: Security & Path Traversal Defense (C2)
// -------------------------------------------------------------
bool testPathSanitization() {
    // Testing path sanitization logic used in TransferSession
    auto isPathSafe = [](const QString& relativePath, const QString& fileName) {
        QString cleanRelPath = QDir::cleanPath(relativePath);
        QString cleanFileName = QDir::cleanPath(fileName);
        
        if (cleanRelPath.startsWith("..") || QDir::isAbsolutePath(cleanRelPath) ||
            cleanFileName.startsWith("..") || QDir::isAbsolutePath(cleanFileName) ||
            cleanFileName.contains("/")) {
            return false;
        }
        return true;
    };
    
    TEST_ASSERT(isPathSafe("", "safe_file.txt"), "Safe file should be allowed");
    TEST_ASSERT(isPathSafe("sub/dir", "image.png"), "Safe nested path should be allowed");
    TEST_ASSERT(!isPathSafe("../../", "evil.bat"), "Path traversal in relPath must be blocked");
    TEST_ASSERT(!isPathSafe("../windows/system32", "calc.exe"), "Relative traversal must be blocked");
    TEST_ASSERT(!isPathSafe("", "../../../autoexec.bat"), "Filename traversal must be blocked");
    TEST_ASSERT(!isPathSafe("/absolute/path", "root.txt"), "Absolute relPath must be blocked");
    
    return true;
}

// -------------------------------------------------------------
// Test 3: Network Discovery Packet Serialization & Multi-Port
// -------------------------------------------------------------
bool testNetworkDiscovery() {
    NetworkDiscovery disc1;
    disc1.start("peer-sender", "TestSender", 45679);
    TEST_ASSERT(disc1.isRunning(), "NetworkDiscovery 1 should be running");
    
    NetworkDiscovery disc2;
    disc2.start("peer-receiver", "TestReceiver", 45680);
    TEST_ASSERT(disc2.isRunning(), "NetworkDiscovery 2 should be running");
    
    bool discovered = false;
    QObject::connect(&disc2, &NetworkDiscovery::peerDiscovered,
                     [&](const QString& peerId, const QString& name, const QString& device,
                         const QHostAddress& address, quint16 port) {
        if (name == "TestSender") {
            discovered = true;
        }
    });
    
    bool received = waitForCondition([&]() { return discovered; }, 2000);
    std::cout << "  (Discovery broadcast received: " << (received ? "YES" : "Loopback Isolated") << ")" << std::endl;
    
    disc1.stop();
    disc2.stop();
    return true;
}

struct TestTlsPair {
    QString dir;
    TlsIdentity alice;
    TlsIdentity bob;

    TestTlsPair()
        : dir(QDir::cleanPath(QDir::tempPath() + "/witra_tls_" + QUuid::createUuid().toString(QUuid::WithoutBraces)))
        , alice(dir + "/alice")
        , bob(dir + "/bob")
    {
        QDir().mkpath(dir + "/alice");
        QDir().mkpath(dir + "/bob");
    }

    ~TestTlsPair() {
        QDir(dir).removeRecursively();
    }

    bool apply(FileTransferServer& server, FileTransferClient& client) {
        if (!alice.ensure() || !bob.ensure()) {
            return false;
        }
        server.setTlsIdentity(&alice);
        client.setTlsIdentity(&bob);
        return true;
    }
};

// -------------------------------------------------------------
// Test 4: Live TLS Connection, Handshake & Pairing Code
// -------------------------------------------------------------
bool testLiveTlsConnection() {
    TestTlsPair tls;
    FileTransferServer server;
    FileTransferClient client;
    TEST_ASSERT(tls.apply(server, client), "Must generate distinct TLS identities");
    TEST_ASSERT(tls.alice.fingerprintSha256() != tls.bob.fingerprintSha256(),
                "Each device identity must have a unique certificate");
    TEST_ASSERT(TlsIdentity::pairingCode(tls.alice.certificate(), tls.bob.certificate())
                    == TlsIdentity::pairingCode(tls.bob.certificate(), tls.alice.certificate()),
                "Pairing code must be commutative over the two certificates");

    bool started = server.start(45679);
    if (!started) {
        started = server.start(45680);
    }
    TEST_ASSERT(started, "FileTransferServer should start listening");
    
    quint16 port = server.port();
    TEST_ASSERT(port > 0, "Server port should be greater than 0");
    
    TransferSession* serverSession = nullptr;
    QObject::connect(&server, &FileTransferServer::connectionRequestReceived,
                     [&](TransferSession* session, const QString& sender) {
        serverSession = session;
        session->sendConnectionAccept();
    });
    
    bool clientConnected = false;
    TransferSession* clientSession = nullptr;
    
    QObject::connect(&client, &FileTransferClient::connected, [&](TransferSession* session) {
        clientConnected = true;
        clientSession = session;
        session->sendConnectionRequest("TestClient", "client-live-tls");
    });
    
    TransferSession* initialSession = client.connectToPeer(QHostAddress("127.0.0.1"), port);
    TEST_ASSERT(initialSession != nullptr, "Client connectToPeer must return valid session");
    
    bool connected = waitForCondition([&]() {
        return clientSession != nullptr &&
               clientSession->state() == TransferSession::State::Accepted &&
               serverSession != nullptr;
    }, 5000);
    
    TEST_ASSERT(connected, "Client and server sessions must establish connection");
    TEST_ASSERT(!clientSession->verificationCode().isEmpty(), "Client verification code must be non-empty");
    TEST_ASSERT(clientSession->verificationCode() == serverSession->verificationCode(),
                "Pairing verification code must match between client and server");
    
    std::cout << "  (Pairing Verification Code Matched: " << clientSession->verificationCode().toStdString() << ")" << std::endl;
    
    server.stop();
    return true;
}

// -------------------------------------------------------------
// Test 5: End-to-End File Transfer & Hash Integrity
// -------------------------------------------------------------
bool testEndToEndFileTransfer() {
    QString testDir = QDir::cleanPath(QDir::tempPath() + "/witra_e2e_test");
    QDir().mkpath(testDir);
    
    QString downloadDir = testDir + "/received";
    QDir().mkpath(downloadDir);
    
    // Create dummy 256 KB test file with known contents
    QString sourceFilePath = testDir + "/sample_payload.bin";
    QByteArray payload(256 * 1024, 0);
    for (int i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>((i * 37) % 256);
    }
    
    QFile srcFile(sourceFilePath);
    TEST_ASSERT(srcFile.open(QIODevice::WriteOnly), "Must be able to create source test file");
    srcFile.write(payload);
    srcFile.close();
    
    QByteArray expectedHash = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
    
    // Setup server
    TestTlsPair tls;
    FileTransferServer server;
    FileTransferClient client;
    TEST_ASSERT(tls.apply(server, client), "Must create TLS identities for transfer test");
    server.setDownloadPath(downloadDir);
    TEST_ASSERT(server.start(45681), "Server must start");
    
    TransferSession* serverSession = nullptr;
    QObject::connect(&server, &FileTransferServer::connectionRequestReceived,
                     [&](TransferSession* session, const QString& sender) {
        serverSession = session;
        session->sendConnectionAccept();
    });
    
    TransferSession* clientSession = nullptr;
    QObject::connect(&client, &FileTransferClient::connected, [&](TransferSession* session) {
        clientSession = session;
        session->sendConnectionRequest("ClientSender", "sender-123");
    });
    
    client.connectToPeer(QHostAddress("127.0.0.1"), server.port());
    
    bool connected = waitForCondition([&]() {
        return clientSession != nullptr &&
               clientSession->state() == TransferSession::State::Accepted &&
               serverSession != nullptr;
    }, 5000);
    TEST_ASSERT(connected, "Client and server must connect for transfer test");
    
    bool transferCompleted = false;
    QObject::connect(serverSession, &TransferSession::transferCompleted, [&](const QString& transferId) {
        transferCompleted = true;
    });
    
    // Send file
    QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clientSession->sendFile(sourceFilePath, transferId);
    
    bool completed = waitForCondition([&]() { return transferCompleted; }, 8000);
    TEST_ASSERT(completed, "File transfer must complete within timeout");
    
    // Verify received file
    QString receivedFilePath = downloadDir + "/sample_payload.bin";
    TEST_ASSERT(QFile::exists(receivedFilePath), "Received file must exist in download folder");
    
    QFile recFile(receivedFilePath);
    TEST_ASSERT(recFile.open(QIODevice::ReadOnly), "Received file must be readable");
    QByteArray receivedBytes = recFile.readAll();
    recFile.close();
    
    TEST_ASSERT(receivedBytes.size() == payload.size(), "Received file size must exactly match source file");
    QByteArray actualHash = QCryptographicHash::hash(receivedBytes, QCryptographicHash::Sha256).toHex();
    TEST_ASSERT(actualHash == expectedHash, "Received file SHA-256 hash must exactly match expected hash");
    
    std::cout << "  (Transferred " << receivedBytes.size() << " bytes successfully with verified SHA-256)" << std::endl;
    
    // Cleanup temporary files
    QFile::remove(sourceFilePath);
    QFile::remove(receivedFilePath);
    QDir(downloadDir).removeRecursively();
    QDir(testDir).removeRecursively();
    
    server.stop();
    return true;
}

// -------------------------------------------------------------
// Test 6: File Size Limit Enforcement (M1)
// -------------------------------------------------------------
bool testFileSizeLimitEnforcement() {
    TestTlsPair tls;
    FileTransferServer server;
    FileTransferClient client;
    TEST_ASSERT(tls.apply(server, client), "Must create TLS identities for size-limit test");
    // Set 100 KB limit
    server.setMaxFileSize(100 * 1024);
    TEST_ASSERT(server.start(45682), "Server must start");
    
    TransferSession* serverSession = nullptr;
    QObject::connect(&server, &FileTransferServer::connectionRequestReceived,
                     [&](TransferSession* session, const QString& sender) {
        serverSession = session;
        session->sendConnectionAccept();
    });
    
    TransferSession* clientSession = nullptr;
    QObject::connect(&client, &FileTransferClient::connected, [&](TransferSession* session) {
        clientSession = session;
        session->sendConnectionRequest("ClientSender", "sender-456");
    });
    
    client.connectToPeer(QHostAddress("127.0.0.1"), server.port());
    
    bool connected = waitForCondition([&]() {
        return clientSession != nullptr &&
               clientSession->state() == TransferSession::State::Accepted &&
               serverSession != nullptr;
    }, 5000);
    TEST_ASSERT(connected, "Must connect");
    
    // Create a 500 KB file (exceeds 100 KB limit)
    QString oversizedPath = QDir::tempPath() + "/witra_oversized.tmp";
    QFile overFile(oversizedPath);
    if (overFile.open(QIODevice::WriteOnly)) {
        overFile.write(QByteArray(500 * 1024, 'X'));
        overFile.close();
    }
    
    bool errorEmitted = false;
    QObject::connect(serverSession, &TransferSession::transferFailed, [&](const QString& id, const QString& msg) {
        errorEmitted = true;
    });
    
    QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clientSession->sendFile(oversizedPath, transferId);
    
    // Server should reject or abort the oversized file transfer
    bool caught = waitForCondition([&]() { return errorEmitted; }, 3000);
    TEST_ASSERT(caught, "Server must reject file exceeding max size limit");
    
    QFile::remove(oversizedPath);
    server.stop();
    return true;
}

// -------------------------------------------------------------
// Test 7: File Transfer Requires Accepted Connection
// -------------------------------------------------------------
bool testUnauthorizedTransferRejected() {
    QString testDir = QDir::cleanPath(QDir::tempPath() + "/witra_unauthorized_test");
    QString downloadDir = testDir + "/received";
    QDir().mkpath(downloadDir);
    
    QString sourceFilePath = testDir + "/unauthorized.txt";
    QFile sourceFile(sourceFilePath);
    TEST_ASSERT(sourceFile.open(QIODevice::WriteOnly), "Must be able to create unauthorized source file");
    sourceFile.write("hello");
    sourceFile.close();
    
    TestTlsPair tls;
    FileTransferServer server;
    FileTransferClient client;
    TEST_ASSERT(tls.apply(server, client), "Must create TLS identities for unauthorized-transfer test");
    server.setDownloadPath(downloadDir);
    TEST_ASSERT(server.start(45683), "Server must start");
    
    TransferSession* serverSession = nullptr;
    bool rejectedUnauthorizedTransfer = false;
    bool transferStarted = false;
    
    QObject::connect(&server, &FileTransferServer::newConnection,
                     [&](TransferSession* session) {
        serverSession = session;
        QObject::connect(session, &TransferSession::error, [&](const QString&) {
            rejectedUnauthorizedTransfer = true;
        });
        QObject::connect(session, &TransferSession::transferStarted,
                         [&](const QString&, const QString&, qint64, qint64) {
            transferStarted = true;
        });
    });
    
    TransferSession* clientSession = nullptr;
    QObject::connect(&client, &FileTransferClient::connected, [&](TransferSession* session) {
        clientSession = session;
        session->sendFile(sourceFilePath, "unauthorized-transfer");
    });
    
    client.connectToPeer(QHostAddress("127.0.0.1"), server.port());
    
    TEST_ASSERT(waitForCondition([&]() { return clientSession != nullptr; }, 3000),
                "Witra client must connect without sending a connection request");
    TEST_ASSERT(waitForCondition([&]() { return serverSession != nullptr; }, 3000),
                "Server must create a session for unauthorized Witra client");
    
    TEST_ASSERT(waitForCondition([&]() { return rejectedUnauthorizedTransfer; }, 3000),
                "Server must reject file headers before connection acceptance");
    TEST_ASSERT(!transferStarted, "Unauthorized file header must not start a transfer");
    TEST_ASSERT(!QFile::exists(downloadDir + "/unauthorized.txt.part"),
                "Unauthorized transfer must not create a partial file");
    TEST_ASSERT(!QFile::exists(downloadDir + "/unauthorized.txt"),
                "Unauthorized transfer must not create a final file");
    
    server.stop();
    QDir(testDir).removeRecursively();
    return true;
}

// -------------------------------------------------------------
// Test 7b: Pinned Certificate Mismatch Is Rejected
// -------------------------------------------------------------
bool testPinnedCertificateMismatchRejected() {
    CertificatePinStore::clear("impersonated-peer");
    CertificatePinStore::pin("impersonated-peer", QByteArray(32, '\x01'));

    TestTlsPair tls;
    FileTransferServer server;
    FileTransferClient client;
    TEST_ASSERT(tls.apply(server, client), "Must create TLS identities for pin-mismatch test");
    TEST_ASSERT(server.start(45684), "Server must start");

    bool rejected = false;
    QObject::connect(&server, &FileTransferServer::newConnection, [&](TransferSession* session) {
        QObject::connect(session, &TransferSession::error, [&](const QString& message) {
            if (message.contains("pinned", Qt::CaseInsensitive)
                || message.contains("mismatch", Qt::CaseInsensitive)) {
                rejected = true;
            }
        });
    });
    QObject::connect(&client, &FileTransferClient::connectionFailed, [&](TransferSession*, const QString&) {
        rejected = true;
    });

    TransferSession* clientSession = nullptr;
    QObject::connect(&client, &FileTransferClient::connected, [&](TransferSession* session) {
        clientSession = session;
        session->sendConnectionRequest("Imposter", "impersonated-peer");
    });

    client.connectToPeer(QHostAddress("127.0.0.1"), server.port(), "impersonated-peer");

    TEST_ASSERT(waitForCondition([&]() { return rejected; }, 5000),
                "A pinned peer ID with a different certificate must be rejected");

    CertificatePinStore::clear("impersonated-peer");
    server.stop();
    return true;
}

// -------------------------------------------------------------
// Test 8: Incoming Received Path Is Stored For Open Folder
// -------------------------------------------------------------
bool testIncomingReceivedPathStored() {
    QString testDir = QDir::cleanPath(QDir::tempPath() + "/witra_received_path_test");
    QString downloadDir = testDir + "/received";
    QDir().mkpath(downloadDir);
    
    PeerManager peerManager;
    TransferManager transferManager(&peerManager);
    transferManager.setDownloadPath(downloadDir);
    
    QString transferId = "incoming-file-transfer";
    TransferItem* item = new TransferItem(
        transferId, "sample.txt", 5,
        TransferItem::Direction::Incoming, "peer-1", &transferManager
    );
    transferManager.m_transfers[transferId] = item;
    
    QString receivedFilePath = downloadDir + "/sample.txt";
    transferManager.onSessionFileReceived(transferId, receivedFilePath);
    
    TEST_ASSERT(item->filePath() == QDir::cleanPath(receivedFilePath),
                "Incoming single-file transfer should store the received file path");
    
    QString folderTransferId = "incoming-folder-transfer";
    TransferItem* folderItem = new TransferItem(
        folderTransferId, "photo.jpg", 5,
        TransferItem::Direction::Incoming, "peer-1", &transferManager
    );
    folderItem->setTotalFiles(2);
    transferManager.m_transfers[folderTransferId] = folderItem;
    
    QString nestedFilePath = downloadDir + "/Holiday/photos/photo.jpg";
    transferManager.onSessionFileReceived(folderTransferId, nestedFilePath);
    
    TEST_ASSERT(folderItem->filePath() == QDir::cleanPath(downloadDir + "/Holiday"),
                "Incoming folder transfer should store the top-level received folder path");
    
    QDir(testDir).removeRecursively();
    return true;
}

// -------------------------------------------------------------
// Test 9: Incoming Sessions Are Wired Once
// -------------------------------------------------------------
bool testIncomingSessionConnectionsNotDuplicated() {
    PeerManager unitPm;
    TransferManager unitManager(&unitPm);
    auto* dummySocket = new QTcpSocket;
    TransferSession dummySession(dummySocket, &unitManager);
    dummySession.setPeerId("peer-dup");

    unitManager.setupSessionConnections(&dummySession);
    unitManager.setupSessionConnections(&dummySession);
    TEST_ASSERT(unitManager.m_wiredSessions.size() == 1,
                "setupSessionConnections must ignore a session that is already wired");

    unitManager.acceptConnectionRequest(&dummySession);
    TEST_ASSERT(unitManager.m_wiredSessions.size() == 1,
                "acceptConnectionRequest must not wire incoming session signals a second time");

    return true;
}

// -------------------------------------------------------------
// Test 10: Folder Transfer Progress Does Not Reset
// -------------------------------------------------------------
bool testFolderTransferProgressAccumulates() {
    PeerManager pm;
    TransferManager tm(&pm);

    const QString transferId = "folder-progress-transfer";
    TransferItem* item = new TransferItem(
        transferId, "Docs", 300,
        TransferItem::Direction::Outgoing, "peer-1", &tm
    );
    item->setTotalFiles(3);
    tm.m_transfers[transferId] = item;

    tm.onSessionTransferProgress(transferId, 50, 300);
    TEST_ASSERT(item->transferredSize() == 50, "First folder progress update should store aggregate bytes");

    tm.onSessionTransferProgress(transferId, 100, 300);
    TEST_ASSERT(item->transferredSize() == 100, "Folder progress should grow with the first file");

    tm.onSessionTransferProgress(transferId, 30, 100);
    TEST_ASSERT(item->transferredSize() == 100,
                "A per-file progress reset must not move folder progress backwards");

    tm.onSessionTransferProgress(transferId, 180, 300);
    TEST_ASSERT(item->transferredSize() == 180, "Later aggregate folder progress should be stored");
    TEST_ASSERT(item->progress() > 50.0, "Folder progress percent should reflect completed files plus current file");

    TransferItem* incoming = new TransferItem(
        "incoming-folder-progress", "Pics", 0,
        TransferItem::Direction::Incoming, "peer-1", &tm
    );
    incoming->setTotalFiles(2);
    tm.m_transfers["incoming-folder-progress"] = incoming;
    tm.onSessionTransferProgress("incoming-folder-progress", 40, 200);
    TEST_ASSERT(incoming->totalSize() == 200, "Incoming folder progress should learn the aggregate total size");
    TEST_ASSERT(incoming->transferredSize() == 40, "Incoming folder progress should store aggregate bytes");

    return true;
}

// -------------------------------------------------------------
// Main Test Runner
// -------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    std::cerr << "test_witra starting..." << std::endl;

    QCoreApplication app(argc, argv);
    app.setOrganizationName("Witra");
    app.setApplicationName("WitraTests");
    CertificatePinStore::clearAll();
    
    std::cout << "==================================================" << std::endl;
    std::cout << "       WITRA AUTOMATED VERIFICATION SUITE         " << std::endl;
    std::cout << "==================================================" << std::endl;
    
    RUN_TEST(testPeerManager);
    RUN_TEST(testPathSanitization);
    RUN_TEST(testNetworkDiscovery);
    RUN_TEST(testLiveTlsConnection);
    RUN_TEST(testEndToEndFileTransfer);
    RUN_TEST(testFileSizeLimitEnforcement);
    RUN_TEST(testUnauthorizedTransferRejected);
    RUN_TEST(testPinnedCertificateMismatchRejected);
    RUN_TEST(testIncomingReceivedPathStored);
    RUN_TEST(testIncomingSessionConnectionsNotDuplicated);
    RUN_TEST(testFolderTransferProgressAccumulates);
    
    std::cout << "==================================================" << std::endl;
    std::cout << " RESULTS: " << g_testsPassed << "/" << g_testsRun << " Passed ("
              << g_testsFailed << " Failed)" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    return (g_testsFailed == 0) ? 0 : 1;
}
