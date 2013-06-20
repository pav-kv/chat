#!/usr/bin/env python
import socket
from struct import pack, unpack


MSG_UNKNOWN  = (0, "MSG_UNKNOWN")
MSG_SIGN_IN  = (1, "MSG_SIGN_IN")
MSG_SIGN_OUT = (2, "MSG_SIGN_OUT")
MSG_DENY     = (3, "MSG_DENY")
MSG_LIST     = (4, "MSG_LIST")
MSG_TEXT     = (5, "MSG_TEXT")
MSG_ACK      = (6, "MSG_ACK")
MSG_GETTEXT  = (7, "MSG_GETTEXT")
MSG_TEXTFIN  = (8, "MSG_TEXTFIN")

MSG_NAMES = dict([MSG_UNKNOWN, MSG_SIGN_IN, MSG_SIGN_OUT, MSG_DENY, MSG_LIST, MSG_TEXT, MSG_ACK, MSG_GETTEXT, MSG_TEXTFIN])

def read_n_bytes(sock, n):
    r = ""
    while len(r) < n:
        r += sock.recv(n-len(r))
        print len(r), ' vs ', n
    return r

class sscp_msg:

    def __init__(self, ptype, pid = 0):
        self.type = ptype[0]
        self.id   = pid

    def parseHead(self, head):
        self.type, self.id, self.datasize = unpack("3i", head)

    def headerToBin(self, dataSize):
        return pack("3i", self.type, self.id, dataSize)

    def textToBin(self, text):
        return pack('i', len(text)) + text

    def dataToBin(self):
        return ""

    def toBin(self):
        data = self.dataToBin()
        head = self.headerToBin(len(data))
        return head + data

    def parseResults(self, result):
        return True

class sscp_msg_list(sscp_msg):

    def __init__(self, pid = 0):
        sscp_msg.__init__(self, MSG_LIST, pid)

    def dataToBin(self):
        return pack('i', 0)

    def parseResults(self, result):
        usrCount = unpack('i', result[:4])[0]
        result = result[4:]
        usrs = []
        for i in xrange(usrCount):
            usrLen = unpack('i', result[:4])[0]
            usrs.append(result[4:4+usrLen])
            result = result[4+usrLen:]
        return usrs

class sscp_msg_sign_in(sscp_msg):

    def __init__(self, login, pswrd, pid = 0):
        sscp_msg.__init__(self, MSG_SIGN_IN, pid)
        self.login = login
        self.password = pswrd

    def dataToBin(self):
        return self.textToBin(self.login)+self.textToBin(self.password)

class sscp_msg_textout(sscp_msg):

    def __init__(self, sender, reciever, text, pid = 0):
        sscp_msg.__init__(self, MSG_TEXT, pid)
        self.sname = sender
        self.rname = reciever
        self.text  = text

    def dataToBin(self):
        return self.textToBin(self.sname) + self.textToBin(self.rname) + self.textToBin(self.text)
        

class sscp_client:

    def __init__(self, config):
        soc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        soc.connect((config["host"], config["port"]))
        self.chanel = soc

    def signin(self, login, password):
        raw_input("== START ==")
        self.login = login
        msg = sscp_msg_sign_in(login, password)
        self.chanel.sendall(msg.toBin())
        msg.parseHead(read_n_bytes(self.chanel, 12))
        print "[SignIn result]:", MSG_NAMES[msg.type]

    def sendText(self, to, text):
        print "[Msg SENT]"
        self.chanel.sendall(sscp_msg_textout(self.login, to, text).toBin())

    def getUsersList(self):
        msg = sscp_msg_list()
        self.chanel.sendall(msg.toBin())
        msg.parseHead(read_n_bytes(self.chanel, 12))
        resp = read_n_bytes(self.chanel, msg.datasize)
        usrs = msg.parseResults(resp)
        print "\n[Users]:\n" + "\n".join(usrs)+ "\n-----------"

    def signout(self):
        print "[Sign out]"
        self.chanel.sendall(sscp_msg(MSG_SIGN_OUT).toBin())

if __name__ == '__main__':
    config = { 
                #"host"  : "localhost",
                "host"  : "46.182.50.193",
                "port"  : 43227
             }
    cli = sscp_client(config)
    cli.signin("admin", "admin")
    cli.sendText("admin", "TEST MESSAGE!")
    cli.getUsersList()
    cli.signout()