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

#include "undocommand.h"

UndoCommand::UndoCommand(ImageArea &imgArea, QUndoCommand *parent)
    : QUndoCommand(parent), mPrevImage(*imgArea.getImage()), mPrevMarkup(*imgArea.getMarkup()), mImageArea(imgArea)
{
    mCurrImage = mPrevImage;
    mCurrMarkup = mPrevMarkup;
}

void UndoCommand::undo()
{
    mImageArea.clearSelection();
    mCurrImage = *(mImageArea.getImage());
    mCurrMarkup = *(mImageArea.getMarkup());
    mImageArea.setImage(mPrevImage);
    mImageArea.setMarkup(mPrevMarkup);
    mImageArea.fixSize(true);
    mImageArea.update();
    mImageArea.saveImageChanges();
}

void UndoCommand::redo()
{
    mImageArea.setImage(mCurrImage);
    mImageArea.setMarkup(mCurrMarkup);
    mImageArea.fixSize(true);
    mImageArea.update();
    mImageArea.saveImageChanges();
}
