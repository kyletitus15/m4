# M5 Design

# 1. M4 Fixes
Only the M4 fault test AIWS_FAILURE_ACCEPT_STALE failed; M5 no longer builds the M4 switches, so I did not debug it. M5 deletes build_is_current() and the version counters: stale results are now rejected by job id in BuildingState::on_build_finished().

# 2. Threads and shared data
- GUI thread: widgets, MainWindow, every CorpusState object, next_id_
- Worker thread: BuildWorker::run(). One std::thread, started in the BuildWorker constructor, joined in BuildWorker::shutdown() (called from ~MainWindow)

Shared data: 
- pending_, finished_, stop_: each thread touches them only while holding mutex_. stop_ is the cv_ predicate, so changing it unlocked could lose a wakeup.
- wanted_id_ (std::atomic<uint64_t>): the GUI stores, the worker loads between 10 ms sleeps. One independent value, so no lock is needed.
Nothing else is shared: a request carries a copy of the Workspace, and the worker builds its own ProcessingCore.

# 3. States
NotBuilt:
  - build requested: -> Building
  - workspace changed: stay
  - reset: -> NotBuilt
  - build finished: ignore
Building:
  - build requested: ignore
  - workspace changed: cancel -> Stale
  - reset: cancel -> NotBuilt
  - build finished: if id matches -> Current, if error -> Stale, if id differs: ignore
Current:
  - build requested: -> Building
  - workspace changed: -> Stale
  - reset: -> NotBuilt
  - build finished: ignore
Stale:
  - build requested: -> Building
  - workspace changed: stay
  - reset: -> NotBuilt
  - build finished: ignore

Exit is not a state event: ~MainWindow calls shutdown() in any state

# 4. Sequence: Build, Add Document, Build, Exit
1. GUI: build_corpus(), NotBuiltState::on_build_requested(), BuildWorker::submit() sets wanted_id_=1, fills pending_, notifies.
2. Worker: run() leaves cv_.wait, takes pending_, execute(), wait_build_time().
3. GUI: add_document_file(), BuildingState::on_workspace_changed(), BuildWorker::cancel() (wanted_id_=2); state Stale.
4. GUI: Build, StaleState::on_build_requested(), submit() (id 3). No waiting.
5. Worker: sees the id change within 10 ms, returns cancelled, stores finished_ under the lock, queues build_finished, then takes request 3.
6. GUI: build_finished(), BuildingState(3)::on_build_finished() ignores job 1; job 3 becomes Current.
7. GUI: Exit, ~MainWindow, shutdown() bumps wanted_id_, sets stop_ under the lock, notify_all(), join().

# 5. Decisions
1. Build during a build: not allowed. BuildingState::can_build() is false and on_build_requested() is ignored. Add or Reset leaves Building first, so the next request is accepted at once.
2. Remote add mid-build: add_document_file() calls BuildingState::on_workspace_changed(), which calls BuildWorker::cancel(). State becomes Stale; the user sees "Corpus: Out of date, rebuild needed".
3. Shared data: the four items in section 2. Requests and results must cross threads (mutex); cancel needs a lock-free read between sleeps (atomic); stop_ must share the mutex with cv_.

# 6. Fault switches
- AIWS_FAULT_NO_CANCEL: in BuildWorker::wait_build_time() the wanted_id_ check is compiled out. Caught by TEST_NAME_1.
- AIWS_FAULT_SILENT_SHUTDOWN: BuildWorker::shutdown() skips cv_.notify_all(). Caught by TEST_NAME_2 (hangs, killed by the ctest timeout).

# 7. AI use
AI was used to refine code, find syntax/logic errors, and write tests.