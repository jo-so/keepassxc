/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *  Copyright © 2018 Jörg Sommer <joerg@alea.gnuu.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AppBase.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <sodium.h>
#include <sodium/crypto_box.h>
#include <sodium/randombytes.h>

#include <sys/socket.h>

// taken from ../browser/NativeMessagingBase.h
const int NATIVE_MSG_MAX_LENGTH = 1024*1024;

QString AppBase::default_id_path()
{
    return QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
        .first() + "/id_default";
}

bool AppBase::loadIdentity()
{
    QFile file(m_idPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    if (sodium_init() == -1)
        return false;

    const auto obj = doc.object();
    m_myPublicKey = obj["myPublicKey"].toString();
    m_mySecretKey = obj["mySecretKey"].toString();
    m_remotePublicKey = obj["remotePublicKey"].toString();

    return true;
}

bool AppBase::storeIdentity()
{
    QJsonObject obj;
    obj["myPublicKey"] = m_myPublicKey;
    obj["mySecretKey"] = m_mySecretKey;
    obj["remotePublicKey"] = m_remotePublicKey;

    if (!QFileInfo(m_idPath).dir().mkpath("."))
        return false;

    QFile file(m_idPath);
    // TODO: for qt5.11 add QIODevice::NewOnly
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text))
        return false;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool AppBase::generateKeys()
{
    if (sodium_init() == -1)
        return false;

    QByteArray pubkey(crypto_box_PUBLICKEYBYTES, '\0');
    QByteArray seckey(crypto_box_SECRETKEYBYTES, '\0');
    if (crypto_box_keypair(reinterpret_cast<unsigned char*>(pubkey.data()),
        reinterpret_cast<unsigned char*>(seckey.data())) != 0)
        return false;

    m_myPublicKey = pubkey.toBase64();
    m_mySecretKey = seckey.toBase64();

    return true;
}

QString AppBase::encryptMessage(const QJsonObject& data, const QString& nonce)
{
    const auto msg = QJsonDocument(data).toJson();
    const auto nonce_val = QByteArray::fromBase64(nonce.toUtf8());
    const auto remote_pubkey_val = QByteArray::fromBase64(m_remotePublicKey.toUtf8());
    const auto my_seckey_val = QByteArray::fromBase64(m_mySecretKey.toUtf8());

    QByteArray cipher(msg.length() + crypto_box_MACBYTES, '\0');
    Q_ASSERT(crypto_box_easy(reinterpret_cast<unsigned char*>(cipher.data()),
        reinterpret_cast<const unsigned char*>(msg.constData()), msg.length(),
        reinterpret_cast<const unsigned char*>(nonce_val.constData()),
        reinterpret_cast<const unsigned char*>(remote_pubkey_val.constData()),
        reinterpret_cast<const unsigned char*>(my_seckey_val.constData())) == 0);

    return QString(cipher.toBase64());
}

QJsonObject AppBase::decryptMessage(const QString& data, const QString& nonce)
{
    const auto cipher = QByteArray::fromBase64(data.toUtf8());
    const auto nonce_val = QByteArray::fromBase64(nonce.toUtf8());
    const auto remote_pubkey_val = QByteArray::fromBase64(m_remotePublicKey.toUtf8());
    const auto my_seckey_val = QByteArray::fromBase64(m_mySecretKey.toUtf8());

    QByteArray msg(cipher.length() - crypto_box_MACBYTES, '\0');
    if (crypto_box_open_easy(reinterpret_cast<unsigned char*>(msg.data()),
        reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.length(),
        reinterpret_cast<const unsigned char*>(nonce_val.constData()),
        reinterpret_cast<const unsigned char*>(remote_pubkey_val.constData()),
        reinterpret_cast<const unsigned char*>(my_seckey_val.constData())) != 0) {
        qDebug() << "decrypt of message failed";
        return QJsonObject();
    }

    qDebug() << "decrypted message:" << msg;
    return QJsonDocument::fromJson(msg).object();
}

QString socketPath()
{
    const QString serverPath = "/kpxc_server";
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    // Use XDG_RUNTIME_DIR instead of /tmp if it's available
    const auto path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return path.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::TempLocation) + serverPath
        : path + serverPath;
#else // Q_OS_MAC, Q_OS_WIN and others
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + serverPath;
#endif
}

bool AppBase::connectToServer()
{
    if (m_socket.state() != QLocalSocket::UnconnectedState)
        return true;

    m_socket.connectToServer(socketPath());
    m_socket.setReadBufferSize(NATIVE_MSG_MAX_LENGTH);
    if (m_socket.state() == QLocalSocket::UnconnectedState) {
        qCritical() << "Initialization failed. Please check if KeePassXC is running"
            " and browser-integration is enabled:" << m_socket.errorString();
        return false;
    }

    const auto socketDesc = m_socket.socketDescriptor();
    if (socketDesc != -1) {
        const int max = NATIVE_MSG_MAX_LENGTH;
        setsockopt(static_cast<int>(socketDesc), SOL_SOCKET, SO_SNDBUF,
          &max, sizeof(max));
    }

    connect(&m_socket, SIGNAL(readyRead()), this, SLOT(dataReceived()));
    return true;
}

void AppBase::dataReceived()
{
    if (m_socket.bytesAvailable() <= 0)
        return;

    auto data = m_socket.readAll();
    qDebug().noquote() << "Received from KPXC = " << data;
    emit responseReceived(data);
}

QJsonObject AppBase::extractMessage(const QJsonObject& obj)
{
    return decryptMessage(obj["message"].toString(), obj["nonce"].toString());
}

bool AppBase::send(const QByteArray& data)
{
    auto ret = m_socket.write(data.constData(), data.length());
    if (ret == -1)
        return false;
    Q_ASSERT(ret == data.length());
    m_socket.flush();
    qDebug().noquote() << "Send to KPXC =" << data;
    return true;
}

bool AppBase::send(const QJsonObject& obj)
{
    return send(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString AppBase::nonce()
{
    // TODO: implementieren
    return "tZvLrBzkQ9GxXq9PvKJj4iAnfPT0VZ3Q";
}

bool AppBase::send(const QString& action,
  std::initializer_list<QPair<QString, QJsonValue> > args)
{
    auto no = nonce();
    QJsonObject msg {
      {"action", action},
      {"nonce", no},
      {"clientID", "xxx"},
    };

    if (args.size() > 0)
        msg["message"] = encryptMessage(QJsonObject(args), no);

    return send(msg);
}

bool AppBase::changePublicKeys()
{
    return send({
      {"action", "change-public-keys"},
      {"nonce", nonce()},
      {"publicKey", m_myPublicKey},
      {"clientID", "xxx"},
    });
}

bool AppBase::getDatabasehash()
{
    return send("get-databasehash", { {"action", "get-databasehash"} });
}

bool AppBase::associate()
{
    return send("associate", { {"key", m_myPublicKey} });
}

/*
bool AppBase::testAssociate()
{
    return send("test-associate", {
      {"key", responseKey},
      {"id", id},
    });
}
*/

bool AppBase::generatePassword()
{
    return send("generate-password", {});
}

bool AppBase::getLogins(const QString& url)
{
    return send("get-logins", {
      {"url", url},
//      {"submitUrl", ""},
      {"keys", QJsonArray {
//          {{"id", }, {"key", }}
      }},
    });
}

bool AppBase::setLogin(const QString& url)
{
    return send("set-login", {
      {"url", url},
      {"id", url},
      {"login", url},
      {"password", url},
      {"submitUrl", url},
      {"uuid", url},
    });
}

bool AppBase::lockDatabase()
{
    return send("lock-database", { {"action", "lock-database"} });
}
