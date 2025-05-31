#pragma once

#include "effectwithsettings.h"
#include "../ScriptInfo.h"

class ScriptModel;

class ScriptEffectWithSettings : public EffectWithSettings
{
public:
    ScriptEffectWithSettings(ScriptModel* scriptModel, const FunctionInfo& functionInfo, QObject* parent = 0)
        : EffectWithSettings(parent), mScriptModel(scriptModel), mFunctionInfo(functionInfo) {
    }

private:
    ScriptModel* mScriptModel;
    FunctionInfo mFunctionInfo;

    // Inherited via EffectWithSettings
    AbstractEffectSettings* getSettingsWidget() override;
    void convertImage(QImage& image, const QVariantList& matrix) override;
};
