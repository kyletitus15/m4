#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>

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
    parser.addOption(delay);
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
    return app.exec();
}
