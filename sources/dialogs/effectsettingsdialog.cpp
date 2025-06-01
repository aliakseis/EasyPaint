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
#include "../widgets/imagepreview.h"


#include <QVariant>
#include <QHBoxLayout>
#include <QVBoxLayout>


EffectSettingsDialog::EffectSettingsDialog(const QImage &img, 
    EffectWithSettings* effectWithSettings, QWidget *parent) :
    QDialog(parent), mEffectWithSettings(effectWithSettings), mSourceImage(img)
{
    mSettingsWidget = effectWithSettings->getSettingsWidget();
    connect(mSettingsWidget, &AbstractEffectSettings::parametersChanged,
        this, &EffectSettingsDialog::onParametersChanged);

    mImagePreview = new ImagePreview(&mImage, this);
    mImagePreview->setMinimumSize(320, 320);

    mOkButton = new QPushButton(tr("Ok"), this);
    connect(mOkButton, SIGNAL(clicked()), this, SLOT(applyMatrix()));
    connect(mOkButton, SIGNAL(clicked()), this, SLOT(accept()));
    mCancelButton = new QPushButton(tr("Cancel"), this);
    connect(mCancelButton, SIGNAL(clicked()), this, SLOT(reject()));
    mApplyButton = new QPushButton(tr("Apply"), this);
    connect(mApplyButton, SIGNAL(clicked()), this, SLOT(applyMatrix()));
    connect(mApplyButton, SIGNAL(clicked()), mImagePreview, SLOT(update()));

    QHBoxLayout *hLayout_1 = new QHBoxLayout();

    hLayout_1->addWidget(mImagePreview);
    hLayout_1->addWidget(mSettingsWidget);

    QHBoxLayout *hLayout_2 = new QHBoxLayout();

    hLayout_2->addWidget(mOkButton);
    hLayout_2->addWidget(mCancelButton);
    hLayout_2->addWidget(mApplyButton);

    QVBoxLayout *vLayout = new QVBoxLayout();

    vLayout->addLayout(hLayout_1);
    vLayout->addLayout(hLayout_2);

    setLayout(vLayout);
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
