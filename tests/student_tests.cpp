// Your M4 tests (specification, Section 6).
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

#include "aiws/gui/main_window.hpp"

using aiws::gui::MainWindow;

namespace {

std::atomic<bool> g_release{true};  // false = the build blocks until released
std::atomic<bool> g_fail{false};    // true = the build throws

void gated_build(const aiws::Workspace& ws, aiws::ProcessingCore& core) {
    while (!g_release.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    if (g_fail.load()) { throw std::runtime_error("injected failure"); }
    core.rebuild(ws);
}

struct Fixture {
    QTemporaryDir dir;
    MainWindow win;
    int count{0};

    Fixture() { win.set_build_function(&gated_build); }
    ~Fixture() { g_release = true; }  // never leave a worker blocked when win is destroyed

    QString make_file(const QString& text = "alpha beta gamma delta") {
        const QString path = dir.filePath(QString("doc%1.txt").arg(++count));
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) { f.write(text.toUtf8()); }
        return path;
    }
    QPushButton* button(const char* name) { return win.findChild<QPushButton*>(name); }
    QLabel* label(const char* name) { return win.findChild<QLabel*>(name); }
    QLineEdit* query() { return win.findChild<QLineEdit*>("queryEdit"); }
    QString corpus_text() { return label("corpusStatusLabel")->text(); }
};

}  // namespace

class StudentTests : public QObject {
    Q_OBJECT
private slots:
    void init() { g_release = true; g_fail = false; }

    void startup_state() {
        Fixture f;
        QVERIFY(!f.button("buildCorpusButton")->isEnabled());
        QVERIFY(!f.button("searchButton")->isEnabled());
        QCOMPARE(f.corpus_text(), QString("Corpus: Not built"));
    }

    void unreadable_file_is_rejected() {
        Fixture f;
        QVERIFY(!f.win.add_document_file("/no/such/file.txt"));
        QCOMPARE(f.win.findChild<QListWidget*>("documentList")->count(), 0);
    }

    // R1: if the build ran on the GUI thread, click() would block forever here.
    void gui_stays_usable_during_build() {
        Fixture f;
        g_release = false;
        QVERIFY(f.win.add_document_file(f.make_file("hello world")));
        f.button("buildCorpusButton")->click();
        QVERIFY(!f.button("buildCorpusButton")->isEnabled());
        QVERIFY(f.corpus_text().contains("Building"));

        f.win.findChild<QListWidget*>("documentList")->setCurrentRow(0);
        QCOMPARE(f.win.findChild<QPlainTextEdit*>("documentText")->toPlainText(), QString("hello world"));
        f.query()->setText("alpha");
        QVERIFY(!f.button("searchButton")->isEnabled());  // no corpus yet

        g_release = true;
        QTRY_VERIFY_WITH_TIMEOUT(f.button("buildCorpusButton")->isEnabled(), 3000);
        QVERIFY(f.corpus_text().contains("chunks"));
        QVERIFY(f.button("searchButton")->isEnabled());
    }

    // R2 / fault AIWS_FAULT_ACCEPT_STALE
    void add_during_build_discards_result() {
        Fixture f;
        g_release = false;
        QVERIFY(f.win.add_document_file(f.make_file()));
        f.button("buildCorpusButton")->click();
        QVERIFY(f.win.add_document_file(f.make_file()));
        g_release = true;
        QTRY_VERIFY_WITH_TIMEOUT(f.button("buildCorpusButton")->isEnabled(), 3000);
        QVERIFY(!f.corpus_text().contains("chunks"));
        f.query()->setText("alpha");
        QVERIFY(!f.button("searchButton")->isEnabled());
    }

    // R2 / fault AIWS_FAULT_ACCEPT_STALE
    void reset_during_build_discards_result() {
        Fixture f;
        g_release = false;
        QVERIFY(f.win.add_document_file(f.make_file()));
        f.button("buildCorpusButton")->click();
        f.button("resetButton")->click();
        QVERIFY(f.win.add_document_file(f.make_file()));
        QVERIFY(!f.button("buildCorpusButton")->isEnabled());  // worker still running
        g_release = true;
        QTRY_VERIFY_WITH_TIMEOUT(f.button("buildCorpusButton")->isEnabled(), 3000);
        QVERIFY(!f.corpus_text().contains("chunks"));
        f.query()->setText("alpha");
        QVERIFY(!f.button("searchButton")->isEnabled());
    }

    // R3 / fault AIWS_FAULT_LOSE_FAILURE
    void failed_build_is_reported_and_app_recovers() {
        Fixture f;
        g_fail = true;
        QVERIFY(f.win.add_document_file(f.make_file()));
        f.button("buildCorpusButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(f.button("buildCorpusButton")->isEnabled(), 2000);
        QVERIFY(f.label("statusLabel")->text().contains("failed"));
        QVERIFY(!f.corpus_text().contains("chunks"));

        g_fail = false;  // still usable: a retry works
        f.button("buildCorpusButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(f.corpus_text().contains("chunks"), 3000);
    }

    // R4: exit mid-build is prompt (default build + long delay; cancel is checked every 10 ms)
    void exit_during_build_is_prompt() {
        auto win = std::make_unique<MainWindow>();
        QTemporaryDir dir;
        const QString path = dir.filePath("a.txt");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("alpha beta");
        file.close();
        win->set_build_delay_ms(20000);
        QVERIFY(win->add_document_file(path));
        win->findChild<QPushButton*>("buildCorpusButton")->click();
        QElapsedTimer timer;
        timer.start();
        win.reset();
        QVERIFY2(timer.elapsed() < 2000, "destructor should cancel and join quickly");
    }
};

QTEST_MAIN(StudentTests)
#include "student_tests.moc"