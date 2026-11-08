# DESIGN_NOTE.md - M4

# 1. Components and threads
- GUI thread: MainWindow (all slots, state), workspace_widget, search_widget, context_widget, term_inspector_widget, 'build_finished' slot
- Worker thread: 'run_build()' function

# 2. Corpus states and events
States: NotBuilt, Building, Current, Stale
Events:
- NotBuilt: when 'build' starts -> Building
- Building: when 'finished' -> if version matches is good -> Current
            when 'finished' -> if version changed -> Stale
            when 'finished' -> error occurs -> Stale or NotBuilt
- Current: when 'document_added' -> Stale
- Stale: when 'build' starts -> Building

# 3. Threatening sequences
R2:
- Build, then Add, then build finishes and the version captured at start no longer matches -> result will be discarded, state Stale, Build reenabled, and no chunk count is shown
- Build, then Reset, then finishes, making the reset bump version and clear everything -> late result is discarded and UI stays at startup state
R3:
- Build fails -> GUI shows a error message, leaves Building state, and app stays usable
- Exit during build -> destructor sets cancel flag, worker stops, and no threads are detached
R4:
- Repeated Build clicks -> 

Walkthrough (a): docs A,B loaded, version = 2 -> build clicked (snapshot of A,B, job version = 2, state = building, outputs cleared) -> user adds C (workspace chnages, version = 3, state is still building) -> worker finishes and posts build finished -> slot sees job_version 2 != version 3 -> core destroyed on GUI thread (state = stale, status = corpurs out of date, build is enabled)

# 4: Ownership path of built corpus
created by worker as unqiue_ptr<ProcessingCore> inside BuildResult -> moved
into the promise -> taken by build_finished on the GUI thread ->
if valid, moved into MainWindow::processing_core_ -> previous core destroyed -> current destroyed on next accepted build, or in ~MainWindow -> If discarded, destroyed
at the end of build_finished

# Rejected alternative
Calling rebuild() on live processingcore from worker, locked GUI out -> rejected because the GUI would read a partially built core and protecting it needs things that are out of scope.
