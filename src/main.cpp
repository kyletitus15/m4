#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>

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
    aiws::gui::MainWindow window;
    window.set_exit_callback(&exit_page);
    window.show();
    return app.exec();
}
