#include "ScriptModel.h"

#include "datasingleton.h"

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
#include <QAction>
#include <QMenu>

#include <map>
#include <utility>

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

struct DocParamInfo {
    QString type;
    QString description;
};

// Returns a list of parameter definitions as QVariantMaps.
// Each QVariantMap contains keys like "name", "type", and "description".
std::pair<QString, std::map<QString, DocParamInfo>> parseDocstring(const QString& docString) {

    if (docString.isEmpty())
        return {};

    //QVariantMap result;
    std::map<QString, DocParamInfo> params;

    // Extract the common description (first paragraph of the docstring)
    QRegularExpression descExp(R"(^([\s\S]*?)\n\n)");
    auto descMatch = descExp.match(docString);
    QString description = descMatch.hasMatch() ? descMatch.captured(1).trimmed() : "";

    // Locate the Args: section (a simple approach)
    QRegularExpression argsSectionExp(R"(Args:\s*((?:.|\n)*))");
    auto argsMatch = argsSectionExp.match(docString);
    if (argsMatch.hasMatch()) {
        QString argsSection = argsMatch.captured(1);

        // Use a regex to capture parameter lines (assumes one parameter per line)
        QRegularExpression paramExp(R"(^\s*(\w+)\s*\(([^,)]+)(?:,\s*optional)?\):\s*(.+)$)",
            QRegularExpression::MultilineOption);
        QRegularExpressionMatchIterator it = paramExp.globalMatch(argsSection);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            auto name = match.captured(1);
            DocParamInfo param;
            param.type = match.captured(2);
            param.description = match.captured(3);
            params[name] = param;
        }
    }

    //result["parameters"] = params;
    //return result;
    return { description, params };
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

    PythonQtObjectPtr mainContext = PythonQt::self()->getMainModule(); // Get the main Python module

    // Load the helper code to define get_function_info() and get_all_functions_info()
    mainContext.evalScript(
        "import inspect\n"
        "def _get_function_info(func):\n"
        "    sig = inspect.signature(func)\n"
        "    parameters = []\n"
        "    for name, param in sig.parameters.items():\n"
        "        parameters.append({\n"
        "            'name': name,\n"
        "            'kind': str(param.kind),\n"
        "            'default': None if param.default is param.empty else param.default,\n"
        "            'annotation': None if param.annotation is param.empty else str(param.annotation)\n"
        "        })\n"
        "    info = {\n"
        "        'name': func.__name__,\n"
        "        'signature': str(sig),\n"
        "        'parameters': parameters,\n"
        "        'doc': inspect.getdoc(func) or \"\"\n"
        "    }\n"
        "    return info\n"
        "\n"
        "def _get_all_functions_info():\n"
        "    functions_info = []\n"
        "    for name, obj in globals().items():\n"
        "        if callable(obj) and not name.startswith('_'):\n"
        "            try:\n"
        "                functions_info.append(_get_function_info(obj))\n"
        "            except Exception as e:\n"
        "                # Optionally log or handle functions that cannot be introspected\n"
        "                pass\n"
        "    return functions_info\n"
    );

    mainContext.evalFile(":/script.py");

    auto allFunctionsInfo = mainContext.call("_get_all_functions_info").toList();

    for (auto& functionInfo : allFunctionsInfo)
    {
        FunctionInfo info;

        auto map = functionInfo.toMap();
        info.name = map["name"].toString();
        info.signature = map["signature"].toString();
        info.doc = map["doc"].toString();
        auto docInfo = parseDocstring(info.doc);
        info.fullName = docInfo.first.isEmpty() ? info.name : docInfo.first;

        auto parameters = map["parameters"].toList();
        for (const auto& v : parameters)
        {
            auto in = v.toMap();
            ParameterInfo param;
            param.name = in["name"].toString();
            param.fullName = param.name;
            param.kind = in["kind"].toString();
            param.defaultValue = in["default"];
            param.description = in["description"].toString();
            param.annotation = in["annotation"].toString();
            auto it = docInfo.second.find(param.name);
            if (it != docInfo.second.end())
            {
                if (!it->second.description.isEmpty())
                {
                    param.fullName = it->second.description;
                    param.description = it->second.description;
                }
            }
            info.parameters.push_back(std::move(param));
        }

        mFunctionInfos.push_back(std::move(info));
    }

    qDebug() << "All functions' info:" << allFunctionsInfo;


    /////////////////////


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

    /*
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

void ScriptModel::setupActions(QMenu* effectsMenu, QMap<int, QAction*>& effectsActMap)
{
    const auto parent = this->parent();

    for (const auto& funcInfo : mFunctionInfos)
    {
        int type = DataSingleton::Instance()->addScriptActionHandler(this, funcInfo);
        QAction* effectAction = new QAction(funcInfo.fullName, parent);
        connect(effectAction, SIGNAL(triggered()), parent, SLOT(effectsAct()));
        effectsMenu->addAction(effectAction);
        effectsActMap.insert(type, effectAction);
    }
}
