#include "HttpMicroServer.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <microhttpd.h>

#include <CxxPtr/CPtr.h>
#include <CxxPtr/GlibPtr.h>

#include "Log.h"


namespace {

const auto Log = HttpServerLog;

struct MHDFree
{
    void operator() (char* str)
        { MHD_free(str); }
};

typedef
    std::unique_ptr<
        char,
        MHDFree> MHDCharPtr;

const char *const ConfigFile = "/Config.js";
const char *const IndexFile = "/index.html";
const char *const IndexFilePath = "/";
const char *const IndexFilePrefix = "/v/";

const char* AuthCookieName = "WebRTSP-Auth";
const char* AuthCookieStaticAttributes = "; HttpOnly; SameSite=Strict; Secure; Path=/";
#ifdef NDEBUG
const unsigned AuthCookieMaxAge = 30 * 24 * 60 * 60; // seconds
const unsigned AuthCookieCleanupInterval = 15; // seconds
#else
const unsigned AuthCookieMaxAge = 5; // seconds
const unsigned AuthCookieCleanupInterval = 1; // seconds
#endif

const MHD_DigestAuthAlgorithm DigestAlgorithm = MHD_DIGEST_ALG_MD5;
//const MHD_DigestAuthAlgorithm DigestAlgorithm = MHD_DIGEST_ALG_AUTO;

}


namespace http {

struct MicroServer::Private
{
    static const std::string AccessDeniedResponse;
    static const std::string NotFoundResponse;

    Private(
        MicroServer*,
        const Config&,
        const std::string& configJsc,
        const MicroServer::OnNewAuthToken&,
        GMainContext* context);
    ~Private();

    bool init();

    MHD_Result httpCallback(
        struct MHD_Connection* connection,
        const char* url,
        const char* method,
        const char* version,
        const char* uploadData,
        size_t* uploadDataSize,
        void ** conCls);

    bool isValidCookie(const char* cookie);
    void addCookie(MHD_Response*);
    void refreshCookie(MHD_Response* response, const std::string& inCookie);
    void addExpiredCookie(MHD_Response*) const;
    MHD_Result queueAccessDeniedResponse(
        MHD_Connection* connection,
        bool expireCookie,
        bool isStale) const;
    void postToken(
        const std::string& token,
        std::chrono::steady_clock::time_point expiresAt) const;
    void cleanupCookies();

    MicroServer *const owner;

    const Config config;
    const std::string wwwRootPath;
    const std::string configJsPath;
    MHD_Daemon* daemon = nullptr;
    std::vector<uint8_t> configJsBuffer;
    const MicroServer::OnNewAuthToken onNewAuthTokenCallback;
    GMainContext* context;

    std::unordered_map<std::string, const MicroServer::AuthCookieData> authCookies;
    std::chrono::steady_clock::time_point nextAuthCookiesCleanupTime =
        std::chrono::steady_clock::time_point::min();
};

const std::string MicroServer::Private::AccessDeniedResponse = "Access denied";
const std::string MicroServer::Private::NotFoundResponse = "Not found";

MicroServer::Private::Private(
    MicroServer* owner,
    const Config& config,
    const std::string& configJs,
    const OnNewAuthToken& onNewAuthTokenCallback,
    GMainContext* context) :
    owner(owner),
    config(config),
    wwwRootPath(GCharPtr(g_canonicalize_filename(config.wwwRoot.c_str(), nullptr)).get()),
    configJsPath(GCharPtr(g_build_filename(wwwRootPath.c_str(), ConfigFile, nullptr)).get()),
    configJsBuffer(configJs.begin(), configJs.end()),
    onNewAuthTokenCallback(onNewAuthTokenCallback),
    context(context)
{
}

bool MicroServer::Private::init()
{
    assert(!daemon);
    if(daemon) return false;

    auto callback = [] (
        void* cls,
        struct MHD_Connection* connection,
        const char* url,
        const char* method,
        const char* version,
        const char* uploadData,
        size_t* uploadDataSize,
        void ** conCls) -> MHD_Result
    {
        Private* p =static_cast<Private*>(cls);
        return p->httpCallback(connection, url, method, version, uploadData, uploadDataSize, conCls);
    };

    Log()->info("Starting HTTP server on port {} in \"{}\"", config.port, wwwRootPath);

    daemon =
        MHD_start_daemon(
            MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
            config.port,
            nullptr, nullptr,
            callback, this,
            MHD_OPTION_NONCE_NC_SIZE, 1000,
            MHD_OPTION_END);

    return daemon != nullptr;
}

MicroServer::Private::~Private()
{
    MHD_stop_daemon(daemon);
}

void MicroServer::Private::postToken(
    const std::string& token,
    std::chrono::steady_clock::time_point expiresAt) const
{
    GSourcePtr idleSourcePtr(g_idle_source_new());
    GSource* idleSource = idleSourcePtr.get();

    struct CallbackData {
        const MicroServer::OnNewAuthToken onNewAuthTokenCallback;
        std::string token;
        std::chrono::steady_clock::time_point expiresAt;
    };
    CallbackData* callbackData = new CallbackData { onNewAuthTokenCallback, token, expiresAt };
    g_source_set_callback(
        idleSource,
        [] (gpointer userData) -> gboolean {
            const CallbackData* callbackData = reinterpret_cast<CallbackData*>(userData);

            callbackData->onNewAuthTokenCallback(
                callbackData->token,
                callbackData->expiresAt);

            return false;
        },
        callbackData,
        [] (gpointer userData) {
            delete reinterpret_cast<CallbackData*>(userData);
        }) ;
    g_source_attach(idleSource, context);
}

bool MicroServer::Private::isValidCookie(const char* inCookie)
{
    if(!inCookie)
        return false;

    auto it = authCookies.find(inCookie);
    if(it == authCookies.end())
        return false;

    const AuthCookieData& cookieData = it->second;
    if(cookieData.expiresAt < std::chrono::steady_clock::now()) {
        authCookies.erase(it);
        return false;
    }

     // FIXME! add check of source IP address

    return true;
}

void MicroServer::Private::addCookie(MHD_Response* response)
{
    GCharPtr randomPtr(g_uuid_string_random()); // FIXME! use something more sequre
    const std::string random = randomPtr.get();

    std::string cookie = AuthCookieName;
    cookie += "=" + random;
    cookie += "; Max-Age=";
    cookie += std::to_string(AuthCookieMaxAge);
    cookie += AuthCookieStaticAttributes;

    MHD_add_response_header(response, MHD_HTTP_HEADER_SET_COOKIE, cookie.c_str());

    const std::chrono::steady_clock::time_point expiresAt =
        std::chrono::steady_clock::now() + std::chrono::seconds(AuthCookieMaxAge);
    authCookies.emplace(random, AuthCookieData { expiresAt });

    postToken(random, expiresAt);

    Log()->debug("Added auth cookie \"{}\"", random);
}

void MicroServer::Private::refreshCookie(MHD_Response* response, const std::string& inCookie)
{
    Log()->debug("Refreshing auth cookie \"{}\"...", inCookie);

    std::string cookie = AuthCookieName;
    cookie += "=" + inCookie;
    cookie += "; Max-Age=";
    cookie += std::to_string(AuthCookieMaxAge);
    cookie += AuthCookieStaticAttributes;

    MHD_add_response_header(response, MHD_HTTP_HEADER_SET_COOKIE, cookie.c_str());

    const std::chrono::steady_clock::time_point expiresAt =
        std::chrono::steady_clock::now() + std::chrono::seconds(AuthCookieMaxAge);
    authCookies.emplace(inCookie, AuthCookieData { expiresAt });

    postToken(inCookie, expiresAt);
}

void MicroServer::Private::addExpiredCookie(MHD_Response* response) const
{
    Log()->debug("Expiring auth cookie...");

    std::string cookie = AuthCookieName;
    cookie += "= ";
    cookie += "; Max-Age=0";
    cookie += AuthCookieStaticAttributes;

    MHD_add_response_header(response, MHD_HTTP_HEADER_SET_COOKIE, cookie.c_str());
}

MHD_Result MicroServer::Private::queueAccessDeniedResponse(
    MHD_Connection* connection,
    bool expireCookie,
    bool isStale) const
{
    MHD_Response* response =
        MHD_create_response_from_buffer(
            AccessDeniedResponse.size(),
            (void*)AccessDeniedResponse.c_str(),
            MHD_RESPMEM_PERSISTENT);
    if(expireCookie) addExpiredCookie(response);
    const MHD_Result queueResult =
        MHD_queue_auth_fail_response2(
            connection,
            config.realm.c_str(),
            config.opaque.c_str(),
            response,
            isStale ? MHD_YES : MHD_NO,
            DigestAlgorithm);
    MHD_destroy_response(response);

    return queueResult;
}

void MicroServer::Private::cleanupCookies()
{
    const auto now = std::chrono::steady_clock::now();
    if(nextAuthCookiesCleanupTime > now)
        return;

    for(auto it = authCookies.begin(); it != authCookies.end();) {
        if(it->second.expiresAt < now)
            it = authCookies.erase(it);
        else
            ++it;
    }

    nextAuthCookiesCleanupTime = now + std::chrono::seconds(AuthCookieCleanupInterval);
}

// running on worker thread!
MHD_Result MicroServer::Private::httpCallback(
    struct MHD_Connection* connection,
    const char *const url,
    const char *const method,
    const char *const version,
    const char *const uploadData,
    size_t* uploadDataSize,
    void ** conCls)
{
    if(0 != strcmp(method, MHD_HTTP_METHOD_GET))
      return MHD_NO;

    if(*conCls == nullptr) {
        // first phase, skipping
        *conCls = GINT_TO_POINTER(TRUE);
        return MHD_YES;
    }

    Log()->debug("Serving \"{}\"...", url);

    cleanupCookies();

    bool addAuthCookie = false;

    const char* inAuthCookie= MHD_lookup_connection_value(connection, MHD_COOKIE_KIND, AuthCookieName);
    const bool authCookieValid = isValidCookie(inAuthCookie);
    if(inAuthCookie) {
        if(authCookieValid)
            Log()->debug("Got valid auth cookie \"{}\"", inAuthCookie);
        else
            Log()->debug("Got invalid auth cookie \"{}\"", inAuthCookie);
    }
    if(!config.passwd.empty() && !authCookieValid) {
        MHDCharPtr userNamePtr(MHD_digest_auth_get_username(connection));
        const char* userName = userNamePtr.get();
        if(!userName) {
            Log()->debug("User name is missing");
            return queueAccessDeniedResponse(connection, inAuthCookie, false);
        }

        auto it = config.passwd.find(userName);
        if(it == config.passwd.end()) {
            Log()->error("User \"{}\" not allowed", userName);
            return queueAccessDeniedResponse(connection, inAuthCookie, false);
        }

        const int authCheckResult =
            MHD_digest_auth_check2(
                connection,
                config.realm.c_str(),
                it->first.c_str(),
                it->second.c_str(),
                300,
                DigestAlgorithm);
        if(authCheckResult != MHD_YES) {
            Log()->error("User \"{}\" authentication failed for \"{}\"", userName, url);
            return queueAccessDeniedResponse(connection, inAuthCookie, authCheckResult == MHD_INVALID_NONCE);
        }

        addAuthCookie = true;

        Log()->info("User \"{}\" authorized...", userName);
    }

    GCharPtr safePathPtr;
    if(0 == strcmp(url, IndexFilePath) || g_str_has_prefix(url, IndexFilePrefix)) {
        Log()->debug("Routing \"{}\" to \"{}\"...", url, IndexFile);
        safePathPtr.reset(g_build_filename(wwwRootPath.c_str(), IndexFile, nullptr));
    } else {
        GCharPtr fullPathPtr(g_build_filename(wwwRootPath.c_str(), url, nullptr));
        safePathPtr.reset(g_canonicalize_filename(fullPathPtr.get(), nullptr));
        if(!g_str_has_prefix(safePathPtr.get(), wwwRootPath.c_str())) {
            Log()->error("Try to escape from WWW dir detected: {}\n", url);
            return MHD_NO;
        }
    }

    MHD_Response* response = nullptr;
    if(configJsPath == safePathPtr.get()) {
        response =
            MHD_create_response_from_buffer(
                configJsBuffer.size(),
                configJsBuffer.data(),
                MHD_RESPMEM_PERSISTENT);
    } else {
        const int fd = open(safePathPtr.get(), O_RDONLY);
        FDAutoClose fdAutoClose(fd);

        struct stat fileStat = {};
        if(fd != -1 && fstat(fd, &fileStat) == -1)
            return MHD_NO;

        if(fd == -1 || !S_ISREG(fileStat.st_mode)) {
            MHD_Response* response =
                MHD_create_response_from_buffer(
                    NotFoundResponse.size(),
                    (void*)NotFoundResponse.c_str(),
                    MHD_RESPMEM_PERSISTENT);
            MHD_Result queueResult = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
            MHD_destroy_response(response);

            return queueResult;
        }

        response =  MHD_create_response_from_fd64(fileStat.st_size, fd);
        if(!response)
            return MHD_NO;

        fdAutoClose.cancel();
    }

    if(addAuthCookie) {
        addCookie(response);
    } else if(authCookieValid) {
        refreshCookie(response, inAuthCookie);
    }

    if(g_str_has_suffix(safePathPtr.get(), ".js") || g_str_has_suffix(safePathPtr.get(), ".mjs"))
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/javascript");

    MHD_Result queueResult = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return queueResult;
}


MicroServer::MicroServer(
    const Config& config,
    const std::string& configJs,
    const OnNewAuthToken& onNewAuthTokenCallback,
    GMainContext* context) noexcept :
    _p(std::make_unique<Private>(this, config, configJs, onNewAuthTokenCallback, context))
{
}

MicroServer::~MicroServer()
{
}

bool MicroServer::init() noexcept
{
    return _p->init();
}

}
