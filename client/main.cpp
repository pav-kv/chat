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

void PrintHelp() {
    cout
        << "=== Help ===\n"
        << " <enter>   - check for incoming messages\n"
        << " :h        - help\n"
        << " :ls       - display chat users list\n"
        << " :to       - switch messages reciever\n"
        << " :q        - quit\n\n";
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    string ip;
    int port = 0;
    cout << "Server IP and port: ";
    cin >> ip >> port;

    TClient client(ip, port);
    if (!client.Connect())
        return 0;

    string login, pass;
    cout << "Login: ";
    cin >> login;
    cout << "Password: ";
    cin >> pass;
    string fictive;
    getline(cin, fictive);
    if (!client.SignIn(login, pass)) {
        cout << "Failed to sign in.\n";
        client.SignOut();
        return 0;
    }

    PrintHelp();
    string reciever("server");
    while (true) {
        cout << "[" << login << " -> " << reciever << "] $ ";
        string line;
        getline(cin, line);
        const vector<TMessageText>& text = client.GetText();
        if (!text.empty()) {
            cout << "=== New messages ===\n";
            for (size_t i = 0; i < text.size(); ++i) {
                cout << text[i].Sender << ": ";
                cout << text[i].Text << '\n';
            }
            cout << '\n';
        }

        if (line.empty())
            continue;
        if (line == ":q")
            break;
        if (line == ":ls") {
            const vector<string>& users = client.GetUsers();
            cout << "=== Chat users ===\n";
            for (size_t i = 0; i < users.size(); ++i)
                cout << users[i] << ((((i + 1) & 3) && i + 1 < users.size()) ? "\t\t" : "\n");
            cout << '\n';
        } else if (line == ":to") {
            cout << "Select dude: ";
            cin >> reciever;
            getline(cin, fictive);
        } else if (line == ":h") {
            PrintHelp();
        } else {
            client.SendText(line, reciever);
        }
    }

    client.SignOut();

    return 0;
}

