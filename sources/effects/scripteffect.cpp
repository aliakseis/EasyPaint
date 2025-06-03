#include "scripteffect.h"

#include "../imagearea.h"

#include "../ScriptModel.h"

ImageArea* ScriptEffect::applyEffect(ImageArea* imageArea)
{
    makeUndoCommand(imageArea);

    QVariantList args;
    if (imageArea)
    {
        args << *(imageArea->getImage());
    }
    QVariant result = mScriptModel->call(mFunctionInfo.name, args);
    if (imageArea)
    {
        imageArea->setEdited(true);
    }
    else
    {
        imageArea = initializeNewTab();
    }
    imageArea->setImage(result.value<QImage>());
    imageArea->fixSize(true);
    imageArea->update();

    return imageArea;
}
