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

#ifndef GITAPP_H
#define GITAPP_H

class GitApp : App
{
public:
    enum class Action { Get, Store, Erase };

    GitApp(QString&, Action);

private:
    Action m_action;
};

#endif // GITAPP_H

GitApp::GitApp(QString& id_path, Action action)
    : App(id_path), m_action(action)
{
}

bool GitApp::init()
{
    if (!load_identity())
        return false;

    if (!connect())
        return false;

    // TODO:
    // https://stackoverflow.com/questions/6878507/using-qsocketnotifier-to-select-on-a-char-device/7389622#7389622
    // QSocketNotifier stdin_notifier(fileno(stdin), QSocketNotifier::Read);
    switch (m_action) {
    case Action::Get:
        // connect(stdin_notifier.data(), SIGNAL(activated(int)), this, SLOT(text()));
        // TODO: wait for std::cin; split_lines()
        break;

    case Action::Store:
        // TODO: wait for std::cin; split_lines()
        break;

    case Action::Erase:
        // TODO: wait for std::cin; split_lines()
        break;

    default:
        qFatal("Unexpected m_action: %d", m_action);
        break;
    }
}
