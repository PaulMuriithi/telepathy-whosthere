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

#include "protocol.h"
#include "connection.h"

#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>

using namespace Tp;

SimpleStatusSpecMap Protocol::getSimpleStatusSpecMap()
{
    //Presence
    SimpleStatusSpec spAvailable;
    spAvailable.type = ConnectionPresenceTypeAvailable;
    spAvailable.maySetOnSelf = false;
    spAvailable.canHaveMessage = true;

    SimpleStatusSpec spOffline;
    spOffline.type = ConnectionPresenceTypeOffline;
    spOffline.maySetOnSelf = false;
    spOffline.canHaveMessage = false;

    SimpleStatusSpec spUnknown;
    spUnknown.type = ConnectionPresenceTypeUnknown;
    spUnknown.maySetOnSelf = false;
    spUnknown.canHaveMessage = false;

    SimpleStatusSpecMap specs;
    specs.insert(QLatin1String("available"), spAvailable);
    specs.insert(QLatin1String("offline"), spOffline);
    specs.insert(QLatin1String("unknown"), spUnknown);
    return specs;
}

Protocol::Protocol(const QDBusConnection &dbusConnection, const QString &name)
    : BaseProtocol(dbusConnection, name)
{
    setParameters(ProtocolParameterList()
        << ProtocolParameter(QLatin1String("account"),
                             QLatin1String("s"), ConnMgrParamFlagRequired)
        << ProtocolParameter(QLatin1String("password"),
                             QLatin1String("s"), ConnMgrParamFlagRequired | ConnMgrParamFlagSecret)
        /*<< ProtocolParameter(QLatin1String("uid"),
                             QLatin1String("s"), ConnMgrParamFlagRegister)*/);

    //FIXME: add RoomList
    setRequestableChannelClasses(
            RequestableChannelClassSpecList() << RequestableChannelClassSpec::textChat()
                                              << RequestableChannelClassSpec::textChatroom());

    setEnglishName(QLatin1String("WhatsApp"));
    setIconName(QLatin1String("whosthere"));
    setVCardField(QLatin1String("tel"));
    //FIXME: setConnectionInterfaces(...);

    // callbacks
    setCreateConnectionCallback(memFun(this, &Protocol::createConnection));
    setIdentifyAccountCallback(memFun(this, &Protocol::identifyAccount));
    setNormalizeContactCallback(memFun(this, &Protocol::normalizeContact));

    // Adressing
    addrIface = BaseProtocolAddressingInterface::create();
    addrIface->setAddressableVCardFields(QStringList() << QLatin1String("tel"));
    addrIface->setAddressableUriSchemes(QStringList() << QLatin1String("tel"));
    addrIface->setNormalizeVCardAddressCallback(memFun(this, &Protocol::normalizeVCardAddress));
    addrIface->setNormalizeContactUriCallback(memFun(this, &Protocol::normalizeContactUri));
    plugInterface(AbstractProtocolInterfacePtr::dynamicCast(addrIface));

    // Avatars
    avatarsIface = BaseProtocolAvatarsInterface::create();
    avatarsIface->setAvatarDetails(AvatarSpec(QStringList() << QLatin1String("image/png"),
                16, 64, 32, 16, 64, 32, 1024));
    plugInterface(AbstractProtocolInterfacePtr::dynamicCast(avatarsIface));

    presenceIface = BaseProtocolPresenceInterface::create();
        presenceIface->setStatuses(PresenceSpecList(getSimpleStatusSpecMap()));
    plugInterface(AbstractProtocolInterfacePtr::dynamicCast(presenceIface));
}

Protocol::~Protocol()
{
}

BaseConnectionPtr Protocol::createConnection(const QVariantMap &parameters, Tp::DBusError *error) {

    if(!parameters.contains("account")) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("account is missing"));
        return BaseConnectionPtr();
    } else {
        return BaseConnection::create<YSConnection>( "whosthere", name().toLatin1(), parameters);
    }
}

QString Protocol::identifyAccount(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << "Protocol::identifyAccount " << parameters;
    error->set(QLatin1String("IdentifyAccount.Error.Test"), QLatin1String(""));
    return QString();
}

QString Protocol::normalizeContact(const QString &contactId, Tp::DBusError *error)
{
    qDebug() << "Protocol::normalizeContact " << contactId;
    error->set(QLatin1String("NormalizeContact.Error.Test"), QLatin1String(""));
    return QString();
}

QString Protocol::normalizeVCardAddress(const QString &vcardField, const QString vcardAddress,
        Tp::DBusError *error)
{
    qDebug() << "Protocol::normalizeVCardAddress " << vcardField;
    error->set(QLatin1String("NormalizeVCardAddress.Error.Test"), QLatin1String(""));
    return QString();
}

QString Protocol::normalizeContactUri(const QString &uri, Tp::DBusError *error)
{
    qDebug() << "Protocol::normalizeContactUri " << uri;
    error->set(QLatin1String("NormalizeContactUri.Error.Test"), QLatin1String(""));
    return QString();
}
