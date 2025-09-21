#include "autorun_utils.h"

#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_WIN
  // nothing extra
#elif defined(Q_OS_DARWIN)
  #import <CoreServices/CoreServices.h>
#elif defined(Q_OS_LINUX)
  #include <QStandardPaths>
#endif

namespace utilities {
namespace {

// Windows registry path for HKCU autorun entries
#ifdef Q_OS_WIN
  const char runKey[] =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
#endif

// Helper for macOS: find our app in the login items list
#ifdef Q_OS_DARWIN
LSSharedFileListItemRef FindLoginItemForCurrentBundle(CFArrayRef items)
{
    CFURLRef bundleURL = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    if (!bundleURL) return nullptr;

    LSSharedFileListItemRef found = nullptr;
    CFIndex cnt = CFArrayGetCount(items);
    for (CFIndex i = 0; i < cnt; ++i) {
        LSSharedFileListItemRef item =
            (LSSharedFileListItemRef)CFArrayGetValueAtIndex(items, i);
        CFURLRef itemURL = nullptr;
        if (LSSharedFileListItemResolve(item,
                kLSSharedFileListNoUserInteraction |
                kLSSharedFileListDoNotMountVolumes,
                &itemURL, nullptr) == noErr)
        {
            if (itemURL && CFEqual(itemURL, bundleURL)) {
                found = item;
                CFRelease(itemURL);
                break;
            }
            if (itemURL) CFRelease(itemURL);
        }
    }

    CFRelease(bundleURL);
    return found;
}
#endif

// Linux: compute and ensure ~/.config/autostart exists, then build .desktop path
#ifdef Q_OS_LINUX
QString desktopFilePath()
{
    QString dir = QStandardPaths::writableLocation(
                      QStandardPaths::ConfigLocation)
                  + "/autostart";
    QDir().mkpath(dir);
    return dir + "/"
         + QCoreApplication::applicationName()
         + ".desktop";
}
#endif

} // namespace

bool isAutorunEnabled()
{
#ifdef Q_OS_WIN
    QSettings s(runKey, QSettings::NativeFormat);
    return !s.value(QCoreApplication::applicationName()).toString().isEmpty();

#elif defined(Q_OS_DARWIN)
    LSSharedFileListRef loginItems =
        LSSharedFileListCreate(nullptr,
                               kLSSharedFileListSessionLoginItems,
                               nullptr);
    if (!loginItems) return false;

    CFArrayRef items = LSSharedFileListCopySnapshot(loginItems, nullptr);
    bool enabled = (FindLoginItemForCurrentBundle(items) != nullptr);

    if (items)       CFRelease(items);
    CFRelease(loginItems);
    return enabled;

#elif defined(Q_OS_LINUX)
    return QFile::exists(desktopFilePath());

#else
    return false;
#endif
}

bool setAutorun(bool runWithOS)
{
#ifdef Q_OS_WIN
    QSettings s(runKey, QSettings::NativeFormat);
    if (runWithOS) {
        s.setValue(QCoreApplication::applicationName(),
                   QDir::toNativeSeparators(
                     QCoreApplication::applicationFilePath())
                   + " -autorun");
    } else {
        s.remove(QCoreApplication::applicationName());
    }
    s.sync();
    return s.status() == QSettings::NoError;

#elif defined(Q_OS_DARWIN)
    LSSharedFileListRef loginItems =
        LSSharedFileListCreate(nullptr,
                               kLSSharedFileListSessionLoginItems,
                               nullptr);
    if (!loginItems) return false;

    CFArrayRef items = LSSharedFileListCopySnapshot(loginItems, nullptr);
    LSSharedFileListItemRef item =
        FindLoginItemForCurrentBundle(items);

    if (runWithOS && !item) {
        CFURLRef url = CFBundleCopyBundleURL(
                         CFBundleGetMainBundle());
        LSSharedFileListInsertItemURL(loginItems,
                                      kLSSharedFileListItemBeforeFirst,
                                      nullptr, nullptr,
                                      url, nullptr, nullptr);
        CFRelease(url);

    } else if (!runWithOS && item) {
        LSSharedFileListItemRemove(loginItems, item);
    }

    if (items)       CFRelease(items);
    CFRelease(loginItems);
    return true;

#elif defined(Q_OS_LINUX)
    QString path = desktopFilePath();
    if (runWithOS) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&f);
        out << "[Desktop Entry]\n"
               "Type=Application\n"
               "Exec="
            << QDir::toNativeSeparators(
                 QCoreApplication::applicationFilePath())
            << " -autorun\n"
               "Hidden=false\n"
               "NoDisplay=false\n"
               "X-GNOME-Autostart-enabled=true\n"
               "Name="
            << QCoreApplication::applicationName()
            << "\n";
        f.close();
        return true;
    } else {
        return QFile::remove(path);
    }

#else
    Q_UNUSED(runWithOS);
    return false;
#endif
}

} // namespace utilities
