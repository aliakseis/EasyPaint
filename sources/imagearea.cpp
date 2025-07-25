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

#include "imagearea.h"
#include "datasingleton.h"
#include "undocommand.h"

#include "instruments/abstractinstrument.h"
#include "instruments/selectioninstrument.h"
#include "instruments/pencilinstrument.h"
#include "instruments/lineinstrument.h"
#include "instruments/eraserinstrument.h"
#include "instruments/rectangleinstrument.h"
#include "instruments/ellipseinstrument.h"
#include "instruments/fillinstrument.h"
#include "instruments/sprayinstrument.h"
#include "instruments/magnifierinstrument.h"
#include "instruments/colorpickerinstrument.h"
#include "instruments/curvelineinstrument.h"
#include "instruments/textinstrument.h"

#include "dialogs/resizedialog.h"

#include "effects/abstracteffect.h"

#include "avir/avir.h"

#include <QApplication>
#include <QPainter>
#include <QFileDialog>
#include <QtCore/QDebug>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPrinter>
#include <QPrintDialog>
#include <QtCore/QTimer>
#include <QImageReader>
#include <QImageWriter>
#include <QUndoStack>
#include <QtCore/QDir>
#include <QMessageBox>
#include <QClipboard>
#include <QBitmap>

namespace {

QImage doResizeImage(const QImage& source, const QSize& newSize)
{
    int step = 0;
    const auto format = source.format();
    switch (format)
    {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        step = 4;
        break;
    case QImage::Format_RGB888:
        step = 3;
        break;
    case QImage::Format_Grayscale8:
        step = 1;
        break;
    default: return source.scaled(newSize);
    }

    avir::CImageResizer<> ImageResizer(8);
    QImage result(newSize, format);
    ImageResizer.resizeImage(source.bits(), source.size().width(), source.size().height(), 
        source.bytesPerLine(), result.bits(), newSize.width(), newSize.height(), result.bytesPerLine(), step, 0);

    return result;
}

void doResizeCanvas(ImageArea *mPImageArea, int width, int height, bool flag, bool resizeWindow)
{
    if(flag)
    {
        ResizeDialog resizeDialog(QSize(width, height), qobject_cast<QWidget *>(mPImageArea->parent()));
        if(resizeDialog.exec() == QDialog::Accepted)
        {
            QSize newSize = resizeDialog.getNewSize();
            width = newSize.width();
            height = newSize.height();
        } else {
            return;
        }
    }

    if(width < 1 || height < 1)
        return;
    {
        QImage tempImage(width, height, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&tempImage);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(Qt::white));
        painter.drawRect(QRect(0, 0, width, height));
        painter.drawImage(0, 0, *mPImageArea->getImage());
        painter.end();
        mPImageArea->setImage(tempImage);
    }
    {
        QImage tempImage(width, height, QImage::Format_Grayscale8);
        QPainter painter(&tempImage);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(Qt::white));
        painter.drawRect(QRect(0, 0, width, height));
        painter.drawImage(0, 0, *mPImageArea->getMarkup());
        painter.end();
        mPImageArea->setMarkup(tempImage);
    }
    if (resizeWindow)
    {
        mPImageArea->fixSize();
    }
    mPImageArea->setEdited(true);
    mPImageArea->clearSelection();
}

}

ImageArea::ImageArea(bool openFile, bool askCanvasSize, const QString &filePath, QWidget *parent) :
    QWidget(parent), mIsEdited(false), mIsPaint(false), mIsResize(false)
{
    setMouseTracking(true);

    mRightButtonPressed = false;
    mFilePath = QString();
    makeFormatsFilters();
    initializeImage();
    mZoomFactor = 1;

    mUndoStack = new QUndoStack(this);
    mUndoStack->setUndoLimit(DataSingleton::Instance()->getHistoryDepth());

    if(openFile)
    {
        if (filePath.isEmpty())
            open();
        else
            open(filePath);
    }
    else
    {
        int width = DataSingleton::Instance()->getBaseSize().width();
        int height = DataSingleton::Instance()->getBaseSize().height();
        if (askCanvasSize)
        {
            QClipboard *globalClipboard = QApplication::clipboard();
            QImage mClipboardImage = globalClipboard->image();
            if (!mClipboardImage.isNull())
            {
                width = mClipboardImage.width();
                height = mClipboardImage.height();
            }
            ResizeDialog resizeDialog(QSize(width, height), this);
            if(resizeDialog.exec() != QDialog::Accepted)
                return;
            QSize newSize = resizeDialog.getNewSize();
            width = newSize.width();
            height = newSize.height();
            doResizeCanvas(this, width, height, false, false);
            mIsEdited = false;
        }
        QPainter *painter = new QPainter(&mImage);
        painter->fillRect(0, 0, width, height, Qt::white);
        painter->end();

        fixSize();
        mFilePath = QString(""); // empty name indicate that user has accepted tab creation
    }

    QTimer *autoSaveTimer = new QTimer(this);
    autoSaveTimer->setInterval(DataSingleton::Instance()->getAutoSaveInterval() * 1000);
    connect(autoSaveTimer, SIGNAL(timeout()), this, SLOT(autoSave()));

    autoSaveTimer->start();

    SelectionInstrument* selectionInstrument = new SelectionInstrument(this);
    connect(selectionInstrument, SIGNAL(sendEnableCopyCutActions(bool)), this, SIGNAL(sendEnableCopyCutActions(bool)));
    connect(selectionInstrument, SIGNAL(sendEnableSelectionInstrument(bool)), this, SIGNAL(sendEnableSelectionInstrument(bool)));

    // Instruments handlers
    mInstrumentsHandlers.fill(0, (int)INSTRUMENTS_COUNT);
    mInstrumentsHandlers[CURSOR] = selectionInstrument;
    mInstrumentsHandlers[PEN] = new PencilInstrument(this);
    mInstrumentsHandlers[LINE] = new LineInstrument(this);
    mInstrumentsHandlers[ERASER] = new EraserInstrument(this);
    mInstrumentsHandlers[RECTANGLE] = new RectangleInstrument(this);
    mInstrumentsHandlers[ELLIPSE] = new EllipseInstrument(this);
    mInstrumentsHandlers[FILL] = new FillInstrument(this);
    mInstrumentsHandlers[SPRAY] = new SprayInstrument(this);
    mInstrumentsHandlers[MAGNIFIER] = new MagnifierInstrument(this);
    mInstrumentsHandlers[COLORPICKER] = new ColorpickerInstrument(this);
    mInstrumentsHandlers[CURVELINE] = new CurveLineInstrument(this);
    mInstrumentsHandlers[TEXT] = new TextInstrument(this);
}

ImageArea::~ImageArea()
{

}

void ImageArea::initializeImage()
{
    const auto size = DataSingleton::Instance()->getBaseSize();
    mImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
    mMarkup = QImage(size, QImage::Format_Grayscale8);
    mMarkup.fill(Qt::white);
}

void ImageArea::open()
{
    QString fileName(mFilePath);
    QFileDialog dialog(this, tr("Open image..."), "", mOpenFilter);
    QString prevPath = DataSingleton::Instance()->getLastFilePath();

    if (!prevPath.isEmpty())
        dialog.selectFile(prevPath);
    else
        dialog.setDirectory(QDir::homePath());

    if (dialog.exec())
    {
        QStringList selectedFiles = dialog.selectedFiles();
        if (!selectedFiles.isEmpty())
        {
          open(selectedFiles.takeFirst());
        }
    }
}



void ImageArea::open(const QString &filePath)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if(mImage.load(filePath))
    {
        mImage = mImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        mMarkup = QImage(mImage.size(), QImage::Format_Grayscale8);
        mMarkup.fill(Qt::white);
        mFilePath = filePath;
        DataSingleton::Instance()->setLastFilePath(filePath);
        fixSize();
        QApplication::restoreOverrideCursor();
    }
    else
    {
        qDebug()<<QString("Can't open file %1").arg(filePath);
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error opening file"), tr("Can't open file \"%1\".").arg(filePath));
    }
}

bool ImageArea::save()
{
    if(mFilePath.isEmpty())
    {
        return saveAs();
    }
    clearSelection();
    if (mImage.save(mFilePath))
    {
        QMessageBox::warning(this, tr("Error saving file"), tr("Can't save file \"%1\".").arg(mFilePath));
        return false;
    }
    mIsEdited = false;
    return true;
}

bool ImageArea::saveAs()
{
    bool result = true;
    QString filter;
    QString fileName(mFilePath);
    clearSelection();
    if(fileName.isEmpty())
    {
        fileName = QDir::homePath() + "/" + tr("Untitled image") + ".png";
    }
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save image..."), fileName, mSaveFilter,
                                                    &filter,
                                                    QFileDialog::DontUseNativeDialog);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    //parse file extension
    if(!filePath.isEmpty())
    {
        QString extension;
        //we should test it on windows, because of different slashes
        QString temp = filePath.split("/").last();
        //if user entered some extension
        if(temp.contains('.'))
        {
            temp = temp.split('.').last();
            if(QImageWriter::supportedImageFormats().contains(temp.toLatin1()))
                extension = temp;
            else
                extension = "png"; //if format is unknown, save it as png format, but with user extension
        }
        else
        {
            extension = filter.split('.').last().remove(')');
            filePath += '.' + extension;
        }

        if(mImage.save(filePath, extension.toLatin1().data()))
        {
            mFilePath = filePath;
            mIsEdited = false;
        }
        else
        {
            QMessageBox::warning(this, tr("Error saving file"), tr("Can't save file \"%1\".").arg(filePath));
            result = false;
        }
    }
    QApplication::restoreOverrideCursor();
    return result;
}

void ImageArea::autoSave()
{
    if(mIsEdited && !mFilePath.isEmpty() && DataSingleton::Instance()->getIsAutoSave())
    {
        if(mImage.save(mFilePath)) {
            mIsEdited = false;
        }
    }
}

void ImageArea::print()
{
    QPrinter *printer = new QPrinter();
    QPrintDialog *printDialog = new QPrintDialog(printer, this);
    if(printDialog->exec())
    {
        QPainter painter(printer);
        QRect rect = painter.viewport();
        QSize size = mImage.size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(mImage.rect());
        painter.drawImage(0, 0, mImage);
    }
}

void ImageArea::resizeImage()
{
    ResizeDialog resizeDialog(getImage()->size(), qobject_cast<QWidget*>(parent()));
    if (resizeDialog.exec() == QDialog::Accepted)
    {
        setImage(doResizeImage(*getImage(), resizeDialog.getNewSize())); //mPImageArea->getImage()->scaled(resizeDialog.getNewSize()));
        setMarkup(doResizeImage(*getMarkup(), resizeDialog.getNewSize()));
        fixSize(true);
        setEdited(true);
    }
}

void ImageArea::resizeCanvas()
{
    doResizeCanvas(this, mImage.width(), mImage.height(), true, true);
    emit sendNewImageSize(mImage.size());
}

void ImageArea::resizeCanvas(int width, int height)
{
    doResizeCanvas(this, width, height, false, true);
    emit sendNewImageSize(mImage.size());
}

void ImageArea::rotateImage(bool flag)
{
    QTransform transform;
    transform.rotate(flag? 90 : -90);
    setImage(getImage()->transformed(transform));
    setMarkup(getMarkup()->transformed(transform));
    resize(getImage()->rect().right() * mZoomFactor + 6, getImage()->rect().bottom() * mZoomFactor + 6);
    update();
    setEdited(true);
    clearSelection();

    emit sendNewImageSize(mImage.size());
}

void ImageArea::applyEffect(int effect)
{
    mEffectHandler = DataSingleton::Instance()->mEffectsHandlers.at(effect);
    mEffectHandler->applyEffect(this);
}

void ImageArea::copyImage()
{
    SelectionInstrument *instrument = static_cast <SelectionInstrument*> 
        (mInstrumentsHandlers.at(CURSOR));
    instrument->copyImage(*this);
}

void ImageArea::pasteImage()
{
    if(DataSingleton::Instance()->getInstrument() != CURSOR)
        emit sendSetInstrument(CURSOR);
    SelectionInstrument *instrument = static_cast <SelectionInstrument*> 
        (mInstrumentsHandlers.at(CURSOR));
    instrument->pasteImage(*this);
}

void ImageArea::cutImage()
{
    SelectionInstrument *instrument = static_cast <SelectionInstrument*> 
        (mInstrumentsHandlers.at(CURSOR));
    instrument->cutImage(*this);
}

void ImageArea::mousePressEvent(QMouseEvent *event)
{
    const auto pos = event->pos() / getZoomFactor();

    if(event->button() == Qt::LeftButton &&
        pos.x() < mImage.rect().right() + 6 &&
        pos.x() > mImage.rect().right() &&
        pos.y() > mImage.rect().bottom() &&
        pos.y() < mImage.rect().bottom() + 6)
    {
        mIsResize = true;
        mIsSavedBeforeResize = false;
        setCursor(Qt::SizeFDiagCursor);
    }
    else if(DataSingleton::Instance()->getInstrument() != NONE_INSTRUMENT)
    {
        mInstrumentHandler = mInstrumentsHandlers.at(
            DataSingleton::Instance()->getInstrument());
        mInstrumentHandler->mousePressEvent(event, *this);
    }
}

void ImageArea::mouseMoveEvent(QMouseEvent *event)
{
    const auto pos = event->pos() / getZoomFactor();

    InstrumentsEnum instrument = DataSingleton::Instance()->getInstrument();
    mInstrumentHandler = mInstrumentsHandlers.at(
        DataSingleton::Instance()->getInstrument());
    if(mIsResize)
    {
        if (!mIsSavedBeforeResize)
        {
            pushUndoCommand(new UndoCommand(*this));
            mIsSavedBeforeResize = true;
        }
        doResizeCanvas(this, pos.x(), pos.y(), false, false);
        update();
        emit sendNewImageSize(mImage.size());
    }
    else if(pos.x() < mImage.rect().right() + 6 &&
        pos.x() > mImage.rect().right() &&
        pos.y() > mImage.rect().bottom() &&
        pos.y() < mImage.rect().bottom() + 6)
    {
        setCursor(Qt::SizeFDiagCursor);
        if (qobject_cast<AbstractSelection*>(mInstrumentHandler))
            return;
    }
    else if (!qobject_cast<AbstractSelection*>(mInstrumentHandler))
    {
        restoreCursor();
    }
    if(pos.x() < mImage.width() &&
        pos.y() < mImage.height())
    {
        emit sendCursorPos(pos);
    }

    if(instrument != NONE_INSTRUMENT)
    {
        mInstrumentHandler->mouseMoveEvent(event, *this);
    }
}

void ImageArea::mouseReleaseEvent(QMouseEvent *event)
{
    if(mIsResize)
    {
        fixSize();
        mIsResize = false;
        mIsSavedBeforeResize = false;
        restoreCursor();
    }
    else if(DataSingleton::Instance()->getInstrument() != NONE_INSTRUMENT)
    {
        mInstrumentHandler = mInstrumentsHandlers.at(
            DataSingleton::Instance()->getInstrument());
        mInstrumentHandler->mouseReleaseEvent(event, *this);
    }
}

void ImageArea::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (mImage.isNull())
    {
        painter.setBrush(QBrush(QPixmap(":media/textures/transparent.jpg")));
        painter.drawRect(rect());
    }
    else
    {
        painter.save();
        painter.scale(mZoomFactor, mZoomFactor);
        painter.drawImage(QPoint(0, 0), mImage);

        // Convert monochrome mask to a QBitmap and then QRegion:
        QImage monoMask = mMarkup.convertToFormat(QImage::Format_Mono);
        QBitmap bitmapMask = QBitmap::fromImage(monoMask);
        QRegion clipRegion(bitmapMask);

        painter.setClipRegion(clipRegion);

        painter.fillRect(mImage.rect(), DataSingleton::Instance()->getPrimaryColor());

        painter.restore();
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::black));
    auto start = mImage.rect().size() * mZoomFactor;
    painter.drawRect(QRect(start.width(), start.height(), 6, 6));
}

void ImageArea::restoreCursor()
{
    switch(DataSingleton::Instance()->getInstrument())
    {
    case INSTRUMENTS_COUNT:
        break;
    case MAGNIFIER:
        mPixmap = new QPixmap(":/media/instruments-icons/cursor_loupe.png");
        mCurrentCursor = new QCursor(*mPixmap);
        setCursor(*mCurrentCursor);
        break;
    case NONE_INSTRUMENT:
        mCurrentCursor = new QCursor(Qt::ArrowCursor);
        setCursor(*mCurrentCursor);
        break;
    case CURSOR:
        mCurrentCursor = new QCursor(Qt::CrossCursor);
        setCursor(*mCurrentCursor);
        break;
    case ERASER: case PEN:
        ImageArea::drawCursor();
        mCurrentCursor = new QCursor(*mPixmap);
        setCursor(*mCurrentCursor);
        break;
    case COLORPICKER:
        mPixmap = new QPixmap(":/media/instruments-icons/cursor_pipette.png");
        mCurrentCursor = new QCursor(*mPixmap);
        setCursor(*mCurrentCursor);
        break;
    case RECTANGLE: case ELLIPSE: case LINE: case CURVELINE: case TEXT:
        mCurrentCursor = new QCursor(Qt::CrossCursor);
        setCursor(*mCurrentCursor);
        break;
    case SPRAY:
        mPixmap = new QPixmap(":/media/instruments-icons/cursor_spray.png");
        mCurrentCursor = new QCursor(*mPixmap);
        setCursor(*mCurrentCursor);
        break;
    case FILL:
        mPixmap = new QPixmap(":/media/instruments-icons/cursor_fill.png");
        mCurrentCursor = new QCursor(*mPixmap);
        setCursor(*mCurrentCursor);
        break;
    }
}

bool ImageArea::setZoomFactor(qreal factor)
{
    auto zoomFactor = mZoomFactor * factor;
    if (zoomFactor <= 0.25)
    {
        if (mZoomFactor == 0.25)
        {
            return false;
        }
        mZoomFactor = 0.25;
    }
    else if (zoomFactor >= 8)
    {
        if (mZoomFactor == 8)
        {
            return false;
        }
        mZoomFactor = 8;
    }
    else
    {
        mZoomFactor = zoomFactor;
    }

    fixSize(true);

    return true;
}

void ImageArea::fixSize(bool cleanUp /*= false*/)
{
    resize(mImage.width() * mZoomFactor + 6, mImage.height() * mZoomFactor + 6);
    if (cleanUp)
    {
        emit sendNewImageSize(mImage.size());
        clearSelection();
    }
}

void ImageArea::drawCursor()
{
    QPainter painter;
    mPixmap = new QPixmap(25, 25);
    QPoint center(13, 13);
    switch(DataSingleton::Instance()->getInstrument())
    {
    case NONE_INSTRUMENT: case LINE: case COLORPICKER: case MAGNIFIER: case  SPRAY:
    case FILL: case RECTANGLE: case ELLIPSE: case CURSOR: case INSTRUMENTS_COUNT:
    case CURVELINE: case TEXT:
        break;
    case PEN: case ERASER:
        mPixmap->fill(QColor(0, 0, 0, 0));
        break;
    }
    painter.begin(mPixmap);
    switch(DataSingleton::Instance()->getInstrument())
    {
    case NONE_INSTRUMENT: case LINE: case COLORPICKER: case MAGNIFIER: case  SPRAY:
    case FILL: case RECTANGLE: case ELLIPSE: case CURSOR: case INSTRUMENTS_COUNT:
    case CURVELINE: case TEXT:
        break;
    case PEN:
        if(mRightButtonPressed)
        {
            painter.setPen(QPen(DataSingleton::Instance()->getSecondaryColor()));
            painter.setBrush(QBrush(DataSingleton::Instance()->getSecondaryColor()));
        }
        else
        {
            painter.setPen(QPen(DataSingleton::Instance()->getPrimaryColor()));
            painter.setBrush(QBrush(DataSingleton::Instance()->getPrimaryColor()));
        }
        painter.drawEllipse(center, DataSingleton::Instance()->getPenSize()/2,
                        DataSingleton::Instance()->getPenSize()/2);
        break;
    case ERASER:
        painter.setBrush(QBrush(Qt::white));
        painter.drawEllipse(center, DataSingleton::Instance()->getPenSize()/2,
                        DataSingleton::Instance()->getPenSize()/2);
        break;
    }
    painter.setPen(Qt::black);
    painter.drawPoint(13, 13);
    painter.drawPoint(13, 3);
    painter.drawPoint(13, 5);
    painter.drawPoint(13, 21);
    painter.drawPoint(13, 23);
    painter.drawPoint(3, 13);
    painter.drawPoint(5, 13);
    painter.drawPoint(21, 13);
    painter.drawPoint(23, 13);
    painter.setPen(Qt::white);
    painter.drawPoint(13, 12);
    painter.drawPoint(13, 14);
    painter.drawPoint(12, 13);
    painter.drawPoint(14, 13);
    painter.drawPoint(13, 4);
    painter.drawPoint(13, 6);
    painter.drawPoint(13, 20);
    painter.drawPoint(13, 22);
    painter.drawPoint(4, 13);
    painter.drawPoint(6, 13);
    painter.drawPoint(20, 13);
    painter.drawPoint(22, 13);
    painter.end();
}

void ImageArea::makeFormatsFilters()
{
    QList<QByteArray> ba = QImageReader::supportedImageFormats();
    //make "all supported" part
    mOpenFilter = "All supported (";
    foreach (QByteArray temp, ba)
        mOpenFilter += "*." + temp + " ";
    mOpenFilter[mOpenFilter.length() - 1] = ')'; //delete last space
    mOpenFilter += ";;";

    //using ";;" as separator instead of "\n", because Qt's docs recomended it :)
    if(ba.contains("png"))
        mOpenFilter += "Portable Network Graphics(*.png);;";
    if(ba.contains("bmp"))
        mOpenFilter += "Windows Bitmap(*.bmp);;";
    if(ba.contains("gif"))
        mOpenFilter += "Graphic Interchange Format(*.gif);;";
    if(ba.contains("jpg") || ba.contains("jpeg"))
        mOpenFilter += "Joint Photographic Experts Group(*.jpg *.jpeg);;";
    if(ba.contains("mng"))
        mOpenFilter += "Multiple-image Network Graphics(*.mng);;";
    if(ba.contains("pbm"))
        mOpenFilter += "Portable Bitmap(*.pbm);;";
    if(ba.contains("pgm"))
        mOpenFilter += "Portable Graymap(*.pgm);;";
    if(ba.contains("ppm"))
        mOpenFilter += "Portable Pixmap(*.ppm);;";
    if(ba.contains("tiff") || ba.contains("tif"))
        mOpenFilter += "Tagged Image File Format(*.tiff, *tif);;";
    if(ba.contains("xbm"))
        mOpenFilter += "X11 Bitmap(*.xbm);;";
    if(ba.contains("xpm"))
        mOpenFilter += "X11 Pixmap(*.xpm);;";
    if(ba.contains("svg"))
        mOpenFilter += "Scalable Vector Graphics(*.svg);;";

    mOpenFilter += "All Files(*.*)";

    //make saveFilter
    ba = QImageWriter::supportedImageFormats();
    if(ba.contains("png"))
        mSaveFilter += "Portable Network Graphics(*.png)";
    if(ba.contains("bmp"))
        mSaveFilter += ";;Windows Bitmap(*.bmp)";
    if(ba.contains("jpg") || ba.contains("jpeg"))
        mSaveFilter += ";;Joint Photographic Experts Group(*.jpg)";
    if(ba.contains("ppm"))
        mSaveFilter += ";;Portable Pixmap(*.ppm)";
    if(ba.contains("tiff") || ba.contains("tif"))
        mSaveFilter += ";;Tagged Image File Format(*.tiff)";
    if(ba.contains("xbm"))
        mSaveFilter += ";;X11 Bitmap(*.xbm)";
    if(ba.contains("xpm"))
        mSaveFilter += ";;X11 Pixmap(*.xpm)";
}

void ImageArea::saveImageChanges()
{
    foreach (AbstractInstrument* instrument, mInstrumentsHandlers)
    {
        if (AbstractSelection *selection = qobject_cast<AbstractSelection*>(instrument))
            selection->saveImageChanges(*this);
    }
}

void ImageArea::clearSelection()
{
    foreach (AbstractInstrument* instrument, mInstrumentsHandlers)
    {
        if (AbstractSelection *selection = qobject_cast<AbstractSelection*>(instrument))
            selection->clearSelection(*this);
    }
}

void ImageArea::pushUndoCommand(UndoCommand *command)
{
    if(command != 0)
        mUndoStack->push(command);
}

bool ImageArea::isMarkupMode()
{
    return DataSingleton::Instance()->isMarkupMode();
}
