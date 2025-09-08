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

#include <QApplication>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QTranslator>
#include <QDir>
#include <QStyleFactory>

#include "mainwindow.h"
#include "datasingleton.h"
#include "set_dark_theme.h"
#include "ScriptModel.h"

void printHelpMessage()
{
    qDebug()<<"EasyPaint - simple graphics painting program\n"
              "Usage: easypaint [options] [filename]\n\n"
              "Options:\n"
              "\t-h, --help\t\tshow this help message and exit\n"
              "\t-v, --version\t\tshow program's version number and exit";
}

void printVersion()
{
    qDebug()<< QApplication::applicationVersion();
}

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("EasyPaint");
    QApplication::setOrganizationName("EasyPaint");
    QApplication::setOrganizationDomain("github.com");
    QApplication::setApplicationVersion(EASYPAINT_VERSION);

    QStringList args = a.arguments();
    QRegExp rxArgHelp("--help"), rxArgH("-h");
    QRegExp rxArgVersion("--version"), rxArgV("-v");
    QRegExp rxCheckPython(CHECK_PYTHON_OPTION);
    QRegExp rxArgScript("--script"), rxArgScriptShort("-s");

    bool isHelp(false), isVer(false), isCheckPython(false);
    QStringList filePaths;
    QString pythonScriptPath;

    for (int i = 1; i < args.size(); ++i)
    {
        const QString& arg = args.at(i);

        if (rxArgHelp.indexIn(arg) != -1 || rxArgH.indexIn(arg) != -1)
        {
            isHelp = true;
        }
        else if (rxArgVersion.indexIn(arg) != -1 || rxArgV.indexIn(arg) != -1)
        {
            isVer = true;
        }
        else if (rxCheckPython.indexIn(arg) != -1)
        {
            isCheckPython = true;
        }
        else if (rxArgScript.indexIn(arg) != -1 || rxArgScriptShort.indexIn(arg) != -1)
        {
            if (i + 1 < args.size())
            {
                QString candidate = args.at(++i);
                if (candidate.endsWith(".py", Qt::CaseInsensitive)
                    && QFile::exists(candidate))
                {
                    pythonScriptPath = candidate;
                }
                else
                {
                    qDebug() << "Python script not found or invalid:" << candidate;
                }
            }
            else
            {
                qDebug() << "--script option requires a file path";
            }
        }
        else
        {
            if (QFile::exists(arg))
                filePaths.append(arg);
            else
                qDebug() << "File not found:" << arg;
        }
    }

    if (isHelp)
    {
        printHelpMessage();
        return 0;
    }
    if (isVer)
    {
        printVersion();
        return 0;
    }
    if (isCheckPython)
    {
        return ScriptModel::ValidatePythonSystem();
    }

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    if (DataSingleton::Instance()->getIsDarkMode())
        ui_utils::setDarkTheme(true);

    QTranslator appTranslator;
    QString translationsPath(
#ifdef Q_OS_WIN
        QDir(QApplication::applicationDirPath()).absoluteFilePath("translations/")
#else
        "/usr/share/easypaint/translations/"
#endif
    );
    QString appLanguage = DataSingleton::Instance()->getAppLanguage();
    appTranslator.load(translationsPath + ((appLanguage == "system")
        ? ("easypaint_" + QLocale::system().name())
        : appLanguage));
    a.installTranslator(&appTranslator);

    if (!pythonScriptPath.isEmpty())
    {
        DataSingleton::Instance()->setIsLoadScript(true);
        DataSingleton::Instance()->setScriptPath(pythonScriptPath);
    }

    MainWindow w(filePaths);
    w.show();

    return a.exec();
}
