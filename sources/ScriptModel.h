#pragma once

#include "ScriptInfo.h"

#include <QObject>
#include <QVariant>

#include <vector>
#include <memory>

class QMenu;
class QAction;

class ScriptModel  : public QObject
{
    Q_OBJECT

public:
    ScriptModel(QObject *parent);
    ~ScriptModel();

    void LoadScript(const QString& path);

    void setupActions(QMenu* effectsMenu, QMap<int, QAction*>& effectsActMap);

    QVariant call(const QString& callable, const QVariantList& args = QVariantList(), const QVariantMap& kwargs = QVariantMap());

    class PythonScope;

private:
    std::unique_ptr<PythonScope> mPythonScope;
    std::vector<FunctionInfo> mFunctionInfos;
};
