#include "ForwardContext.h"

#include <map>


struct ForwardContext::Private
{
    std::map<std::string, BackSession*> backSessions;
};

ForwardContext::ForwardContext() :
    _p(std::make_unique<Private>())
{
}

ForwardContext::~ForwardContext()
{
}

bool ForwardContext::registerBackSession(
    const std::string& name,
    BackSession* session)
{
    auto pair = _p->backSessions.emplace(name, session);
    if(!pair.second)
        return false;

    return true;
}
