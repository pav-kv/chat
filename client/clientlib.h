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
        unique_ptr<TMessage> msg(PopMessage(SocketFD).release());
        if (!msg.get())
            return false;
        if (msg->Header.Tag == MSG_ACK) {
            Login = login;
            Pass = password;
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

    void Reconnect() {
        bool reconnect = true;
        while (reconnect) {
            cerr << "Server died. Reconnect in 3 seconds.\n";
            sleep(3);
            SignOut();
            Connect();
            reconnect = !SignIn(Login, Pass);
        }
    }

    vector<string> GetUsers() {
        TMessageList list;
        while (!list.Write(SocketFD))
            Reconnect();
        list = *dynamic_cast<TMessageList*>(PopMessage(SocketFD).get());
        return list.Users;
    }

    void SendText(const string& text, const string& reciever) {
        TMessageText msgText;
        msgText.Sender = Login;
        msgText.Reciever = reciever;
        msgText.Text = text;
        while (!msgText.Write(SocketFD))
            Reconnect();
    }

    vector<TMessageText> GetText() {
        TMessage getText(MSG_GETTEXT);
        while (!getText.Write(SocketFD))
            Reconnect();
        unique_ptr<TMessage> msg;
        vector<TMessageText> result;
        bool finished = false;
        while (true) {
            msg.reset(PopMessage(SocketFD).release());
            if (msg.get() == NULL) {
                cerr << "PopMessage() get NULL.\n";
                break;
            }
            if (msg->Header.Tag == MSG_TEXTFIN) {
                finished = true;
                break;
            }
            result.push_back(*dynamic_cast<TMessageText*>(msg.get()));
        }
        return finished ? result : GetText();
    }

private:
    string ServerHost;
    int ServerPort;

    int SocketFD;
    string Login;
    string Pass;
};

