#include <deque>

#include <glib.h>

#include <libwebsockets.h>

#include "CxxPtr/libconfigDestroy.h"

#include "Common/ConfigHelpers.h"
#include "Common/LwsLog.h"

#include "Signalling/Log.h"

#include "../InverseProxyServer/Log.h"
#include "../InverseProxyServer/InverseProxyServer.h"


static const auto Log = InverseProxyServerLog;


static bool LoadConfig(InverseProxyServerConfig* config)
{
    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(configDirs.empty())
        return false;

    InverseProxyServerConfig loadedConfig {};

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
                // loadedConfig.proxyHost = host;
            }
            int frontPort = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(proxyConfig, "front-port", &frontPort)) {
                loadedConfig.frontPort = static_cast<unsigned short>(frontPort);
            }
            int backPort = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(proxyConfig, "back-port", &backPort)) {
                loadedConfig.backPort = static_cast<unsigned short>(backPort);
            }
            const char* certificate = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(proxyConfig, "certificate", &certificate)) {
                // loadedConfig.authCertificate = FullPath(configDir, authCert);
            }
            const char* privateKey = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(proxyConfig, "key", &privateKey)) {
                // loadedConfig.authKey = FullPath(configDir, authKey);
            }
        }

        config_setting_t* sourcesConfig = config_lookup(&config, "sources");
        if(sourcesConfig && CONFIG_TRUE == config_setting_is_list(sourcesConfig)) {
            const int sourcesCount = config_setting_length(sourcesConfig);
            for(int sourceIdx = 0; sourceIdx < sourcesCount; ++sourceIdx) {
                config_setting_t* sourceConfig =
                    config_setting_get_elem(sourcesConfig, sourceIdx);
                if(!sourceConfig || CONFIG_FALSE == config_setting_is_group(sourceConfig)) {
                    Log()->warn("Wrong source config format. Source skipped.");
                    break;
                }

                const char* name;
                if(CONFIG_FALSE == config_setting_lookup_string(sourceConfig, "name", &name)) {
                    Log()->warn("Missing source name. Source skipped.");
                    break;
                }

                const char* token;
                if(CONFIG_FALSE == config_setting_lookup_string(sourceConfig, "token", &token)) {
                    Log()->warn(
                        "Missing auth token for source \"{}\". Source skipped.",
                        name);
                    break;
                }

                loadedConfig.backAuthTokens.emplace(name, token);
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

    if(!loadedConfig.frontPort) {
        Log()->error("Missing proxy front port");
        success = false;
    }

    if(!loadedConfig.backPort) {
        Log()->error("Missing proxy back port");
        success = false;
    }

    if(success)
        *config = loadedConfig;

    return success;
}

int main(int argc, char *argv[])
{
    InverseProxyServerConfig config;
    if(!LoadConfig(&config))
        return -1;

    InitLwsLogger(config.logLevel);
    InitWsServerLogger(config.logLevel);
    InitInverseProxyServerLogger(config.logLevel);

    return InverseProxyServerMain(config);
}
