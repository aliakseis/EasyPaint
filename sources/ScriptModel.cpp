#include "ScriptModel.h"

#include "PythonQt.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

#include <QWidget>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QVariantMap>
#include <QVariantList>
#include <QPushButton>
#include <QDialog>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString getDocString(const QString& callable)
{ 
    // Get the docstring using PythonQt's introspection.
    if (auto obj = PythonQt::self()->lookupObject(PythonQt::self()->getMainModule(), callable)) {
        if (auto pDocString = PyObject_GetAttrString(obj, "__doc__")) {
            const char* docString = PyUnicode_AsUTF8(pDocString);
            if (docString) {
                QString result(docString);
                qInfo() << "Docstring for" << callable << ":" << result;
                // Now you have the docstring to parse.
            }
            Py_DECREF(pDocString);
        }
    }
}


// Returns a list of parameter definitions as QVariantMaps.
// Each QVariantMap contains keys like "name", "type", and "description".
QVariantList parseDocstring(const QString& docString) {
    QVariantList params;
    // Locate the Args: section (a simple approach)
    QRegularExpression argsSectionExp(R"(Args:\s*((?:.|\n)*))");
    auto argsMatch = argsSectionExp.match(docString);
    if (!argsMatch.hasMatch())
        return params;

    QString argsSection = argsMatch.captured(1);
    // Use a regex to capture parameter lines (assumes one parameter per line)
    // Format assumed: <name> (<type>[, optional]): <description> [Defaults to <default>].
    QRegularExpression paramExp(R"(^\s*(\w+)\s*\(([^,)]+)(?:,\s*optional)?\):\s*(.+)$)",
        QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = paramExp.globalMatch(argsSection);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QVariantMap param;
        param["name"] = match.captured(1);
        param["type"] = match.captured(2);
        param["description"] = match.captured(3);
        // Further parsing could extract default values if needed.
        params.append(param);
    }
    return params;
}

/// Creates a widget with input controls for each parameter.
QWidget* createParameterDialog(const QVariantList& params, QMap<QString, QWidget*>& outWidgets, QWidget* parent = nullptr) {
    QWidget* widget = new QWidget(parent);
    QFormLayout* formLayout = new QFormLayout(widget);

    // Iterate over each parameter extracted from the docstring.
    for (const QVariant& v : params) {
        QVariantMap param = v.toMap();
        QString name = param["name"].toString();
        QString type = param["type"].toString().toLower();

        QWidget* inputWidget = nullptr;
        if (type.contains("int")) {
            // For int parameters, use a spin box.
            QSpinBox* spinBox = new QSpinBox(widget);
            spinBox->setMinimum(0);  // Set suitable min/max as needed
            spinBox->setMaximum(100);
            spinBox->setValue(5);    // Default value, could be parsed from the docstring too.
            inputWidget = spinBox;
        }
        else if (type.contains("float") || type.contains("double")) {
            // For floating point numbers, use a double spin box.
            QDoubleSpinBox* doubleSpinBox = new QDoubleSpinBox(widget);
            doubleSpinBox->setMinimum(0.0);
            doubleSpinBox->setMaximum(100.0);
            doubleSpinBox->setValue(1.0);
            inputWidget = doubleSpinBox;
        }
        else if (type.contains("str") || type.contains("string")) {
            // For string parameters, use a line edit.
            QLineEdit* lineEdit = new QLineEdit(widget);
            inputWidget = lineEdit;
        }
        // Extend this with more types as needed.
        if (inputWidget) {
            formLayout->addRow(name, inputWidget);
            // Store the widget so we can later retrieve its value.
            outWidgets[name] = inputWidget;
        }
    }

    widget->setLayout(formLayout);
    return widget;
}

QVariantList gatherParameters(const QMap<QString, QWidget*>& widgets) {
    QVariantList args;
    // Iterate over the stored widgets.
    for (auto key : widgets.keys()) {
        QWidget* w = widgets.value(key);
        if (auto spin = qobject_cast<QSpinBox*>(w))
            args.append(spin->value());
        else if (auto dspin = qobject_cast<QDoubleSpinBox*>(w))
            args.append(dspin->value());
        else if (auto lineEdit = qobject_cast<QLineEdit*>(w))
            args.append(lineEdit->text());
        // Extend as needed.
    }
    return args;
}

} // namespace


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
#ifdef Q_OS_WIN
            QString root = QFileInfo(path.toString()).dir().path();
            SetDllDirectoryW(qUtf16Printable(root));
#endif
            //pyQtInst->addSysPath(root);
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

    if (globalsKeysVariant.isValid() && globalsKeysVariant.type() == QVariant::List) {
        QVariantList keysList = globalsKeysVariant.toList();
        for (const QVariant& key : keysList) {
            QString name = key.toString();

            if (name.startsWith('_'))
                continue;

            // Build an expression to check if the object in globals() is callable.
            // This looks up the object by name and calls callable() on it.
            QString expr = QString("callable(globals()['%1'])").arg(name);
            QVariant isCallableVariant = mainContext.evalScript(expr, Py_eval_input);

            if (isCallableVariant.isValid() && isCallableVariant.toBool()) {
                mFunctionList.append(name);
            }
        }
        qDebug() << "Available Python functions:" << mFunctionList;
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

void ScriptModel::setupActions(QMenu* effectsMenu, QMap<EffectsEnum, QAction*>& effectsActMap)
{
}
