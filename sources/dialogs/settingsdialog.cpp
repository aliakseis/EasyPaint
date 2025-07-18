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

#include "settingsdialog.h"
#include "../datasingleton.h"
#include "../widgets/shortcutedit.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent)
{
    initializeGui();
    layout()->setSizeConstraint(QLayout::SetMinimumSize);
    setWindowTitle(tr("Settings"));

    // After layout is initialized and widgets added
    adjustSize();  // Compute natural size based on layout

    // Now scale the width proportionally (e.g. 1.3x wider)
    QSize size = this->size();
    resize(static_cast<int>(size.width() * 1.3), size.height());
}

SettingsDialog::~SettingsDialog()
{

}

// Helper function to create language selection UI
QGroupBox* SettingsDialog::createLanguageSettings() {
    QLabel* label = new QLabel(tr("Language:"));
    mLanguageBox = new QComboBox();
    mLanguageBox->addItems({ tr("<System>"), "English", "Czech", "French", "Russian", "Chinese" });
    mLanguageBox->setCurrentIndex(getLanguageIndex());

    QHBoxLayout* layout = new QHBoxLayout();
    layout->addWidget(label);
    layout->addWidget(mLanguageBox);

    QLabel* noteLabel = new QLabel(tr("Note: Language changing requires application restart"));

    QVBoxLayout* vLayout = new QVBoxLayout();
    vLayout->addLayout(layout);
    vLayout->addWidget(noteLabel);

    QGroupBox* groupBox = new QGroupBox(tr("Language Settings"));
    groupBox->setLayout(vLayout);

    return groupBox;
}

// Helper function to create general UI settings
QGroupBox* SettingsDialog::createUISettings() {
    mIsRestoreWindowSize = new QCheckBox(tr("Restore window size on start"));
    mIsRestoreWindowSize->setChecked(DataSingleton::Instance()->getIsRestoreWindowSize());

    mIsAskCanvasSize = new QCheckBox(tr("Ask canvas size on new image creation"));
    mIsAskCanvasSize->setChecked(DataSingleton::Instance()->getIsAskCanvasSize());

    mIsDarkMode = new QCheckBox(tr("Enable dark mode"));
    mIsDarkMode->setChecked(DataSingleton::Instance()->getIsDarkMode());

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(mIsRestoreWindowSize);
    layout->addWidget(mIsAskCanvasSize);
    layout->addWidget(mIsDarkMode);

    QGroupBox* groupBox = new QGroupBox(tr("User Interface"));
    groupBox->setLayout(layout);

    return groupBox;
}

// Helper function to create image settings
QGroupBox* SettingsDialog::createImageSettings() {
    QLabel* labelSize = new QLabel(tr("Base size:"));
    QLabel* labelSep = new QLabel(" x ");
    mWidth = new QSpinBox();
    mWidth->setRange(1, 9999);
    mWidth->setValue(DataSingleton::Instance()->getBaseSize().width());

    mHeight = new QSpinBox();
    mHeight->setRange(1, 9999);
    mHeight->setValue(DataSingleton::Instance()->getBaseSize().height());

    QHBoxLayout* sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(mWidth);
    sizeLayout->addWidget(labelSep);
    sizeLayout->addWidget(mHeight);

    QLabel* labelHistoryDepth = new QLabel(tr("History depth:"));
    mHistoryDepth = new QSpinBox();
    mHistoryDepth->setRange(1, 99);
    mHistoryDepth->setValue(DataSingleton::Instance()->getHistoryDepth());
    mHistoryDepth->setFixedWidth(80);

    mIsAutoSave = new QCheckBox(tr("Autosave"));
    mIsAutoSave->setChecked(DataSingleton::Instance()->getIsAutoSave());

    QLabel* labelAutoSave = new QLabel(tr("Autosave interval (sec):"));
    mAutoSaveInterval = new QSpinBox();
    mAutoSaveInterval->setRange(1, 3000);
    mAutoSaveInterval->setValue(DataSingleton::Instance()->getAutoSaveInterval());
    mAutoSaveInterval->setFixedWidth(80);

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->addWidget(labelSize, 0, 0);
    gridLayout->addLayout(sizeLayout, 0, 1);
    gridLayout->addWidget(labelHistoryDepth, 1, 0);
    gridLayout->addWidget(mHistoryDepth, 1, 1);
    gridLayout->addWidget(mIsAutoSave, 2, 0);
    gridLayout->addWidget(labelAutoSave, 3, 0);
    gridLayout->addWidget(mAutoSaveInterval, 3, 1);

    QGroupBox* groupBox = new QGroupBox(tr("Image Settings"));
    groupBox->setLayout(gridLayout);

    return groupBox;
}

// Helper function to create keyboard shortcut UI
QGroupBox* SettingsDialog::createKeyboardSettings() {
    mShortcutsTree = new QTreeWidget();
    mShortcutsTree->setHeaderLabels({ tr("Command"), tr("Shortcut") });
    connect(mShortcutsTree, &QTreeWidget::itemSelectionChanged, this, &SettingsDialog::itemSelectionChanged);

    createItemsGroup("File", DataSingleton::Instance()->getFileShortcuts());
    createItemsGroup("Edit", DataSingleton::Instance()->getEditShortcuts());
    createItemsGroup("Instruments", DataSingleton::Instance()->getInstrumentsShortcuts());
    createItemsGroup("Tools", DataSingleton::Instance()->getToolsShortcuts());

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(mShortcutsTree);

    QGroupBox* groupBox = new QGroupBox(tr("Keyboard Shortcuts"));
    groupBox->setLayout(layout);

    return groupBox;
}

// Helper function to create shortcut editing UI
QGroupBox* SettingsDialog::createShortcutSettings() {
    QLabel* label = new QLabel(tr("Key sequence:"));
    mShortcutEdit = new ShortcutEdit();
    mShortcutEdit->setEnabled(false);
    connect(mShortcutEdit, &ShortcutEdit::textChanged, this, &SettingsDialog::textChanged);

    QHBoxLayout* layout = new QHBoxLayout();
    layout->addWidget(label);
    layout->addWidget(mShortcutEdit);

    QGroupBox* groupBox = new QGroupBox(tr("Shortcut Settings"));
    groupBox->setLayout(layout);

    return groupBox;
}

// Helper function to create script - loading settings
QGroupBox* SettingsDialog::createScriptSettings()
{
    // Script Section

    mLoadScriptCheckbox = new QCheckBox(tr("Load Script"));
    mLoadScriptCheckbox->setChecked(
        DataSingleton::Instance()->getIsLoadScript()
    );

    QLabel* scriptPathLabel = new QLabel(tr("Script Path:"));
    mScriptPathInput = new QLineEdit(
        DataSingleton::Instance()->getScriptPath()
    );
    mScriptPathInput->setEnabled(mLoadScriptCheckbox->isChecked());

    QPushButton* chooseScriptBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x84"));
    chooseScriptBtn->setEnabled(mLoadScriptCheckbox->isChecked());
    chooseScriptBtn->setFixedWidth(chooseScriptBtn->fontMetrics().height() * 2);

    // toggle enabling of script widgets
    connect(mLoadScriptCheckbox, &QCheckBox::toggled,
        this, [=](bool on) {
            mScriptPathInput->setEnabled(on);
            chooseScriptBtn->setEnabled(on);
        });

    // file-picker for .py
    connect(chooseScriptBtn, &QPushButton::clicked, this, [=]() {
        QString f = QFileDialog::getOpenFileName(
            this,
            tr("Select Script"),
            QString(),
            tr("Python Scripts (*.py);;All Files (*)")
        );
        if (!f.isEmpty())
            mScriptPathInput->setText(f);
        });

    QHBoxLayout* scriptLayout = new QHBoxLayout;
    scriptLayout->addWidget(scriptPathLabel);
    scriptLayout->addWidget(mScriptPathInput);
    scriptLayout->addWidget(chooseScriptBtn);

    // Virtual-Env Section

    //mUseVenvCheckbox = new QCheckBox(tr("Use Virtual Environment"));
    //mUseVenvCheckbox->setChecked(
    //    DataSingleton::Instance()->getUseVirtualEnv()
    //);

    QLabel* venvPathLabel = new QLabel(tr("Venv Path:"));
    mVenvPathInput = new QLineEdit(
        DataSingleton::Instance()->getVirtualEnvPath()
    );
    mVenvPathInput->setClearButtonEnabled(true);
    mVenvPathInput->setEnabled(mLoadScriptCheckbox->isChecked());

    QPushButton* chooseVenvBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x82"));
    chooseVenvBtn->setEnabled(mLoadScriptCheckbox->isChecked());
    chooseVenvBtn->setFixedWidth(chooseVenvBtn->fontMetrics().height() * 2);

    // toggle enabling of venv widgets
    connect(mLoadScriptCheckbox, &QCheckBox::toggled,
        this, [=](bool on) {
            mVenvPathInput->setEnabled(on);
            chooseVenvBtn->setEnabled(on);
        });

    // folder-picker for virtual env directory
    connect(chooseVenvBtn, &QPushButton::clicked, this, [=]() {
        QString d = QFileDialog::getExistingDirectory(
            this,
            tr("Select Virtual Environment"),
            QString(),
            QFileDialog::ShowDirsOnly |
            QFileDialog::DontResolveSymlinks
        );
        if (!d.isEmpty())
            mVenvPathInput->setText(d);
        });

    QHBoxLayout* venvLayout = new QHBoxLayout;
    venvLayout->addWidget(venvPathLabel);
    venvLayout->addWidget(mVenvPathInput);
    venvLayout->addWidget(chooseVenvBtn);

    // Combine Everything

    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(mLoadScriptCheckbox);
    vLayout->addLayout(scriptLayout);
    vLayout->addSpacing(10);               // small gap
    //vLayout->addWidget(mUseVenvCheckbox);
    vLayout->addLayout(venvLayout);

    QGroupBox* groupBox = new QGroupBox(tr("Script & Virtual-Env Settings"));
    groupBox->setLayout(vLayout);
    return groupBox;
}

// Main function to initialize GUI
void SettingsDialog::initializeGui() {
    QTabWidget* tabWidget = new QTabWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    // **Tab 1: User Interface & Language**
    QVBoxLayout* uiLanguageLayout = new QVBoxLayout();
    uiLanguageLayout->addWidget(createLanguageSettings());
    uiLanguageLayout->addWidget(createUISettings());

    QWidget* uiLanguageTab = new QWidget();
    uiLanguageTab->setLayout(uiLanguageLayout);
    tabWidget->addTab(uiLanguageTab, tr("General"));

    // **Tab 2: Image Settings & Script**
    QVBoxLayout* imageScriptLayout = new QVBoxLayout();
    imageScriptLayout->addWidget(createImageSettings());
    imageScriptLayout->addWidget(createScriptSettings());

    QWidget* imageScriptTab = new QWidget();
    imageScriptTab->setLayout(imageScriptLayout);
    tabWidget->addTab(imageScriptTab, tr("Image"));

    // **Tab 3: Keyboard Shortcuts**
    QVBoxLayout* keyboardLayout = new QVBoxLayout();
    keyboardLayout->addWidget(createKeyboardSettings());
    keyboardLayout->addWidget(createShortcutSettings());

    QWidget* keyboardTab = new QWidget();
    keyboardTab->setLayout(keyboardLayout);
    tabWidget->addTab(keyboardTab, tr("Keyboard"));
}

int SettingsDialog::getLanguageIndex()
{
    QStringList languages;
    languages<<"system"<<"easypaint_en_EN"<<"easypaint_cs_CZ"<<"easypaint_fr_FR"<<"easypaint_ru_RU";
    return languages.indexOf(DataSingleton::Instance()->getAppLanguage());
}

void SettingsDialog::sendSettingsToSingleton()
{
    DataSingleton::Instance()->setBaseSize(QSize(mWidth->value(), mHeight->value()));
    DataSingleton::Instance()->setHistoryDepth(mHistoryDepth->value());
    DataSingleton::Instance()->setIsAutoSave(mIsAutoSave->isChecked());
    DataSingleton::Instance()->setIsRestoreWindowSize(mIsRestoreWindowSize->isChecked());
    DataSingleton::Instance()->setIsAskCanvasSize(mIsAskCanvasSize->isChecked());
    DataSingleton::Instance()->setIsDarkMode(mIsDarkMode->isChecked());
    DataSingleton::Instance()->setAutoSaveInterval(mAutoSaveInterval->value());
    DataSingleton::Instance()->setIsLoadScript(mLoadScriptCheckbox->isChecked());
    DataSingleton::Instance()->setScriptPath(mScriptPathInput->text());
    DataSingleton::Instance()->setVirtualEnvPath(mVenvPathInput->text());

    QStringList languages;
    languages << "system" << "easypaint_en_EN" << "easypaint_cs_CZ" << "easypaint_fr_FR" << "easypaint_ru_RU" << "easypaint_zh_CN";
    DataSingleton::Instance()->setAppLanguage(languages.at(mLanguageBox->currentIndex()));

    QTreeWidgetItem *item;
    for(int i(0); i < mShortcutsTree->topLevelItemCount(); i++)
    {
        item = mShortcutsTree->topLevelItem(i);
        for(int y(0); y < item->childCount(); y++)
        {
            if(item->text(0) == "File")
            {
                DataSingleton::Instance()->setFileShortcutByKey(item->child(y)->text(0),
                                                                item->child(y)->data(1, Qt::DisplayRole).value<QKeySequence>());
            }
            else if(item->text(0) == "Edit")
            {
                DataSingleton::Instance()->setEditShortcutByKey(item->child(y)->text(0),
                                                                item->child(y)->data(1, Qt::DisplayRole).value<QKeySequence>());
            }
            else if(item->text(0) == "Instruments")
            {
                DataSingleton::Instance()->setInstrumentShortcutByKey(item->child(y)->text(0),
                                                                item->child(y)->data(1, Qt::DisplayRole).value<QKeySequence>());
            }
            else if(item->text(0) == "Tools")
            {
                DataSingleton::Instance()->setToolShortcutByKey(item->child(y)->text(0),
                                                                item->child(y)->data(1, Qt::DisplayRole).value<QKeySequence>());
            }
        }
    }
}

void SettingsDialog::createItemsGroup(const QString &name, const QMap<QString, QKeySequence> &shortcuts)
{
    QTreeWidgetItem *topLevel = new QTreeWidgetItem(mShortcutsTree);
    mShortcutsTree->addTopLevelItem(topLevel);
    topLevel->setText(0, name);
    topLevel->setExpanded(true);
    QMapIterator<QString, QKeySequence> iterator(shortcuts);
    while(iterator.hasNext())
    {
        iterator.next();
        QTreeWidgetItem *subLevel = new QTreeWidgetItem(topLevel);
        subLevel->setText(0, iterator.key());
        subLevel->setData(1, Qt::DisplayRole, iterator.value());
    }
}

void SettingsDialog::itemSelectionChanged()
{
    if(mShortcutsTree->selectedItems().at(0)->childCount() != 0)
    {
        mShortcutEdit->setEnabled(false);
        mShortcutEdit->clear();
    }
    else
    {
        mShortcutEdit->setEnabled(true);
        mShortcutEdit->setText(mShortcutsTree->selectedItems().at(0)->text(1));
    }
    mShortcutEdit->setFocus();
}

void SettingsDialog::textChanged(const QString &text)
{
    mShortcutsTree->selectedItems().at(0)->setData(1, Qt::DisplayRole, text);
}

void SettingsDialog::reset()
{
    mShortcutEdit->clear();
}
