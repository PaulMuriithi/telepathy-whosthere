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

const char* YowsupInterfacePy =
R"(
from Yowsup.connectionmanager import YowsupConnectionManager

def createCallback(handler, sig):
    return lambda *args: getattr(handler,sig)(*args)

def init(handler):
    #!don't use any methods on handler before returning!
    connectionManager = YowsupConnectionManager()
    connectionManager.setAutoPong(True)
    signalsInterface = connectionManager.getSignalsInterface()
    for sig in signalsInterface.signals:
        signalsInterface.registerListener(sig, createCallback(handler, sig))
    return connectionManager

def call(connectionManager,methodName,*args):
    return connectionManager.getMethodsInterface().call(methodName, args)

def onSignal(i, *args):
    print 'YI: Got signal ' + i
    getattr(Emb, i)(*args)

def runThread(connectionManager):
    print 'In runThread'
    connectionManager.startReader()
    connectionManager.readerThread.join()
)";
