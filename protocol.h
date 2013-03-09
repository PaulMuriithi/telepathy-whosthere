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

#include <TelepathyQt/BaseProtocol>

class Protocol : public Tp::BaseProtocol
{
    Q_OBJECT
    Q_DISABLE_COPY(Protocol)

public:
    Protocol(const QDBusConnection &dbusConnection, const QString &name);
    virtual ~Protocol();

    static Tp::SimpleStatusSpecMap getSimpleStatusSpecMap();
private:
    Tp::BaseConnectionPtr createConnection(const QVariantMap &parameters, Tp::DBusError *error);
    QString identifyAccount(const QVariantMap &parameters, Tp::DBusError *error);
    QString normalizeContact(const QString &contactId, Tp::DBusError *error);

    // Proto.I.Addressing
    QString normalizeVCardAddress(const QString &vCardField, const QString vCardAddress,
            Tp::DBusError *error);
    QString normalizeContactUri(const QString &uri, Tp::DBusError *error);

    Tp::BaseProtocolAddressingInterfacePtr addrIface;
    Tp::BaseProtocolAvatarsInterfacePtr avatarsIface;
    Tp::BaseProtocolPresenceInterfacePtr presenceIface;
};
