#pragma once

#include <memory>
#include <functional>

#include <QObject>


class QActor: public QObject
{
    Q_OBJECT

    QActor(QActor&) = delete;
    QActor& operator = (QActor&) = delete;

public:
    QActor(QObject* parent = nullptr);
    ~QActor();

    QThread* actorThread() const noexcept;

    typedef std::function<void ()> Action;
    void postAction(const Action&);
    void postAction(Action&&);
    void sendAction(const Action&);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
