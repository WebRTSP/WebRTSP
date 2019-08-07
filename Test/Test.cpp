#include "TestParseRequest.h"

#include <thread>

#include "Signalling/Signalling.h"


int main(int argc, char *argv[])
{
    TestParse();

    std::thread signallingThread(
        [] () {
            signalling::Config config {};
            config.port = 8081;

            signalling::Signalling(&config);
        });

    if(signallingThread.joinable())
        signallingThread.join();

    return 0;
}
