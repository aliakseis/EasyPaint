/*
 * This source file is part of EasyPaint.
 *
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cmath>

#include "effectwithsettings.h"
#include "../imagearea.h"
#include "../dialogs/effectsettingsdialog.h"

EffectWithSettings::EffectWithSettings(QObject *parent) :
    AbstractEffect(parent)
{
}

ImageArea* EffectWithSettings::applyEffect(ImageArea* imageArea)
{
    EffectSettingsDialog dlg(imageArea? imageArea->getImage() : nullptr, this);
    connect(this, &EffectWithSettings::sendImage, &dlg, &EffectSettingsDialog::updatePreview);
    if(dlg.exec())
    {
        makeUndoCommand(imageArea);

        if (!imageArea)
        {
            imageArea = initializeNewTab();
        }

        imageArea->setImage(dlg.getChangedImage());
        imageArea->fixSize(true);
        imageArea->setEdited(true);
        imageArea->update();
    }

    return imageArea;
}
