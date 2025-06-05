#include "scripteffectwithsettings.h"

#include "../widgets/scripteffectsettings.h"

#include "../ScriptModel.h"

#include <QSettings>
//#include <QEventLoop>
//#include <QFuture>
//#include <QtConcurrent>

const char PREFIX[] = "/ScriptEffectSettings/";

ScriptEffectWithSettings::ScriptEffectWithSettings(ScriptModel* scriptModel, const FunctionInfo& functionInfo, QObject* parent)
    : EffectWithSettings(parent), mScriptModel(scriptModel), mFunctionInfo(functionInfo) 
{
    connect(scriptModel, &ScriptModel::sendImage, this, &ScriptEffectWithSettings::sendImage);
}

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

    /*
    QFuture<QImage> future = QtConcurrent::run([this, args]() {
        QVariant result = mScriptModel->call(mFunctionInfo.name, args);
        return result.value<QImage>();
        });

    QFutureWatcher<QImage> watcher;
    QEventLoop loop;  // Message loop to keep UI responsive

    QObject::connect(&watcher, &QFutureWatcher<QImage>::finished, [&]() {
        loop.quit();  // Quit loop when future completes
        });

    watcher.setFuture(future);
    loop.exec();  // Start message pumping (UI remains responsive)

    image = watcher.result();  // Get the final image after loop finishes
    */

    QSettings().setValue(PREFIX + mFunctionInfo.name, matrix);
}
