# M4 Decision Record

See specification Section 8.

# 1. Threads and ownership
- GUI thread: MainWindow (all slots, build_corpus, build_finished, remote_add, reset_application, add_document_file), workspace_widget, search_widget, context_widget, term_inspector_widget, workspace_, processing_core_, state_, ws_version_, job_version_, worker_running_.
- Worker thread: 'run_build()' function, works only on its own Workspace copy and a new ProcessingCore. Started by build_corpus(), joined by build_finished() or ~MainWindow().
- IPC server thread (one): IpcServer::run()/serve_one() in src/ipc_server.cpp, started by
  IpcServer::start(), joined by IpcServer::stop() (called from ~IpcServer).

# 2. Validity of finished build
MainWindow::build_is_current(job_version): true only if job_version == ws_version_. ws_version_ increments in add_document_file() and reset_application(). build_finished() calls this first: if they aren't the same version, the result is discarded. If the build was current, it is reported as failed.

# 3. Changes since checkpoint
- Reset and Add do not cancel a running worker, only can be done by destructor (cancel flag). Version check is the only thing that makes build valid. 
- Failure is shown in the status and corpus labels instead of a dialog
- Build is disabled with worker_running_ = true, and that stays true until build_finished() is joined to the thread (only one worker at a time)
- Added test seam so tests can block/fail a build
- Add Document was split into 2 functions: add_document() & add_document_file()

# 4: CR-1 impact
- New: IpcServer, ipc_protocol.hpp, tools/aiws_send.cpp, slot (MainWindow::remote_add(QByteArray)), --ipc-socket in main.cpp
- Changed: main.cpp -> now starts the server, MainWindow -> now has remote_add()
- Unchanged and why: add_document_file(), build_corpus(), build_finished(), build_is_current(). A remote add is just add_document_file() called from a queued slot, which already bumps ws_version_ and invalidates the corpus, so R2 holds with no new code

# 5. Fault switches
- AIWS_FAULT_ACCEPT_STALE: MainWindow::build_is_current() in src/main_window.cpp, caught by add_during_build_discards_result and reset_during_build_discards_result
- AIWS_FAULT_LOSE_FAILURE: run_build() in src/main_window.cpp (skips the queued post on failure), caught by failed_build_is_reported_and_app_recovers

# Pair programming log
- Partner: Shaheer Tareen
- Session 1: 11/8/26, 2:00-2:30, on my code, we worked on aiws_send.cpp, I started as driver for 15 min, then we swapped roles and worked for 15 more min.
- Session 2: 11/8/26, 2:30-3:00, on his code, we worked on aiws_send.cpp, he started as driver for 15 min, then we swapped roles and worked for 15 more min.