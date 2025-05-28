#include "scripteffectwithsettings.h"

#include "../widgets/scripteffectsettings.h"

AbstractEffectSettings* ScriptEffectWithSettings::getSettingsWidget()
{
    return new ScriptEffectSettings(mFunctionInfo);
}

void ScriptEffectWithSettings::convertImage(QImage& image, const QVariantList& matrix)
{
}
