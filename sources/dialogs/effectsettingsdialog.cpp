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

#include "SpinnerOverlay.h"

#include <QVariant>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>

#include <QApplication>
#include <QScreen>

#include <QTimer>

#include <QEventLoop>
#include <QFuture>
#include <QtConcurrent>

#include <QLabel>

#include <QMainWindow>
#include <QMessageBox>

static QMainWindow* GetMainWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* w = qobject_cast<QMainWindow*>(widget)) {
            return w;
        }
    }
    return nullptr;
}

bool isDummyImage(const QImage& image) {
    if (image.isNull())
        return true;

    const QColor refColor = image.pixelColor(0, 0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QColor color = image.pixelColor(x, y);
            if (color != refColor) {
                return false;
            }
        }
    }
    return true;
}

class EffectSettingsDialog::FutureContext
{
    QFuture<QImage> mFuture;
    QFutureWatcher<QImage> watcher;
    QMainWindow* mainWindow;

    std::shared_ptr<EffectRunCallback> mEffectRunCallback;

    QMetaObject::Connection mImageConnection;

public:
    FutureContext(EffectSettingsDialog* dlg) : mainWindow(GetMainWindow()),
        mEffectRunCallback(new EffectRunCallback(), std::mem_fn(&QObject::deleteLater))
    {
        QObject::connect(mEffectRunCallback.get(), &EffectRunCallback::sendImage, dlg, &EffectSettingsDialog::updatePreview);

        mFuture = QtConcurrent::run([this, dlg]() {
            QImage result;
            dlg->mEffectWithSettings->convertImage(dlg->mSourceImage, dlg->mMarkupImage, result, dlg->mSettingsWidget->getEffectSettings(),
                mEffectRunCallback);
            return result;
            }),

        watcher.setFuture(mFuture);
        mImageConnection = QObject::connect(&watcher, &QFutureWatcher<QImage>::finished, dlg, [this, dlg]() {
            dlg->updatePreview(watcher.result());
            dlg->mApplyButton->setEnabled(dlg->mApplyNeeded);
            dlg->mInterruptButton->setEnabled(false);
            });
    }

    FutureContext::~FutureContext()
    {
        QObject::disconnect(mImageConnection);
    }

    bool isFinished() const{ return mFuture.isFinished(); }

    QImage getResult(bool disableUI)
    {
        // If already done, just return
        if (mFuture.isFinished())
            return mFuture.result();

        // Connect a watcher to the future
        QFutureWatcher<QImage> watcher;
        watcher.setFuture(mFuture);

        // Setup an event loop that quits when the future is ready
        QEventLoop loop;
        QObject::connect(&watcher, &QFutureWatcher<QImage>::finished,
            &loop, &QEventLoop::quit);

        // Optionally disable the main UI and spin
        std::unique_ptr<SpinnerOverlay> spinner;
        if (disableUI && mainWindow) {
            mainWindow->setEnabled(false);
            spinner.reset(new SpinnerOverlay(mainWindow));
        }

        // Block here (but UI stays responsive)
        loop.exec();

        // Tear down spinner + re-enable UI automatically via RAII
        if (disableUI && mainWindow) {
            mainWindow->setEnabled(true);
        }

        // Finally return the result
        return watcher.result();
    }

    void interrupt() { mEffectRunCallback->interrupt(); }
};

EffectSettingsDialog::EffectSettingsDialog(const QImage* img, const QImage* markup,
    EffectWithSettings* effectWithSettings, QWidget *parent) :
    QDialog(parent? parent : GetMainWindow()), mEffectWithSettings(effectWithSettings), 
        mSourceImage(img), mMarkupImage(markup)
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
    connect(mOkButton, SIGNAL(clicked()), this, SLOT(accept()));
    mCancelButton = new QPushButton(tr("Cancel"), this);
    connect(mCancelButton, SIGNAL(clicked()), this, SLOT(reject()));
    mApplyButton = new QPushButton(tr("Apply"), this);
    connect(mApplyButton, SIGNAL(clicked()), this, SLOT(applyMatrix()));
    mInterruptButton = new QPushButton(tr("Interrupt"), this);
    connect(mInterruptButton, SIGNAL(clicked()), this, SLOT(onInterrupt()));
    mInterruptButton->setEnabled(false);

    QHBoxLayout *hLayout_1 = new QHBoxLayout();

    hLayout_1->addWidget(mPreviewView);
    hLayout_1->addWidget(mSettingsWidget);

    QHBoxLayout *hLayout_2 = new QHBoxLayout();

    hLayout_2->addWidget(mOkButton);
    hLayout_2->addWidget(mCancelButton);
    hLayout_2->addWidget(mApplyButton);
    hLayout_2->addWidget(mInterruptButton);

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

EffectSettingsDialog::~EffectSettingsDialog() = default;

void EffectSettingsDialog::updatePreview(const QImage& image) {
    if (!isDummyImage(image))
    {
        mImage = image;
        if (!mAccepted)
        {
            mPreviewScene->clear();
            auto mPreviewPixmapItem = mPreviewScene->addPixmap(QPixmap::fromImage(image));
            //mPreviewScene->setSceneRect(mPreviewPixmapItem->boundingRect());
            mPreviewView->fitInView(mPreviewPixmapItem, Qt::KeepAspectRatio);
            zoomFactor = mPreviewView->transform().m11();  // Extract current scale from transformation
        }
    }
}

void EffectSettingsDialog::onParametersChanged()
{
    mApplyButton->setEnabled(true);
    mApplyNeeded = true;
}

void EffectSettingsDialog::onInterrupt()
{
    mInterruptButton->setEnabled(false);
    if (mFutureContext) {
        if (mFutureContext->isFinished())
            return;
        mFutureContext->interrupt();
    }
    mFutureContext.reset();
    mApplyButton->setEnabled(true);
    mApplyNeeded = true;
}

void EffectSettingsDialog::applyMatrix()
{
    if (mApplyNeeded)
    {
        if (mFutureContext && !mFutureContext->isFinished()) {
            mFutureContext->interrupt();
        }
        mFutureContext = std::make_unique<FutureContext>(this);
        mApplyNeeded = false;
        mApplyButton->setEnabled(false);
        mInterruptButton->setEnabled(true);
    }
}

QImage  EffectSettingsDialog::getChangedImage() 
{
    if (mFutureContext)
    {
        const auto image = mFutureContext->getResult(true);
        if (!isDummyImage(image))
            mImage = image;
        mFutureContext.reset();
    }
    return mImage; 
}

void EffectSettingsDialog::accept()
{
    if (mApplyNeeded && !!mFutureContext)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Simulation Parameters Changed"));
        msgBox.setText(tr("The simulation parameters have changed. What would you like to do?"));
        msgBox.setIcon(QMessageBox::Question);

        // Add three buttons for clear choices
        QPushButton* startNewBtn = msgBox.addButton(tr("Start New Simulation"), QMessageBox::AcceptRole);
        QPushButton* continueBtn = msgBox.addButton(tr("Continue with Last Data"), QMessageBox::RejectRole);
        QPushButton* stayBtn = msgBox.addButton(tr("Stay on This Screen"), QMessageBox::DestructiveRole);

        msgBox.exec();  // Show the dialog and wait for user choice

        // Handle user selection
        if (msgBox.clickedButton() == startNewBtn) {
            applyMatrix();
        }
        else if (msgBox.clickedButton() == continueBtn) {
            mApplyNeeded = false;
        }
        else {
            // Stay on the screen (no action needed)
            return;
        }
    }
    QDialog::accept();

    mAccepted = true;
}

void EffectSettingsDialog::reject()
{
    onInterrupt();
    QDialog::reject();
}
