import argparse
import subprocess
import threading
import time
from pathlib import Path

DEFAULT_QEMU = r"D:\RT-ThreadStudio\repo\Extract\Debugger_Support_Packages\RealThread\QEMU\4.2.0.4\qemu-system-arm.exe"
DEFAULT_MACHINE = "stm32f407-atk-explorer"
DEFAULT_FIRMWARE = "output/firmware/firmware.bin"
PROMPT = "puppy>"
CHAR_DELAY = 0.20
POST_PROMPT_DELAY = 1.0
CHAR_ECHO_TIMEOUT = 1.0


class QemuCmdRunner:
    def __init__(self, qemu_path: str, machine: str, firmware_path: str):
        self.qemu_path = qemu_path
        self.machine = machine
        self.firmware_path = firmware_path
        self.process = None
        self.reader_thread = None
        self.buffer = ""
        self.lock = threading.Lock()
        self.stop_reader = False

    def start(self) -> None:
        cmd = [
            self.qemu_path,
            "-M",
            self.machine,
            "-kernel",
            self.firmware_path,
            "-serial",
            "stdio",
            "-monitor",
            "none",
        ]
        self.process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
            creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0),
        )
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

    def _reader_loop(self) -> None:
        assert self.process is not None
        assert self.process.stdout is not None

        while not self.stop_reader:
            chunk = self.process.stdout.read(1)
            if not chunk:
                break

            text = chunk.decode("utf-8", errors="replace")
            print(text, end="", flush=True)
            with self.lock:
                self.buffer += text

    def wait_for_text(self, text: str, timeout: float) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if text in self.buffer:
                    return True
            if self.process is not None and self.process.poll() is not None:
                break
            time.sleep(0.05)
        return False

    def snapshot_len(self) -> int:
        with self.lock:
            return len(self.buffer)

    def wait_for_text_since(self, text: str, start_len: int, timeout: float) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if text in self.buffer[start_len:]:
                    return True
            if self.process is not None and self.process.poll() is not None:
                break
            time.sleep(0.05)
        return False

    def count_occurrences(self, text: str) -> int:
        with self.lock:
            return self.buffer.count(text)

    def wait_for_echo(self, char: str, start_len: int, timeout: float) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                suffix = self.buffer[start_len:]
                if char in suffix:
                    return True
            if self.process is not None and self.process.poll() is not None:
                break
            time.sleep(0.02)
        return False

    def send_command(self, command: str) -> None:
        assert self.process is not None
        assert self.process.stdin is not None

        for ch in command:
            with self.lock:
                start_len = len(self.buffer)
            self.process.stdin.write(ch.encode("utf-8"))
            self.process.stdin.flush()
            if not self.wait_for_echo(ch, start_len, CHAR_ECHO_TIMEOUT):
                time.sleep(CHAR_DELAY)

        self.process.stdin.write(b"\r\n")
        self.process.stdin.flush()

    def terminate(self) -> None:
        self.stop_reader = True
        if self.process is None:
            return
        if self.process.poll() is None:
            self.process.kill()
        if self.reader_thread is not None:
            self.reader_thread.join(timeout=1.0)


def run_default_sequence(runner: QemuCmdRunner, isolation: bool = False) -> int:
    """Run the default test sequence.

    When ``isolation`` is False (default, PUP_MPU_APP_ISOLATION_ENABLED=0) the
    per-app MPU profile is not active, so boundary-violation tests are skipped.
    Pass ``isolation=True`` to enable those tests when the kernel is built with
    PUP_MPU_APP_ISOLATION_ENABLED=1.
    """
    steps = [
        {
            "name": "boot prompt",
            "command": None,
            "expects": [PROMPT],
            "timeout": 8.0,
        },
        {
            "name": "self range",
            "command": "/rom/mpu_test self",
            "expects": ["[mpu_test] Self RAM range:", PROMPT],
            "timeout": 8.0,
        },
    ]

    if isolation:
        steps += [
        {
            "name": "violate end",
            "command": "/rom/mpu_test violate_end",
            "expects": [
                "[mpu_test] Violate end boundary",
                "MPU Memory Management Fault",
                "User application memory fault, terminating",
                PROMPT,
            ],
            "timeout": 10.0,
        },
        {
            "name": "violate start",
            "command": "/rom/mpu_test violate_start",
            "expects": [
                "[mpu_test] Violate start boundary",
                "MPU Memory Management Fault",
                "User application memory fault, terminating",
                PROMPT,
            ],
            "timeout": 10.0,
        },
        {
            "name": "driver chain",
            "command": "/rom/test driver",
            "expects": [
                "[driver] chain test completed",
                "Test driver PASSED",
                PROMPT,
            ],
            "timeout": 10.0,
        },
    ]

    steps += [
        {
            "name": "start lfs service",
            "command": "/rom/lfs &",
            "expects": [
                "[lfs_service] starting...",
                "[userfs] service 'lfs' started",
                PROMPT,
            ],
            "timeout": 8.0,
        },
        {
            "name": "userfs test",
            "command": "/rom/test fs",
            "expects": [
                "=== UserFS (lfs) Test ===",
                "[fs] Mounted lfs at /data",
                "[fs] PASSED: Data verified",
                "[fs] UserFS test PASSED",
                PROMPT,
            ],
            "timeout": 15.0,
        },
    ]

    for step in steps:
        print(f"\n[runner] Step: {step['name']}")
        start_len = runner.snapshot_len()
        if step["command"]:
            time.sleep(POST_PROMPT_DELAY)
            runner.send_command(step["command"])
        for expected in step["expects"]:
            if not runner.wait_for_text_since(expected, start_len, step["timeout"]):
                print(f"\n[runner] ERROR: missing expected text: {expected}")
                return 1
            print(f"\n[runner] Matched: {expected}")
    return 0


def run_switch_stress_sequence(runner: QemuCmdRunner, cycles: int = 5) -> int:
    print(f"\n[runner] Step: switch stress ({cycles} cycles)")

    for i in range(cycles):
        print(f"\n[runner] Stress cycle {i + 1}/{cycles}")

        # Start one lightweight app in background, then switch to another app in foreground.
        start_len = runner.snapshot_len()
        time.sleep(POST_PROMPT_DELAY)
        runner.send_command("/rom/mpu_test self &")

        bg_expects = [
            "[mpu_test] Self RAM range:",
            PROMPT,
        ]
        for expected in bg_expects:
            if not runner.wait_for_text_since(expected, start_len, 12.0):
                print(f"\n[runner] ERROR: missing background expected text: {expected}")
                return 1
            print(f"\n[runner] Matched: {expected}")

        start_len = runner.snapshot_len()
        time.sleep(POST_PROMPT_DELAY)
        runner.send_command("/rom/test")

        fg_expects = [
            "=== Puppy User Test App ===",
            PROMPT,
        ]
        for expected in fg_expects:
            if not runner.wait_for_text_since(expected, start_len, 14.0):
                print(f"\n[runner] ERROR: missing foreground expected text: {expected}")
                return 1
            print(f"\n[runner] Matched: {expected}")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Simple QEMU command injection runner for puppy-next")
    parser.add_argument("--qemu", default=DEFAULT_QEMU, help="Path to qemu-system-arm.exe")
    parser.add_argument("--machine", default=DEFAULT_MACHINE, help="QEMU machine name")
    parser.add_argument("--firmware", default=DEFAULT_FIRMWARE, help="Firmware path relative to repo root")
    parser.add_argument("--isolation", action="store_true",
                        help="Enable isolation-mode tests (requires PUP_MPU_APP_ISOLATION_ENABLED=1 build)")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    firmware_path = Path(args.firmware)
    if not firmware_path.is_absolute():
        firmware_path = repo_root / firmware_path

    if not Path(args.qemu).exists():
        print(f"[runner] ERROR: qemu not found: {args.qemu}")
        return 2
    if not firmware_path.exists():
        print(f"[runner] ERROR: firmware not found: {firmware_path}")
        return 2

    runner = QemuCmdRunner(args.qemu, args.machine, str(firmware_path))
    try:
        runner.start()
        ret = run_default_sequence(runner, isolation=args.isolation)
        if ret != 0:
            return ret
        return run_switch_stress_sequence(runner)
    finally:
        runner.terminate()
        print("\n[runner] QEMU stopped")


if __name__ == "__main__":
    raise SystemExit(main())
