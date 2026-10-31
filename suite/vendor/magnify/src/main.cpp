#include <QApplication>
#include <QCommandLineParser>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>

#include "core/FormatRegistry.h"
#include "ui/MainWindow.h"

namespace {

const QString kServerName = QStringLiteral("MagnifyFactory-single-instance");
const QString kClassesRoot = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
const QString kMenuKey = QStringLiteral("MagnifyFactory");

// Sends a request (target format + files) to an already-running instance.
// Returns false if none is listening.
bool forwardToRunningInstance(const QString &format, const QStringList &files) {
    QLocalSocket socket;
    socket.connectToServer(kServerName);
    if (!socket.waitForConnected(500)) {
        return false;
    }
    QByteArray payload;
    QDataStream out(&payload, QDataStream::WriteOnly);
    out << format << files;
    socket.write(payload);
    socket.flush();
    socket.waitForBytesWritten(2000);
    socket.disconnectFromServer();
    return true;
}

#ifdef Q_OS_WIN
// Per-extension cascading menu: right-clicking "photo.png" shows
// "Convert with MagnifyFactory >" with one entry per output format of the
// same category. Registered under HKCU (no elevation needed).
void unregisterContextMenu() {
    QSettings classes(kClassesRoot, QSettings::NativeFormat);
    // Legacy flat entry from older versions.
    classes.remove(QStringLiteral("*/shell/") + kMenuKey);
    const auto &formats = magnify::core::FormatRegistry::instance().all();
    for (const auto &descriptor : formats) {
        classes.remove(QStringLiteral("SystemFileAssociations/.%1/shell/%2").arg(descriptor.extension, kMenuKey));
    }
    classes.sync();
}

void registerContextMenu(const QString &exePath) {
    using magnify::core::FormatCategory;
    unregisterContextMenu();

    const QString nativeExe = QDir::toNativeSeparators(exePath);
    QSettings classes(kClassesRoot, QSettings::NativeFormat);
    const auto &registry = magnify::core::FormatRegistry::instance();

    for (const auto &source : registry.all()) {
        if (!source.supportsInput || source.category == FormatCategory::Unknown) {
            continue;
        }
        const QString base = QStringLiteral("SystemFileAssociations/.%1/shell/%2").arg(source.extension, kMenuKey);
        classes.setValue(base + QStringLiteral("/MUIVerb"), QStringLiteral("Convert with MagnifyFactory"));
        classes.setValue(base + QStringLiteral("/Icon"), QStringLiteral("\"%1\"").arg(nativeExe));
        classes.setValue(base + QStringLiteral("/SubCommands"), QString());

        if (source.category == FormatCategory::Archive) {
            // Archives only have "extract here".
            const QString key = base + QStringLiteral("/shell/00extract");
            classes.setValue(key + QStringLiteral("/MUIVerb"), QStringLiteral("Extract here"));
            classes.setValue(key + QStringLiteral("/command/Default"),
                             QStringLiteral("\"%1\" \"%2\"").arg(nativeExe, QStringLiteral("%1")));
            continue;
        }

        int order = 1;
        for (const auto &target : registry.formatsInCategory(source.category)) {
            if (!target.supportsOutput || target.extension == source.extension) {
                continue;
            }
            const QString key = base + QStringLiteral("/shell/%1_%2").arg(order++, 3, 10, QLatin1Char('0')).arg(target.extension);
            classes.setValue(key + QStringLiteral("/MUIVerb"), target.name.isEmpty() ? target.extension.toUpper() : target.name);
            classes.setValue(key + QStringLiteral("/command/Default"),
                             QStringLiteral("\"%1\" --convert-to %2 \"%3\"").arg(nativeExe, target.extension, QStringLiteral("%1")));
        }
        // Always allow opening the full dialog (presets, editors, trim, ...).
        const QString more = base + QStringLiteral("/shell/999_more");
        classes.setValue(more + QStringLiteral("/MUIVerb"), QStringLiteral("More options..."));
        classes.setValue(more + QStringLiteral("/CommandFlags"), 0x40); // separator above
        classes.setValue(more + QStringLiteral("/command/Default"),
                         QStringLiteral("\"%1\" \"%2\"").arg(nativeExe, QStringLiteral("%1")));
    }
    classes.sync();
}
#endif

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MagnifyFactory"));
    QApplication::setOrganizationName(QStringLiteral("MagnifyFactory"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    QCommandLineParser parser;
    QCommandLineOption convertTo(QStringLiteral("convert-to"), QStringLiteral("Target format."), QStringLiteral("format"));
    QCommandLineOption registerMenu(QStringLiteral("register-context-menu"));
    QCommandLineOption unregisterMenu(QStringLiteral("unregister-context-menu"));
    parser.addOptions({convertTo, registerMenu, unregisterMenu});
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Files to open."), QStringLiteral("[files...]"));
    parser.parse(QApplication::arguments());

#ifdef Q_OS_WIN
    if (parser.isSet(registerMenu)) {
        registerContextMenu(QApplication::applicationFilePath());
        return 0;
    }
    if (parser.isSet(unregisterMenu)) {
        unregisterContextMenu();
        return 0;
    }
#endif

    QStringList files;
    for (const QString &arg : parser.positionalArguments()) {
        files << QFileInfo(arg).absoluteFilePath();
    }
    const QString format = parser.value(convertTo).toLower();

    // Explorer launches one process per selected file; funnel them all into
    // the first window instead of spawning one window each.
    if (!files.isEmpty() && forwardToRunningInstance(format, files)) {
        return 0;
    }

    QLocalServer::removeServer(kServerName);
    QLocalServer server;
    // If another instance won the race and is listening, forward to it.
    if (!server.listen(kServerName)) {
        if (!files.isEmpty() && forwardToRunningInstance(format, files)) {
            return 0;
        }
    }

    magnify::ui::MainWindow window;
    window.show();

    QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window]() {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &window]() {
                QDataStream in(socket->readAll());
                QString fmt;
                QStringList received;
                in >> fmt >> received;
                if (!received.isEmpty()) {
                    window.openExternalFiles(received, fmt);
                }
            });
        }
    });

    if (!files.isEmpty()) {
        window.openExternalFiles(files, format);
    }

    return QApplication::exec();
}
