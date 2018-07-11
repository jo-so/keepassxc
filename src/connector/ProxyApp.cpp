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

#include "ProxyApp.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#include <iostream>
#include <unistd.h>

ProxyApp::ProxyApp()
    : m_cin_notifier(STDIN_FILENO, QSocketNotifier::Read)
{
    m_cin_notifier.setEnabled(false);
}

bool ProxyApp::start(const QStringList& args)
{
    (void)args;
    connect(this, &AppBase::responseReceived, this, &ProxyApp::handleResponse);

    if (!connectToServer()) {
        disconnect(this, &AppBase::responseReceived, this, &ProxyApp::handleResponse);
        return false;
    }

    // TODO: for windows https://gist.github.com/gjorquera/2576569#gistcomment-2303103
    connect(&m_cin_notifier, &QSocketNotifier::activated, this, &ProxyApp::handleInput);
    m_cin_notifier.setEnabled(true);

    return true;
}

void ProxyApp::handleResponse(QByteArray data)
{
    if (data.isEmpty())
        return;

    const quint32 len = data.length();
    std::cout.write(reinterpret_cast<const char*>(&len), sizeof(len));
    std::cout.write(data.constData(), data.length());
    std::cout.flush();
}

void ProxyApp::handleInput(int socket)
{
    (void)socket;
    if (!std::cin.good()) {
        qDebug() << "Exiting, due to stdin is not in a good shape";
        QCoreApplication::quit();
        return;
    }

    quint32 length = 0;
    std::cin.read(reinterpret_cast<char*>(&length), sizeof(length));
    qDebug() << length << "characters of input data on" << socket;
    if (length > 0) {
        QByteArray data(length, '\0');
        std::cin.read(data.data(), length);
        if (std::cin.good())
            // cin didn't contain `length` characters
            send(data);
    }
}
