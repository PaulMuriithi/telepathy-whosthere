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

#include <QCoreApplication>
#include <QDebug>

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/Constants>
#include <TelepathyQt/Debug>
#include <TelepathyQt/Types>


#include "protocol.h"
#include "pythoninterface.h"

using namespace Tp;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    
    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    BaseProtocolPtr proto = BaseProtocol::create<Protocol>(
            QDBusConnection::sessionBus(),
            QLatin1String("whatsapp"));
    BaseConnectionManagerPtr cm = BaseConnectionManager::create(
            QDBusConnection::sessionBus(), QLatin1String("whosthere"));
    cm->addProtocol(proto);
    cm->registerObject();

    PythonInterface::initPython();
    return a.exec();
}
