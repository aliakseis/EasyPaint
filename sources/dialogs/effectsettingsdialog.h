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

#ifndef ABSTRACTEFFECTSDIALOG_H
#define ABSTRACTEFFECTSDIALOG_H

#include <QDialog>
#include <QPushButton>

#include <QWheelEvent>
#include <QGraphicsView>


class EffectWithSettings;
class AbstractEffectSettings;


class EffectSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EffectSettingsDialog(const QImage* img, const QImage* markup, EffectWithSettings* effectWithSettings, QWidget *parent = 0);
    ~EffectSettingsDialog();
    
    QImage getChangedImage();
signals:
    
public slots:
    void updatePreview(const QImage& image);
    void accept() override;
    void reject() override;

protected:
    void wheelEvent(QWheelEvent* event) override {
        const double scaleFactor = 1.15;  // Adjust zoom speed
        if (event->angleDelta().y() > 0) {
            zoomFactor *= scaleFactor;
        }
        else {
            zoomFactor /= scaleFactor;
        }
        mPreviewView->setTransform(QTransform().scale(zoomFactor, zoomFactor));
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton) {
            mPreviewView->setDragMode(QGraphicsView::ScrollHandDrag);
        }
        QDialog::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton) {
            mPreviewView->setDragMode(QGraphicsView::NoDrag);
        }
        QDialog::mouseReleaseEvent(event);
    }

private:
    QPushButton *mOkButton;
    QPushButton *mCancelButton;
    QPushButton *mApplyButton;
    QPushButton* mInterruptButton;

    EffectWithSettings* mEffectWithSettings;
    AbstractEffectSettings *mSettingsWidget;

    QGraphicsView* mPreviewView;
    QGraphicsScene* mPreviewScene;
    //QGraphicsPixmapItem* mPreviewPixmapItem;
    double zoomFactor = 1.;

    const QImage* mSourceImage;
    const QImage* mMarkupImage;
    QImage mImage;

    bool mApplyNeeded = true;

    class FutureContext;
    std::unique_ptr<FutureContext> mFutureContext;

    bool mAccepted = false;

private slots:
    void applyMatrix();
    void onParametersChanged();
    void onInterrupt();
};

#endif // ABSTRACTEFFECTSDIALOG_H
