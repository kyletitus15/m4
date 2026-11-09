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

#ifdef AIWS_HAS_IPC
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "aiws/ipc_protocol.hpp"
#include "aiws/ipc_server.hpp"
#endif

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

#ifdef AIWS_HAS_IPC
int connect_to(const std::string& path) {
    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) { ::close(fd); return -1; }
    timeval tv{3, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    return fd;
}

std::string request_bytes(const std::string& path) {
    std::string r = "AIWS";
    r.push_back(1);  // version
    r.push_back(1);  // ADD
    unsigned char len[4];
    aiws::ipc::put_u32(len, static_cast<std::uint32_t>(path.size()));
    r.append(reinterpret_cast<const char*>(len), 4);
    return r + path;
}

void send_all(int fd, const std::string& s) {
    std::size_t off = 0;
    while (off < s.size()) {
        const ssize_t w = ::send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (w <= 0) { return; }
        off += static_cast<std::size_t>(w);
    }
}

bool recv_all(int fd, void* buf, std::size_t n) {
    char* p = static_cast<char*>(buf);
    while (n > 0) {
        const ssize_t got = ::recv(fd, p, n, 0);
        if (got <= 0) { return false; }
        p += got;
        n -= static_cast<std::size_t>(got);
    }
    return true;
}

struct Reply { bool got{false}; int status{-1}; std::string text; };

Reply read_reply(int fd) {
    Reply rep;
    unsigned char h[aiws::ipc::kReplyHeaderSize];
    if (!recv_all(fd, h, sizeof h)) { return rep; }
    std::string text(aiws::ipc::get_u32(h + 1), '\0');
    if (!recv_all(fd, text.data(), text.size())) { return rep; }
    rep.got = true;
    rep.status = h[0];
    rep.text = text;
    return rep;
}

struct IpcFixture : Fixture {
    std::unique_ptr<aiws::ipc::IpcServer> server;  // destroyed before `win` (reverse order)
    std::string sock;
    bool start() {
        sock = QFile::encodeName(dir.filePath("s.sock")).toStdString();
        server = std::make_unique<aiws::ipc::IpcServer>(&win, sock);
        std::string err;
        return server->start(err);
    }
    int docs() { return win.findChild<QListWidget*>("documentList")->count(); }
    Reply send_add(const std::string& path) {
        const int fd = connect_to(sock);
        if (fd < 0) { return Reply{}; }
        send_all(fd, request_bytes(path));
        const Reply r = read_reply(fd);
        ::close(fd);
        return r;
    }
};
#endif

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

#ifdef AIWS_HAS_IPC
    // E1
    void ipc_add_valid_file() {
        IpcFixture f;
        QVERIFY(f.start());
        const Reply r = f.send_add(QFile::encodeName(f.make_file()).toStdString());
        QVERIFY(r.got);
        QCOMPARE(r.status, 0);
        QTRY_COMPARE_WITH_TIMEOUT(f.docs(), 1, 3000);
    }

    // E3: a request arriving in pieces, with pauses
    void ipc_request_in_pieces() {
        IpcFixture f;
        QVERIFY(f.start());
        const std::string req = request_bytes(QFile::encodeName(f.make_file()).toStdString());
        const int fd = connect_to(f.sock);
        QVERIFY(fd >= 0);
        const std::size_t cuts[] = {3, 10, 14, req.size()};
        std::size_t from = 0;
        for (const std::size_t to : cuts) {
            send_all(fd, req.substr(from, to - from));
            from = to;
            QTest::qSleep(100);
        }
        const Reply r = read_reply(fd);
        ::close(fd);
        QVERIFY(r.got);
        QCOMPARE(r.status, 0);
        QTRY_COMPARE_WITH_TIMEOUT(f.docs(), 1, 3000);
    }

    // E2: spaces, a newline, non-ASCII UTF-8 (run with LC_ALL=C.UTF-8)
    void ipc_special_path_characters() {
        IpcFixture f;
        QVERIFY(f.start());
        const QString path = f.dir.filePath(QString::fromUtf8("sp ace\nnew \xC3\xA9\xE2\x82\xAC.txt"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { QSKIP("filesystem/locale cannot create this name"); }
        file.write("alpha");
        file.close();
        const Reply r = f.send_add(QFile::encodeName(path).toStdString());
        QVERIFY(r.got);
        QCOMPARE(r.status, 0);
        QTRY_COMPARE_WITH_TIMEOUT(f.docs(), 1, 3000);
    }

    // E1: rejection
    void ipc_rejects_unreadable_file() {
        IpcFixture f;
        QVERIFY(f.start());
        const Reply r = f.send_add("/no/such/file.txt");
        QVERIFY(r.got);
        QCOMPARE(r.status, 1);
        QVERIFY(!r.text.empty());
        QTest::qWait(100);
        QCOMPARE(f.docs(), 0);
    }

    // E1 + R2: a remote add during a build behaves like a local add
    void ipc_add_during_build_discards_result() {
        IpcFixture f;
        QVERIFY(f.start());
        g_release = false;
        QVERIFY(f.win.add_document_file(f.make_file()));
        f.button("buildCorpusButton")->click();
        const Reply r = f.send_add(QFile::encodeName(f.make_file()).toStdString());
        QVERIFY(r.got);
        QCOMPARE(r.status, 0);
        QTRY_COMPARE_WITH_TIMEOUT(f.docs(), 2, 3000);
        g_release = true;
        QTRY_VERIFY_WITH_TIMEOUT(f.button("buildCorpusButton")->isEnabled(), 3000);
        QVERIFY(!f.corpus_text().contains("chunks"));
    }

    // E4: stops promptly and removes the socket file
    void ipc_stop_is_prompt_and_removes_socket() {
        IpcFixture f;
        QVERIFY(f.start());
        QCOMPARE(::access(f.sock.c_str(), F_OK), 0);
        QElapsedTimer timer;
        timer.start();
        f.server->stop();
        QVERIFY2(timer.elapsed() < 500, "server should stop within 500 ms");
        QVERIFY(::access(f.sock.c_str(), F_OK) != 0);
    }
#endif
};

QTEST_MAIN(StudentTests)
#include "student_tests.moc"