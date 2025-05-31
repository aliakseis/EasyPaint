#include "scripteffectwithsettings.h"

#include "../widgets/scripteffectsettings.h"

#include "../ScriptModel.h"


AbstractEffectSettings* ScriptEffectWithSettings::getSettingsWidget()
{
    return new ScriptEffectSettings(mFunctionInfo);
}

void ScriptEffectWithSettings::convertImage(QImage& image, const QVariantList& matrix)
{
    QVariantList args;
    args << image;
    args << matrix;
    QVariant result = mScriptModel->call(mFunctionInfo.name, args);
    image = result.value<QImage>();
}
