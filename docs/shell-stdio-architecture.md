# Shell And Standard I/O Architecture Design

## 1. Goals

- Make kernel shell the default interactive entry.
- Launch user shell from kernel shell and return back automatically when user shell exits.
- Unify standard input/output path used by kernel shell and user shell.
- Keep backend pluggable while defaulting to DTM debug UART.
- Eliminate duplicated UART RX/TX logic in syscall and shell paths.

## 2. Runtime Topology

### 2.1 Boot and shell lifecycle

1. Board init configures clock, UART and timer.
2. `pup_console_init()` initializes console backend and RX interrupt pipeline.
3. System starts kernel shell (`puppy_shell_thread`).
4. Kernel shell command `ushell` executes `/rom/shell_app` with wait mode.
5. User shell runs as user app and uses syscall read/write.
6. User shell command `exit` calls `user_exit(0)`.
7. Kernel app manager observes app exit and returns control to kernel shell.

### 2.2 Address-space model

- Kernel image:
  - ROM: from `DTM_FLASH_BASE` (`dtm/link.lds`)
  - RAM: from `DTM_RAM_BASE` (`dtm/link.lds`)
- User app image:
  - ROM: `0x08080000` (`user_apps/linker/app.lds`)
  - RAM: `0x20010000` (`user_apps/linker/app.lds`)
- MPU protects kernel/user RAM partition and user-mode execution boundary.

## 3. Layered Design

### Layer A: Console abstraction (new)

File:
- `core/dev/pup_console.h`
- `core/dev/pup_console.c`

Public API:
- `pup_console_init()`
- `pup_console_set_backend()`
- `pup_console_write()`
- `pup_console_read()`
- `pup_console_putchar()`
- `pup_console_getchar()`

Backend interface:
- `pup_console_backend_t.write`
- `pup_console_backend_t.read`

Responsibility:
- Provide one kernel-wide standard I/O abstraction.
- Hold backend pointer and route read/write calls.
- Implement default DTM UART backend.

### Layer B: Default UART backend

Implemented in `core/dev/pup_console.c`.

TX path:
- polling `dtm_uart_putc`.
- newline normalization `\n` -> `\r\n`.

RX path:
- UART interrupt callback pushes bytes into ring buffer.
- ring buffer guarded by IRQ lock.
- readers block on semaphore when buffer is empty.

Responsibility:
- Deterministic and simple behavior for board bring-up.
- Avoid busy-spin in input consumers.

### Layer C: Kernel and syscall consumers

Kernel output path:
- `PUP_PRINTK` -> `libcm_printf` -> `libcm_console_output` -> `pup_console_write`.

Kernel shell path:
- input: `pup_console_getchar`
- output: `pup_console_putchar` / `pup_console_write`

User shell path:
- `user_read(fd, ...)` -> `SYS_READ` -> `sys_read`
- `user_write(fd, ...)` -> `SYS_WRITE` -> `sys_write`
- fd dispatch in syscall layer:
  - `fd=0(stdin)` -> `pup_console_read`
  - `fd=1(stdout)` / `fd=2(stderr)` -> `pup_console_write`
  - `fd>=3` -> VFS `pup_read` / `pup_write`

Responsibility:
- Reuse exactly the same console backend for kernel and user shell.
- Keep syscall layer independent from UART-specific details.
- Provide a POSIX-like fd-oriented I/O model to user apps.

## 4. Data Flow

### 4.1 Output flow

1. Producer generates text (`PUP_PRINTK` or user `printf`).
2. Kernel syscall/output bridge calls `pup_console_write`.
3. Active backend handles transmit (default UART backend).
4. Characters are written to DTM debug UART.

### 4.2 Input flow

1. UART RX interrupt receives bytes.
2. Callback writes bytes into ring buffer and posts semaphore.
3. Consumer (`pup_console_read` / `pup_console_getchar`) blocks on semaphore when empty.
4. Consumer receives bytes in FIFO order.

### 4.3 Shell switch flow

1. Kernel shell executes `ushell`.
2. `app_exec("shell_app", "/rom/shell_app", 1)` starts app and waits.
3. User shell loop handles commands.
4. User enters `exit`.
5. `user_exit` triggers `SYS_EXIT`; app manager marks app exited.
6. Wait returns and kernel shell prompt is shown again.

## 5. Interface Contract

### 5.0 Standard fd reservation

- `0`: stdin
- `1`: stdout
- `2`: stderr
- `>=3`: regular VFS file descriptors returned by `pup_open`

Contract:
- VFS allocator reserves `0/1/2` and allocates regular file descriptors from `3`.
- Syscall read/write use fd dispatch and must not treat `>=3` as console.

### 5.1 `pup_console_*`

- `pup_console_init()`:
  - idempotent.
  - safe to call multiple times.
- `pup_console_write(buf, len)`:
  - returns written length or 0 for invalid input.
- `pup_console_read(buf, len)`:
  - blocking read.
  - returns read length or negative error.
- `pup_console_set_backend(backend)`:
  - switch backend at runtime.
  - fallback to default UART backend when `backend == NULL`.

### 5.2 Syscall compatibility

- `SYS_READ` / `SYS_WRITE` are now unified fd-style interfaces.
- Legacy `SYS_READ_FD` / `SYS_WRITE_FD` are removed.
- User wrappers (`user_fs_read` / `user_fs_write`) are kept and mapped to unified syscall interfaces.

## 6. Migration Summary

Completed migration:
- Moved duplicated UART RX semaphore/ring logic out of `core/kernel/syscall.c`.
- Merged `read/write` and `read_fd/write_fd` into fd-based `sys_read/sys_write`.
- Added fd dispatch in syscall (`0/1/2` console, `>=3` VFS).
- Reserved standard fds in VFS allocator and started regular fd allocation from `3`.
- Converted kernel shell input/output to `pup_console_*`.
- Converted `libcm_console_output` to unified console output path.
- Kept `syscall_uart_rx_init` as compatibility shim forwarding to `pup_console_init`.

## 7. Error Handling and Concurrency Notes

- RX ring buffer is protected by IRQ lock, not by mutex.
- Input consumers block on semaphore; this avoids polling loops.
- Buffer full policy is drop-new-byte (current behavior).
- Backend function pointers are expected to be valid after init.

## 8. Future Enhancements

- Add optional non-blocking read mode for shell/editor features.
- Add line discipline support (echo mode, backspace normalization, simple history).
- Add backend implementations for USB CDC and semihosting.
- Add console statistics (`rx_drop`, `rx_overrun`, `tx_bytes`) for diagnostics.
- Add unit/integration tests for console read/write and shell switch behavior.
