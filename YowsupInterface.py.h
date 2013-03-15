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
from Yowsup.Contacts.contacts import WAContactsSyncRequest
from Yowsup.Registration.v2.coderequest import WACodeRequest as WACodeRequestV2
from Yowsup.Registration.v2.regrequest import WARegRequest as WARegRequestV2
from Yowsup.Common.debugger import Debugger
import signal
#Don't swallow SIGINT
signal.signal(signal.SIGINT, signal.SIG_DFL)
Debugger.enabled = False

def resultToString(result):
    out = []
    for k, v in result.items():
        if v is None:
            continue
        if type(v) is str:
            v = v.encode('utf-8')
        out.append("%s: %s" %(k, v))

    return "\n".join(out)

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

def syncContact(login, password, contact):
    wsync = WAContactsSyncRequest(login, password, (contact,))
    result = wsync.send()
    print resultToString(result)
    ret = result[u'c'][0][u'w']
    print "Returning ", ret
    return ret

def code_request(self, countryCode, phoneNumber, identity, useText):
    print "code_request entered ", countryCode, phoneNumber, identity, "sms" if(useText) else "voice"
    we = WACodeRequestV2(countryCode, phoneNumber, identity, "sms" if(useText) else "voice")
    #result = we.send()
    #print resultToString(result)
    #return result['status']
    return u'123'

def code_register(self, countryCode, phoneNumber, identity, code):
    we = WARegRequestV2(countryCode, phoneNumber, code, identity)
    #result = we.send()
    #print resultToString(result)
    #return result['pw'] if result['pw'] else ''
    return u'ok'
)";
