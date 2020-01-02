#include <deque>

#include <glib.h>

#include "CxxPtr/libconfigDestroy.h"

#include "../InverseProxyServer/Log.h"
#include "../InverseProxyServer/InverseProxyServer.h"


static const auto Log = InverseProxyServerLog;


static std::deque<std::string> ConfigDirs()
{
    std::deque<std::string> dirs;

#ifdef SNAPCRAFT_BUILD
    if(const gchar* data = g_getenv("SNAP"))
        dirs.push_back(std::string(data) + "/etc");

    if(const gchar* common = g_getenv("SNAP_COMMON"))
        dirs.push_back(common);
#else
    const gchar * const *systemConfigDirs = g_get_system_config_dirs();
    while(*systemConfigDirs) {
        dirs.push_back(*systemConfigDirs);
        ++systemConfigDirs;
    }

    const gchar* configDir = g_get_user_config_dir();
    if(configDir)
        dirs.push_back(configDir);
#endif

    return dirs;
}

static std::string FullPath(const std::string& configDir, const std::string& path)
{
    std::string fullPath;
    if(!g_path_is_absolute(path.c_str())) {
        gchar* tmpPath =
            g_build_filename(configDir.c_str(), path.c_str(), NULL);
        fullPath = tmpPath;
        g_free(tmpPath);
    } else
        fullPath = path;

    return fullPath;
}

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

    return InverseProxyServerMain(config);
}
