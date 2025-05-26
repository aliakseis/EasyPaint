#pragma once

#include "easypaintenums.h"

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

    void setupActions(QMenu* effectsMenu, QMap<EffectsEnum, QAction*>& effectsActMap);

    struct ParameterInfo {
        QString name;
        QString fullName;
        QString kind; // POSITIONAL_OR_KEYWORD
        QString description;
        QVariant default;
        QString annotation; // <class 'float'>
    };

    struct FunctionInfo {
        QString name;
        QString fullName;
        QString signature;
        QString doc;
        std::vector<ParameterInfo> parameters;
    };


private:
    std::vector<FunctionInfo> mFunctionInfos;
};
