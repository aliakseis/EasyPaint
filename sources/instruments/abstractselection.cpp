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

#include "abstractselection.h"
#include "../imagearea.h"
#include "../undocommand.h"
#include "math.h"

#include <QPainter>

static QPoint clampPointToRect(const QRect& r, const QPoint& p)
{
    return { qBound(r.left(), p.x(), r.right()), qBound(r.top(), p.y(), r.bottom()) };
}


AbstractSelection::AbstractSelection(QObject *parent) :
    AbstractInstrument(parent)
{
    mIsSelectionExists = mIsSelectionMoving = mIsSelectionResizing
            = mIsPaint = mIsImageSelected = mIsMouseMoved
            = mIsSelectionAdjusting = false;
}

void AbstractSelection::mousePressEvent(QMouseEvent *event, ImageArea &imageArea)
{
    const auto pos = event->pos();
    const auto topLeftPoint = mTopLeftPoint * imageArea.getZoomFactor();
    const auto bottomRightPoint = mBottomRightPoint * imageArea.getZoomFactor();

    mButton = event->button();
    mIsMouseMoved = false;
    if (mIsSelectionExists)
    {
        applyStash(imageArea);
        paint(imageArea);
        if (mButton == Qt::RightButton)
        {
            mIsSelectionAdjusting = true;
            startAdjusting(imageArea);
        }
        if (pos.x() > topLeftPoint.x() &&
            pos.x() < bottomRightPoint.x() &&
            pos.y() > topLeftPoint.y() &&
            pos.y() < bottomRightPoint.y())
        {
            if (!mIsSelectionAdjusting)
            {
                makeUndoCommand(imageArea);
            }
            if (!mIsImageSelected)
            {
                startMoving(imageArea);
                if (!mIsSelectionAdjusting)
                {
                    mIsImageSelected = true;
                }
            } 
            else
            {
                drawBorder(imageArea);
            }
            mIsSelectionMoving = true;
            mMoveDiffPoint = mBottomRightPoint - pos / imageArea.getZoomFactor();
            return;
        }
        else if (pos.x() >= bottomRightPoint.x() &&
            pos.x() <= bottomRightPoint.x() + 6 &&
            pos.y() >= bottomRightPoint.y() &&
            pos.y() <= bottomRightPoint.y() + 6)
        {
            if (!mIsSelectionAdjusting)
            {
                makeUndoCommand(imageArea);
            }
            startResizing(imageArea);
            mIsSelectionResizing = true;
            return;
        }
        else
        {
            clearSelection(imageArea);
        }
    }
    if (event->button() == Qt::LeftButton)
    {
        mBottomRightPoint = mTopLeftPoint = clampPointToRect(
            { { 0, 0 }, imageArea.getImage()->size() },
            event->pos() / imageArea.getZoomFactor());
        mHeight =  mWidth = 0;
        stash(imageArea);
        startSelection(imageArea);
        mIsPaint = true;
    }
}

void AbstractSelection::mouseMoveEvent(QMouseEvent *event, ImageArea &imageArea)
{
    const auto pos = clampPointToRect({ { 0, 0 }, imageArea.getImage()->size() },
        event->pos() / imageArea.getZoomFactor());

    mIsMouseMoved = true;
    if (mIsSelectionExists)
    {
        if (mIsSelectionMoving)
        {
            mBottomRightPoint = pos + mMoveDiffPoint;
            mTopLeftPoint = pos + mMoveDiffPoint -
                                  QPoint(mWidth - 1, mHeight - 1);
            applyStash(imageArea);
            move(imageArea);
            drawBorder(imageArea);
            mIsPaint = false;
        }
        else if (mIsSelectionResizing)
        {
            mBottomRightPoint = pos;
            mHeight = fabs(mTopLeftPoint.y() - mBottomRightPoint.y()) + 1;
            mWidth = fabs(mTopLeftPoint.x() - mBottomRightPoint.x()) + 1;
            applyStash(imageArea);
            resize(imageArea);
            drawBorder(imageArea);
            mIsPaint = false;
        }
    }
    if (mIsPaint)
    {
        mBottomRightPoint = pos;
        mHeight = fabs(mTopLeftPoint.y() - mBottomRightPoint.y()) + 1;
        mWidth = fabs(mTopLeftPoint.x() - mBottomRightPoint.x()) + 1;
        applyStash(imageArea);
        drawBorder(imageArea);
        select(imageArea);
    }
    updateCursor(event, imageArea);
}

void AbstractSelection::mouseReleaseEvent(QMouseEvent *event, ImageArea &imageArea)
{
    std::tie(mTopLeftPoint.rx(), mBottomRightPoint.rx()) = std::minmax(mTopLeftPoint.x(), mBottomRightPoint.x());
    std::tie(mTopLeftPoint.ry(), mBottomRightPoint.ry()) = std::minmax(mTopLeftPoint.y(), mBottomRightPoint.y());
    if (mIsSelectionExists)
    {
        updateCursor(event, imageArea);
        if (mButton == Qt::RightButton && !mIsMouseMoved)
        {
            showMenu(imageArea);
            paint(imageArea);
            drawBorder(imageArea);
            mIsPaint = false;
            mIsSelectionMoving = mIsImageSelected = false;
        }
        else if (mIsSelectionMoving)
        {
            applyStash(imageArea);
            completeMoving(imageArea);
            paint(imageArea);
            drawBorder(imageArea);
            mIsPaint = false;
            mIsSelectionMoving = false;
        }
        else if (mIsSelectionResizing)
        {
            applyStash(imageArea);
            paint(imageArea);
            completeResizing(imageArea);
            paint(imageArea);
            drawBorder(imageArea);
            mIsPaint = false;
            mIsSelectionResizing = false;
        }
    }
    if (mIsPaint)
    {
        if (event->button() == Qt::LeftButton)
        {
            applyStash(imageArea);
            if (mTopLeftPoint != mBottomRightPoint)
            {
                applyStash(imageArea);
                paint(imageArea);
                completeSelection(imageArea);
                paint(imageArea);
                mIsSelectionExists = true;
            }
            drawBorder(imageArea);
            mIsPaint = false;
        }
    }
    mIsSelectionAdjusting = false;
}

void AbstractSelection::drawBorder(ImageArea &imageArea)
{
    if (mWidth > 1 && mHeight > 1)
    {
        QPainter painter(imageArea.getImage());
        painter.setPen(QPen(Qt::blue, qMax(1, int(1 / imageArea.getZoomFactor())), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBackgroundMode(Qt::TransparentMode);
        if(mTopLeftPoint != mBottomRightPoint)
        {
            painter.drawRect(QRect(mTopLeftPoint, mBottomRightPoint - QPoint(1, 1)));
        }
        imageArea.setEdited(true);
        painter.end();
        imageArea.update();
    }
}

void AbstractSelection::clearSelection(ImageArea &imageArea)
{
    if (mIsSelectionExists)
    {
        applyStash(imageArea);
        paint(imageArea);
        stash(imageArea);
        mIsSelectionExists = mIsSelectionMoving = mIsSelectionResizing
                = mIsPaint = mIsImageSelected = false;
        imageArea.update(); 
        imageArea.restoreCursor();
        clear();
    }
}

void AbstractSelection::saveImageChanges(ImageArea &)
{
}

void AbstractSelection::updateCursor(QMouseEvent *event, ImageArea &imageArea)
{
    const auto pos = event->pos();
    const auto topLeftPoint = mTopLeftPoint * imageArea.getZoomFactor();
    const auto bottomRightPoint = mBottomRightPoint * imageArea.getZoomFactor();

    if (mIsSelectionExists)
    {
        if (pos.x() > topLeftPoint.x() &&
            pos.x() < bottomRightPoint.x() &&
            pos.y() > topLeftPoint.y() &&
            pos.y() < bottomRightPoint.y())
        { 
            imageArea.setCursor(Qt::SizeAllCursor);
        }
        else if (pos.x() >= bottomRightPoint.x() &&
            pos.x() <= bottomRightPoint.x() + 6 &&
            pos.y() >= bottomRightPoint.y() &&
            pos.y() <= bottomRightPoint.y() + 6)
        {
            imageArea.setCursor(Qt::SizeFDiagCursor);
        }
        else
        {
            imageArea.restoreCursor();
        }
    }
    else
    {
        imageArea.restoreCursor();
    }
}
