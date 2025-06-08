#include "scripteffectwithsettings.h"

#include "../widgets/scripteffectsettings.h"

#include "../ScriptModel.h"

#include <QSettings>

const char PREFIX[] = "/ScriptEffectSettings/";

ScriptEffectWithSettings::ScriptEffectWithSettings(ScriptModel* scriptModel, const FunctionInfo& functionInfo, QObject* parent)
    : EffectWithSettings(parent), mScriptModel(scriptModel), mFunctionInfo(functionInfo) 
{
}

AbstractEffectSettings* ScriptEffectWithSettings::getSettingsWidget()
{
    auto effectSettings = QSettings().value(PREFIX + mFunctionInfo.name).toList();
    return new ScriptEffectSettings(mFunctionInfo, effectSettings);
}

void ScriptEffectWithSettings::convertImage(const QImage* source, QImage& image, const QVariantList& matrix, std::weak_ptr<EffectRunCallback> callback)
{
    QVariantList args;
    if (source)
    {
        args << *source;
    }
    args << matrix;
    
    QVariant result = mScriptModel->call(mFunctionInfo.name, args, callback);
    image = result.value<QImage>();

    QSettings().setValue(PREFIX + mFunctionInfo.name, matrix);
}
