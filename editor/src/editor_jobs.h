#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QPromise>
#include <QString>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <utility>

namespace Editor::Jobs {

template <class Made> class Watcher : public QFutureWatcher<Made> {
public:
    explicit Watcher(QObject* parent) : QFutureWatcher<Made>(parent) {}
};

template <class Made>
QFutureWatcher<Made>*
Start(QObject* owner, QThreadPool& pool, std::function<void(QPromise<Made>&)> work,
      std::function<void(Made)> done, std::function<void(int, int, QString)> moved = {}) {
    auto* watcher = new Watcher<Made>(owner);
    if (moved) {
        QObject::connect(watcher, &QFutureWatcher<Made>::progressValueChanged, owner,
                         [watcher, moved](int value) {
                             moved(value, watcher->progressMaximum(), watcher->progressText());
                         });
    }
    QObject::connect(watcher, &QFutureWatcher<Made>::finished, owner, [watcher, done] {
        if (!watcher->isCanceled() && watcher->future().resultCount() > 0) done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&pool, std::move(work)));
    return watcher;
}

}
