/*
 * Copyright (C) 2013 Matthias Gehre <gehre.matthias@gmail.com>
 *
 * This work is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <tuple>
#include <QHash>
#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/BaseChannel>
#include <boost/bimap.hpp>

#include "pythoninterface.h"

class YSConnection : public Tp::BaseConnection
{
    Q_OBJECT
    Q_DISABLE_COPY(YSConnection)
public:
    /* Implementation of BaseConnection */
    YSConnection( 	const QDBusConnection &  	dbusConnection,
                    const QString &  	cmName,
                    const QString &  	protocolName,
                    const QVariantMap &  	parameters);
    uint getSelfHandle(Tp::DBusError *error) override;
    void connect(Tp::DBusError *error) override;
    bool holdHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error) override;
    QStringList inspectHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error) override;
    Tp::BaseChannelPtr createChannel(const QString& channelType, uint targetHandleType,
                                     uint targetHandle, Tp::DBusError *error);
    Tp::ContactAttributesMap getContactAttributes(const Tp::UIntList& handles,
                                                  const QStringList& interfaces,
                                                  Tp::DBusError *error);
    Tp::UIntList requestHandles(uint handleType, const QStringList& identifiers, Tp::DBusError* error);
    void messageAcknowledged(QString id);
    QString sendMessage(const QString& jid, const Tp::MessagePartList& message, uint flags, Tp::DBusError* error);

private slots:
    void on_yowsup_auth_success(QString phonenumber);
    void on_yowsup_auth_fail(QString mobilenumber, QString reason);
    void on_yowsup_status_dirty();
    void on_yowsup_message_received(QString msgId, QString jid, QString content, uint timestamp, bool wantsReceipt, QString pushName);
    void on_yowsup_disconnected(QString reason);
    void on_yowsup_receipt_messageSent(QString jid,QString msgId);
    void on_yowsup_receipt_messageDelivered(QString jid, QString msgId);
private:
    uint addContact(QString jid);
    uint ensureContact(QString jid);

    Tp::BaseConnectionRequestsInterfacePtr requestsIface;
    Tp::BaseConnectionContactsInterfacePtr contactsIface;
    /* handle "0" is never valid accordin to spec */
    boost::bimap<uint,QString> contacts;

    /* Mapping the telepathy-id to the yowsup <jid,msgId,wantsReceipt> */
    QHash<QString,std::tuple<QString,QString,bool>> pendingMessages;
    /* increasing id for unique telepathy-ids */
    uint lastMessageId;

    PythonInterface* pythonInterface;

    QString mPhoneNumber;
    QByteArray mPassword;
    YowsupInterface yowsupInterface;
};
