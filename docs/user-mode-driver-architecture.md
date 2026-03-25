# User-Mode Driver Architecture Design

## 1. Goals

- Move device-specific logic out of the kernel and into user-mode driver apps.
- Keep the kernel responsible only for VFS/DevFS routing, syscall validation, driver lifecycle cleanup, and minimal IRQ forwarding.
- Validate the architecture first with GPIO/LED/KEY before migrating IRQ-sensitive devices such as UART/console.
- Preserve current app isolation and fault containment: a broken driver should fail as an app, not crash the kernel.

## 2. Scope

Included in this design:
- Dynamic device registration from user mode into DevFS.
- Driver ownership binding to app pid.
- Automatic device cleanup on app exit, fault, or kill.
- IRQ subscription and event-wait primitives for future user-mode interrupt handling.
- First migration target definition for GPIO, LED, and KEY.

Excluded from this phase:
- Full UART and console migration.
- Direct user callback execution inside ISR context.
- Driver auto-restart and service supervision.
- DMA-aware high-throughput driver path.

## 3. Kernel/User Boundary

### 3.1 Kernel retains

- VFS path lookup and fd lifecycle management.
- DevFS namespace and mount integration.
- Syscall dispatch and user pointer validation.
- Driver ownership tracking and cleanup hooks.
- Minimal IRQ forwarding infrastructure.
- Policy checks such as reserved device names and access restrictions.

### 3.2 User-mode driver owns

- Device-specific open, read, write, and ioctl semantics.
- Device private state machine.
- Request handling loops and buffering policy.
- Future IRQ consumption logic via event-based wait APIs.

## 4. Architecture Overview

Today, device nodes are backed by kernel driver file operations registered directly into DevFS. The target architecture keeps the DevFS namespace in kernel space but allows a user-mode driver app to register a device endpoint dynamically. VFS remains unaware of hardware details and continues routing file operations through DevFS.

Target runtime flow:
1. A user-mode driver app starts.
2. The driver calls a syscall to register one or more devices in DevFS.
3. DevFS stores the device entry with owner pid and source type `user`.
4. Client apps open `/dev/<name>` through the normal VFS path.
5. DevFS dispatches operations to a driver-facing proxy path owned by that user driver.
6. If the driver exits, the kernel marks the device offline and reclaims the node.

## 5. ABI v1 (Channel + Pulse)

### 5.1 Required syscalls

- `SYS_DEVFS_REGISTER`
- `SYS_DEVFS_UNREGISTER`
- `SYS_CHANNEL_CREATE`
- `SYS_CHANNEL_DESTROY`
- `SYS_MSG_SEND`
- `SYS_MSG_RECEIVE`
- `SYS_MSG_REPLY`
- `SYS_IRQ_SUBSCRIBE`
- `SYS_IRQ_UNSUBSCRIBE`

`SYS_IRQ_WAIT` remains available for compatibility in this phase, but the
recommended path is IRQ-to-pulse delivery into a channel.

### 5.2 Rules

- Device names must be NUL-terminated and fully readable from the caller app address space.
- Reserved kernel device names cannot be overridden by user-mode drivers.
- A registered device is bound to exactly one owner pid.
- Unregister may only be performed by the owner pid or kernel cleanup path.
- IRQ subscription returns an integer handle owned by the caller pid.
- IRQ wait is event-based and must not execute user code in ISR context.
- For new drivers, IRQ subscription should include `channel_id`; IRQ events are
	delivered as pulse messages and consumed by `msg_receive`.

### 5.3 Channel message model

Channel message types:
- Request message (`PUP_MSG_TYPE_REQUEST`): requires driver reply via `msg_reply`.
- Pulse message (`PUP_MSG_TYPE_PULSE`): asynchronous notification, no reply.

DevFS proxy requests are encoded in a shared payload (`pup_devfs_proxy_payload_t`):
- `DEVFS_PROXY_OP_OPEN`
- `DEVFS_PROXY_OP_CLOSE`
- `DEVFS_PROXY_OP_READ`
- `DEVFS_PROXY_OP_WRITE`
- `DEVFS_PROXY_OP_IOCTL`

For channel multiplexing, each payload also carries `device_id` (driver-defined
logical id from `devfs_register`). This allows one server channel to host
multiple device nodes in a single receive loop.

## 6. DevFS Dynamic Device Model

DevFS distinguishes between:
- Kernel device
- User driver device

Recommended device metadata:
- `owner_pid`
- `source_type`
- `flags`
- `capabilities`

Registration must fail when:
- The name already exists.
- The name is reserved for kernel bootstrap devices.
- The supplied registration structure is invalid or outside caller-owned readable memory.
- The device table is exhausted.

Cleanup order on owner exit:
1. Mark the device offline.
2. Reject new open requests.
3. Drain and release driver-owned proxy objects.
4. Remove the device node.
5. Emit a log line including device name, owner pid, and reason.

## 7. Proxy Model Recommendation

The first version should avoid storing long-lived raw user pointers directly as VFS runtime objects. Instead, DevFS keeps a kernel-side proxy entry that records device metadata and ownership, while request dispatch is mediated by a channel endpoint owned by the user-mode driver.

This keeps recovery simple when a driver exits and makes a later switch to message-based drivers easier.

Current implementation status in this repository:
- DevFS user devices now store `channel_id` at registration time.
- DevFS user devices also store a driver-defined `device_id` for multiplexing.
- DevFS open/read/write/ioctl operations on user devices dispatch through
	channel requests and wait for driver reply.
- Request payload memory is allocated from shared heap and passed to the driver.

## 8. IRQ Delivery Model

Chosen strategy: mixed model, but only the event-notification path is enabled in the first migration phase.

Meaning:
- Critical IRQs may still keep a kernel fallback path.
- Non-critical IRQs notify a user-mode driver via a kernel-managed event or counter.
- No ISR executes user code directly.

For Phase 1 and the GPIO pilot:
- Build IRQ subscription tables and channel pulse semantics.
- Do not migrate console and UART behavior yet.
- Do not require GPIO to use IRQ forwarding in its first user-mode version.

Channel-pulse rule:
- An IRQ subscription may bind to a `channel_id`.
- Kernel injects a pulse message (`pulse_code = irq_num`) into that channel.
- Driver handles both client requests and IRQ pulses in one `msg_receive` loop.

## 9. First Migration Target: GPIO, LED, KEY

GPIO is the correct first target because:
- It does not require hard real-time ISR delivery in the initial implementation.
- Existing semantics are compact and clear in the kernel driver.
- It exercises registration, routing, ownership, cleanup, and error behavior.

Expected device set:
- `/dev/gpio0`
- `/dev/led0`
- `/dev/key0`

Semantics should remain compatible with the current kernel implementation for:
- `open`
- `read`
- `write`
- `ioctl`

## 10. Startup Strategy

Recommended first version:
- Do not auto-start drivers from system boot.
- Start the GPIO driver explicitly from shell or a test script.

This keeps the boot path stable while the new ABI is being validated and makes failure modes easier to isolate.

## 11. Failure Model

Chosen first-version policy:
- Driver failure causes device failure plus explicit logging.
- No automatic restart in the first version.

This keeps kernel policy simple and makes bugs observable. Supervision can be added only after the ABI and cleanup path are stable.

## 12. Implementation Plan

### 12.1 Phase 1: kernel minimum capability

1. Add syscall ABI and user wrappers.
2. Add DevFS dynamic device ownership model.
3. Add owner-exit cleanup hook.
4. Add IRQ subscription table skeleton.
5. Validate with no device migration yet.

### 12.2 Phase 2: GPIO pilot

1. Introduce `driver` as a user-mode app.
2. Register `/dev/gpio0`, `/dev/led0`, `/dev/key0` from that app.
3. Disable kernel GPIO registration.
4. Add regression coverage for device registration and driver failure.

### 12.3 Phase 3: IRQ-backed devices

1. Route UART or SPI IRQs through the subscription table.
2. Move device buffering and policy into the user-mode driver.
3. Evaluate whether any IRQ needs a kernel fallback path.

## 13. Verification Plan

### 13.1 ABI checks

- Syscall numbering matches between kernel and user headers.
- Invalid pointers are rejected safely.
- Reserved names are rejected.

### 13.2 Functional checks

- A driver can register GPIO, LED, and KEY nodes.
- Client apps can open and use those nodes through normal VFS calls.
- A driver can unregister its own nodes.

### 13.3 Failure checks

- Killing the driver removes or disables its nodes.
- Existing clients receive a stable error path.
- The kernel remains alive and shell stays usable.

### 13.4 Stability checks

- Repeated driver start and stop does not leak device entries.
- Repeated open and close does not leak fd state.
- The IRQ wait skeleton does not perturb existing console behavior.

## 14. File Impact Map

Primary implementation targets:
- `core/kernel/syscall.h`
- `core/kernel/syscall.c`
- `apps/include/syscall.h`
- `core/fs/pup_devfs.h`
- `core/fs/pup_devfs.c`
- `core/app/app_manager.c`
- `core/dev/dev_gpio.c`
- `xmake.lua`
- `core/init.c`

## 15. Document Placement

Recommended project document path:
- `docs/user-mode-driver-architecture.md`

This matches the current docs naming style and fits naturally after the shell and VFS architecture documents.