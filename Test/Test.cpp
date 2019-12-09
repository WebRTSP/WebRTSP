#include <thread>

#include "TestParse.h"
#include "TestSerialize.h"

#include "Signalling/Signalling.h"

#include "Client/Client.h"

#define ENABLE_CLIENT 0

int main(int argc, char *argv[])
{
    enum {
        SERVER_PORT = 8081,
    };

    TestParse();
    TestSerialize();

    std::thread signallingThread(
        [] () {
            signalling::Config config {};
            config.port = SERVER_PORT;

            signalling::Signalling(&config);
        });

#if ENABLE_CLIENT
    std::thread clientThread(
        [] () {
            client::Config config {};
            config.server = "localhost";
            config.serverPort = SERVER_PORT;

            client::Client(&config);
        });
#endif

    if(signallingThread.joinable())
        signallingThread.join();

#if ENABLE_CLIENT
    if(clientThread.joinable())
        clientThread.join();
#endif

    return 0;
}
