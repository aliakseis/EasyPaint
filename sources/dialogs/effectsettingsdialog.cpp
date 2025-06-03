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

#include "effectsettingsdialog.h"

#include "../effects/effectwithsettings.h"
#include "../widgets/abstracteffectsettings.h"


#include <QVariant>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>

#include <QApplication>
#include <QScreen>

#include <QTimer>

EffectSettingsDialog::EffectSettingsDialog(const QImage* img, 
    EffectWithSettings* effectWithSettings, QWidget *parent) :
    QDialog(parent), mEffectWithSettings(effectWithSettings), mSourceImage(img)
{
    mSettingsWidget = effectWithSettings->getSettingsWidget();
    connect(mSettingsWidget, &AbstractEffectSettings::parametersChanged,
        this, &EffectSettingsDialog::onParametersChanged);

    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();  // Gets the usable screen area
    auto previewSize = qMax(qMin(screenGeometry.width(), screenGeometry.height()) * 5 / 8, 320);

    // Create GraphicsView for image preview
    mPreviewView = new QGraphicsView(this);
    mPreviewScene = new QGraphicsScene(this);
    mPreviewView->setScene(mPreviewScene);
    mPreviewView->setFixedSize(previewSize, previewSize);

    // Enable smooth panning and zooming
    mPreviewView->setRenderHint(QPainter::Antialiasing);
    mPreviewView->setDragMode(QGraphicsView::ScrollHandDrag);  // Enable panning with mouse drag
    mPreviewView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);  // Zoom where the cursor is

    mOkButton = new QPushButton(tr("Ok"), this);
    connect(mOkButton, SIGNAL(clicked()), this, SLOT(applyMatrix()));
    connect(mOkButton, SIGNAL(clicked()), this, SLOT(accept()));
    mCancelButton = new QPushButton(tr("Cancel"), this);
    connect(mCancelButton, SIGNAL(clicked()), this, SLOT(reject()));
    mApplyButton = new QPushButton(tr("Apply"), this);
    connect(mApplyButton, SIGNAL(clicked()), this, SLOT(applyMatrix()));
    connect(mApplyButton, &QAbstractButton::clicked, 
        [this] { updatePreview(mImage); });

    QHBoxLayout *hLayout_1 = new QHBoxLayout();

    hLayout_1->addWidget(mPreviewView);
    hLayout_1->addWidget(mSettingsWidget);

    QHBoxLayout *hLayout_2 = new QHBoxLayout();

    hLayout_2->addWidget(mOkButton);
    hLayout_2->addWidget(mCancelButton);
    hLayout_2->addWidget(mApplyButton);

    QVBoxLayout *vLayout = new QVBoxLayout();

    vLayout->addLayout(hLayout_1);
    vLayout->addLayout(hLayout_2);

    setLayout(vLayout);

    //Call updatePreview asynchronously after the UI is fully initialized
    if (mSourceImage)
    {
        QTimer::singleShot(0, this, [this]() {
            updatePreview(*mSourceImage);
        });
    }
}

void EffectSettingsDialog::updatePreview(const QImage& image) {
    mPreviewScene->clear();
    auto mPreviewPixmapItem = mPreviewScene->addPixmap(QPixmap::fromImage(image));
    //mPreviewScene->setSceneRect(mPreviewPixmapItem->boundingRect());
    mPreviewView->fitInView(mPreviewPixmapItem, Qt::KeepAspectRatio);
    zoomFactor = mPreviewView->transform().m11();  // Extract current scale from transformation
}

void EffectSettingsDialog::onParametersChanged()
{
    mApplyNeeded = true;
}

void EffectSettingsDialog::applyMatrix()
{
    if (mApplyNeeded)
    {
        mEffectWithSettings->convertImage(mSourceImage, mImage, mSettingsWidget->getEffectSettings());
        mApplyNeeded = false;
    }
}
