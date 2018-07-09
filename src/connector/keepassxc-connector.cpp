/*
*  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
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

#include "GenericApp.h"
#include "ProxyApp.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcessEnvironment>

#include <iostream>

#ifndef Q_OS_WIN
# include <initializer_list>
# include <signal.h>
# include <unistd.h>

// (C) Gist: https://gist.github.com/azadkuh/a2ac6869661ebd3f8588
void catchUnixSignals(std::initializer_list<int> quitSignals)
{
    auto handler = [](int sig) -> void {
        (void)sig;
        QCoreApplication::quit();
    };

    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);
    for (auto sig : quitSignals) {
        sigaddset(&blocking_mask, sig);
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_mask = blocking_mask;
    sa.sa_flags = 0;

    for (auto sig : quitSignals) {
        sigaction(sig, &sa, nullptr);
    }
}
#endif

/*
void print_quit(QJsonObject obj)
{
    auto error_code = obj["errorCode"];
    if (!error_code.isUndefined()) {
        qDebug() << obj;
        goto end;
    }

    auto action = obj["action"].toString();
    if (action == "get-databasehash") {
        qDebug().nospace() << "Hash: " << obj["hash"] << ", Version: " << obj["version"];
    } else if (action == "change-public-keys") {
    } else {
        qCritical() << "Unexpected action:" << obj;
    }

end:
    QCoreApplication::quit();
}
*/

int main(int argc, char* argv[])
{
    // keepassxc-connector [-i identity-file] [-m mode] init|get|store|erase
    // keepassxc-proxy
    // git-credential-keepassxc [-i identity-file] get|store|erase
    // pinentry-keepassxc [-i identity-file]
    // askpass-keepassxc [-i identity-file]
    QCoreApplication app(argc, argv);
    app.setApplicationName("keepassxc-connector");

    QCommandLineParser parser;
    parser.setApplicationDescription("Connector to a running KeePassXC instance");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"i", "Use <file> to identify to KeePassXC.", "file"});

    enum Mode { generic, askpass, git, pinentry, proxy } mode;
    {
        const auto prog_name = QFileInfo(app.arguments().first()).fileName();
        if (prog_name == "git-credential-keepassxc") {
            mode = git;
            parser.addPositionalArgument("command", "action to execute");
        } else if (prog_name == "pinentry" || prog_name == "pinentry-keepassxc") {
            mode = pinentry;
        } else if (prog_name == "askpass" || prog_name == "askpass-keepassxc") {
            mode = askpass;
        } else if (prog_name == "keepassxc-proxy") {
            mode = proxy;
        } else {
            mode = generic;
            parser.addOption({"m", "Operation mode: askpass, git-credential, pinentry", "mode"});
            parser.addPositionalArgument("command", "action to execute");
        }
    }

    parser.process(app);

    if (mode == generic && parser.isSet("m")) {
        const auto arg_m = parser.value("m");

        if (arg_m == "askpass") {
            mode = askpass;
        } else if (arg_m == "git-credential") {
            mode = git;
        } else if (arg_m == "pinentry") {
            mode = pinentry;
        } else if (arg_m == "proxy") {
            mode = proxy;
        } else {
            qCritical() << "Invalid mode:" << arg_m;
            return 1;
        }
    }

    QScopedPointer<AppBase> handler;
    const auto args = parser.positionalArguments();

    if (mode == proxy) {
        handler.reset(new ProxyApp());
/*
    } else if (mode == git) {
        if (args.isEmpty()) {
            qCritical() << "No action given. Please provide an action as argument";
            return 1;
        }

        GitApp::Action act;
        if (args.first() == "get") {
            act = GitApp::Mode::Get;
        } else if (args.first() == "store") {
            act = GitApp::Mode::Store;
        } else if (args.first() == "erase") {
            act = GitApp::Mode::Erase;
        } else {
            qCritical() << "Invalid action:" << args.first();
            return 1;
        }

        handler = new GitApp(id_path, act);
    } else if (mode == pinentry) {
        // TODO: listen for pinentry commands
    } else if (mode == askpass) {
        // TODO: std::cout << kpxc.getLogins().first().password();
*/
    } else {
        if (args.isEmpty()) {
            qCritical() << "No action given. Please provide an action as argument";
            return 1;
        }

        GenericApp::Action act;
        if (args.first() == "dbhash") {
            act = GenericApp::Action::Databasehash;
        } else if (args.first() == "genpwd") {
            act = GenericApp::Action::GeneratePassword;
        } else if (args.first() == "get") {
            act = GenericApp::Action::Get;
        } else if (args.first() == "init") {
            act = GenericApp::Action::Init;
        } else if (args.first() == "lockdb") {
            act = GenericApp::Action::LockDatabase;
        } else if (args.first() == "ping") {
            act = GenericApp::Action::Ping;
        } else {
            qCritical() << "Invalid action:" << args.first();
            return 1;
        }

        handler.reset(new GenericApp(act));
    }

    if (parser.isSet("i"))
        handler->setIdPath(parser.value("i"));
    else {
        const auto env(QProcessEnvironment::systemEnvironment());
        if (env.contains("KEEPASSXC_ID"))
            handler->setIdPath(env.value("KEEPASSXC_ID"));
        else
            handler->setIdPath(AppBase::default_id_path());
    }

    if (!handler->start())
        return 1;

#if defined(Q_OS_UNIX) || defined(Q_OS_LINUX)
    catchUnixSignals({SIGQUIT, SIGINT, SIGTERM, SIGHUP});
#endif
    return app.exec();
}
