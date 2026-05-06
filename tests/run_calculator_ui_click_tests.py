#!/usr/bin/env python3
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path


PROJECT_DIR = Path("/Users/zhuzhe/Workspace/m5stack/M5CardputerZero-Calculator/projects/Calculator")
APP_PATH = PROJECT_DIR / "dist" / "M5CardputerZero-Calculator"
ARTIFACT_DIR = PROJECT_DIR / "tests" / "artifacts" / "ui_click"
SCREENSHOT_HELPER = Path("/Users/zhuzhe/.codex/skills/screenshot/scripts/take_screenshot.py")
KEY_HELPER_SOURCE = PROJECT_DIR / "tests" / "send_key_sequence.swift"
KEY_HELPER_BINARY = ARTIFACT_DIR / "send_key_sequence"
PROCESS_NAMES = ["M5CardputerZero-Calculator", "Calculator"]
WINDOW_POS = (980, 240)
WINDOW_SIZE = (320, 198)
ATTACH_ONLY = os.environ.get("CALCULATOR_ATTACH_ONLY") == "1"

LEFT = 123
RIGHT = 124
DOWN = 125
UP = 126
ENTER = 36
ESC = 53

BUTTON_POSITIONS = {
    "1": (0, 0),
    "2": (0, 1),
    "3": (0, 2),
    "4": (0, 3),
    "5": (0, 4),
    "6": (0, 5),
    "7": (0, 6),
    "8": (0, 7),
    "9": (0, 8),
    "0": (0, 9),
    "Del": (0, 10),
    "C": (1, 0),
    "sin": (1, 1),
    "cos": (1, 2),
    "+": (1, 3),
    "-": (1, 4),
    "/": (1, 5),
    "*": (1, 6),
    "^": (1, 7),
    "(": (1, 8),
    ")": (1, 9),
    "Rad": (1, 10),
    ".": (2, 0),
    "tan": (2, 1),
    "ln": (2, 2),
    "log": (2, 3),
    "sqrt": (2, 4),
    "=": (2, 5),
    "abs": (2, 6),
    "pi": (2, 7),
    "e": (2, 8),
    "!": (2, 9),
    "OK": (2, 10),
    "2nd": (3, 0),
    "Ans": (3, 1),
    "Rand": (3, 2),
    "1/x": (3, 3),
    "x^2": (3, 4),
    "x^3": (3, 5),
    "x^y": (3, 6),
    "e^x": (3, 7),
    "10^x": (3, 8),
    "%": (3, 9),
    "+/-": (3, 10),
}

SCENARIOS = [
    {
        "id": "parentheses_times_twenty",
        "description": "Basic precedence with parentheses",
        "clicks": ["(", "1", "+", "9", ")", "*", "2", "0", "="],
        "expected_expression": "(1+9)*20",
        "expected_result": "200",
    },
    {
        "id": "ignore_unmatched_close_paren",
        "description": "A closing parenthesis without an opening parenthesis should be ignored",
        "clicks": [")", "1", "+", "2", "="],
        "expected_expression": "1+2",
        "expected_result": "3",
    },
    {
        "id": "sin_30_degree",
        "description": "Sine in degree mode",
        "clicks": ["Rad", "sin", "3", "0", "="],
        "expected_expression": "sin(30)",
        "expected_result": "0.5",
    },
    {
        "id": "cos_60_degree",
        "description": "Cosine in degree mode",
        "clicks": ["Rad", "cos", "6", "0", "="],
        "expected_expression": "cos(60)",
        "expected_result": "0.5",
    },
    {
        "id": "tan_45_degree",
        "description": "Tangent in degree mode",
        "clicks": ["Rad", "tan", "4", "5", "="],
        "expected_expression": "tan(45)",
        "expected_result": "1",
    },
    {
        "id": "asin_half_degree",
        "description": "Inverse sine in degree mode",
        "clicks": ["Rad", "2nd", "sin", "0", ".", "5", "="],
        "expected_expression": "asin(0.5)",
        "expected_result": "30",
    },
    {
        "id": "ln_e",
        "description": "Natural logarithm of e",
        "clicks": ["ln", "e", "="],
        "expected_expression": "ln(e)",
        "expected_result": "1",
    },
    {
        "id": "log_1000",
        "description": "Common logarithm",
        "clicks": ["log", "1", "0", "0", "0", "="],
        "expected_expression": "log(1000)",
        "expected_result": "3",
    },
    {
        "id": "sqrt_81",
        "description": "Square root",
        "clicks": ["sqrt", "8", "1", "="],
        "expected_expression": "sqrt(81)",
        "expected_result": "9",
    },
    {
        "id": "reciprocal_four",
        "description": "Reciprocal",
        "clicks": ["4", "1/x", "="],
        "expected_expression": "1/(4)",
        "expected_result": "0.25",
    },
    {
        "id": "ten_pow_three",
        "description": "Power of ten",
        "clicks": ["10^x", "3", "="],
        "expected_expression": "10^3",
        "expected_result": "1000",
    },
    {
        "id": "long_result_many_ones",
        "description": "Long integer result should stay fully readable",
        "clicks": ["1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "*", "8", "="],
        "expected_expression": "1111111111*8",
        "expected_result": "8888888888",
    },
    {
        "id": "long_decimal_reciprocal",
        "description": "Long decimal result should stay fully readable",
        "clicks": ["1", "/", "(", "4", "3", "5", "6", ")", "="],
        "expected_expression": "1/(4356)",
        "expected_result": "0.000229568411387",
    },
    {
        "id": "ignore_duplicate_decimal",
        "description": "A duplicate decimal point in the same number should be ignored",
        "clicks": ["1", ".", ".", "2", "="],
        "expected_expression": "1.2",
        "expected_result": "1.2",
    },
]


def run(cmd, **kwargs):
    return subprocess.run(cmd, check=True, text=True, **kwargs)


def run_osascript(script: str) -> str:
    result = run(["osascript", "-"], input=script, capture_output=True)
    return result.stdout.strip()


def ensure_key_helper():
    if KEY_HELPER_BINARY.exists() and KEY_HELPER_BINARY.stat().st_mtime >= KEY_HELPER_SOURCE.stat().st_mtime:
        return
    run(["swiftc", "-O", "-o", str(KEY_HELPER_BINARY), str(KEY_HELPER_SOURCE)])


def find_app_pid() -> int:
    result = run(["ps", "-ax", "-o", "pid=,command="], capture_output=True)
    for line in reversed(result.stdout.splitlines()):
        if str(APP_PATH) in line or "dist/M5CardputerZero-Calculator" in line:
            return int(line.strip().split(None, 1)[0])
    raise RuntimeError("Calculator PID not found")


def wait_for_window(timeout: float = 10.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for app_name in PROCESS_NAMES:
            result = subprocess.run(
                ["python3", str(SCREENSHOT_HELPER), "--list-windows", "--app", app_name],
                check=True,
                text=True,
                capture_output=True,
            )
            for line in result.stdout.splitlines():
                bounds = parse_window_info(line)
                if bounds is None:
                    continue
                if bounds["w"] == WINDOW_SIZE[0] and bounds["h"] == WINDOW_SIZE[1]:
                    run_osascript(
                        "\n".join([
                            'tell application "System Events"',
                            f'  if exists process "{bounds["process"]}" then',
                            f'    tell process "{bounds["process"]}"',
                            "      set frontmost to true",
                            "    end tell",
                            "  end if",
                            "end tell",
                        ])
                    )
                    return bounds
        time.sleep(0.1)
    raise RuntimeError("Timed out waiting for calculator window")


def parse_window_info(text: str):
    match = re.match(r"^(\d+)\t([^\t]+)\t([^\t]+)\t(\d+)x(\d+)\+(-?\d+)\+(-?\d+)$", text.strip())
    if not match:
        return None
    return {
        "window_id": int(match.group(1)),
        "process": match.group(2),
        "name": match.group(3),
        "w": int(match.group(4)),
        "h": int(match.group(5)),
        "x": int(match.group(6)),
        "y": int(match.group(7)),
    }


def build_key_sequence(clicks):
    key_codes = [LEFT] * 12 + [UP] * 8
    current_row, current_col = 0, 0
    for label in clicks:
        target_row, target_col = BUTTON_POSITIONS[label]
        while current_col > target_col:
            key_codes.append(LEFT)
            current_col -= 1
        while current_col < target_col:
            key_codes.append(RIGHT)
            current_col += 1
        while current_row > target_row:
            key_codes.append(UP)
            current_row -= 1
        while current_row < target_row:
            key_codes.append(DOWN)
            current_row += 1
        key_codes.append(ENTER)
    return key_codes


def send_key_sequence(process_name: str, app_pid: int, key_codes):
    run_osascript(
        "\n".join([
            'tell application "System Events"',
            f'  if exists process "{process_name}" then',
            f'    tell process "{process_name}"',
            "      set frontmost to true",
            "    end tell",
            "  end if",
            "end tell",
        ])
    )
    run([str(KEY_HELPER_BINARY), str(app_pid), *[str(code) for code in key_codes]])


def launch_app(state_file: Path, log_file: Path):
    env = os.environ.copy()
    env["LV_SDL_VIDEO_WIDTH"] = "320"
    env["LV_SDL_VIDEO_HEIGHT"] = "170"
    env["CALCULATOR_STATE_PATH"] = str(state_file)
    log_fp = log_file.open("w")
    proc = subprocess.Popen(["script", "-q", "/dev/null", str(APP_PATH)],
                            cwd=PROJECT_DIR, env=env, stdout=log_fp,
                            stderr=subprocess.STDOUT)
    return proc, log_fp


def kill_app(proc, log_fp):
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)
    log_fp.close()


def read_state_file(path: Path):
    if not path.exists():
        return {}
    state = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            state[key] = value
    return state


def wait_for_clear_state(state_file: Path, timeout: float = 5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        state = read_state_file(state_file)
        if state.get("expression", "") == "" and state.get("result") == "0":
            return state
        time.sleep(0.05)
    raise RuntimeError(f"Clear-state mismatch: {read_state_file(state_file)}")


def wait_for_expected_state(state_file: Path, expected_expression: str, expected_result: str,
                            timeout: float = 5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        state = read_state_file(state_file)
        if (state.get("expression") == expected_expression and
                state.get("result") == expected_result):
            return state
        time.sleep(0.05)
    raise RuntimeError(
        f"State mismatch. expected expression={expected_expression} result={expected_result} "
        f"actual={read_state_file(state_file)}"
    )


def reset_running_app(process_name: str, app_pid: int, state_file: Path):
    send_key_sequence(process_name, app_pid, build_key_sequence(["C"]))
    state = wait_for_clear_state(state_file)
    if state.get("angle_mode") == "degree":
        send_key_sequence(process_name, app_pid, build_key_sequence(["Rad"]))
        time.sleep(0.1)


def capture_window(bounds, output_path: Path):
    run([
        "screencapture",
        "-x",
        f"-R{bounds['x']},{bounds['y']},{bounds['w']},{bounds['h']}",
        str(output_path),
    ])


def write_report(results):
    report_path = ARTIFACT_DIR / "report.md"
    lines = ["# Calculator UI Click Test Report", ""]
    for result in results:
        lines.append(f"## {result['id']}")
        lines.append(result["description"])
        lines.append("")
        lines.append(f"- Clicks: `{' -> '.join(result['clicks'])}`")
        lines.append(f"- Expected formula: `{result['expected_expression']}`")
        lines.append(f"- Expected result: `{result['expected_result']}`")
        lines.append(f"- Actual formula: `{result['actual_expression']}`")
        lines.append(f"- Actual result: `{result['actual_result']}`")
        lines.append(f"- Screenshot: [{result['screenshot'].name}]({result['screenshot']})")
        lines.append("")
    report_path.write_text("\n".join(lines))
    return report_path


def main():
    if not APP_PATH.exists():
        raise RuntimeError(f"Calculator binary not found: {APP_PATH}")

    ARTIFACT_DIR.mkdir(parents=True, exist_ok=True)
    ensure_key_helper()
    results = []

    if ATTACH_ONLY:
        shared_state_file = Path(os.environ.get("CALCULATOR_STATE_PATH", ARTIFACT_DIR / "attached.state"))
        bounds = wait_for_window()
        process_name = bounds["process"]
        app_pid = find_app_pid()
        for scenario in SCENARIOS:
            state_file = ARTIFACT_DIR / f"{scenario['id']}.state"
            screenshot_path = ARTIFACT_DIR / f"{scenario['id']}.png"
            reset_running_app(process_name, app_pid, shared_state_file)
            send_key_sequence(process_name, app_pid, build_key_sequence(scenario["clicks"]))
            state = wait_for_expected_state(
                shared_state_file,
                scenario["expected_expression"],
                scenario["expected_result"],
            )
            state_file.write_text(shared_state_file.read_text())
            capture_window(bounds, screenshot_path)
            results.append({
                "id": scenario["id"],
                "description": scenario["description"],
                "clicks": scenario["clicks"],
                "expected_expression": scenario["expected_expression"],
                "expected_result": scenario["expected_result"],
                "actual_expression": state.get("expression", ""),
                "actual_result": state.get("result", ""),
                "screenshot": screenshot_path,
            })
    else:
        for scenario in SCENARIOS:
            state_file = ARTIFACT_DIR / f"{scenario['id']}.state"
            log_file = ARTIFACT_DIR / f"{scenario['id']}.log"
            screenshot_path = ARTIFACT_DIR / f"{scenario['id']}.png"
            if state_file.exists():
                state_file.unlink()

            proc, log_fp = launch_app(state_file, log_file)
            try:
                bounds = wait_for_window()
                send_key_sequence(bounds["process"], find_app_pid(), build_key_sequence(scenario["clicks"]))
                state = wait_for_expected_state(
                    state_file,
                    scenario["expected_expression"],
                    scenario["expected_result"],
                )
                capture_window(bounds, screenshot_path)
                results.append({
                    "id": scenario["id"],
                    "description": scenario["description"],
                    "clicks": scenario["clicks"],
                    "expected_expression": scenario["expected_expression"],
                    "expected_result": scenario["expected_result"],
                    "actual_expression": state.get("expression", ""),
                    "actual_result": state.get("result", ""),
                    "screenshot": screenshot_path,
                })
            finally:
                kill_app(proc, log_fp)
                time.sleep(0.15)

    report_path = write_report(results)
    print(f"Completed {len(results)} UI click scenarios")
    print(report_path)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"UI click test failed: {exc}", file=sys.stderr)
        sys.exit(1)
