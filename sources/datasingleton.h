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

#ifndef DATASINGLETON_H
#define DATASINGLETON_H

#include <QColor>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QKeySequence>
#include <QFont>

#include "easypaintenums.h"

class AbstractEffect;
class AbstractInstrument;
struct FunctionInfo;
class ScriptModel;

/**
 * @brief Singleton for variables needed for the program.
 *
 */
class DataSingleton : public QObject
{
public:
    /**
     * @brief Instance of singleton (static)
     *
     * @return DataSingleton Pointer of singleton
     */
    static DataSingleton* Instance();

    QColor getPrimaryColor() { return mPrimaryColor; }
    void setPrimaryColor(const QColor &color) { mPrimaryColor = color; }
    QColor getSecondaryColor() { return mSecondaryColor; }
    void setSecondaryColor(const QColor &color) { mSecondaryColor = color; }
    int getPenSize() { return mPenSize; }
    void setPenSize(int size) { mPenSize = size; }
    InstrumentsEnum getInstrument() { return mCurrentInstrument; }
    void setInstrument(const InstrumentsEnum &instrument) { mCurrentInstrument = instrument; mIsResetCurve = true; }
    InstrumentsEnum getPreviousInstrument() { return mPreviousInstrument; }
    void setPreviousInstrument(const InstrumentsEnum &instrument) { mPreviousInstrument = instrument; }
    QSize getBaseSize() { return mBaseSize; }
    void setBaseSize(const QSize &baseSize) { mBaseSize = baseSize; }
    bool getIsAutoSave() { return mIsAutoSave; }
    void setIsAutoSave(bool isAutoSave) { mIsAutoSave = isAutoSave; }
    int getAutoSaveInterval() { return mAutoSaveInterval; }
    void setAutoSaveInterval(int interval) { mAutoSaveInterval = interval; }
    int getHistoryDepth() { return mHistoryDepth; }
    void setHistoryDepth(const int &historyDepth) { mHistoryDepth = historyDepth; }
    QString getAppLanguage() { return mAppLanguage; }
    void setAppLanguage(const QString &appLanguage) { mAppLanguage = appLanguage; }
    bool getIsRestoreWindowSize() { return mIsRestoreWindowSize; }
    void setIsRestoreWindowSize(bool isRestoreWindowSize) { mIsRestoreWindowSize = isRestoreWindowSize; }
    bool getIsAskCanvasSize() { return mIsAskCanvasSize; }
    void setIsAskCanvasSize(bool isAskCanvasSize) { mIsAskCanvasSize = isAskCanvasSize; }
    bool getIsDarkMode() { return mIsDarkMode; }
    void setIsDarkMode(bool isDarkMode) { mIsDarkMode = isDarkMode; }
    bool getIsLoadScript() { return mIsLoadScript; }
    void setIsLoadScript(bool isLoadScript) { mIsLoadScript = isLoadScript; }
    QString getScriptPath() { return mScriptPath; }
    void setScriptPath(const QString& scriptPath) { mScriptPath = scriptPath; }

    bool getIsInitialized() { return mIsInitialized; }
    void setIsInitialized(bool isInitialized = true) { mIsInitialized = isInitialized; }
    QString getLastFilePath() { return mLastFilePath; }
    void setLastFilePath(const QString &lastFilePath) { mLastFilePath = lastFilePath; }
    QSize getWindowSize() { return mWindowSize; }
    void setWindowSize(const QSize &winSize) { mWindowSize = winSize; }
    QFont getTextFont() { return mTextFont; }
    void setTextFont(const QFont& textFont) { mTextFont = textFont; }
    QMap<QString, QKeySequence> getFileShortcuts() { return mFileShortcuts; }
    QKeySequence getFileShortcutByKey(const QString &key) { return mFileShortcuts[key]; }
    void setFileShortcutByKey(const QString &key, const QKeySequence &value) { mFileShortcuts[key] = value; }
    QMap<QString, QKeySequence> getEditShortcuts() { return mEditShortcuts; }
    QKeySequence getEditShortcutByKey(const QString &key) { return mEditShortcuts[key]; }
    void setEditShortcutByKey(const QString &key, const QKeySequence &value) { mEditShortcuts[key] = value; }
    QMap<QString, QKeySequence> getInstrumentsShortcuts() { return mInstrumentsShortcuts; }
    QKeySequence getInstrumentShortcutByKey(const QString &key) { return mInstrumentsShortcuts[key]; }
    void setInstrumentShortcutByKey(const QString &key, const QKeySequence &value) { mInstrumentsShortcuts[key] = value; }
    QMap<QString, QKeySequence> getToolsShortcuts() { return mToolsShortcuts; }
    QKeySequence getToolShortcutByKey(const QString &key) { return mToolsShortcuts[key]; }
    void setToolShortcutByKey(const QString &key, const QKeySequence &value) { mToolsShortcuts[key] = value; }

    //Needs for correct work of Bezier curve instrument
    void setResetCurve(bool b) { mIsResetCurve = b; }
    bool isResetCurve() { return mIsResetCurve; }

    void readSetting();
    void writeSettings();
    void readState();
    void writeState();

    QVector<AbstractEffect*> mEffectsHandlers;
    int addScriptActionHandler(ScriptModel* scriptModel, const FunctionInfo& functionInfo);

private:
    DataSingleton();
    DataSingleton(DataSingleton const&) = delete;
    DataSingleton& operator=(DataSingleton const&) = delete;

    static DataSingleton* m_pInstance;
    QColor mPrimaryColor,
           mSecondaryColor;
    int mPenSize;
    InstrumentsEnum mCurrentInstrument, mPreviousInstrument;
    QSize mBaseSize, mWindowSize;
    bool mIsAutoSave, mIsRestoreWindowSize, mIsAskCanvasSize, mIsDarkMode;
    bool mIsLoadScript;
    QString mScriptPath;

    bool mIsInitialized;
    bool mIsResetCurve; /**< Needs to correct work of Bezier curve instrument */
    int mAutoSaveInterval, mHistoryDepth;
    QString mAppLanguage;
    QString mLastFilePath; /* last opened file */
    QFont mTextFont;
    QMap<QString, QKeySequence> mFileShortcuts, mEditShortcuts, mInstrumentsShortcuts, mToolsShortcuts;

};

#endif // DATASINGLETON_H
