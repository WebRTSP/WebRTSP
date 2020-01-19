#pragma once

#include <string>


std::string TurnTemporaryUsername(
    const std::string& temporaryUsername,
    unsigned passwordTTL);

std::string TurnTemporaryPassword(
    const std::string& temporaryUsername,
    const std::string& staticAuthSecret);

std::string IceServer(
    const std::string& username,
    unsigned passwordTTL,
    const std::string& staticAuthSecret,
    const std::string& iceEndpoint);
