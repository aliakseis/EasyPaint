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

#ifndef IMAGEAREA_H
#define IMAGEAREA_H

#include "easypaintenums.h"

#include <QWidget>
#include <QImage>

QT_BEGIN_NAMESPACE
class QUndoStack;
QT_END_NAMESPACE

class UndoCommand;
class AbstractInstrument;
class AbstractEffect;

/**
 * @brief Base class which contains view image and controller for painting
 *
 */
class ImageArea : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     *
     * @param isOpen Flag which shows opens a new image or from file.
     * @param filePath Image file path to open.
     * @param parent Pointer for parent.
     */
    explicit ImageArea(bool openFile, bool askCanvasSize, const QString &filePath, QWidget *parent);
    ~ImageArea();

    /**
     * @brief Save image to file with existing path.
     *
     */
    bool save();
    /**
     * @brief Save image to file with unknown path.
     *
     * @return returns true in case of success
     */
    bool saveAs();
    /**
     * @brief Print image.
     *
     */
    void print();
    /**
     * @brief Resize image.
     *
     */
    void resizeImage();
    /**
     * @brief Resize canvas using resize dialog.
     *
     */
    void resizeCanvas();
    void resizeCanvas(int width, int height);
    /**
     * @brief Rotate image.
     *
     * @param flag Rotate to left or to right.
     */
    void rotateImage(bool flag);

    QString getFilePath() { return mFilePath; }
    QString getFileName() { return (mFilePath.isEmpty() ? mFilePath :
                                    mFilePath.split('/').last()); }
    QImage* getImage() { return &mImage; }
    void setImage(const QImage &image) { mImage = image; }
    QImage* getMarkup() { return &mMarkup; }
    void setMarkup(const QImage& image) { mMarkup = image; }
    /**
     * @brief Set flag which shows that image edited.
     *
     * @param flag Boolean flag.
     */
    void setEdited(bool flag) { mIsEdited = flag; }
    /**
     * @brief Get flag which shows that image edited.
     *
     * @return bool Flag.
     */
    bool getEdited() { return mIsEdited; }
    /**
     * @brief applyEffect Apply effect for image.
     * @param effect Name of affect for apply.
     */
    void applyEffect(int effect);
    /**
     * @brief Restores previous cursor image.
     *
     */
    void restoreCursor();
    /**
     * @brief Zoom image
     *
     * @param factor Scale factor
     */
    bool setZoomFactor(qreal factor);
    qreal getZoomFactor() { return mZoomFactor; }
    
    void fixSize(bool cleanUp = false);
    
    QUndoStack* getUndoStack() { return mUndoStack; }
    void setIsPaint(bool isPaint) { mIsPaint = isPaint; }
    bool isPaint() { return mIsPaint; }
    bool isMarkupMode();

    void emitPrimaryColorView() { emit sendPrimaryColorView(); }
    void emitSecondaryColorView() { emit sendSecondaryColorView(); }
    void emitColor(QColor &color) { emit sendColor(color); }
    void emitRestorePreviousInstrument() { emit sendRestorePreviousInstrument(); }

    /**
     * @brief Copying image to the clipboard.
     *
     */
    void copyImage();
    /**
     * @brief Paste image from the clipboard.
     *
     */
    void pasteImage();
    /**
     * @brief Cut image to the clipboard.
     *
     */
    void cutImage();
    /**
     * @brief Save all image changes to image copy.
     *
     */
    void saveImageChanges();
    /**
     * @brief Removes selection borders from image and clears all selection varaibles to default.
     *
     */
    void clearSelection();
    /**
     * @brief Push current image to undo stack.
     *
     */
    void pushUndoCommand(UndoCommand *command);
    
private:
    /**
     * @brief Initialize image with base params.
     *
     */
    void initializeImage();
    /**
     * @brief Open file from file.
     *
     */
    void open();
    /**
     * @brief Open file from file.
     *
     * @param filePath File path
     */
    void open(const QString &filePath);
    /**
     * @brief Draw cursor for instruments 'pencil' and 'lastic', that depends on pencil's width.
     *
     */
    void drawCursor();
    /**
     * @brief Creates filters' strings, which contain supported formats for reading & writing.
     *
     */
    void makeFormatsFilters();

    QImage mImage;  /**< Main image. */
    QImage mMarkup;

    QString mFilePath; /**< Path where located image. */
    QString mOpenFilter; /**< Supported open formats filter. */
    QString mSaveFilter; /**< Supported save formats filter. */
    bool mIsEdited, mIsPaint, mIsResize, mRightButtonPressed;
    bool mIsSavedBeforeResize = false;
    QPixmap *mPixmap;
    QCursor *mCurrentCursor;
    qreal mZoomFactor;
    QUndoStack *mUndoStack;
    AbstractInstrument *mInstrumentHandler;
    QVector<AbstractInstrument*> mInstrumentsHandlers;
    AbstractEffect *mEffectHandler;

signals:
    /**
     * @brief Send primary color for ToolBar.
     *
     */
    void sendPrimaryColorView();
    /**
     * @brief Send secondary color for ToolBar.
     *
     */
    void sendSecondaryColorView();
    void sendNewImageSize(const QSize&);
    void sendCursorPos(const QPoint&);
    void sendColor(const QColor&);
    /**
     * @brief Send signal to restore previous checked instrument for ToolBar.
     *
     */
    void sendRestorePreviousInstrument();
    /**
     * @brief Send instrument for ToolBar.
     *
     */
    void sendSetInstrument(InstrumentsEnum);
    /**
     * @brief Send signal to enable copy cut actions in menu.
     *
     */
    void sendEnableCopyCutActions(bool enable);
    /**
     * @brief Send signal to selection instrument.
     *
     */
    void sendEnableSelectionInstrument(bool enable);
    
private slots:
    void autoSave();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);
    
};

#endif // IMAGEAREA_H
