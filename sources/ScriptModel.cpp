#include "ScriptModel.h"

#include "PythonQt.h"

#include <QFileInfo>

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
        }
    }

    QObject::connect(pyQtInst, &PythonQt::pythonStdOut, [](const QString& str) { qInfo() << str; });
    QObject::connect(pyQtInst, &PythonQt::pythonStdErr, [](const QString& str) { qCritical() << str; });

    pyQtInst->getMainModule().evalFile(":/script.py");


    /////////////////////

    PythonQtObjectPtr mainContext = PythonQt::self()->getMainModule(); // Get the main Python module

    QVariant  dirResult = mainContext.call("dir");  // Get all attributes
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
