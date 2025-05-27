#pragma once

//#include "easypaintenums.h"
#include "ScriptInfo.h"

#include <QObject>
#include <QVariant>

#include <vector>

class QMenu;
class QAction;

class ScriptModel  : public QObject
{
    Q_OBJECT

public:
    ScriptModel(QObject *parent);
    ~ScriptModel();

    void setupActions(QMenu* effectsMenu, QMap<int, QAction*>& effectsActMap);

private:
    std::vector<FunctionInfo> mFunctionInfos;
};
