#pragma once

#include "easypaintenums.h"

#include <QObject>

class QMenu;
class QAction;

class ScriptModel  : public QObject
{
    Q_OBJECT

public:
    ScriptModel(QObject *parent);
    ~ScriptModel();

    void setupActions(QMenu* effectsMenu, QMap<EffectsEnum, QAction*>& effectsActMap);

private:
    QStringList mFunctionList;

};
