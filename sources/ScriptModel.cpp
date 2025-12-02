// ScriptModel.cpp
#include "ScriptModel.h"
#include "datasingleton.h"

#include "makeguard.h" 

#include "effects/scripteffect.h"
#include "effects/scripteffectwithsettings.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>
#include <QVariantMap>
#include <QVariantList>
#include <QImage>
#include <QAction>
#include <QMenu>
#include <QProcess>
#include <QApplication>
#include <QMessageBox>

#undef slots

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif


namespace py = pybind11;
using namespace py::literals;

namespace {

void showErrorAsync(const QString& title, const QString& message) {
    QMetaObject::invokeMethod(
        qApp,
        [title, message]() {
            QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
            QWidget *parent = app ? app->activeWindow() : nullptr;
            QMessageBox::critical(parent, title, message);
        },
        Qt::QueuedConnection);
}

bool isPythonInstalled()
{
    const int status =
        QProcess::execute(QCoreApplication::applicationFilePath(),
            QStringList() << CHECK_PYTHON_OPTION);

    return status == 0;
}

//------------------------------------------------------------------------------
// Converts a QImage to a contiguous pybind11::array (NumPy array).
// The function ensures the QImage is in Format_RGB888 (3-channel format).
py::array qimage_to_nparray(const QImage& inImage) {
    // Convert image to a well-defined RGB format.
    QImage image = inImage.convertToFormat(QImage::Format_RGB888);

    int height = image.height();
    int width = image.width();
    constexpr int channels = 3; // RGB

    // Allocate a new contiguous NumPy array with shape (height, width, 3).
    py::array_t<uchar> arr({ height, width, channels });
    py::buffer_info buf = arr.request();
    uchar* dest = static_cast<uchar*>(buf.ptr);

    // Copy row-by-row to ensure a contiguous memory layout.
    for (int i = 0; i < height; i++) {
        const uchar* src = image.constScanLine(i);
        std::memcpy(dest + i * (width * channels), src, width * channels);
    }

    return arr;
}

uint8_t clip_uint8(long a) {
    const uint8_t noOverflowCandidate = static_cast<uint8_t>(a);
    return (noOverflowCandidate == a) ? noOverflowCandidate : ((noOverflowCandidate < a) ? UINT8_MAX : 0);
}

//------------------------------------------------------------------------------
// Converts a NumPy array (pybind11::array) back into a QImage.
// The array must be contiguous and have shape (height, width, 3), dtype uint8.
QImage nparray_to_qimage(const py::array& a) {
    // Get buffer info.
    py::buffer_info info = a.request();

    // Ensure the NumPy array has the correct shape and type.
    if (info.ndim != 3 || info.shape[2] != 3) {
        throw std::invalid_argument("nparray_to_qimage: Expected shape (height, width, 3)");
    }

    int height = static_cast<int>(info.shape[0]);
    int width = static_cast<int>(info.shape[1]);
    constexpr int channels = 3;

    // Create an empty QImage with Format_RGB888.
    QImage image(width, height, QImage::Format_RGB888);

    if (info.format == py::format_descriptor<uchar>::format()) {

        // Copy row-by-row.
        const uchar* src = static_cast<const uchar*>(info.ptr);
        for (int i = 0; i < height; i++) {
            uchar* dest = image.scanLine(i);
            std::memcpy(dest, src + i * (width * channels), width * channels);
        }
    }
    else if (info.format == py::format_descriptor<float>::format()) 
    {
        // Convert floating-point values to uint8 (scale [0,1] -> [0,255])
        /*
        const float* src = static_cast<const float*>(info.ptr);
        for (int i = 0; i < height; i++) {
            uchar* dest = image.scanLine(i);
            for (int j = 0; j < width * channels; j++) {
                dest[j] = static_cast<uchar>(std::round(src[i * (width * channels) + j] * 255.0f));
            }
        }
        */
        const float* src = static_cast<const float*>(info.ptr);
        for (int i = 0; i < height; i++) {
            uchar* dest = image.scanLine(i);
            for (int j = 0; j < width; j++) 
                for (int k = 0; k < channels; ++k)
                {
                    dest[j * channels + k] = 
                        clip_uint8(std::lround(src[k * (width * height) + i * width + j] * 255.0f));
                }
        }
    }
    else
    {
        throw std::invalid_argument("nparray_to_qimage: Expected dtype=uint8");
    }

    return image;
}

//------------------------------------------------------------------------------
// Helper: Convert a QVariant to a py::object with better type handling
py::object convertQVariantToPyObject(const QVariant& var)
{
    // If the QVariant wraps a QImage, use our conversion function.
    if (var.canConvert<QImage>()) {
        QImage image = var.value<QImage>();
        return qimage_to_nparray(image);  // Returns a py::array for an image.
    }
    // Handle fundamental types.
    if (var.type() == QMetaType::Int)
        return py::cast(var.toInt());
    if (var.type() == QMetaType::Double)
        return py::cast(var.toDouble());
    if (var.type() == QMetaType::Bool)
        return py::cast(var.toBool());
    if (var.type() == QMetaType::QString)
        return py::cast(var.toString().toStdString());
    if (var.type() == QVariant::PointF) {
        QPointF point = var.toPointF();
        return py::cast(std::complex<double>(point.x(), point.y()));  // Convert to Python complex
    }

    // Handle lists.
    if (var.canConvert<QVariantList>()) {
        QVariantList list = var.toList();
        py::list pyList;
        for (const QVariant& item : qAsConst(list)) {
            pyList.append(convertQVariantToPyObject(item));
        }
        return pyList;
    }
    // Handle maps.
    if (var.canConvert<QVariantMap>()) {
        QVariantMap map = var.toMap();
        py::dict pyDict;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            pyDict[py::cast(it.key().toStdString())] =
                convertQVariantToPyObject(it.value());
        }
        return pyDict;
    }
    // Fallback: convert to string.
    return py::cast(var.toString().toStdString());
}

// Helper structure for docstring parameter info.
struct DocParamInfo {
    QString type;
    QString description;
};

// Returns a pair where the first element is the common description (first paragraph)
// and the second element is a map from parameter names to DocParamInfo.
std::pair<QString, std::map<QString, DocParamInfo>> parseDocstring(const QString& docString) {
    if (docString.isEmpty())
        return {};

    std::map<QString, DocParamInfo> params;

    // Extract the common description (first paragraph)
    QRegularExpression descExp(R"(^([\s\S]*?)\n\n)");
    auto descMatch = descExp.match(docString);
    QString description = descMatch.hasMatch() ? descMatch.captured(1).trimmed() : "";

    // Locate the Args: section
    QRegularExpression argsSectionExp(R"(Args:\s*((?:.|\n)*))");
    auto argsMatch = argsSectionExp.match(docString);
    if (argsMatch.hasMatch()) {
        QString argsSection = argsMatch.captured(1);
        // Capture parameter lines (one parameter per line)
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
    return { description, params };
}

bool send_image(const std::weak_ptr<EffectRunCallback>& mCallback, const pybind11::array& src)
{
    //qDebug() << "In send_image().";
    if (auto obj = mCallback.lock())
    {
        if (obj->isInterrupted())
            return false;
        auto img = nparray_to_qimage(src);
        //qDebug() << img;
        emit obj->sendImage(img);
        return true;
    }
    return false;
}

} // namespace

class ScriptModel::PythonScope
{
    py::scoped_interpreter interpreter;
    py::gil_scoped_release release_gil;
};

// ----------------------------------------------------------------
// ScriptModel implementation using pybind11 for embedding Python.

ScriptModel::ScriptModel(QWidget* parent, const QString& venvPath)
    : QObject(parent), mVenvPath(venvPath.trimmed())
{
    if (isPythonInstalled())
    {
        if (!mVenvPath.isEmpty())
        {
            // 1. Set VIRTUAL_ENV
            qputenv("VIRTUAL_ENV", mVenvPath.toUtf8());

            // 2. Prepend Scripts/ or bin/ to PATH
#ifdef Q_OS_WIN
            QString scriptsPath = mVenvPath + "/Scripts";
#else
            QString scriptsPath = venvPath + "/bin";
#endif
            QByteArray currentPath = qgetenv("PATH");
            qputenv("PATH", (scriptsPath + ";" + currentPath).toUtf8());

            // 3. Optionally unset PYTHONHOME to avoid conflicts
            qputenv("PYTHONHOME", QByteArray());
        }
        try {
            mPythonScope = std::make_unique<PythonScope>();
            mValid = true;
        }
        catch (const std::exception& e) {
            qWarning() << "Error initializing Python: " << e.what();
        }
    }
    else
        QMessageBox::warning(
            parent,
            QObject::tr("Matching Python is not installed."),
            QObject::tr("Matching Python is not installed: ") + PY_VERSION
        );
}

void ScriptModel::LoadScript(const QString& path)
{
    if (!mValid)
        return;

    std::unique_lock<std::mutex> lock(mCallMutex);

    py::gil_scoped_acquire acquire;  // Ensures proper GIL acquisition
    // Get the sys module and adjust sys.path.
    py::module_ sys = py::module_::import("sys");
    py::list sysPath = sys.attr("path");
    qDebug() << "Python sys.path:" << QString::fromStdString(py::str(sysPath).cast<std::string>());
    for (py::handle path_item : sysPath) {
        std::string pathStr = py::str(path_item).cast<std::string>();
        QString qpath = QString::fromStdString(pathStr) + "/site-packages";
        if (QFileInfo::exists(qpath)) {
            // Append path to sys.path if needed.
            if (mVenvPath.isEmpty())
                sys.attr("path").attr("append")(qpath.toStdString());
#ifdef Q_OS_WIN
            QString root = QFileInfo(QString::fromStdString(pathStr)).dir().path();
            SetDllDirectoryW(reinterpret_cast<LPCWSTR>(root.utf16()));
#endif
        }
    }

    if (!mVenvPath.isEmpty())
        sys.attr("path").attr("insert")(0, (mVenvPath + "/Lib/site-packages").toStdString());

    // (Optional) Redirect Python stdout/stderr by reassigning sys.stdout/sys.stderr if desired.

    // Get the __main__ module.
    py::module_ mainModule = py::module_::import("__main__");
    py::dict globals = mainModule.attr("__dict__");

    // Inject functions into Python globals
    globals["_send_image"] = py::cpp_function([this](const py::array& image) { 
            send_image(mCallback, image);
        });
    globals["_check_interrupt"] = py::cpp_function([this]() { return check_interrupt(); });

    // Execute an external script file.
    // (Assuming "script.py" is accessible—adjust the path as needed.)
    try {
        // Load the script from Qt's resource system
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw std::runtime_error("Failed to open script.py resource");
        }

        QTextStream in(&file);
        auto text = in.readAll().toStdString();
        py::eval<py::eval_statements>(text, globals);
    }
    catch (const std::exception& e) {
        qWarning() << "Error executing script.py:" << e.what();
        showErrorAsync(QObject::tr("Script Execution Error"),
            QObject::tr("Error executing script: ") + e.what());
    }

    py::module_ inspect = py::module_::import("inspect");
    // cache Parameter.empty
    py::object param_empty = inspect.attr("Parameter").attr("empty");

    // get items as an iterable
    py::object items = globals.attr("items")();
    for (py::handle item : items) {
        py::tuple pair = item.cast<py::tuple>();
        std::string name = py::str(pair[0]).cast<std::string>();
        py::object obj = pair[1];

        if (name.empty() || name[0] == '_')
            continue;

        if (!inspect.attr("isfunction")(obj).cast<bool>())
            continue;

        if (py::str(obj.attr("__module__")).cast<std::string>() != "__main__")
            continue;

        std::string qualname = py::str(obj.attr("__qualname__")).cast<std::string>();
        if (qualname.find('.') != std::string::npos)
            continue;

        FunctionInfo info;
        info.name = QString::fromStdString(name);

        py::object sig = inspect.attr("signature")(obj);
        info.signature = QString::fromStdString(py::str(sig).cast<std::string>());

        py::object docObj = inspect.attr("getdoc")(obj);
        std::string docStr = docObj.is_none() ? std::string() : py::str(docObj).cast<std::string>();
        info.doc = QString::fromStdString(docStr);

        auto docInfo = parseDocstring(info.doc);
        info.fullName = docInfo.first.isEmpty() ? info.name : docInfo.first;

        // parameters is an ordered mapping; iterate items()
        py::object params_items = sig.attr("parameters").attr("items")();
        for (py::handle paramItem : params_items) {
            py::tuple p = paramItem.cast<py::tuple>();
            std::string paramName = py::str(p[0]).cast<std::string>();
            py::object paramObj = p[1];

            ParameterInfo param;
            param.name = QString::fromStdString(paramName);
            param.fullName = param.name;
            param.kind = QString::fromStdString(py::str(paramObj.attr("kind")).cast<std::string>());

            py::object def = paramObj.attr("default");
            if (!def.is_none() && def.ptr() != param_empty.ptr()) {
                param.defaultValue = QString::fromStdString(py::str(def).cast<std::string>());
            }
            else {
                qDebug() << "Default for " << param.name << " is missing.";
                //param.defaultValue = QVariant();
            }

            py::object ann = paramObj.attr("annotation");
            if (ann.ptr() != param_empty.ptr()) {
                param.annotation = QString::fromStdString(py::str(ann).cast<std::string>());
            }
            else {
                qDebug() << "Annotation for " << param.name << " is missing.";
                //param.annotation.clear();
            }

            // Convert paramName to the same key type as docInfo.second before lookup
            QString qParamName = QString::fromStdString(paramName);
            auto it = docInfo.second.find(qParamName);
            if (it != docInfo.second.end() && !it->second.description.isEmpty()) {
                param.fullName = it->second.description;
                param.description = it->second.description;
            }

            info.parameters.push_back(std::move(param));
        }

        mFunctionInfos.push_back(std::move(info));
    }
    qDebug() << "All functions' info loaded.";
}

ScriptModel::~ScriptModel() {
    mIsShuttingDown = true;
    std::unique_lock<std::mutex> lock(mCallMutex);
}

void ScriptModel::setupActions(QMenu* fileMenu, QMenu* effectsMenu, QMap<int, QAction*>& effectsActMap)
{
    if (!mValid)
        return;

    const auto parent = this->parent();
    const auto firstAction = fileMenu->actions().isEmpty() ? nullptr : fileMenu->actions().constFirst();
    for (const auto& funcInfo : mFunctionInfos) {
        QAction* effectAction = new QAction(funcInfo.fullName, parent);
        if (funcInfo.isCreatingFunction())
        {
            auto effect = funcInfo.parameters.empty()
                ? static_cast<AbstractEffect*>(new ScriptEffect(this, funcInfo))
                : new ScriptEffectWithSettings(this, funcInfo);

            QObject::connect(effectAction, &QAction::triggered, [effect] { effect->applyEffect(nullptr); });
            fileMenu->insertAction(firstAction, effectAction);  // Insert at the start
        }
        else
        {
            // Use DataSingleton to register each function.
            const int type = DataSingleton::Instance()->addScriptActionHandler(this, funcInfo);
            QObject::connect(effectAction, SIGNAL(triggered()), parent, SLOT(effectsAct()));
            effectsMenu->addAction(effectAction);
            effectsActMap.insert(type, effectAction);
        }
    }
}

QVariant ScriptModel::call(const QString& callable,
    const QVariantList& args,
    std::weak_ptr<EffectRunCallback> callback,
    const QVariantMap& kwargs)
{
    const bool isStoppable = !callback.expired();

    qDebug() << "Entering ScriptModel::call.";

    auto stateGuard = MakeGuard(this, [](ScriptModel* pThis) {
        qDebug() << "Leaving ScriptModel::call.";
        });

    if (!mValid)
        return {};

    if (auto obj = mCallback.lock())
    {
        if (obj->isInterrupted())
            return {};
    }

    std::unique_lock<std::mutex> lock(mCallMutex);

    py::gil_scoped_acquire acquire;  // Ensures proper GIL acquisition
    // Obtain the __main__ module and its globals.
    py::module_ mainModule = py::module_::import("__main__");
    py::dict globals = mainModule.attr("__dict__");

    // Lookup the callable function by name.
    std::string funcName = callable.toStdString();
    py::object pyFunc = globals[funcName.c_str()];
    if (!pyFunc) {
        qWarning() << "Function" << callable << "not found.";
        return QVariant();
    }

    // Build positional arguments list.
    py::list pyArgs;
    for (const QVariant& arg : args) {
        pyArgs.append(convertQVariantToPyObject(arg));
    }

    // Build keyword arguments dictionary.
    py::dict pyKwargs;
    for (auto it = kwargs.constBegin(); it != kwargs.constEnd(); ++it) {
        std::string key = it.key().toStdString();
        pyKwargs[py::cast(key)] = convertQVariantToPyObject(it.value());
    }

    // Call the Python function.
    py::object result;
    try {
        mCallback = callback;
        result = pyFunc(*pyArgs, **pyKwargs);
    }
    catch (const std::exception& e) {
        qWarning() << "Error calling function" << callable << ":" << e.what();
        auto ptr = callback.lock();
        if (!isStoppable || (ptr && !ptr->isInterrupted())) {
            showErrorAsync(QObject::tr("Python Call Error"),
                QObject::tr("Error calling function ") + callable + ": " + e.what());
        }
        return QVariant();
    }

    // If the result is a NumPy array, assume it is an image and convert to QImage.
    if (py::isinstance<py::array>(result)) {
        try {
            py::array arr = result.cast<py::array>();
            QImage img = nparray_to_qimage(arr);
            return QVariant::fromValue(img);
        }
        catch (const std::exception& ex) {
            qWarning() << "Failed to convert numpy array to QImage:" << ex.what();
            // Fall through to return as a string.
        }
    }

    // For other types, convert the result to a string.
    std::string resStr = py::str(result);
    return QVariant(QString::fromStdString(resStr));
}

bool ScriptModel::check_interrupt()
{
    if (mIsShuttingDown)
        return true;

    if (auto obj = mCallback.lock())
    {
        return obj->isInterrupted();
    }

    return true;
}

int ScriptModel::ValidatePythonSystem() {
    //*
    try {
        py::scoped_interpreter interpreter;
        py::gil_scoped_release release_gil;

        // Acquire the GIL so that Python calls are thread-safe.
        py::gil_scoped_acquire acquire;

        // Import sys module.
        py::module_ sys = py::module_::import("sys");

        // Check that sys.path exists and is a non-empty list.
        py::object sysPathObj = sys.attr("path");
        if (!py::isinstance<py::list>(sysPathObj)) {
            qWarning() << "Python system invalid: sys.path is not a list.";
            return 1;
        }
        py::list sysPath = sysPathObj.cast<py::list>();
        if (sysPath.size() == 0) {
            qWarning() << "Python system invalid: sys.path is empty.";
            return 2;
        }

        // Attempt to import a common module; using 'os' as an example.
        py::module_::import("os");

    }
    catch (const std::exception& e) {
        qWarning() << "Python system validation failed:" << e.what();
        return 3;
    }
    //*/
    return 0; // Python system appears valid.
}
