#include "clientlib.h"

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

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    string ip;
    int port = 0;
    cout << "Server IP and port: ";
    cin >> ip >> port;

    TClient client(ip, port);
    client.Connect();

    string login, pass;
    cout << "Login: ";
    cin >> login;
    cout << "Password: ";
    cin >> pass;
    client.SignIn(login, pass);

    string reciever("server");
    while (true) {
        cout << "[" << login << " -> " << reciever << "] $ ";
        string line;
        getline(cin, line);
        const vector<TMessageText*>& text = client.GetText();
        if (!text.empty()) {
            cout << "=== New messages ===\n";
            for (size_t i = 0; i < text.size(); ++i) {
                cout << text[i]->Sender << ": ";
                cout << text[i]->Text << '\n';
            }
        }

        if (line.empty())
            continue;
        if (line == ":q")
            break;
        if (line == ":ls") {
            const vector<string>& users = client.GetUsers();
            cout << "=== Chat users ===\n";
            for (size_t i = 0; i < users.size(); ++i)
                cout << users[i] << (((i & 3) && i + 1 < users.size()) ? "\t\t" : "\n");
            continue;
        }
        if (line == ":to") {
            cout << "Dude: ";
            cin >> reciever;
            continue;
        }

        client.SendText(line, reciever);
    }

    client.SignOut();

    return 0;
}

