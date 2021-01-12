#include "WebRTCPeer.h"

#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>


void WebRTCPeer::ResolveIceCandidate(
    const std::string& candidate,
    std::string* resolvedCandidate)
{
    if(!resolvedCandidate)
        return;

    resolvedCandidate->clear();

    if(candidate.empty())
        return;

    enum {
        ConnectionAddressPos = 4,
        PortPos = 5,
    };

    std::string::size_type pos = 0;
    unsigned token = 0;

    std::string::size_type connectionAddressPos = std::string::npos;
    std::string::size_type portPos = std::string::npos;
    while(std::string::npos == portPos &&
        (pos = candidate.find(' ', pos)) != std::string::npos)
    {
        ++pos;
        ++token;

        switch(token) {
        case ConnectionAddressPos:
            connectionAddressPos = pos;
            break;
        case PortPos:
            portPos = pos;
            break;
        }
    }

    if(connectionAddressPos != std::string::npos &&
        portPos != std::string::npos)
    {
        std::string connectionAddress(
            candidate, connectionAddressPos,
            portPos - connectionAddressPos - 1);

        const std::string::value_type localSuffix[] = ".local";
        if(connectionAddress.size() > (sizeof(localSuffix) - 1) &&
           connectionAddress.compare(
               connectionAddress.size() - (sizeof(localSuffix) - 1),
               std::string::npos,
               localSuffix) == 0)
        {
            if(hostent* host = gethostbyname(connectionAddress.c_str())) {
                int ipLen;
                switch(host->h_addrtype) {
                case AF_INET:
                    ipLen = INET_ADDRSTRLEN;
                    break;
                case AF_INET6:
                    ipLen = INET6_ADDRSTRLEN;
                    break;
                default:
                    return;
                }

                char ip[ipLen];
                if(nullptr == inet_ntop(
                    host->h_addrtype,
                    host->h_addr_list[0],
                    ip, ipLen))
                {
                   return;
                }

                if(resolvedCandidate) {
                    ipLen = strlen(ip);

                    *resolvedCandidate = candidate.substr(0, connectionAddressPos);
                    resolvedCandidate->append(ip, ipLen);
                    resolvedCandidate->append(
                        candidate,
                        portPos - 1,
                        std::string::npos);
                }
            }
        }
    }
}
