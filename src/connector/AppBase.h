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

#ifndef APPBASE_H
#define APPBASE_H

#include <QFile>
#include <QLocalSocket>

class AppBase : public QObject
{
    Q_OBJECT

public:
    static QString default_id_path();

    QString idPath() const { return m_idPath; }
    void setIdPath(const QString& val) { m_idPath = val; }

    virtual bool start() =0;

protected:
    bool loadIdentity();
    bool storeIdentity();
    void setRemotePublicKey(const QString& val) { m_remotePublicKey = val; }
    bool generateKeys();
    QString encryptMessage(const QJsonObject&, const QString&);
    QJsonObject decryptMessage(const QString&, const QString&);

    bool connectToServer();
    QString nonce();
    QJsonObject extractMessage(const QJsonObject&);
    bool send(const QByteArray&);
    bool send(const QJsonObject&);
    bool send(const QString&, std::initializer_list<QPair<QString, QJsonValue> >);

    bool changePublicKeys();
    bool associate();
    bool testAssociate();
    bool getDatabasehash();
    bool generatePassword();
    bool getLogins(const QString&);
    bool setLogin(const QString&);
    bool lockDatabase();

signals:
    void responseReceived(QByteArray);

private slots:
    void dataReceived();

private:
    QString m_idPath;
    QLocalSocket m_socket;
    QString m_myPublicKey;
    QString m_mySecretKey;
    QString m_remotePublicKey;
};

#endif // APPBASE_H
