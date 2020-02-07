#include "LwsSource.h"

#include <cassert>
#include <map>
#include <deque>

#include <sys/eventfd.h>

#ifndef G_SOURCE_FUNC
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))
#endif


namespace {

struct FdInfo
{
    int events;
    gpointer tag;
};

struct LwsFdSource
{
    GSource base;

    lws_sockfd_type fd;
    int events;
    gpointer tag;
};

struct CXX
{
    std::map<lws_sockfd_type, LwsFdSource*> childSources;
};

}

struct LwsSource
{
    GSource base;
    lws_context* context;
    CXX* cxx;
    guint idle;
};


void LwsSourceUnref::operator() (LwsSource* lwsSource)
{
    g_source_unref(reinterpret_cast<GSource*>(lwsSource));
}

static gboolean fdSourceCheck(GSource* source)
{
    LwsFdSource* fdSource = reinterpret_cast<LwsFdSource*>(source);

    if(g_source_query_unix_fd(source, fdSource->tag) != 0)
        return true;

    return false;
}

static gboolean fdSourceDispatch(
    GSource* source,
    GSourceFunc,
    gpointer userData)
{
    LwsFdSource* fdSource = reinterpret_cast<LwsFdSource*>(source);

    GIOCondition condition = g_source_query_unix_fd(source, fdSource->tag);
    if(condition) {
        lws_pollfd pollfd {
            .fd = fdSource->fd,
            .events = static_cast<short>(fdSource->events),
            .revents = condition,
        };
        lws_context* lwsContext = reinterpret_cast<lws_context*>(userData);
        lws_service_fd(lwsContext, &pollfd);
    }

    return G_SOURCE_CONTINUE;
}

static void fdSourceFinalize(GSource* source)
{
    // LwsSource* lwsSource = reinterpret_cast<LwsSource*>(source);
}

static LwsFdSource* FdSourceNew(
    GSource* parent,
    lws_context* lwsContext,
    const lws_pollargs& args)
{
    static GSourceFuncs funcs = {
        .prepare = nullptr,
        .check = fdSourceCheck,
        .dispatch = fdSourceDispatch,
        .finalize = fdSourceFinalize,
    };

    GSource* source = g_source_new(&funcs, sizeof(LwsFdSource));
    LwsFdSource* fdSource = reinterpret_cast<LwsFdSource*>(source);
    fdSource->fd = args.fd;
    fdSource->events = args.events;

    fdSource->tag =
        g_source_add_unix_fd(
            source,
            args.fd,
            static_cast<GIOCondition>(args.events));

    g_source_set_callback(
        source,
        G_SOURCE_FUNC(lws_service_fd),
        lwsContext, nullptr);

    g_source_add_child_source(parent, source);
    g_source_unref(source);

    return fdSource;
}

static void fdSourceChange(LwsFdSource* fdSource, const lws_pollargs& args)
{
    GSource* source = reinterpret_cast<GSource*>(fdSource);

    g_source_modify_unix_fd(
        source,
        fdSource->tag,
        static_cast<GIOCondition>(args.events));
    fdSource->events = args.events;
}

static void lwsSourceFinalize(GSource* source)
{
    LwsSource* lwsSource = reinterpret_cast<LwsSource*>(source);

    delete lwsSource->cxx;
    lwsSource->cxx = nullptr;
}

static gboolean LwsIdle(LwsSource* lwsSource)
{
    lws_context *const lwsContext = lwsSource->context;

    const int tsi = 0;
    while(0 == lws_service_adjust_timeout(lwsContext, 1, tsi))
        lws_service_tsi(lwsContext, -1, tsi);

    lwsSource->idle = 0;

    return G_SOURCE_REMOVE;
}

static gboolean lwsSourceDispatch(
    GSource* source,
    GSourceFunc,
    gpointer userData)
{
    LwsSource* lwsSource = reinterpret_cast<LwsSource*>(source);

    if(!lwsSource->idle) {
        GSource* idleSource = g_idle_source_new();
        g_source_set_callback(
            idleSource,
            G_SOURCE_FUNC(LwsIdle),
            lwsSource, nullptr);
        lwsSource->idle = g_source_attach(idleSource, g_source_get_context(source));
        g_source_unref(idleSource);
    }

    return G_SOURCE_CONTINUE;
}

#if LWS_LIBRARY_VERSION_NUMBER < 3002000
static gboolean LwsTimeout(lws_context* lwsContext)
{
    lws_service_fd(lwsContext, NULL);

    return G_SOURCE_CONTINUE;
}
#endif

LwsSourcePtr LwsSourceNew(lws_context* lwsContext, GMainContext* gMainContext)
{
    static GSourceFuncs funcs = {
        .prepare = nullptr,
        .check = nullptr,
        .dispatch = lwsSourceDispatch,
        .finalize = lwsSourceFinalize,
    };

    GSource* source = g_source_new(&funcs, sizeof(LwsSource));
    LwsSource* lwsSource = reinterpret_cast<LwsSource*>(source);
    lwsSource->context = lwsContext;
    lwsSource->cxx = new CXX;
    g_source_attach(source, gMainContext);
    g_source_unref(source);

#if LWS_LIBRARY_VERSION_NUMBER < 3002000
    GSource* timeoutSource = g_timeout_source_new_seconds(1);
    g_source_set_callback(
        timeoutSource,
        G_SOURCE_FUNC(LwsTimeout),
        lwsContext, nullptr);
    g_source_add_child_source(source, timeoutSource);
    g_source_unref(timeoutSource);
#endif

    return LwsSourcePtr(lwsSource);
}

int LwsSourceCallback(
    LwsSourcePtr& lwsSource,
    lws* wsi,
    lws_callback_reasons reason,
    void* in, size_t len)
{
    CXX& cxx = *lwsSource->cxx;
    GSource* source = reinterpret_cast<GSource*>(lwsSource.get());
    const lws_pollargs* args = static_cast<lws_pollargs*>(in);

    switch(reason) {
        case LWS_CALLBACK_ADD_POLL_FD: {
            assert(cxx.childSources.find(args->fd) == cxx.childSources.end());

            LwsFdSource* fdSource = FdSourceNew(source, lwsSource->context, *args);
            cxx.childSources.emplace(args->fd, fdSource);

            break;
        }
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD: {
            auto it = cxx.childSources.find(args->fd);
            assert(it != cxx.childSources.end());
            if(it != cxx.childSources.end())
                fdSourceChange(it->second, *args);

            break;
        }
        case LWS_CALLBACK_DEL_POLL_FD: {
            auto it = cxx.childSources.find(args->fd);
            assert(it != cxx.childSources.end());
            if(it != cxx.childSources.end()) {
                GSource* childSource = reinterpret_cast<GSource*>(it->second);
                g_source_remove_child_source(source, childSource);
                cxx.childSources.erase(it);
            }

            break;
        }
        default:
            return 0;
    }

    return 0;
}
