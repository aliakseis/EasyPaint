#include "scripteffect.h"

#include "../imagearea.h"

#include "../ScriptModel.h"

#include "../dialogs/SpinnerOverlay.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QApplication>
#include <QMainWindow>

static QMainWindow* GetMainWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* w = qobject_cast<QMainWindow*>(widget)) {
            return w;
        }
    }
    return nullptr;
}

ImageArea* ScriptEffect::applyEffect(ImageArea* imageArea)
{
    // 1) Prepare undo & args exactly as before
    makeUndoCommand(imageArea);

    QVariantList args;
    if (imageArea) {
        args << *(imageArea->getImage());
        if (mFunctionInfo.usesMarkup())
            args << *(imageArea->getMarkup());
    }

    // 2) Kick off the script call on a worker thread
    auto future = QtConcurrent::run([this, args]() -> QVariant {
        // all default params on call() will be used
        return mScriptModel->call(mFunctionInfo.name, args);
        });

    // 3) Watch the future and drive a local event loop
    QFutureWatcher<QVariant> watcher;
    watcher.setFuture(future);

    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<QVariant>::finished,
        &loop, &QEventLoop::quit);

    // 4) Optionally disable the UI & show spinner
    std::unique_ptr<SpinnerOverlay> spinner;
    const auto mainWindow = GetMainWindow();
    if (mainWindow) {
        mainWindow->setEnabled(false);
        spinner.reset(new SpinnerOverlay(mainWindow));
    }

    // 5) Block here until the script is done
    loop.exec();

    // 6) Teardown spinner + re-enable UI (RAII + explicit)
    spinner.reset();            // destroys the overlay (hides + stops)
    if (mainWindow)
        mainWindow->setEnabled(true);

    // 7) Pull the result and apply it just like before
    QVariant result = future.result();
    if (result.canConvert<QImage>()) {
        QImage img = result.value<QImage>();
        if (!img.isNull()) {
            if (!imageArea)
                imageArea = initializeNewTab();

            imageArea->setImage(img);
            imageArea->fixSize(true);
            imageArea->setEdited(true);
            imageArea->update();
        }
    }

    // 8) Return the (possibly new) ImageArea
    return imageArea;
}
