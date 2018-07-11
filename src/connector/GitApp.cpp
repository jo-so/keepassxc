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

#include "GitApp.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

bool GitApp::start(const QStringList& args)
{
    if (args.isEmpty()) {
        qCritical() << "No action given. Please provide an action as argument";
        return false;
    }

    if (args.first() == "get") {
        m_action = Action::Get;
    } else if (args.first() == "store") {
        m_action = Action::Store;
    } else if (args.first() == "erase") {
        m_action = Action::Erase;
    } else {
        qCritical() << "Invalid action:" << args.first();
        return false;
    }

    QMap<QString, QString> data;
    {
        QTextStream stream(stdin, QIODevice::ReadOnly);
        while (1) {
            auto line = stream.readLine();
            if (line.isEmpty())
                break;
            auto split = line.split('=');
            if (split.size() == 2)
                data.insert(split[0], split[1]);
            else
                qWarning() << "Broken input line: " << split;
        }
    }

    auto url = data.value("protocol", "http") + "://" + data["host"] + "/";
    qDebug() << "URL =" << url;

    if (!loadIdentity()) {
        qCritical().noquote() << "Failed to load the identity from " << idPath();
        return false;
    }

    connect(this, &AppBase::responseReceived, this, &GitApp::handleResponse);

    if (!connectToServer()) {
        disconnect(this, &AppBase::responseReceived, this, &GitApp::handleResponse);
        return false;
    }

    switch (m_action) {
    case Action::Get:
        if (getLogins(url))
            return true;
        break;

    case Action::Store:
        // TODO: not implemented
        break;

    case Action::Erase:
        // TODO: not implemented
        break;

    default:
        qFatal("%s Unexpected m_action: %u", __PRETTY_FUNCTION__,
          static_cast<unsigned>(m_action));
        Q_UNREACHABLE();
    }

    disconnect(this, &AppBase::responseReceived, this, &GitApp::handleResponse);

    return false;
}

void GitApp::handleResponse(QByteArray data)
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
    case Action::Get:
        Q_ASSERT(act == "get-logins");
        {
            const auto o = extractMessage(obj).value("entries")
                .toArray().first().toObject();
            std::cout
                << "username=" << o["login"].toString().toStdString() << std::endl
                << "password=" << o["password"].toString().toStdString() << std::endl;
        }
        break;

    case Action::Store:
        Q_UNIMPLEMENTED();
        break;

    case Action::Erase:
        Q_UNIMPLEMENTED();
        break;

    default:
        qFatal("%s Unexpected m_action: %u", __PRETTY_FUNCTION__,
          static_cast<unsigned>(m_action));
        Q_UNREACHABLE();
    }

    QCoreApplication::quit();
}
