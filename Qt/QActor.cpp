#include "QActor.h"

#include <QThread>

#include <mutex>
#include <condition_variable>

#include "Helpers/EventSource2.h"


namespace {

struct GlibUnref {
    void operator() (GMainContext* context)
        { g_main_context_unref(context); }
    void operator() (GMainLoop* loop)
        { g_main_loop_unref(loop); }
    void operator() (GAsyncQueue* queue)
        { g_async_queue_unref(queue); }
};

typedef
    std::unique_ptr<
        GMainContext,
        GlibUnref> GMainContextPtr;
typedef
    std::unique_ptr<
        GMainLoop,
        GlibUnref> GMainLoopPtr;
typedef
    std::unique_ptr<
        GAsyncQueue,
        GlibUnref> GAsyncQueuePtr;

struct Action {
    QActor::Action action;
};

void OnEvent(GAsyncQueue* queue)
{
    while(gpointer item = g_async_queue_try_pop(queue)) {
        std::unique_ptr<Action>(static_cast<Action*>(item))->action();
    }
}

void ActorMain(
    GMainContext* mainContext,
    GMainLoop* mainLoop,
    GAsyncQueue* queue,
    EventSource2* notifier)
{
    g_main_context_push_thread_default(mainContext);

    notifier->subscribe(std::bind(&OnEvent, queue));

    g_main_loop_run(mainLoop);

    g_main_context_pop_thread_default(mainContext);
}

}

struct QActor::Private {
    Private();

    GMainContextPtr mainContextPtr;
    GMainLoopPtr mainLoopPtr;
    GAsyncQueuePtr queuePtr;
    EventSource2 notifier;
    QThread *const actorThread;
};

QActor::Private::Private() :
    mainContextPtr(g_main_context_new()),
    mainLoopPtr(g_main_loop_new(mainContextPtr.get(), FALSE)),
    queuePtr(g_async_queue_new()),
    notifier(mainContextPtr.get()),
    actorThread(
        QThread::create(
            ActorMain,
            mainContextPtr.get(),
            mainLoopPtr.get(),
            queuePtr.get(),
            &notifier))
{
}

QActor::QActor(QObject* parent) :
    QObject(parent),
    _p(std::make_unique<Private>())
{
    _p->actorThread->start();
}

QActor::~QActor()
{
    if(_p->actorThread->isRunning()) {
        postAction([loop = _p->mainLoopPtr.get()] () {
            g_main_loop_quit(loop);
        });
        _p->actorThread->wait();
    }
}

QThread* QActor::actorThread() const noexcept
{
    return _p->actorThread;
}

void QActor::postAction(const Action& action)
{
    g_async_queue_push(
        _p->queuePtr.get(),
        new ::Action { action });
    _p->notifier.postEvent();
}

void QActor::postAction(Action&& action)
{
    g_async_queue_push(
        _p->queuePtr.get(),
        new ::Action { std::move(action) });
    _p->notifier.postEvent();
}

void QActor::sendAction(const Action& action)
{
    std::mutex guard;
    std::condition_variable conditional;
    bool handled = false;

    std::unique_lock guardLock(guard);

    g_async_queue_push(
        _p->queuePtr.get(),
        new ::Action {
            [&guard, &conditional, &action, &handled] () {
                std::unique_lock guardLock(guard);
                action();
                handled = true;
                conditional.notify_one();
            }
        });
    _p->notifier.postEvent();

    conditional.wait(guardLock, [&handled] () { return handled; });
}
