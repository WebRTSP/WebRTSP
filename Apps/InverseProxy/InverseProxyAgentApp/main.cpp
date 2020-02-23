#include <deque>

#include <glib.h>

#include <libwebsockets.h>

#include "CxxPtr/libconfigDestroy.h"

#include "Common/ConfigHelpers.h"
#include "Common/LwsLog.h"

#include "Client/Log.h"

#include "GstStreaming/LibGst.h"

#include "../InverseProxyAgent/Log.h"
#include "../InverseProxyAgent/InverseProxyAgent.h"


static const auto Log = InverseProxyAgentLog;


static bool LoadConfig(InverseProxyAgentConfig* config)
{
    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(configDirs.empty())
        return false;

    InverseProxyAgentConfig loadedConfig {};

    bool someConfigFound = false;
    for(const std::string& configDir: configDirs) {
        const std::string configFile = configDir + "/webrtsp-agent.conf";
        if(!g_file_test(configFile.c_str(),  G_FILE_TEST_IS_REGULAR)) {
            Log()->info("Config \"{}\" not found", configFile);
            continue;
        }

        someConfigFound = true;

        config_t config;
        config_init(&config);
        ConfigDestroy ConfigDestroy(&config);

        Log()->info("Loading config \"{}\"", configFile);
        if(!config_read_file(&config, configFile.c_str())) {
            Log()->error("Fail load config. {}. {}:{}",
                config_error_text(&config),
                configFile,
                config_error_line(&config));
            return false;
        }

        config_setting_t* targetConfig = config_lookup(&config, "target");
        if(targetConfig && CONFIG_TRUE == config_setting_is_group(targetConfig)) {
            const char* host = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(targetConfig, "host", &host)) {
                loadedConfig.clientConfig.server = host;
            }
            int port = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(targetConfig, "port", &port)) {
                loadedConfig.clientConfig.serverPort = static_cast<unsigned short>(port);
            }
            int timeout = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(targetConfig, "reconnect-timeout", &timeout)) {
                loadedConfig.reconnectTimeout = static_cast<unsigned>(timeout);
            }
        }
        config_setting_t* authConfig = config_lookup(&config, "auth");
        if(authConfig && CONFIG_TRUE == config_setting_is_group(authConfig)) {
            const char* name = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(authConfig, "name", &name)) {
                loadedConfig.name = name;
            }
            const char* authToken = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(authConfig, "token", &authToken)) {
                loadedConfig.authToken = authToken;
            }
        }
        config_setting_t* streamersConfig = config_lookup(&config, "streamers");
        if(streamersConfig && CONFIG_TRUE == config_setting_is_list(streamersConfig)) {
            const int streamersCount = config_setting_length(streamersConfig);
            for(int streamerIdx = 0; streamerIdx < streamersCount; ++streamerIdx) {
                config_setting_t* streamerConfig =
                    config_setting_get_elem(streamersConfig, streamerIdx);
                if(!streamerConfig || CONFIG_FALSE == config_setting_is_group(streamerConfig)) {
                    Log()->warn("Wrong streamer config format. Streamer skipped.");
                    break;
                }
                const char* name;
                if(CONFIG_FALSE == config_setting_lookup_string(streamerConfig, "name", &name)) {
                    Log()->warn("Missing streamer name. Streamer skipped.");
                    break;
                }
                const char* type;
                if(CONFIG_FALSE == config_setting_lookup_string(streamerConfig, "type", &type)) {
                    Log()->warn("Missing streamer type. Streamer skipped.");
                    break;
                }
                const char* uri;
                if(CONFIG_FALSE == config_setting_lookup_string(streamerConfig, "uri", &uri)) {
                    Log()->warn("Missing streamer uri. Streamer skipped.");
                    break;
                }
                StreamerConfig::Type streamerType;
                if(0 == strcmp(type, "test"))
                    streamerType = StreamerConfig::Type::Test;
                else if(0 == strcmp(type, "restreamer"))
                    streamerType = StreamerConfig::Type::ReStreamer;
                else {
                    Log()->warn("Unknown streamer type. Streamer skipped.");
                    break;
                }

                loadedConfig.streamers.emplace(
                    name,
                    StreamerConfig { streamerType, uri });
            }
        }
        config_setting_t* debugConfig = config_lookup(&config, "debug");
        if(debugConfig && CONFIG_TRUE == config_setting_is_group(debugConfig)) {
            int logLevel = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(debugConfig, "log-level", &logLevel)) {
                if(logLevel > 0) {
                    loadedConfig.logLevel =
                        static_cast<spdlog::level::level_enum>(
                            spdlog::level::critical - std::min<int>(logLevel, spdlog::level::critical));
                }
            }
        }
    }

    if(!someConfigFound)
        return false;

    bool success = true;

    if(loadedConfig.clientConfig.server.empty()) {
        Log()->error("Missing target host");
        success = false;
    }
    if(!loadedConfig.clientConfig.serverPort) {
        Log()->error("Missing target port");
        success = false;
    }
    if(loadedConfig.name.empty()) {
        Log()->error("Missing auth name");
        success = false;
    }
    if(loadedConfig.authToken.empty()) {
        Log()->error("Missing auth token");
        success = false;
    }

    if(success)
        *config = loadedConfig;

    return success;
}

int main(int argc, char *argv[])
{
    LibGst libGst;

    InverseProxyAgentConfig config;
    if(!LoadConfig(&config))
        return -1;

    InitLwsLogger(config.logLevel);
    InitWsClientLogger(config.logLevel);

    return InverseProxyAgentMain(config);
}
