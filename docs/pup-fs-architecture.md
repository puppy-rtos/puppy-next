# PUP FS Architecture Design

## 1. Goals

- Provide a single file descriptor based file API for kernel and user syscall layers.
- Keep API shape close to POSIX style (open/close/read/write/ioctl/lseek/getdents/stat/unlink/rename).
- Remove duplicated file table logic and reduce implementation redundancy.
- Keep file system backend pluggable through filesystem ops.

## 2. Layered Model

### Layer A: Public VFS API

- `pup_open`
- `pup_close`
- `pup_read`
- `pup_write`
- `pup_ioctl`
- `pup_lseek`
- `pup_fsync`
- `pup_getdents`
- `pup_stat`
- `pup_unlink`
- `pup_rename`
- `pup_fs_mount`
- `pup_fs_unmount`
- `pup_fs_statfs`

Responsibility:
- Path normalization
- Mount-point lookup
- File descriptor allocation and lifetime management
- Dispatch to backend filesystem ops

### Layer B: VFS Core State

- Filesystem type registry: `g_pup_fs_ops_table`
- Mounted filesystem table: `g_pup_fs_table`
- Descriptor table: `g_pup_fd_table`

Responsibility:
- Manage global VFS resources
- Guarantee reference lifetime of descriptors
- Maintain simple lock guards for fs table and fd table

### Layer C: Filesystem Backend

Example:
- ROM backend in `pup_romfs.c`
- Device backend in `pup_devfs.c`

Responsibility:
- Implement `pup_file_ops` and `pup_fs_ops`
- Handle backend-specific open/read/stat/getdents details

## 3. Data Flow

### open path flow

1. `pup_open(path, flags)`
2. `pup_path_normalize` normalizes path
3. `pup_fs_lookup_by_path` resolves mounted filesystem
4. `pup_fd_alloc_slot` allocates descriptor slot
5. Convert mount path to backend relative subpath if needed
6. Call backend `fops->open`
7. Return fd index

### read/write/ioctl flow

1. `pup_fd_get(fd)` acquires descriptor reference
2. Dispatch to backend operation
3. `pup_fd_put(fd, fd_obj)` drops reference and frees slot if needed

### close flow

1. `pup_fd_get(fd)`
2. Optional backend close callback
3. Release path memory
4. `pup_fd_put(fd, fd_obj)` final release

## 4. Naming Rules Used

- Global PUP FS state variables start with `g_pup_`
- Internal helper functions use clear prefixes:
  - path helpers: `pup_path_*`
  - fd slot helpers: `pup_fd_*`
  - fs lookup helper: `pup_fs_*`
- Error returns use PUP FS named constants where applicable:
  - `PUP_FS_EOK`
  - `PUP_FS_EFULL`
  - `PUP_FS_ENOENT`
  - `PUP_FS_ENOSYS`
  - `PUP_FS_EIO`
  - `PUP_FS_EXDEV`

## 5. Simplifications Done

- Unified fd reference release with one helper: `pup_fd_put`
- Removed repeated ref_count decrement branches in read/write/ioctl/lseek/fsync/getdents/close paths
- Replaced mixed helper names with consistent naming
- Reduced raw magic error returns by introducing PUP FS error-code macros

## 6. Compatibility and API Direction

- External API is unified to `pup_*` and `pup_fs_*`, this is the only file API surface.
- Device registration is handled by `pup_devfs_*` registry API.
- Legacy duplicated file API layers should not be reintroduced.

## 7. Future Enhancements

- Replace spin-wait locks with proper arch lock primitives or spinlock wrappers.
- Introduce explicit descriptor state flags (readable/writable/error) instead of overloading open flags.
- Add shared errno mapping for syscall layer to keep user-visible errors stable.
- Add basic PUP FS unit tests for open/close race, mount conflicts, and getdents boundaries.
