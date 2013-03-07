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

#include <algorithm>
#include <QDebug>
#include <TelepathyQt/Constants>
#include "connection.h"

using namespace Tp;
using namespace std;

YSConnection::YSConnection( const QDBusConnection &  	dbusConnection,
                            const QString &  	cmName,
                            const QString &  	protocolName,
                            const QVariantMap &  	parameters
                            ) : BaseConnection(dbusConnection, cmName, protocolName, parameters),
                                lastMessageId(1),
                                yowsupInterface(this)
{
    qDebug() << "YSConnection::YSConnection proto: " << protocolName
             << " parameters: " << parameters;
    mPhoneNumber = parameters["phonenumber"].toString();
    mPassword = QByteArray::fromBase64( parameters["password"].toString().toLatin1() );


    uint selfId = addContact(mPhoneNumber + "@s.whatsapp.net");
    assert(selfId == 1);

    setCreateChannelCallback( Tp::memFun(this,&YSConnection::createChannel) );
    setRequestHandlesCallback( Tp::memFun(this,&YSConnection::requestHandles) );
    //Fill requestableChannelClasses

    requestsIface = BaseConnectionRequestsInterface::create(this);
    /* TODO: add contact list */
    RequestableChannelClass text;
    text.fixedProperties[TP_QT_IFACE_CHANNEL+".ChannelType"] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    text.fixedProperties[TP_QT_IFACE_CHANNEL+".TargetHandleType"]  = HandleTypeContact;
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL+".TargetHandle");
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL+".TargetID");
    requestsIface->requestableChannelClasses << text;
    plugInterface(AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    contactsIface = BaseConnectionContactsInterface::create();
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this,&YSConnection::getContactAttributes));
    plugInterface(AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    pythonInterface = new PythonInterface(&yowsupInterface);
    yowsupInterface.setObjectName("yowsup");
    QMetaObject::connectSlotsByName(this);
#if 0
#define D(X) QObject::connect(&yowsupInterface, &YowsupInterface::X, this, &YSConnection::X)
    D(auth_success);
    D(auth_fail);
    D(status_dirty);
    D(message_received);
#undef D
#endif
    qDebug() << "Thread id constructor " << QThread::currentThreadId();
}

uint YSConnection::getSelfHandle(Tp::DBusError *error)
{
    qDebug() << "getSelfHandle";
    return 1;
}

void YSConnection::connect(Tp::DBusError *error) {
    qDebug() << "Thread id connect " << QThread::currentThreadId();
    qDebug() << "YSConnection::connect";
    setStatus(ConnectionStatusConnecting, ConnectionStatusReasonRequested);
    pythonInterface->call("auth_login", mPhoneNumber, mPassword );
}

//This is basically a no-op for the current spec
bool YSConnection::holdHandles(uint /*handleType*/, const Tp::UIntList& /*handles*/, Tp::DBusError* /*error*/)
{
    //qDebug() << "YSConnection::holdHandles " << handles;
    return true;
}

QStringList YSConnection::inspectHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error)
{
    qDebug() << "YSConnection::inspectHandles " << handles;

    if( handleType == Tp::HandleTypeContact ) {
        QStringList ret;
        for( uint handle : handles ) {
            auto i = contacts.left.find(handle);
            if( i == contacts.left.end() ) {
                error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
                return QStringList();
            }
            qDebug() << "inspectHandles " << handle << " = " << i->second;
            ret.append( i->second );
        }
        return ret;

    } else {
        qDebug() << "YSConnection::inspectHandles: unsupported handle type " << handleType;
        error->set(TP_QT_ERROR_INVALID_ARGUMENT,"Type unknown");
        return QStringList();
    }
}

Tp::ContactAttributesMap YSConnection::getContactAttributes(const Tp::UIntList& handles,
                                                            const QStringList& interfaces,
                                                            Tp::DBusError*)
{
    Tp::ContactAttributesMap ret;
    for( uint handle : handles ) {
        auto i = contacts.left.find(handle);
        if( i == contacts.left.end())
            continue;
        qDebug() << "getContactAttributes " << handle << " = " << i->second;
        QVariantMap attributes;
        attributes["org.freedesktop.Telepathy.Connection/contact-id"] = i->second;
        ret[handle] = attributes;
    }
    qDebug() << "YSConnection::getContactAttributes " << handles
             << " = " << ret;
    return ret;

}

Tp::UIntList YSConnection::requestHandles(uint handleType, const QStringList& identifiers, Tp::DBusError* error)
{
    Tp::UIntList ret;
    if( handleType == Tp::HandleTypeContact ) {
        for( const QString& identifier : identifiers ) {
            auto i = contacts.right.find(identifier);
            if( i == contacts.right.end()) {
                error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
                return Tp::UIntList();
            } else {
                ret.push_back(i->second);
            }
        }
    } else {
        qDebug() << "YSConnection::requestHandles: handleType not supported " << handleType;
        error->set(TP_QT_ERROR_INVALID_ARGUMENT,"Type unknown");
    }
    qDebug() << "YSConnection::requestHandles " << identifiers
             << " = " << ret;
    return ret;
}

Tp::BaseChannelPtr YSConnection::createChannel(const QString& channelType, uint targetHandleType,
                                               uint targetHandle, Tp::DBusError *error)
{
    qDebug() << "YSConnection::createChannel " << channelType
             << " " << targetHandleType
             << " " << targetHandle;
    if( targetHandleType != Tp::HandleTypeContact || targetHandle == 0)
    {
        error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
        return BaseChannelPtr();
    }

    auto i = contacts.left.find(targetHandle);
    if( i == contacts.left.end() ) {
        error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
        return BaseChannelPtr();
    }
    QString jid = i->second;

    BaseChannelPtr baseChannel = BaseChannel::create(this, channelType, targetHandle, targetHandleType);

    if(channelType == TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        BaseChannelTextTypePtr textType = BaseChannelTextType::create(baseChannel.data());
        qDebug() << "Text interface is called " << textType->interfaceName();
        baseChannel->plugInterface(AbstractChannelInterfacePtr::dynamicCast(textType));


        QStringList supportedContentTypes = QStringList() << "text/plain";
        UIntList messageTypes = UIntList() << ChannelTextMessageTypeNormal << ChannelTextMessageTypeDeliveryReport;
        uint messagePartSupportFlags = 0;
        uint deliveryReportingSupport = DeliveryReportingSupportFlagReceiveSuccesses;
        BaseChannelMessagesInterfacePtr messagesIface = BaseChannelMessagesInterface::create(textType.data(),
                                                                                             supportedContentTypes,
                                                                                             messageTypes,
                                                                                             messagePartSupportFlags,
                                                                                             deliveryReportingSupport);
        messagesIface->setSendMessageCallback(
                    [this, jid] (const Tp::MessagePartList& message, uint flags, Tp::DBusError* error) {
                        return sendMessage(jid, message, flags, error );
                    });
        baseChannel->plugInterface(AbstractChannelInterfacePtr::dynamicCast(messagesIface));
    }

    baseChannel->registerObject(error);
    qDebug() << "addChannel";
    addChannel( baseChannel );

    return baseChannel;
}

/* Called when a telepathy client has acknowledged receiving this message */
void YSConnection::messageAcknowledged(QString /*id*/) {
#if 0
    auto i = pendingMessages.find(id);
    if( i == pendingMessages.end()) {
        qDebug() << "YSConnection::messageAcknowledged: Warning: id " << id << " not found";
        return;
    }
    QString jid = get<0>(*i);
    QString msgId = get<1>(*i);
    bool wantsReceipt = get<2>(*i);
    qDebug() << "YSConnection::messageAcknowledged: acknowleding " << jid << " " << msgId;

    pendingMessages.erase(i);
#endif
}

QString YSConnection::sendMessage(const QString& jid, const Tp::MessagePartList& message, uint /*flags*/,
                                  Tp::DBusError* error) {
    //We ignore flags and always post delivery reports
    qDebug() << "YSConnection::sendMessage ";

    QString content;
    for(MessagePartList::const_iterator i = message.begin()+1; i != message.end(); ++i)
        if(i->count(QLatin1String("content-type"))
            && i->value(QLatin1String("content-type")).variant().toString() == QLatin1String("text/plain")
            && i->count(QLatin1String("content")))
        {
            content = i->value(QLatin1String("content")).variant().toString();
            break;
        }

    boost::python::object pMsgId = pythonInterface->call("message_send", jid, content);
    boost::python::extract<QString> get_msgId(pMsgId);
    if(!get_msgId.check()) {
        qDebug() << "message_send did not return a string";
        error->set(TP_QT_ERROR_INVALID_ARGUMENT,"Internal error");
        return "";
    }

    qDebug() << "YSConnection::sendMessage with id " << get_msgId();
    return get_msgId();
}

/*                                             YowsupInterface                                      */
void YSConnection::on_yowsup_auth_success(QString phonenumber) {
    qDebug() << "YSConnection::auth_success " << phonenumber;
    qDebug() << "Thread id auth_success " << QThread::currentThreadId();
    setStatus(ConnectionStatusConnected, ConnectionStatusReasonRequested);
    pythonInterface->runReaderThread();
}

void YSConnection::on_yowsup_auth_fail(QString mobilenumber, QString reason) {
    qDebug() << "YSConnection::auth_fail for " << mobilenumber << " reason: " << reason;
    setStatus(ConnectionStatusDisconnected, ConnectionStatusReasonAuthenticationFailed);
}

void YSConnection::on_yowsup_status_dirty() {
    qDebug() << "YSConnection::status_dirty";
}

void YSConnection::on_yowsup_message_received(QString msgId, QString jid, QString content, uint timestamp,
                                    bool wantsReceipt, QString pushName) {
    qDebug() << "YSConnection::message_received " << msgId <<  " " << jid << " " << content;
    qDebug() << "Thread id message_received " << QThread::currentThreadId();

    //We cannot wait until messageAcknowledged(), because that indicates that the user saw the message,
    //not that it was received. Yowsup won't tolerate such long delays.
    if(wantsReceipt)
        pythonInterface->call("message_ack", jid, msgId );

    uint handle = ensureContact(jid);
    Tp::DBusError error;
    BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT,HandleTypeContact, handle, &error);

    BaseChannelTextTypePtr textChannel = BaseChannelTextTypePtr::dynamicCast(channel->interface(TP_QT_IFACE_CHANNEL_TYPE_TEXT));

#if 0
    uint telepathyMsgId = ++lastMessageId;
    pendingMessages[QString("%1").arg(telepathyMsgId)] = make_tuple(jid,msgId,wantsReceipt);
#endif
    if(!textChannel) {
        qDebug() << "Error, channel is not a textChannel??";
        return;
    }
    MessagePartList partList;
    MessagePart header, body;
    header["message-token"]         = QDBusVariant(msgId);
    header["message-received"]      = QDBusVariant(timestamp);
    header["message-sender"]        = QDBusVariant(handle);
    header["message-sender-id"]     = QDBusVariant(jid);
    header["sender-nickname"]       = QDBusVariant(pushName);
    header["message-type"]          = QDBusVariant(ChannelTextMessageTypeNormal);
    body["content-type"]            = QDBusVariant("text/plain");
    body["content"]                 = QDBusVariant(content);
    partList << header << body;
    textChannel->addReceivedMessage(partList);
}

void YSConnection::on_yowsup_disconnected(QString reason) {
    qDebug() << "YSConnection::on_yowsup_disconnected: reason=" << reason;
    setStatus(ConnectionStatusDisconnected, ConnectionStatusReasonNetworkError);
}

void YSConnection::on_yowsup_receipt_messageSent(QString jid,QString msgId) {

    uint handle = ensureContact(jid);
    BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT, HandleTypeContact, handle, 0);
    BaseChannelMessagesInterfacePtr messagesIface = BaseChannelMessagesInterfacePtr::dynamicCast(
                                                    channel->interface(TP_QT_IFACE_CHANNEL_INTERFACE_MESSAGES));
    if(!messagesIface)
        return;

    MessagePartList partList;
    MessagePart header;
    header["message-sender"]        = QDBusVariant(handle);
    header["message-sender-id"]     = QDBusVariant(jid);
    header["message-type"]          = QDBusVariant(ChannelTextMessageTypeDeliveryReport);
    header["delivery-status"]       = QDBusVariant(DeliveryStatusAccepted);
    header["delivery-token"]        = QDBusVariant(msgId);
    partList << header;

    messagesIface->messageReceived(partList);
}

void YSConnection::on_yowsup_receipt_messageDelivered(QString jid, QString msgId) {
    //TODO: dispatch to telepathy
    pythonInterface->call("delivered_ack", jid, msgId);

    uint handle = ensureContact(jid);
    BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT, HandleTypeContact, handle, 0);
    BaseChannelMessagesInterfacePtr messagesIface = BaseChannelMessagesInterfacePtr::dynamicCast(
                                                    channel->interface(TP_QT_IFACE_CHANNEL_INTERFACE_MESSAGES));
    if(!messagesIface)
        return;

    MessagePartList partList;
    MessagePart header;
    header["message-sender"]        = QDBusVariant(handle);
    header["message-sender-id"]     = QDBusVariant(jid);
    header["message-type"]          = QDBusVariant(ChannelTextMessageTypeDeliveryReport);
    header["delivery-status"]       = QDBusVariant(DeliveryStatusDelivered);
    header["delivery-token"]        = QDBusVariant(msgId);
    partList << header;

    messagesIface->messageReceived(partList);
}

/* Convenience */
uint YSConnection::ensureContact(QString jid) {
    auto i = contacts.right.find(jid);
    if( i == contacts.right.end() )
        return addContact(jid);
    else
        return i->second;
}

uint YSConnection::addContact(QString jid) {
    uint id = 0;
    for (auto i : contacts.left)
        if( id < i.first )
            id = i.first;
    id++; // id = maximum + 1, never 0

    contacts.left.insert( make_pair(id, jid) );
    return id;
}
