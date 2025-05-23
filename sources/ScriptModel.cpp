#include "ScriptModel.h"

#include "PythonQt.h"

#include <QFileInfo>
#include <QDir>

#include <windows.h>

ScriptModel::ScriptModel(QObject *parent)
    : QObject(parent)
{
    PythonQt::init(/*PythonQt::IgnoreSiteModule |*/ PythonQt::RedirectStdOut);

    const auto pyQtInst = PythonQt::self();

    auto sys = pyQtInst->importModule("sys");
    auto paths = pyQtInst->getVariable(sys, "path");

    qDebug() << "Python sys.path:" << paths;

    for (auto path : paths.value<QVariantList>())
    {
        auto sitePackages = path.toString() + QStringLiteral("/site-packages");

        if (QFileInfo::exists(sitePackages))
        {
            pyQtInst->addSysPath(sitePackages);
            QString root = QFileInfo(path.toString()).dir().path();
            pyQtInst->addSysPath(root);
            SetDllDirectoryW(qUtf16Printable(root));
            //qputenv("PATH", qgetenv("PATH") + ";" + root.toLocal8Bit());  // Extend PATH for current session
        }
    }

    QObject::connect(pyQtInst, &PythonQt::pythonStdOut, [](const QString& str) { if (str.length() > 1) qInfo() << str; });
    QObject::connect(pyQtInst, &PythonQt::pythonStdErr, [](const QString& str) { if (str.length() > 1) qCritical() << str; });

    pyQtInst->getMainModule().evalFile(":/script.py");


    /////////////////////

    PythonQtObjectPtr mainContext = PythonQt::self()->getMainModule(); // Get the main Python module

    /*
    //QVariant  dirResult = mainContext.call("dir");  // Get all attributes
    QVariant dirResult = mainContext.evalScript("dir()");

    QStringList functionList;

    if (dirResult.canConvert<QStringList>()) {  // Ensure it's convertible
        for (const QVariant& item : dirResult.toList()) {
            QString name = item.toString();

            // Use PythonQt's `evalScript` to determine if an attribute is a function
            QVariant isFunction = mainContext.evalScript(QString("callable(%1)").arg(name));

            if (isFunction.toBool()) {  // If the attribute is callable, it's a function
                functionList.append(name);
            }
        }
    }
    // Print out the function names
    qDebug() << "Functions loaded from script.py:" << functionList;
    */

    //*
    // Instead of using "dir()", use globals()
    QVariant globalsKeysVariant = mainContext.evalScript("list(globals().keys())", Py_eval_input);
    qDebug() << "Globals keys:" << globalsKeysVariant;

    QStringList functionList;

    if (globalsKeysVariant.isValid() && globalsKeysVariant.type() == QVariant::List) {
        QVariantList keysList = globalsKeysVariant.toList();
        for (const QVariant& key : keysList) {
            QString name = key.toString();

            // Build an expression to check if the object in globals() is callable.
            // This looks up the object by name and calls callable() on it.
            QString expr = QString("callable(globals()['%1'])").arg(name);
            QVariant isCallableVariant = mainContext.evalScript(expr, Py_eval_input);

            if (isCallableVariant.isValid() && isCallableVariant.toBool()) {
                functionList.append(name);
            }
        }
        qDebug() << "Available Python functions:" << functionList;
    }
    else {
        qDebug() << "Error: Could not obtain a valid globals keys list!";
    }
    //*/

    /*

    // Call the helper function "list_functions" defined in the script.
    //QVariant funcNames = mainContext.evalScript("list_functions()");
    auto funcNames = mainContext.call("list_functions", {});

    // Validate and convert the result to a QStringList.
    if (funcNames.isValid() && funcNames.type() == QVariant::List)
    {
        QStringList functionList = funcNames.toStringList();
        qDebug() << "Available Python functions:" << functionList;
    }
    else
    {
        qCritical() << "Error: list_functions() did not return a valid list:" << funcNames;
    }
    */

    /*

    PythonQtObjectPtr script = mainContext.getVariable("script"); // Get the script object
    QList<PythonQtArgument> arguments = script->getArguments(); // Get the arguments of the script
    for (auto argument : arguments) {
        QString name = argument.name(); // Get the name of each argument
        QString type = argument.type(); // Get the type of each argument
        QVariant default_value = argument.defaultValue(); // Get the default value of each argument
        QString doc = argument.doc(); // Get the docstring of each argument

    */        
}

ScriptModel::~ScriptModel()
{
    PythonQt::cleanup();
}
