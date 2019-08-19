#include "TestParse.h"

#include <thread>

#include "Signalling/Signalling.h"
#include "Client/Client.h"


int main(int argc, char *argv[])
{
    enum {
        SERVER_PORT = 8081,
    };

    TestParse();

    std::thread signallingThread(
        [] () {
            signalling::Config config {};
            config.port = SERVER_PORT;

            signalling::Signalling(&config);
        });

    std::thread clientThread(
        [] () {
            client::Config config {};
            config.server = "localhost";
            config.serverPort = SERVER_PORT;

            client::Client(&config);
        });

    if(signallingThread.joinable())
        signallingThread.join();

    if(clientThread.joinable())
        clientThread.join();

    return 0;
}
