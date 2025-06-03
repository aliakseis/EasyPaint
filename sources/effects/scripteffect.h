#pragma once

#include "abstracteffect.h"
#include "../ScriptInfo.h"

class ScriptModel;

class ScriptEffect : public AbstractEffect
{
public:
    ScriptEffect(ScriptModel* scriptModel, const FunctionInfo& functionInfo, QObject* parent = 0)
        : AbstractEffect(parent), mScriptModel(scriptModel), mFunctionInfo(functionInfo) {}

private:
    ScriptModel* mScriptModel;
    FunctionInfo mFunctionInfo;

    // Inherited via AbstractEffect
    ImageArea* applyEffect(ImageArea* imageArea) override;
};
