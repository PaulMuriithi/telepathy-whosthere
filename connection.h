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

//There is no client with support for that
//#define USE_CAPTCHA_FOR_REGISTRATION

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
    ~YSConnection();
    uint getSelfHandle(Tp::DBusError *error) override;
    void connect(Tp::DBusError *error) override;
    bool holdHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error) override;
    QStringList inspectHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error) override;
    QString uniqueName() const override;
    Tp::BaseChannelPtr createChannel(const QString& channelType, uint targetHandleType,
                                     uint targetHandle, Tp::DBusError *error);
    Tp::ContactAttributesMap getContactAttributes(const Tp::UIntList& handles,
                                                  const QStringList& interfaces,
                                                  Tp::DBusError *error);
    Tp::UIntList requestHandles(uint handleType, const QStringList& identifiers, Tp::DBusError* error);
    void messageAcknowledged(QString id);
    QString sendMessage(const QString& jid, const Tp::MessagePartList& message, uint flags, Tp::DBusError* error);
    uint setPresence(const QString& status, const QString& message, Tp::DBusError* error);
    Tp::ContactAttributesMap getContactListAttributes(const QStringList& interfaces, bool hold, Tp::DBusError* error);
    void requestSubscription(const Tp::UIntList& contacts, const QString& message, Tp::DBusError* error);


    void getContactsByVCardField(const QString& field, const QStringList& addresses,const QStringList& interfaces,
                                 Tp::AddressingNormalizationMap& addressingNormalizationMap,
                                 Tp::ContactAttributesMap& contactAttributesMap, Tp::DBusError* error);
    void getContactsByURI(const QStringList& URIs, const QStringList& interfaces,
                         Tp::AddressingNormalizationMap& addressingNormalizationMap,
                         Tp::ContactAttributesMap& contactAttributesMap, Tp::DBusError* error);
#ifdef USE_CAPTCHA_FOR_REGISTRATION
    /* Authentication */
    void getCaptchas(Tp::CaptchaInfoList& captchaInfoList, uint& numberRequired, QString& language, Tp::DBusError* error);
    QByteArray getCaptchaData(uint ID, const QString& mimeType, Tp::DBusError* error);
    void answerCaptchas(const Tp::CaptchaAnswers& answers, Tp::DBusError* error);
    void cancelCaptcha(uint reason, const QString& debugMessage, Tp::DBusError* error);
#endif
private slots:
    void on_yowsup_auth_success(QString phonenumber);
    void on_yowsup_auth_fail(QString mobilenumber, QString reason);
    void on_yowsup_status_dirty();
    void on_yowsup_message_received(QString msgId, QString jid, QString content, uint timestamp, bool wantsReceipt, QString pushName);
    void on_yowsup_disconnected(QString reason);
    void on_yowsup_receipt_messageSent(QString jid,QString msgId);
    void on_yowsup_receipt_messageDelivered(QString jid, QString msgId);
    void on_yowsup_presence_available(QString jid);
    void on_yowsup_presence_unavailable(QString jid);
private:
    bool isValidContact(const QString& identifier);
    bool isValidId(const QString& jid);
    uint addContact(QString jid);
    uint ensureContact(QString jid);
    QString getContactByHandle(uint handle);
    void setPresenceState(uint handle, const QString& status);
    void setSubscriptionState(const QString& jid, uint handle, uint state);
    QString generateUID();

    Tp::BaseConnectionRequestsInterfacePtr requestsIface;
    Tp::BaseConnectionContactsInterfacePtr contactsIface;
    Tp::BaseConnectionSimplePresenceInterfacePtr simplePresenceIface;
    Tp::BaseConnectionContactListInterfacePtr contactListIface;
    Tp::BaseConnectionAddressingInterfacePtr addressingIface;
#ifdef USE_CAPTCHA_FOR_REGISTRATION
    /* Only valid during registration */
    Tp::BaseChannelCaptchaAuthenticationInterfacePtr captchaIface;
#endif
    /* handle "0" is never valid accordin to spec */
    boost::bimap<uint,QString> contacts;

    /* Mapping the telepathy-id to the yowsup <jid,msgId,wantsReceipt> */
    QHash<QString,std::tuple<QString,QString,bool>> pendingMessages;
    /* increasing id for unique telepathy-ids */
    uint lastMessageId;
    uint selfHandle;

    PythonInterface* pythonInterface;

    QString mUsername;
    QByteArray mPassword;
    QString mUID;
    YowsupInterface yowsupInterface;
};
