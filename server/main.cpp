#include "serverlib.h"

#include <signal.h>

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    TServer server;
    server.Start();

    return 0;
}

