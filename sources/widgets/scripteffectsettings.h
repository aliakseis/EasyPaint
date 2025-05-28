#pragma once

#include "abstracteffectsettings.h"
#include "../ScriptInfo.h"

class ScriptEffectSettings : public AbstractEffectSettings
{
public:
    ScriptEffectSettings(const FunctionInfo& functionInfo, QWidget* parent = 0)
        : AbstractEffectSettings(parent), mFunctionInfo(functionInfo) {}

private:
    FunctionInfo mFunctionInfo;

    // Inherited via AbstractEffectSettings
    QList<QVariant> getEffectSettings() override;
};

