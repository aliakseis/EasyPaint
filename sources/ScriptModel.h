#pragma once

#include "ScriptInfo.h"

#include <QObject>
#include <QVariant>

#include <vector>
#include <memory>

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

    QVariant call(const QString& callable, const QVariantList& args = QVariantList(), const QVariantMap& kwargs = QVariantMap());

    void interrupt() { mInterrupt = true; }

signals:
    void sendImage(const QImage& img);

private:
    void send_image(const pybind11::array& image);
    bool check_interrupt();

    class PythonScope;
    std::unique_ptr<PythonScope> mPythonScope;
    std::vector<FunctionInfo> mFunctionInfos;
    std::atomic_bool mInterrupt = false;
};
