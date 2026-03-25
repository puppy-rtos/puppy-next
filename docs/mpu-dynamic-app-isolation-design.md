# Dynamic MPU App Isolation Design (with MoM Reference)

## 1. Goals

- Keep existing kernel memory MPU protection unchanged as baseline safety.
- Add dynamic app memory isolation on app switch.
- Ensure current running app can only access its own app RAM range.
- Add app self-memory-range introspection API so app can get its own RAM start/end for targeted tests.
- Reuse existing precise dual-overlap MPU region algorithm to minimize rewrite risk.

## 2. Current Puppy-Next Baseline

### 2.1 Existing behavior

- MPU is initialized and kernel profile is applied at boot.
- App profile is currently applied at app thread startup, not on every context switch.
- Context switch callback currently explicitly avoids MPU operations.

Key paths:
- `core/init.c`: `pup_mpu_init()`, `pup_mpu_setup_kernel_protection()`
- `core/app/app_loader.c`: `user_app_thread()` -> `pup_mpu_setup_app_protection(...)`
- `core/kernel/puppy_core.c`: `pup_sched_swap_done_cb()` (current no MPU switch)

### 2.2 Limitation

- If multiple apps exist concurrently, MPU profile is not guaranteed to match current scheduled app after switch.
- Existing test app cannot query its own exact RAM range, making boundary tests less deterministic.

## 3. MoM Design Insights Worth Borrowing

This section summarizes practical patterns from MoM (`e:/MyWorkspace/puppy-project/mom`) that are directly reusable.

### 3.1 Per-process MPU profile cache

MoM stores per-process MPU data in process object (`lwp->mpu_info`) and keeps precomputed region control values (`struct mpu_reg` with `f_addr/f_ctrl/b_addr/b_ctrl`).

Borrowing value:
- Move costly calculations out of switch path.
- Switch path only writes precomputed registers.

### 3.2 MPU switch in context-switch path

MoM calls `rt_lwp_mpu_switch` directly in PendSV context switch path.

Borrowing value:
- Confirms Cortex-M interrupt context can safely perform MPU register update when carefully ordered with enable/disable and barriers.
- Validates our plan to switch MPU on scheduling transition.

### 3.3 Switch dedup flag

MoM uses a static usage flag (`use_flag`) to avoid repeated setup of some fixed regions.

Borrowing value:
- Add active profile cache (`active_app_pid`) and skip redundant MPU writes when profile unchanged.

### 3.4 App memory metadata exposure

MoM app header includes `ram_size`, `heap_offset`, `stack_offset`, `data_offset`.

Borrowing value:
- Keep kernel truth from app process metadata.
- Expose minimal self-view API to app for deterministic test generation.

## 4. Target Runtime Model

### 4.1 Isolation semantics

- Number of app processes can scale beyond MPU region count.
- At any instant, only current app private RAM is mapped with user RW permission.
- Other app private RAM is inaccessible to current app.
- Shared RAM keeps explicit policy (default allow RW for app IPC usage).

### 4.2 MPU profile invariant

- Region 0: flash RX (user readable/executable as needed).
- Region 1: kernel RAM privileged only.
- Region 2/3: current app RAM precise coverage (dual overlapping region + subregion mask).
- Region 4: shared RAM RW.
- Region 5/6/7: cleared (or reserved for future strict extensions).

### 4.3 Adaptive granularity policy (no fixed 32KB)

- MPU region size must be power-of-two and base aligned to region size.
- Do not force all app mappings to 32KB.
- Compute minimal feasible region/subregion granularity from app actual memory size:
  - prefer the smallest region pair that can fully cover `[app_start, app_end)` with dual-overlap strategy.
  - keep subregion masking enabled to trim head/tail slack.
- Keep an upper bound fallback (for example 32KB) only as compatibility fallback, not default.

Recommended selection rule:

1. Determine app actual span (`app_actual_size`, not allocated slot size).
2. Select minimal `region_size` satisfying dual-overlap coverage and alignment constraints.
3. Derive `subregion_size = region_size / 8`.
4. Generate front/back masks from overlap test.
5. Cache computed profile in app metadata for fast switch-time apply.

## 5. New API: App Self RAM Range Introspection

### 5.1 Shared struct

Add new shared header:
- `core/shared/pup_app_memory.h`

Proposed struct:

```c
typedef struct {
    unsigned int ram_start; /* inclusive */
    unsigned int ram_end;   /* exclusive */
} pup_app_ram_range_t;
```

### 5.2 Syscall definition

- Add syscall number: `SYS_APP_RAM_RANGE`
- Kernel side:
  - `core/kernel/syscall.h`
  - `core/kernel/syscall.c`
- User side wrapper:
  - `apps/include/syscall.h`

Proposed user API:

```c
static inline int app_get_ram_range(pup_app_ram_range_t* range) {
    return (int)syscall(SYS_APP_RAM_RANGE, (long)range, 0, 0, 0);
}
```

### 5.3 Kernel syscall behavior

- Resolve current app via `app_get_current()`.
- Fill `range->ram_start = app->memory_base`.
- Fill `range->ram_end = app->memory_base + app->memory_size` (exclusive end).
- Return 0 on success, negative on failure.

Safety checks:
- Null pointer check.
- User pointer range check: target buffer must be writable in current app RAM.
- If current context is not app thread, return error.

## 6. Dynamic Switch Design

### 6.1 App profile cache in app_process_t

Extend `app_process_t` with cached MPU apply data (conceptual fields):

- `mpu_front_base`
- `mpu_back_base`
- `mpu_front_mask`
- `mpu_back_mask`
- `mpu_region_size` (adaptive)
- `mpu_subregion_size` (adaptive)
- `mpu_align_size` (derived from region size)
- `mpu_profile_version`

Data source:
- computed once during app load (`app_load_into_memory`) using existing precise calculation.
- calculation must use app actual span and adaptive size selection, not fixed 32KB.

### 6.2 MPU service API

Add in `core/kernel/mpu.h` and `core/kernel/mpu.c`:

- `int pup_mpu_apply_kernel_profile(void);`
- `int pup_mpu_apply_app_profile(app_process_t* app);`
- `int pup_mpu_profile_is_active(int pid);`

Internal state:
- `static int g_mpu_active_app_pid = -1;`

### 6.3 Scheduler integration point

In `core/kernel/puppy_core.c` -> `pup_sched_swap_done_cb()`:

1. keep existing thread validity and stack guard checks.
2. set `curr_thread`.
3. find owning app by thread (`app_find_by_thread(curr_thread)`).
4. if app exists -> `pup_mpu_apply_app_profile(app)`.
5. else -> `pup_mpu_apply_kernel_profile()`.

This keeps MPU decision exactly aligned with currently running thread.

### 6.4 Exit and cleanup behavior

- On app exit/free, if exited app profile is active, fallback to kernel profile.
- Clean per-app cached MPU metadata on process reclaim.

## 7. Fault Diagnostics Enhancements

In `core/arch/arch_cortex-m.c` MemManage path, append diagnostics:

- active profile pid/name
- fault address classification:
  - kernel range
  - shared range
  - current app range
  - outside known ranges (likely other app/private area)
- optional app list snapshot (`app_list`) in debug build

This mirrors useful MoM troubleshooting behavior while keeping existing termination policy.

## 8. Test Plan

## 8.1 Manual tests

1. Baseline stability
- Boot and verify shell commands: `help`, `ps`, `apps`, `free`.

2. Introspection API correctness
- Add `mpu_test self` mode to print `app_get_ram_range` output.
- Compare with `apps` listed memory range.

3. Single app legal access
- `mpu_test safe`, `mpu_test readonly` should pass.

3.1 Small-app granularity validation
- Add at least one very small app footprint case (for example < 8KB actual RAM use).
- Verify selected MPU granularity is smaller than legacy fixed 32KB profile when possible.
- Verify legal accesses near app head/tail boundaries still pass.

4. Single app violation
- Access `ram_end` or kernel RAM (`0x20000000`) should trigger MemManage.
- Faulting app should terminate; shell remains alive.
- Add boundary violation probes for `ram_start - 1` and `ram_end` to validate adaptive mask correctness.

5. Dual app isolation
- Run one app in background, another in foreground.
- Foreground app probes known background app RAM range.
- Expected: violation and app termination without system collapse.

## 8.2 Automated tests

Extend `tools/qemu-test.bat` flow:

- run fixed scenario script
- capture serial output
- assert both conditions:
  - expected fault signature exists
  - system remains responsive after fault

Suggested keywords:
- `MPU Memory Management Fault`
- `User application memory fault, terminating`
- follow-up shell prompt or command output after fault

## 9. Implementation Order

1. Add shared struct and syscall IDs (`pup_app_memory.h`, syscall headers).
2. Implement kernel syscall handler and user wrapper.
3. Extend `mpu_test` with `self` and `violate` modes using queried range.
4. Add per-app MPU cache fields and fill during load.
5. Add MPU profile apply APIs with dedup skip.
6. Wire scheduler switch callback for dynamic profile apply.
7. Add fault diagnostics and qemu test assertions.

## 10. Risks and Mitigations

- Risk: extra cost on every switch.
  - Mitigation: profile dedup skip + precomputed register cache.

- Risk: wrong user pointer in new syscall can corrupt memory.
  - Mitigation: strict pointer range validation before write.

- Risk: profile mismatch during app exit race.
  - Mitigation: app-free path forces kernel profile fallback when needed.

## 11. Acceptance Criteria

- Dynamic switch works: running app always has matching MPU profile.
- App can query own RAM start/end via syscall.
- Cross-app access attempt triggers MemManage and only kills faulting app.
- Kernel shell stays interactive after app fault.
- No regression in existing single-app workflows.

## 12. Execution Tasks With Per-Task Acceptance

This section is the concrete implementation roadmap. Execute tasks in order. Each task has an explicit acceptance test and done criteria.

### Common pre-step for all tasks (mandatory)

Before any build or runtime verification, run:

```powershell
./prebuild-clean.bat
```

Reason:
- In this project, `xmake clean` may occasionally leave stale cache/artifacts.
- Residual outputs can cause app startup crash/hang and lead to false code-level diagnosis.

### Task 1: Add app memory range shared type

Implementation scope:
- Add shared header [core/shared/pup_app_memory.h](../core/shared/pup_app_memory.h).
- Define `pup_app_ram_range_t` with `ram_start` (inclusive) and `ram_end` (exclusive).

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build firmware: `xmake f -p cross -a cortex-m4 -m debug` then `xmake`.
2. Confirm no compile error from include path and type visibility.

Done criteria:
- New type is visible from both kernel and app include trees.

### Task 2: Add syscall number and user wrapper

Implementation scope:
- Update [core/kernel/syscall.h](../core/kernel/syscall.h) with `SYS_APP_RAM_RANGE`.
- Update [apps/include/syscall.h](../apps/include/syscall.h) with `app_get_ram_range(...)` wrapper.

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Rebuild with `xmake`.
2. Check wrapper call compiles in one app source (temporary or real usage site).

Done criteria:
- Kernel and user syscall IDs stay aligned.

### Task 3: Implement kernel syscall handler for self RAM range

Implementation scope:
- Add handler in [core/kernel/syscall.c](../core/kernel/syscall.c).
- Resolve current app and return its RAM range.
- Validate pointer and writable range before writing result.

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build with `xmake`.
2. Runtime: app calls `app_get_ram_range`; syscall returns success and values are non-zero and ordered (`ram_start < ram_end`).

Done criteria:
- Invalid pointer returns error and does not crash kernel.

### Task 4: Extend mpu_test for self and boundary probes

Implementation scope:
- Update [apps/mpu_test/main.c](../apps/mpu_test/main.c) to add:
  - `self` mode: print `ram_start`, `ram_end`.
  - `violate` mode: test `ram_start - 1` and `ram_end` boundary probes.

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build firmware: `xmake build firmware`.
2. In QEMU shell:
   - `/rom/mpu_test self` prints valid range.
   - `/rom/mpu_test violate` triggers MemManage and app termination.

Done criteria:
- Shell remains responsive after violation test.

### Task 5: Add adaptive MPU profile calculator

Implementation scope:
- Refactor MPU calculation path to choose adaptive `region_size` and `subregion_size` from app actual span.
- Keep dual-overlap region strategy.
- Do not force fixed 32KB default.

Primary files:
- [dtm/ips/mpu_arm_m4_st_gd.c](../dtm/ips/mpu_arm_m4_st_gd.c)
- [core/kernel/mpu.c](../core/kernel/mpu.c)
- [core/kernel/mpu.h](../core/kernel/mpu.h)

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build with `xmake`.
2. Runtime logs show selected adaptive sizes for at least two apps with different RAM footprint.
3. Small app case uses finer granularity when hardware constraints allow.

Done criteria:
- Boundary legal accesses pass, boundary illegal accesses fault as expected.

### Task 6: Cache per-app MPU apply data

Implementation scope:
- Extend process metadata in [core/app/app_manager.h](../core/app/app_manager.h).
- Fill cache during app load in [core/app/app_loader.c](../core/app/app_loader.c).

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build with `xmake`.
2. Runtime logs confirm cache prepared once per app load.

Done criteria:
- No repeated full recalculation during pure scheduling switches.

### Task 7: Add MPU profile apply APIs and dedup skip

Implementation scope:
- Add APIs in [core/kernel/mpu.h](../core/kernel/mpu.h) and [core/kernel/mpu.c](../core/kernel/mpu.c):
  - apply app profile
  - apply kernel profile
  - check active profile
- Track active app pid and skip redundant apply.

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build with `xmake`.
2. Run two apps with frequent yields/syscalls.
3. Logs show profile apply only when app changes, not every tick.

Done criteria:
- Functional behavior unchanged when only one app is running.

### Task 8: Integrate dynamic apply into scheduler switch callback

Implementation scope:
- Update [core/kernel/puppy_core.c](../core/kernel/puppy_core.c) in `pup_sched_swap_done_cb()`.
- After `curr_thread` update, select app profile or kernel profile.

Acceptance test:
1. Run `./prebuild-clean.bat`.
2. Build firmware and run QEMU.
2. Start two apps concurrently (`&` background + foreground).
3. Cross-app memory probe from foreground app faults reliably.

Done criteria:
- No scheduler lockup or repeated hard fault loop.

### Task 9: Add fault diagnostics enrichment

Implementation scope:
- Update [core/arch/arch_cortex-m.c](../core/arch/arch_cortex-m.c) MemManage logs:
  - active profile info
  - address classification
  - optional app list snapshot in debug build

Acceptance test:
1. Run `./prebuild-clean.bat` and rebuild.
2. Trigger one controlled violation.
2. Verify logs include enough information to identify offender app and fault range class.

Done criteria:
- Fault triage can be completed from one log capture.

### Task 10: Add automated qemu-test assertions

Implementation scope:
- Extend [tools/qemu-test.bat](../tools/qemu-test.bat) to execute scenario and assert output signatures.

Acceptance test:
1. Run `./prebuild-clean.bat` and rebuild firmware.
2. Run `tools/qemu-test.bat`.
2. Script exit code is success only when both conditions hold:
   - expected MemManage fault appears
   - system remains alive after fault

Done criteria:
- Script can be used as regression gate for this feature.

## 13. Recommended Iteration Rhythm

1. Complete Task 1 to Task 4 first as vertical slice for observability and deterministic test input.
2. Complete Task 5 to Task 8 as isolation core.
3. Complete Task 9 to Task 10 as reliability and regression closure.

If a task fails acceptance, revert only that task scope and do not continue to next task until acceptance passes.
