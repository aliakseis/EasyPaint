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
        if (mFunctionInfo.usesMarkup())
        {
            args << *(imageArea->getMarkup());
        }
    }
    QVariant result = mScriptModel->call(mFunctionInfo.name, args);
    if (result.canConvert<QImage>()) {
        QImage image = result.value<QImage>();
        if (!image.isNull()) {
            if (!imageArea)
            {
                imageArea = initializeNewTab();
            }
            imageArea->setImage(image);
            imageArea->fixSize(true);
            imageArea->setEdited(true);
            imageArea->update();
        }
    }

    return imageArea;
}
