#pragma once

#include "../common.h"
#include "../protocol.h"

#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>

using namespace std;

class TClient {
public:
    TClient(const string& host, int port = DEFAULT_PORT)
        : ServerHost(host)
        , ServerPort(port)
    {
    }

    bool Connect() {
        sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = PF_INET;
        serverAddr.sin_port = htons(ServerPort);
        int result = inet_pton(PF_INET, ServerHost.c_str(), &serverAddr.sin_addr);
        if (result < 0) {
            cerr << "Incorrect address type.\n";
            return false;
        } else if (!result) {
            cerr << "Incorrect IP address.\n";
            return false;
        }

        SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (SocketFD == -1) {
            cerr << "Could not create socket.\n";
            return false;
        }
        int flag = 1;
        setsockopt(SocketFD, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

        if (connect(SocketFD, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
            cerr << "Could not connect to server.\n";
            close(SocketFD);
            return false;
        }

        return true;
    }

    bool SignIn(const string& login, const string& password) {
        TMessageSignIn signIn(login, password);
        if (!signIn.Write(SocketFD))
            return false;
        auto_ptr<TMessage> msg = PopMessage(SocketFD);
        if (!msg.get())
            return false;
        if (msg->Header.Tag == MSG_ACK) {
            Login = login;
            cerr << "Signed in as " << login << '\n';
            return true;
        }
        return false;
    }

    void SignOut() {
        TMessage signOut(MSG_SIGN_OUT);
        signOut.Write(SocketFD);
        shutdown(SocketFD, SHUT_RDWR);
        close(SocketFD);
    }

    vector<string> GetUsers() {
        TMessageList list;
        if (!list.Write(SocketFD))
            return list.Users;
        list = *dynamic_cast<TMessageList*>(PopMessage(SocketFD).get());
        return list.Users;
    }

    bool SendText(const string& text, const string& reciever) {
        TMessageText msgText;
        msgText.Sender = Login;
        msgText.Reciever = reciever;
        msgText.Text = text;
        return msgText.Write(SocketFD);
    }

    vector<TMessageText> GetText() {
        TMessage getText(MSG_GETTEXT);
        getText.Write(SocketFD);
        auto_ptr<TMessage> msg(NULL);
        vector<TMessageText> result;
        while ((msg = PopMessage(SocketFD)).get()) {
            if (msg->Header.Tag == MSG_TEXTFIN)
                break;
            result.push_back(*dynamic_cast<TMessageText*>(msg.get()));
        }
        return result;
    }

private:
    string ServerHost;
    int ServerPort;

    int SocketFD;
    string Login;
};

