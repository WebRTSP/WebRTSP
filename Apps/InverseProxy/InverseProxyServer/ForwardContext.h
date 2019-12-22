#pragma once

#include <string>
#include <memory>

class BackSession;


class ForwardContext
{
public:
    ForwardContext();
    ~ForwardContext();

    bool registerBackSession(const std::string& name, BackSession*);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
