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

bool GenericApp::start(const QStringList& args)
{
    if (args.isEmpty()) {
        qCritical() << "No action given. Please provide an action as argument";
        return false;
    }

    if (args.first() == "dbhash") {
        m_action = Action::Databasehash;
    } else if (args.first() == "genpwd") {
        m_action = Action::GeneratePassword;
    } else if (args.first() == "get") {
        if (args.length() < 2) {
            qCritical() << "Missing URL to filter as second argument";
            return false;
        }

        m_action = Action::Get;
    } else if (args.first() == "init") {
        m_action = Action::Init;
    } else if (args.first() == "lockdb") {
        m_action = Action::LockDatabase;
    } else if (args.first() == "ping") {
        m_action = Action::Ping;
    } else {
        qCritical() << "Invalid action:" << args.first();
        return false;
    }

    if (m_action != Action::Init) {
        if (!loadIdentity()) {
            qCritical().noquote() << "Failed to load the identity from "
                                  << idPath();
            return false;
        }
    }

    connect(this, &AppBase::responseReceived, this, &GenericApp::handleResponse);

    if (!connectToServer()) {
        disconnect(this, &AppBase::responseReceived, this, &GenericApp::handleResponse);
        return false;
    }

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
        if (getLogins(args.at(1)))
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
        qFatal("%s Unexpected m_action: %u", __PRETTY_FUNCTION__,
          static_cast<unsigned>(m_action));
        Q_UNREACHABLE();
    }

    disconnect(this, &AppBase::responseReceived, this, &GenericApp::handleResponse);

    return false;
}

void GenericApp::handleResponse(QByteArray data)
{
    auto json(QJsonDocument::fromJson(data));
    if (!json.isObject()) {
        qFatal("%s Received an unexpected answer from KeePassXC: %s",
          __PRETTY_FUNCTION__, data.constData());
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

    const auto act = obj["action"].toString();
    switch (m_action) {
    case Action::Databasehash:
        Q_ASSERT(act == "get-databasehash");
        Q_ASSERT(obj.contains("message"));
        {
            const auto str = extractMessage(obj).value("hash").toString();
            if (!str.isEmpty())
                std::cout << str.toStdString() << std::endl;
        }
        break;

    case Action::GeneratePassword:
        Q_ASSERT(act == "generate-password");
        Q_ASSERT(obj.contains("message"));
        std::cout << extractMessage(obj)
            .value("entries").toArray().first().toObject()
            .value("password").toString().toStdString() << std::endl;
        break;

    case Action::Get:
        Q_ASSERT(act == "get-logins");
        for (auto entry : extractMessage(obj).value("entries").toArray()) {
            const auto o = entry.toObject();
            const auto sep = "\t";
            std::cout << o["name"].toString().toStdString()
                      << sep << o["login"].toString().toStdString()
                      << sep << o["password"].toString().toStdString()
                      << std::endl;
        }
        break;

    case Action::Init:
        if (act == "change-public-keys") {
            auto pk(obj["publicKey"].toString());
            Q_ASSERT(!pk.isNull());
            qDebug() << "KeePassXC's public key:" << pk;
            setRemotePublicKey(pk);

            associate();
            return;
        } else if (act == "associate") {
            qDebug() << extractMessage(obj);

            if (!storeIdentity())
                qCritical().noquote() << "Failed to initialize the identity storage"
                                      << idPath();
        } else {
            qFatal("%s Unexpected action: %s", __PRETTY_FUNCTION__,
              act.toUtf8().constData());
            Q_UNREACHABLE();
        }

        break;

    case Action::LockDatabase:
        Q_ASSERT(act == "lock-database");
        std::cout << "KeePassXC database locked" << std::endl;
        break;

    case Action::Ping:
        if (obj.isEmpty())
            std::cout << "KeePassXC is running" << std::endl;
        break;

    default:
        qFatal("%s Unexpected m_action: %u", __PRETTY_FUNCTION__,
          static_cast<unsigned>(m_action));
        Q_UNREACHABLE();
    }

    QCoreApplication::quit();
}
