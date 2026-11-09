#include "aiws/gui/main_window.hpp"
#ifdef AIWS_HAS_IPC
#include "aiws/ipc_protocol.hpp"
#include "aiws/ipc_server.hpp"
#endif

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>
#include <QFile>
#include <string>

// TODO: Define the ExitCallback function used to confirm an Exit request.
namespace {

void exit_page(QWidget* parent) {
    const QMessageBox::StandardButton ans = QMessageBox::question(parent, "Exit", "Are you sure you want to exit?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans == QMessageBox::Yes) { QApplication::quit(); }
}
}

int main(int argc, char* argv[]) {
    // TODO: Create the QApplication, construct/configure the MainWindow,
    // register the ExitCallback, show the window, and run the event loop.
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption delay("build-delay", "Extra build time per doc, in ms.", "ms", "0");
    const QCommandLineOption ipc("ipc-socket", "Unix socket path for aiws_send (default /tmp/aiws-$USER.sock)", "path");
    parser.addOption(delay);
    parser.addOption(ipc);
    parser.process(app);

    bool ok = false;
    const int d = parser.value(delay).toInt(&ok);
    if (!ok || d < 0) {
        QTextStream(stderr) << "----build-delay needs a non-negative integer\n";
        return 2;
    } 

    aiws::gui::MainWindow window;
    window.set_build_delay_ms(d);
    window.set_exit_callback(&exit_page);
    window.show();

    #ifdef AIWS_HAS_IPC
        const std::string s = parser.isSet(ipc) ? QFile::encodeName(parser.value(ipc)).toStdString() : aiws::ipc::default_socket_path();
        aiws::ipc::IpcServer server(&window, s);
        std::string ipc_error;
        if (!server.start(ipc_error)) {
            QTextStream(stderr) << "IPC disabled: " << QString::fromStdString(ipc_error) << "\n";
        }
    #endif

    return app.exec();
}
