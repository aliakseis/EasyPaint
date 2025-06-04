#include "scripteffectwithsettings.h"

#include "../widgets/scripteffectsettings.h"

#include "../ScriptModel.h"

#include <QSettings>

const char PREFIX[] = "/ScriptEffectSettings/";

AbstractEffectSettings* ScriptEffectWithSettings::getSettingsWidget()
{
    auto effectSettings = QSettings().value(PREFIX + mFunctionInfo.name).toList();
    return new ScriptEffectSettings(mFunctionInfo, effectSettings);
}

void ScriptEffectWithSettings::convertImage(const QImage* source, QImage& image, const QVariantList& matrix)
{
    QVariantList args;
    if (source)
    {
        args << *source;
    }
    args << matrix;
    QVariant result = mScriptModel->call(mFunctionInfo.name, args);
    image = result.value<QImage>();

    QSettings().setValue(PREFIX + mFunctionInfo.name, matrix);
}
