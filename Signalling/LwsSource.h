#pragma once

#include <memory>

#include <glib.h>

#include <libwebsockets.h>


struct LwsSource;

struct LwsSourceUnref
{
    void operator() (LwsSource* queueSource);
};

typedef std::unique_ptr<LwsSource, LwsSourceUnref> LwsSourcePtr;

LwsSourcePtr LwsSourceNew(lws_context*, GMainContext* context = nullptr);

int LwsSourceCallback(
    LwsSourcePtr&,
    lws*,
    lws_callback_reasons,
    void* in, size_t len);
