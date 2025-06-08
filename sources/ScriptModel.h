#pragma once

#include "ScriptInfo.h"

#include "effects/effectruncallback.h"

#include <QObject>
#include <QVariant>

#include <vector>
#include <memory>
#include <mutex>

class QMenu;
class QAction;

namespace pybind11 {

class array;

}

class ScriptModel  : public QObject
{
    Q_OBJECT

public:
    ScriptModel(QObject *parent);
    ~ScriptModel();

    void LoadScript(const QString& path);

    void setupActions(QMenu* fileMenu, QMenu* effectsMenu, QMap<int, QAction*>& effectsActMap);

    QVariant call(const QString& callable, const QVariantList& args = QVariantList(), std::weak_ptr<EffectRunCallback> callback = {}, const QVariantMap & kwargs = QVariantMap());

private:
    bool send_image(const pybind11::array& image);
    bool check_interrupt();

    std::weak_ptr<EffectRunCallback> mCallback;
    class PythonScope;
    std::unique_ptr<PythonScope> mPythonScope;
    std::mutex mCallMutex;
    std::vector<FunctionInfo> mFunctionInfos;
    std::atomic_bool mIsShuttingDown = false;
};
