#include "scripteffect.h"

#include "../imagearea.h"

#include "../ScriptModel.h"

void ScriptEffect::applyEffect(ImageArea& imageArea)
{
    QVariantList args;
    args << *imageArea.getImage();
    QVariant result = mScriptModel->call(mFunctionInfo.name, args);
    imageArea.setImage(result.value<QImage>());
    imageArea.fixSize(true);
    imageArea.setEdited(true);
    imageArea.update();
}
