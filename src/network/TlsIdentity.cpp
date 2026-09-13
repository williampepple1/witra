#include "TlsIdentity.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <string>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <QBuffer>
#endif

namespace Witra {

namespace {

QString defaultStorageDir()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath() + "/witra-tls";
    }
    return root + "/tls";
}

bool writePem(const QString& path, const QByteArray& pem)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(pem) == pem.size();
}

} // namespace

TlsIdentity::TlsIdentity(const QString& storageDir)
    : m_storageDir(storageDir.isEmpty() ? defaultStorageDir() : storageDir)
{
}

TlsIdentity& TlsIdentity::application()
{
    static TlsIdentity identity;
    return identity;
}

bool TlsIdentity::ensure()
{
    if (isValid()) {
        return true;
    }
    QDir().mkpath(m_storageDir);
    if (loadFromDisk()) {
        return true;
    }
    return generateAndSave();
}

bool TlsIdentity::isValid() const
{
    return !m_certificate.isNull() && !m_privateKey.isNull();
}

QByteArray TlsIdentity::fingerprintSha256() const
{
    return fingerprintOf(m_certificate);
}

QSslConfiguration TlsIdentity::socketConfiguration() const
{
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setLocalCertificate(m_certificate);
    config.setPrivateKey(m_privateKey);
    config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    config.setProtocol(QSsl::TlsV1_2OrLater);
    return config;
}

QString TlsIdentity::pairingCode(const QSslCertificate& local, const QSslCertificate& peer)
{
    QByteArray a = fingerprintOf(local);
    QByteArray b = fingerprintOf(peer);
    if (a.isEmpty() || b.isEmpty()) {
        return {};
    }
    if (a > b) {
        qSwap(a, b);
    }
    const QByteArray digest = QCryptographicHash::hash(a + b, QCryptographicHash::Sha256);
    const quint32 value = (static_cast<quint32>(static_cast<quint8>(digest[0])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(digest[1])) << 8)
        | static_cast<quint32>(static_cast<quint8>(digest[2]));
    return QString::number(value % 1000000).rightJustified(6, '0');
}

QByteArray TlsIdentity::fingerprintOf(const QSslCertificate& certificate)
{
    if (certificate.isNull()) {
        return {};
    }
    return certificate.digest(QCryptographicHash::Sha256);
}

bool TlsIdentity::evaluateHandshake(QSslSocket* socket,
                                    const QString& expectedPeerId,
                                    const QList<QSslError>& errors,
                                    QString* reason)
{
    auto fail = [&](const QString& message) {
        if (reason) {
            *reason = message;
        }
        return false;
    };

    if (!socket) {
        return fail(QStringLiteral("Missing TLS socket"));
    }

    const QSslCertificate peerCert = socket->peerCertificate();
    if (peerCert.isNull()) {
        return fail(QStringLiteral("Peer did not present a TLS certificate"));
    }

    for (const QSslError& error : errors) {
        switch (error.error()) {
        case QSslError::SelfSignedCertificate:
        case QSslError::SelfSignedCertificateInChain:
        case QSslError::HostNameMismatch:
        case QSslError::CertificateUntrusted:
        case QSslError::UnableToGetLocalIssuerCertificate:
        case QSslError::UnableToVerifyFirstCertificate:
            break;
        default:
            return fail(error.errorString());
        }
    }

    const QByteArray fingerprint = fingerprintOf(peerCert);
    if (!expectedPeerId.isEmpty() && CertificatePinStore::hasPin(expectedPeerId)
        && !CertificatePinStore::matches(expectedPeerId, fingerprint)) {
        return fail(QStringLiteral("Peer certificate does not match the pinned identity"));
    }

    return true;
}

bool TlsIdentity::loadFromDisk()
{
    QFile certFile(certPath());
    QFile keyFile(keyPath());
    if (!certFile.exists() || !keyFile.exists()) {
        return false;
    }
    if (!certFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QSslCertificate cert(&certFile, QSsl::Pem);
    const QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
    if (cert.isNull() || key.isNull()) {
        return false;
    }
    m_certificate = cert;
    m_privateKey = key;
    return true;
}

bool TlsIdentity::generateAndSave()
{
    if (generateWithOpenSsl()) {
        return true;
    }
#ifdef Q_OS_WIN
    if (generateWithWindows()) {
        return true;
    }
#endif
    return false;
}

static QString findOpenSslBinary()
{
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    const QStringList candidates = {
        QStringLiteral("C:/Program Files/Git/usr/bin/openssl.exe"),
        QStringLiteral("C:/Program Files (x86)/Git/usr/bin/openssl.exe")
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool TlsIdentity::generateWithOpenSsl()
{
    const QString program = findOpenSslBinary();
    if (program.isEmpty()) {
        return false;
    }
    QStringList args = {
        QStringLiteral("req"), QStringLiteral("-x509"),
        QStringLiteral("-newkey"), QStringLiteral("rsa:2048"),
        QStringLiteral("-keyout"), keyPath(),
        QStringLiteral("-out"), certPath(),
        QStringLiteral("-days"), QStringLiteral("3650"),
        QStringLiteral("-nodes"),
        QStringLiteral("-subj"),
        QStringLiteral("/CN=Witra-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
    };

    QProcess process;
    process.setWorkingDirectory(m_storageDir);
    process.start(program, args);
    if (!process.waitForFinished(15000) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        qWarning() << "TlsIdentity: openssl failed:" << process.readAllStandardError();
        return false;
    }
    return loadFromDisk();
}

#ifdef Q_OS_WIN
bool TlsIdentity::generateWithWindows()
{
    const QString containerName = QStringLiteral("WitraTls-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const std::wstring container = containerName.toStdWString();

    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextW(&provider, container.c_str(), MS_ENHANCED_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
        return false;
    }

    HCRYPTKEY key = 0;
    bool ok = false;
    CERT_NAME_BLOB nameBlob{};
    PCCERT_CONTEXT certContext = nullptr;
    HCERTSTORE store = nullptr;
    CRYPT_DATA_BLOB pfx{};
    CRYPT_KEY_PROV_INFO keyProvInfo{};
    DWORD nameSize = 0;
    const wchar_t* subject = L"CN=Witra";

    if (!CryptGenKey(provider, AT_KEYEXCHANGE, (2048u << 16) | CRYPT_EXPORTABLE, &key)) {
        qWarning() << "TlsIdentity: CryptGenKey failed" << quint32(GetLastError());
        goto cleanup;
    }
    if (!CertStrToNameW(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, nullptr, nullptr, &nameSize, nullptr)) {
        goto cleanup;
    }
    nameBlob.pbData = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, nameSize));
    nameBlob.cbData = nameSize;
    if (!nameBlob.pbData
        || !CertStrToNameW(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, nullptr,
                           nameBlob.pbData, &nameBlob.cbData, nullptr)) {
        goto cleanup;
    }

    certContext = CertCreateSelfSignCertificate(provider, &nameBlob, 0, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!certContext) {
        qWarning() << "TlsIdentity: CertCreateSelfSignCertificate failed" << quint32(GetLastError());
        goto cleanup;
    }

    keyProvInfo.pwszContainerName = const_cast<wchar_t*>(container.c_str());
    keyProvInfo.pwszProvName = const_cast<wchar_t*>(MS_ENHANCED_PROV_W);
    keyProvInfo.dwProvType = PROV_RSA_FULL;
    keyProvInfo.dwKeySpec = AT_KEYEXCHANGE;
    if (!CertSetCertificateContextProperty(certContext, CERT_KEY_PROV_INFO_PROP_ID, 0, &keyProvInfo)) {
        qWarning() << "TlsIdentity: CertSetCertificateContextProperty failed" << quint32(GetLastError());
        goto cleanup;
    }

    store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
    if (!store || !CertAddCertificateContextToStore(store, certContext, CERT_STORE_ADD_ALWAYS, nullptr)) {
        qWarning() << "TlsIdentity: CertAddCertificateContextToStore failed" << quint32(GetLastError());
        goto cleanup;
    }

    if (!PFXExportCertStoreEx(store, &pfx, L"witra", nullptr, EXPORT_PRIVATE_KEYS)) {
        qWarning() << "TlsIdentity: PFXExportCertStoreEx size failed" << quint32(GetLastError());
        goto cleanup;
    }
    pfx.pbData = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, pfx.cbData));
    if (!pfx.pbData
        || !PFXExportCertStoreEx(store, &pfx, L"witra", nullptr, EXPORT_PRIVATE_KEYS)) {
        qWarning() << "TlsIdentity: PFXExportCertStoreEx export failed" << quint32(GetLastError());
        goto cleanup;
    }

    {
        QByteArray pfxBytes(reinterpret_cast<const char*>(pfx.pbData), static_cast<int>(pfx.cbData));
        QBuffer buffer(&pfxBytes);
        buffer.open(QIODevice::ReadOnly);
        QSslKey importedKey;
        QSslCertificate importedCert;
        if (!QSslCertificate::importPkcs12(&buffer, &importedKey, &importedCert, nullptr, QByteArrayLiteral("witra"))) {
            qWarning() << "TlsIdentity: importPkcs12 failed";
            goto cleanup;
        }
        if (importedCert.isNull() || importedKey.isNull()) {
            goto cleanup;
        }
        if (!writePem(certPath(), importedCert.toPem()) || !writePem(keyPath(), importedKey.toPem())) {
            goto cleanup;
        }
        m_certificate = importedCert;
        m_privateKey = importedKey;
        ok = true;
    }

cleanup:
    if (pfx.pbData) {
        HeapFree(GetProcessHeap(), 0, pfx.pbData);
    }
    if (store) {
        CertCloseStore(store, 0);
    }
    if (certContext) {
        CertFreeCertificateContext(certContext);
    }
    if (nameBlob.pbData) {
        HeapFree(GetProcessHeap(), 0, nameBlob.pbData);
    }
    if (key) {
        CryptDestroyKey(key);
    }
    if (provider) {
        CryptReleaseContext(provider, 0);
        CryptAcquireContextW(&provider, container.c_str(), nullptr, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    }
    return ok;
}
#endif

QString TlsIdentity::certPath() const
{
    return m_storageDir + "/cert.pem";
}

QString TlsIdentity::keyPath() const
{
    return m_storageDir + "/key.pem";
}

QByteArray CertificatePinStore::pinFor(const QString& peerId)
{
    QSettings settings;
    return QByteArray::fromHex(settings.value(QStringLiteral("PeerCertificatePins/%1").arg(peerId)).toString().toLatin1());
}

bool CertificatePinStore::hasPin(const QString& peerId)
{
    return !pinFor(peerId).isEmpty();
}

bool CertificatePinStore::matches(const QString& peerId, const QByteArray& fingerprint)
{
    return !fingerprint.isEmpty() && pinFor(peerId) == fingerprint;
}

void CertificatePinStore::pin(const QString& peerId, const QByteArray& fingerprint)
{
    if (peerId.isEmpty() || fingerprint.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("PeerCertificatePins/%1").arg(peerId),
                      QString::fromLatin1(fingerprint.toHex()));
}

void CertificatePinStore::clear(const QString& peerId)
{
    QSettings settings;
    settings.remove(QStringLiteral("PeerCertificatePins/%1").arg(peerId));
}

void CertificatePinStore::clearAll()
{
    QSettings settings;
    settings.remove(QStringLiteral("PeerCertificatePins"));
}

} // namespace Witra
