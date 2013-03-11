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
#include "protocol.h"

using namespace Tp;
using namespace std;
namespace python = boost::python;

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
    if(parameters.contains("account"))
        mPhoneNumber = parameters["account"].toString();
    if(parameters.contains("password"))
        mPassword = QByteArray::fromBase64( parameters["password"].toString().toLatin1() );

    selfHandle = addContact(mPhoneNumber + "@s.whatsapp.net");
    assert(selfHandle == 1);

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
    contactsIface->setContactAttributeInterfaces(QStringList()
                                                 << QLatin1String("org.freedesktop.Telepathy.Connection")
                                                 << QLatin1String("org.freedesktop.Telepathy.Connection.Interface.ContactList")
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE);
    plugInterface(AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    simplePresenceIface = BaseConnectionSimplePresenceInterface::create();
    simplePresenceIface->setSetPresenceCallback(Tp::memFun(this,&YSConnection::setPresence));
    plugInterface(AbstractConnectionInterfacePtr::dynamicCast(simplePresenceIface));

    contactListIface = BaseConnectionContactListInterface::create();
    contactListIface->setGetContactListAttributesCallback(Tp::memFun(this,&YSConnection::getContactListAttributes));
    contactListIface->setRequestSubscriptionCallback(Tp::memFun(this,&YSConnection::requestSubscription));
    plugInterface(AbstractConnectionInterfacePtr::dynamicCast(contactListIface));


    addressingIface = BaseConnectionAddressingInterface::create();
    addressingIface->setGetContactsByVCardFieldCallback( Tp::memFun(this,&YSConnection::getContactsByVCardField) );
    addressingIface->setGetContactsByURICallback( Tp::memFun(this,&YSConnection::getContactsByURI) );
    plugInterface(AbstractConnectionInterfacePtr::dynamicCast(addressingIface));

    pythonInterface = new PythonInterface(&yowsupInterface);

    yowsupInterface.setObjectName("yowsup");
    QMetaObject::connectSlotsByName(this);
}

YSConnection::~YSConnection() {
    delete pythonInterface;
}

QString YSConnection::uniqueName() const {
    //May not start with a digit
    return "u" + mPhoneNumber;
}

uint YSConnection::getSelfHandle(Tp::DBusError *error)
{
    return selfHandle;
}

void YSConnection::connect(Tp::DBusError *error) {
    qDebug() << "Thread id connect " << QThread::currentThreadId();
    qDebug() << "YSConnection::connect";
    setStatus(ConnectionStatusConnecting, ConnectionStatusReasonRequested);

#ifdef USE_CAPTCHA_FOR_REGISTRATION
    /* Clear pointer from a previous registration */
    captchaIface.reset();
#endif

    if(mPassword.length() > 0 ) {
        pythonInterface->call("auth_login", mPhoneNumber, mPassword );
    } else {
#ifdef USE_CAPTCHA_FOR_REGISTRATION
        qDebug() << "Opening registration";
        if( mUID.length() == 0 ) {
            qDebug() << "Generating new mUID";
            mUID = generateUID();
            //new SetAccountProperties();
        }
        //Registration
        BaseChannelPtr baseChannel = BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION,
                                                         0, HandleTypeNone, "", false, 0, "");

        BaseChannelServerAuthenticationTypePtr authType
                = BaseChannelServerAuthenticationType::create(TP_QT_IFACE_CHANNEL_INTERFACE_CAPTCHA_AUTHENTICATION);
        baseChannel->plugInterface(AbstractChannelInterfacePtr::dynamicCast(authType));

        captchaIface = BaseChannelCaptchaAuthenticationInterface::create(true);
        captchaIface->setGetCaptchaDataCallback( Tp::memFun(this, &YSConnection::getCaptchaData));
        captchaIface->setGetCaptchasCallback( Tp::memFun(this, &YSConnection::getCaptchas));
        captchaIface->setAnswerCaptchasCallback( Tp::memFun(this, &YSConnection::answerCaptchas));
        captchaIface->setCancelCaptchaCallback( Tp::memFun(this, &YSConnection::cancelCaptcha));
        baseChannel->plugInterface(AbstractChannelInterfacePtr::dynamicCast(captchaIface));

        baseChannel->registerObject(error);
        if(!error->isValid())
            addChannel( baseChannel );
#else
        setStatus(ConnectionStatusDisconnected, ConnectionStatusReasonAuthenticationFailed);
#endif
    }
}

#ifdef USE_CAPTCHA_FOR_REGISTRATION
/* Authentication */
void YSConnection::getCaptchas(Tp::CaptchaInfoList& captchaInfoList, uint& numberRequired,
                               QString& language, Tp::DBusError* error)
{
    CaptchaInfo captchaInfo;
    captchaInfo.ID = 0;
    captchaInfo.type = "qa";
    captchaInfo.label = "";
    captchaInfo.flags = CaptchaFlagRequired;
    captchaInfo.availableMIMETypes = QStringList() << "text/plain";
    captchaInfoList << captchaInfo;

    numberRequired = 1;
    language = "en_US";
}

QByteArray YSConnection::getCaptchaData(uint ID, const QString& mimeType, Tp::DBusError* error)
{
    qDebug() << "YSConnection::getCaptchaData " << ID << " " << mimeType;
    QString countryCode;
    QString phonenumber;
    bool useText = true;
    GILStateHolder gstate;
    python::object ret = pythonInterface->call_intern("code_request", countryCode, phonenumber, mUID, useText);
    python::extract<QString> getStr(ret);
    if(!getStr.check()) {
        qDebug() << "YSConnection::getCaptchaData: return is not a string";
        error->set(TP_QT_ERROR_NOT_AVAILABLE,"Could not request code");
        return QByteArray();
    }
    if(getStr() != "send") {
        qDebug() << "YSConnection::getCaptchaData: status is not send: " << getStr();
        error->set(TP_QT_ERROR_NOT_AVAILABLE,"Could not request code");
        return QByteArray();
    }
    return QByteArray("Please enter the code you received via text");
}

void YSConnection::answerCaptchas(const Tp::CaptchaAnswers& answers, Tp::DBusError* error)
{
    qDebug() << "YSConnection::answerCaptchas " << answers;
    if(!answers.contains(0))
        return;
    QString code = answers[0];
    if(code.length() == 7) //remove the hyphon in the middle
        code = code.left(3) + code.right(3);

    if(!QRegExp("\\d{6}").exactMatch(code)) {
        qDebug() << "YSConnection::answerCaptchas: code does not have right form " << code;
    }

    QString countryCode;
    QString phonenumber;
    GILStateHolder gstate;
    python::object ret = pythonInterface->call_intern("code_register", countryCode, phonenumber, mUID, code);
    python::extract<QString> getStr(ret);
    if(!getStr.check()) {
        qDebug() << "YSConnection::answerCaptchas: return is not a string";
        error->set(TP_QT_ERROR_NOT_AVAILABLE,"Could not request code");
        return;
    }
    if(getStr() != "ok") {
        qDebug() << "YSConnection::answerCaptchas: status is not ok: " << getStr();
        captchaIface->setCaptchaError(TP_QT_ERROR_AUTHENTICATION_FAILED);
        captchaIface->setCaptchaStatus(CaptchaStatusTryAgain);
        return;
    }
    captchaIface->setCaptchaStatus(CaptchaStatusSucceeded);
    //FIXME: do something with the password

    //connect()
}

void YSConnection::cancelCaptcha(uint reason, const QString& debugMessage, Tp::DBusError* error)
{
    qDebug() << "YSConnection::cancelCaptcha " << reason << " " << debugMessage;
    captchaIface->setCaptchaStatus(CaptchaStatusFailed);
    setStatus(ConnectionStatusDisconnected, ConnectionStatusReasonAuthenticationFailed);
}
#endif

//This is basically a no-op for the current spec
bool YSConnection::holdHandles(uint /*handleType*/, const Tp::UIntList& /*handles*/, Tp::DBusError* /*error*/)
{
    //qDebug() << "YSConnection::holdHandles " << handles;
    return true;
}

QStringList YSConnection::inspectHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error)
{
    qDebug() << "YSConnection::inspectHandles " << handles;
    QStringList ret;

    if( handleType == Tp::HandleTypeContact ) {
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

    } else if(handleType == HandleTypeNone) {
        for( uint handle : handles ) {
            if(handle != 0) {
                error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
                return QStringList();
            }
            ret.append( "" );
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
        //org.freedesktop.Telepathy.Connection.Interface.SimplePresence/presence
        attributes["org.freedesktop.Telepathy.Connection/contact-id"] = i->second;
        if(handle != selfHandle) {
            attributes["org.freedesktop.Telepathy.Connection.Interface.ContactList/subscribe"] = SubscriptionStateNo;
            attributes["org.freedesktop.Telepathy.Connection.Interface.ContactList/publish"] = SubscriptionStateNo;
        }
        ret[handle] = attributes;
    }
    qDebug() << "YSConnection::getContactAttributes " << handles
             << " = " << ret;
    return ret;

}

Tp::ContactAttributesMap YSConnection::getContactListAttributes(const QStringList& interfaces,
                                                                bool /*hold*/, Tp::DBusError* /*error*/)
{
    Tp::ContactAttributesMap contactAttributeMap;
    for( auto i : contacts.left )
    {
        if(i.first == selfHandle)
            continue;
        QVariantMap attributes;
        //org.freedesktop.Telepathy.Connection.Interface.ContactList/subscribe
        attributes["org.freedesktop.Telepathy.Connection/contact-id"] = i.second;
        attributes["org.freedesktop.Telepathy.Connection.Interface.ContactList/subscribe"] = SubscriptionStateNo;
        attributes["org.freedesktop.Telepathy.Connection.Interface.ContactList/publish"] = SubscriptionStateNo;
        contactAttributeMap[i.first] = attributes;
    }
    qDebug() << "YSConnection::getContactListAttributesCallback " << interfaces
             << " = " << contactAttributeMap;
    return contactAttributeMap;
}

void YSConnection::requestSubscription(const Tp::UIntList& contacts,
                                       const QString& message, Tp::DBusError* error)
{
    qDebug() << "YSConnection::requestSubscription " << contacts;
    for( uint handle : contacts ) {
        QString jid = getContactByHandle(handle);
        if(jid.length() == 0) {
            error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
            return;
        }
        pythonInterface->call("presence_subscribe", jid);
        setSubscriptionState(jid, handle, SubscriptionStateYes);
    }
}

Tp::UIntList YSConnection::requestHandles(uint handleType, const QStringList& identifiers, Tp::DBusError* error)
{
    Tp::UIntList ret;
    if( handleType == Tp::HandleTypeContact ) {
        for( const QString& identifier : identifiers ) {
            auto i = contacts.right.find(identifier);
            if( i == contacts.right.end()) {
                if(isValidContact(identifier)) {
                    ret.push_back(addContact(identifier));
                } else {
                    qDebug() << "YSConnection::requestHandles: id invalid " << identifier;
                    error->set(TP_QT_ERROR_INVALID_HANDLE,"Handle not found");
                    return Tp::UIntList();
                }
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
    Q_ASSERT(error);
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

    GILStateHolder gstate;
    python::object pMsgId = pythonInterface->call("message_send", jid, content);
    python::extract<QString> get_msgId(pMsgId);
    if(!get_msgId.check()) {
        qDebug() << "message_send did not return a string";
        error->set(TP_QT_ERROR_INVALID_ARGUMENT,"Internal error");
        return "";
    }

    qDebug() << "YSConnection::sendMessage with id " << get_msgId();
    return get_msgId();
}

uint YSConnection::setPresence(const QString& status, const QString& message, Tp::DBusError* error)
{
    qDebug() << "YSConncetion::setPresence " << status << " " << message;
    return selfHandle;
}

void YSConnection::getContactsByVCardField(const QString& field, const QStringList& addresses,const QStringList& interfaces,
                             Tp::AddressingNormalizationMap& addressingNormalizationMap,
                             Tp::ContactAttributesMap& contactAttributesMap, Tp::DBusError* error) {
    qDebug() << "YSConnection::getContactsByVCardField " << field << " " << addresses;

}

void YSConnection::getContactsByURI(const QStringList& URIs, const QStringList& interfaces,
                     Tp::AddressingNormalizationMap& addressingNormalizationMap,
                     Tp::ContactAttributesMap& contactAttributesMap, Tp::DBusError* error) {
    qDebug() << "YSConnection::getContactsByURI " << URIs;
}

/*                                             YowsupInterface                                      */
void YSConnection::on_yowsup_auth_success(QString phonenumber) {
    qDebug() << "YSConnection::auth_success " << phonenumber;

    //FIXME: this crashes in libdbus
    simplePresenceIface->setStatuses(Protocol::getSimpleStatusSpecMap());
    simplePresenceIface->setMaxmimumStatusMessageLength(20); //FIXME

    /* Set presence */
    SimpleContactPresences presences;
    SimplePresence presence;
    presence.status = "available";
    presence.statusMessage = "";
    presence.type = ConnectionPresenceTypeAvailable;
    presences[selfHandle] = presence;
    simplePresenceIface->setPresences(presences);

    /* Set connection status */
    setStatus(ConnectionStatusConnected, ConnectionStatusReasonRequested);

    /* Set ContactList status */
    contactListIface->setContactListState(ContactListStateSuccess);

    pythonInterface->runReaderThread();
    pythonInterface->call("presence_sendAvailable");
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
    bool yours;
    BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT, HandleTypeContact, handle, yours,
                                           handle,
                                           false, &error);

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
    if(reason == "shutdown") //set in PythonInterface::~PythonInterface()
        setStatus(ConnectionStatusDisconnected, ConnectionStatusReasonRequested);
    else
        setStatus(ConnectionStatusDisconnected, ConnectionStatusReasonNetworkError);
}

void YSConnection::on_yowsup_receipt_messageSent(QString jid,QString msgId) {

    uint handle = ensureContact(jid);
    bool yours;
    Tp::DBusError error;
    BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT, HandleTypeContact, handle,
                                           yours, handle, false,
                                           &error);
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
    bool yours;
    Tp::DBusError error;
    BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT, HandleTypeContact, handle,
                                           yours, handle, false, &error);
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

void YSConnection::on_yowsup_presence_available(QString jid) {
    uint handle = ensureContact(jid);
    if(!handle) {
        qDebug() << "YSConnection::on_yowsup_presence_available: could not create contact " << jid;
        return;
    }
    setPresenceState(handle, "available");
    if(handle != selfHandle)
        setSubscriptionState(jid, handle, SubscriptionStateYes);
}

void YSConnection::on_yowsup_presence_unavailable(QString jid) {
    uint handle = ensureContact(jid);
    if(!handle) {
        qDebug() << "YSConnection::on_yowsup_presence_unavailable: could not create contact " << jid;
        return;
    }
    setPresenceState(handle, "offline");
    if(handle != selfHandle)
        setSubscriptionState(jid, handle, SubscriptionStateYes);
}

/* Convenience */
bool YSConnection::isValidContact(const QString& identifier) {
    if(!isValidId(identifier))
        return false;

    GILStateHolder gstate;
    QString number = "+" + identifier.left(identifier.length()-QLatin1String("@s.whatsapp.net").size());
    python::object ret = pythonInterface->call_intern("syncContact", mPhoneNumber, mPassword, number);
    python::extract<int> getInt(ret);
    if(!getInt.check()) {
        qDebug() << "YSConnection::isValidContact: return value is a not a int";
        return false;
    }
    bool isValid = getInt();
    qDebug() << "YSConnection::isValidContact: " << isValid;
    return isValid;
}

bool YSConnection::isValidId(const QString& jid) {
    static QRegExp validId(R"(^\d+@s\.whatsapp\.net$)");
    return validId.exactMatch(jid);
}

uint YSConnection::ensureContact(QString jid) {
    auto i = contacts.right.find(jid);
    if( i == contacts.right.end() )
        return addContact(jid);
    else
        return i->second;
}

QString YSConnection::getContactByHandle(uint handle) {
    auto i = contacts.left.find(handle);
    if( i == contacts.left.end() )
        return "";
    else
        return i->second;
}

uint YSConnection::addContact(QString jid) {
    uint handle = 0;
    for (auto i : contacts.left)
        if( handle < i.first )
            handle = i.first;
    handle++; // id = maximum + 1, never 0

    contacts.left.insert( make_pair(handle, jid) );

    setPresenceState(handle, "unknown");
    if(handle != 1 /*selfHandle is not set yet */)
        setSubscriptionState(jid, handle, SubscriptionStateUnknown);

    return handle;
}

void YSConnection::setSubscriptionState(const QString& jid, uint handle, uint state) {
    /* Send ContactList change signal */
    if(!contactListIface.isNull()) {
        Tp::ContactSubscriptions change;
        change.publish = SubscriptionStateYes;
        change.publishRequest = "";
        change.subscribe = state;
        Tp::ContactSubscriptionMap changes;
        changes[handle] = change;
        Tp::HandleIdentifierMap identifiers;
        identifiers[handle] = jid;
        Tp::HandleIdentifierMap removals;
        contactListIface->contactsChangedWithID(changes, identifiers, removals);
    }
}

void YSConnection::setPresenceState(uint handle, const QString& status) {
    if(!simplePresenceIface.isNull()) {
        SimpleStatusSpecMap statusSpecMap = Protocol::getSimpleStatusSpecMap();
        auto i = statusSpecMap.find(status);
        if(i == statusSpecMap.end()) {
            qDebug() << "YSConnection::setPresenceState: status not found: " << status;
            return;
        }
        SimplePresence presence;
        presence.status = status;
        presence.statusMessage = ""; //FIXME
        presence.type = i->type;
        SimpleContactPresences presences;
        presences[handle] = presence;
        simplePresenceIface->setPresences(presences);
    }
}

QString YSConnection::generateUID()
{
    QString randomHex;

    for(int i = 0; i < 32; i++) {
    int n = qrand() % 16;
        randomHex.append(QString::number(n,16));
    }
    return randomHex;
}
