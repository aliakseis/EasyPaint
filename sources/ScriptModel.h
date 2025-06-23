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

const char CHECK_PYTHON_OPTION[] = "--checkPython";

class ScriptModel  : public QObject
{
    Q_OBJECT

public:
    ScriptModel(QWidget *parent, const QString& venvPath);
    ~ScriptModel();

    void LoadScript(const QString& path);

    void setupActions(QMenu* fileMenu, QMenu* effectsMenu, QMap<int, QAction*>& effectsActMap);

    QVariant call(const QString& callable, const QVariantList& args = QVariantList(), std::weak_ptr<EffectRunCallback> callback = {}, const QVariantMap & kwargs = QVariantMap());

    static int ValidatePythonSystem();

private:
    bool send_image(const pybind11::array& image);
    bool check_interrupt();

    bool mValid = false;
    std::weak_ptr<EffectRunCallback> mCallback;
    class PythonScope;
    std::unique_ptr<PythonScope> mPythonScope;
    std::mutex mCallMutex;
    std::vector<FunctionInfo> mFunctionInfos;
    std::atomic_bool mIsShuttingDown = false;

    QString mVenvPath;
};
