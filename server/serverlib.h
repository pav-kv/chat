#pragma once

#include "../common.h"
#include "../protocol.h"

#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <arpa/inet.h>
#include <memory.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

class TClient {
public:
    TClient(const string& login, const string& password)
        : Login(login)
        , Password(password)
        , Online(false)
    {
    }

    const string& GetLogin() const {
        return Login;
    }

    bool CheckPassword(const string& password) {
        return password == Password;
    }

    void SetStatus(bool online) {
        Online = online;
    }

    bool IsOnline() const {
        return Online;
    }

    unique_lock<mutex> GetLock() {
        return unique_lock<mutex>(Mutex);
    }

    bool PushMessage(const TMessageText& msg) {
        MessageQueue.push_back(msg);
        return true;
    }

    deque<TMessageText>& GetMessages() {
        return MessageQueue;
    }

private:
    mutex Mutex;

    const string Login;
    const string Password;
    bool Online;
    deque<TMessageText> MessageQueue;
};

class TServer {
public:
    TServer(int port = DEFAULT_PORT)
        : Port(port)
    {
    }

    bool Start() {
        int socketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketFD == -1) {
            cerr << "Could not create socket.\n";
            return false;
        }
        int flag = 1;
        setsockopt(socketFD, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        sockaddr_in sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sin_family = PF_INET;
        sockAddr.sin_port = htons(Port);
        sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(socketFD, (sockaddr*)&sockAddr, sizeof(sockAddr)) == -1) {
            cerr << "Could not bind to port.\n";
            close(socketFD);
            return false;
        }

        if (listen(socketFD, 5) == -1) {
            cerr << "Error on listening port.\n";
            close(socketFD);
            return false;
        }
        cerr << "Listening port " << Port << ".\n";

        while (true) {
            sockaddr_in clientAddr;
            socklen_t len = sizeof(clientAddr);
            int connectionFD = accept(socketFD, (sockaddr*)&clientAddr, &len);

            if (connectionFD == -1) {
                cerr << "Error on accepting connection.\n";
                close(socketFD);
                return true;
            }
            cerr << "Accepted connection with " << inet_ntoa(clientAddr.sin_addr) << ".\n";

            new thread(&TServer::RunConnection, this, connectionFD); // FIXME: delete thread
        }

        close(socketFD);
        return true;
    }

private:
    void RunConnection(int connectionFD) {
        auto_ptr<TMessage> msg = PopMessage(connectionFD);
        TMessageHeader* hdr = msg.get() ? &msg->Header : NULL;

        if (!hdr) {
            cerr << "Connection closed unexpectedly.\n";
            close(connectionFD);
            return;
        }
        if (hdr->Tag != MSG_SIGN_IN) {
            cerr << "Protocol violation.\n";
            shutdown(connectionFD, SHUT_RDWR);
            close(connectionFD);
            return;
        }

        bool deny = false;
        TMessageSignIn& signIn = *dynamic_cast<TMessageSignIn*>(msg.get());
        string login = signIn.Login;
        unique_lock<mutex> lock(Mutex);
        TClient*& client = Clients[login];
        lock.unlock();
        if (client) {
            deny = client->IsOnline() || !client->CheckPassword(signIn.Password);
        } else {
            client = new TClient(login, signIn.Password);
        }
        if (deny) {
            TMessage denyMsg(MSG_DENY);
            denyMsg.Write(connectionFD);
            cerr << "Denied user.\n";
            shutdown(connectionFD, SHUT_RDWR);
            close(connectionFD);
            return;
        } else {
            TMessage ack(MSG_ACK);
            ack.Write(connectionFD);
            client->SetStatus(true);
            cerr << "User " << login << " is now online.\n";
        }

        bool signedOut = false;
        while (!signedOut) {
            msg = PopMessage(connectionFD);
            hdr = msg.get() ? &msg->Header : NULL;
            if (!hdr) {
                cerr << "Connection with user " << login << " closed unexpectedly.\n";
                break;
            }

            switch (hdr->Tag) {
            case MSG_SIGN_OUT:
                signedOut = true;
                cerr << "User " << login << " signed out.\n";
                break;

            case MSG_GETTEXT:
                {
                    unique_lock<mutex> clientLock = client->GetLock();
                    deque<TMessageText> messages = client->GetMessages();
                    client->GetMessages().clear();
                    clientLock.unlock();
                    for (size_t i = 0; i < messages.size(); ++i)
                        messages[i].Write(connectionFD);
                    TMessage msgTextFin(MSG_TEXTFIN);
                    msgTextFin.Write(connectionFD);
                    cerr << "User " << login << " requested text and got " << messages.size() << " messages.\n";
                    break;
                }

            case MSG_TEXT:
                {
                    const TMessageText& msgText = *dynamic_cast<TMessageText*>(msg.get());
                    lock.lock();
                    unordered_map<string, TClient*>::iterator it = Clients.find(msgText.Reciever);
                    lock.unlock();
                    if (it == Clients.end()) {
                        cerr << "User " << login << " tried to send text to " << msgText.Reciever << " (not registered).\n";
                        break;
                    }
                    cerr << "User " << login << " sent text as " << msgText.Sender<< " to " << msgText.Reciever << ".\n";
                    TClient* reciever = it->second;
                    unique_lock<mutex> recieverLock = reciever->GetLock();
                    reciever->PushMessage(msgText);
                    break;
                }

            case MSG_LIST:
                {
                    cerr << "User " << login << " requested chat list.\n";
                    TMessageList& msgList = *dynamic_cast<TMessageList*>(msg.get());
                    lock.lock();
                    for (unordered_map<string, TClient*>::const_iterator it = Clients.begin(); it != Clients.end(); ++it)
                        msgList.Users.push_back(it->first);
                    lock.unlock();
                    msgList.Write(connectionFD);
                    break;
                }

            default:
                cerr << "User " << login << " sent unexpected message: tag = " << hdr->Tag << ",\n";
                break;
            }
        }

        shutdown(connectionFD, SHUT_RDWR);
        close(connectionFD);

        client->SetStatus(false);
        cerr << "User " << login << " is now offline.\n";
    }

private:
    mutex Mutex;

    int Port;
    unordered_map<string, TClient*> Clients;
};

