# TFX repository workflow

`tfx-cpp` is the faithful reconstruction; `tfx-redux` is its enhanced fork.
Both repositories are available side by side in the same parent directory.

## Keep the fork current

- When changing `tfx-cpp`, merge its committed `master` into `tfx-redux` as part
  of the same work. Also check for outstanding upstream commits when starting
  work in Redux. Keep this a normal working practice; do not add scheduled sync
  workflows, bots or automatic merge services.
- Use ordinary Git merges to preserve upstream ancestry. Do not rely on copied
  patches or cherry-picks alone, and do not rebase or force-push published
  `master` history.
- Inspect both working trees before merging and preserve unrelated local work.
  Resolve conflicts in context, retaining Redux's enhancements and keeping its
  game behaviour, documentation, gallery and packaging specific to Redux.
- Verify that the current `tfx-cpp` master commit is an ancestor of Redux's
  master, and run checks appropriate to any resulting code changes. Commit and
  push within the task's authorization. If synchronization is blocked, report
  the outstanding merge explicitly.
