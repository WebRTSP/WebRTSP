#include <deque>

#include <glib.h>

#include <libwebsockets.h>

#include "CxxPtr/libconfigDestroy.h"

#include "Common/ConfigHelpers.h"
#include "Common/LwsLog.h"

#include "Http/Log.h"
#include "Http/Config.h"

#include "GstStreaming/LibGst.h"

#include "Signalling/Log.h"

#include "../Proxy/Log.h"
#include "../Proxy/Proxy.h"


static const auto Log = ProxyLog;


static bool LoadConfig(ProxyConfig* config)
{
    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(configDirs.empty())
        return false;

    ProxyConfig loadedConfig {};

    bool someConfigFound = false;
    for(const std::string& configDir: configDirs) {
        const std::string configFile = configDir + "/webrtsp-proxy.conf";
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

        config_setting_t* proxyConfig = config_lookup(&config, "proxy");
        if(proxyConfig && CONFIG_TRUE == config_setting_is_group(proxyConfig)) {
            const char* host = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(proxyConfig, "host", &host)) {
                loadedConfig.serverName = host;
            }
            int port= 0;
            if(CONFIG_TRUE == config_setting_lookup_int(proxyConfig, "port", &port)) {
                loadedConfig.port = static_cast<unsigned short>(port);
            }
            int securePort= 0;
            if(CONFIG_TRUE == config_setting_lookup_int(proxyConfig, "secure-port", &port)) {
                loadedConfig.securePort = static_cast<unsigned short>(securePort);
            }
            const char* certificate = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(proxyConfig, "certificate", &certificate)) {
                loadedConfig.certificate = FullPath(configDir, certificate);
            }
            const char* privateKey = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(proxyConfig, "key", &privateKey)) {
                loadedConfig.key = FullPath(configDir, privateKey);
            }
        }

        config_setting_t* stunServerConfig = config_lookup(&config, "stun");
        if(stunServerConfig && CONFIG_TRUE == config_setting_is_group(stunServerConfig)) {
            const char* server = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(stunServerConfig, "server", &server)) {
                loadedConfig.stunServer = server;
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

    if(!loadedConfig.port) {
        Log()->error("Missing port");
        success = false;
    }

    if(success)
        *config = loadedConfig;

    return success;
}

int main(int argc, char *argv[])
{
    http::Config httpConfig {};
    ProxyConfig config;
    if(!LoadConfig(&config))
        return -1;

    InitHttpServerLogger(config.logLevel);
    InitLwsLogger(config.logLevel);
    InitWsServerLogger(config.logLevel);
    InitProxyLogger(config.logLevel);

    LibGst libGst;

    return ProxyMain(httpConfig, config);
}
