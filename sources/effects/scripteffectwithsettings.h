#pragma once

#include "effectwithsettings.h"
#include "../ScriptInfo.h"

class ScriptModel;

class ScriptEffectWithSettings : public EffectWithSettings
{
public:
    ScriptEffectWithSettings(ScriptModel* scriptModel, const FunctionInfo& functionInfo, QObject* parent = 0);

private:
    ScriptModel* mScriptModel;
    FunctionInfo mFunctionInfo;

    // Inherited via EffectWithSettings
    AbstractEffectSettings* getSettingsWidget() override;
    void convertImage(const QImage* source, QImage& image, const QVariantList& matrix) override;
};
