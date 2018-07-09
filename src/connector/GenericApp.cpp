/*
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

#include "GenericApp.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <sodium.h>
#include <sodium/crypto_box.h>
#include <sodium/randombytes.h>

GenericApp::GenericApp(Action act)
    : m_action(act)
{
}

bool GenericApp::start()
{
    if (m_action != Action::Init) {
        if (!loadIdentity()) {
            qCritical().noquote() << "Failed to load the identity from "
                                  << idPath();
            return false;
        }
    }

    if (!connectToServer())
        return false;

    connect(this, &AppBase::responseReceived, this, &GenericApp::handleResponse);

    switch (m_action) {
    case Action::Databasehash:
        if (getDatabasehash())
            return true;
        break;

    case Action::GeneratePassword:
        if (generatePassword())
            return true;
        break;

    case Action::Get:
        if (getLogins(""))
            return true;
        break;

    case Action::Init:
        if (generateKeys() && changePublicKeys())
            return true;
        break;

    case Action::LockDatabase:
        if (lockDatabase())
            return true;
        break;

    case Action::Ping:
        if (send(QByteArray("{}")))
            return true;
        break;

    default:
        qFatal("Unexpected m_action: %u", static_cast<unsigned>(m_action));
        Q_UNREACHABLE();
    }

    disconnect(this, &AppBase::responseReceived, this, &GenericApp::handleResponse);

    return false;
}

void GenericApp::handleResponse(QByteArray data)
{
    auto json(QJsonDocument::fromJson(data));
    if (!json.isObject()) {
        qFatal("Received an unexpected answer from KeePassXC: %s", data.constData());
        Q_UNREACHABLE();
    }

    auto obj(json.object());
    if (obj.contains("error")) {
        qCritical().noquote().nospace() << "KeePassXC returned an error on action "
                                        << obj["action"].toString() << ": "
                                        << obj["error"].toString();
        QCoreApplication::quit();
        return;
    }

    switch (m_action) {
    case Action::Databasehash:
        Q_ASSERT(obj["action"] == "get-databasehash");
        Q_ASSERT(obj.contains("message"));
        std::cout << extractMessage(obj).value("hash").toString().toStdString() << std::endl;
        break;

    case Action::GeneratePassword:
        Q_ASSERT(obj["action"] == "generate-password");
        Q_ASSERT(obj.contains("message"));
        std::cout << extractMessage(obj)
            .value("entries").toArray().first().toObject()
            .value("password").toString().toStdString() << std::endl;
        break;

    case Action::Init:
        Q_ASSERT(obj["action"] == "change-public-keys");
        {
            auto pk(obj["publicKey"].toString());
            Q_ASSERT(!pk.isNull());
            qDebug() << "KeePassXC's public key:" << pk;
            setRemotePublicKey(pk);
        }

        if (!storeIdentity())
            qCritical().noquote() << "Failed to initialize the identity storage"
                                  << idPath();

        break;

    case Action::LockDatabase:
        Q_ASSERT(obj["action"] == "lock-database");
        std::cout << "KeePassXC database locked" << std::endl;
        break;

    case Action::Ping:
        if (obj.isEmpty())
            std::cout << "KeePassXC is running" << std::endl;
        break;

    default:
        qFatal("Unexpected m_action: %u", static_cast<unsigned>(m_action));
        Q_UNREACHABLE();
    }

    QCoreApplication::quit();
}
