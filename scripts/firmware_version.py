"""Expose the current Git revision to the firmware build."""

from pathlib import Path
import subprocess

Import("env")


project_dir = Path(env.subst("$PROJECT_DIR"))


def git_output(*args: str) -> str:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=project_dir,
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return ""
    return result.stdout.strip()


revision = git_output("rev-parse", "--short=12", "HEAD") or "unknown"
firmware_status = git_output(
    "status",
    "--porcelain",
    "--untracked-files=normal",
    "--",
    "src",
    "include",
    "lib",
    "platformio.ini",
    "scripts/firmware_version.py",
)
if firmware_status and revision != "unknown":
    revision += "-dirty"

env.Append(CPPDEFINES=[("FIRMWARE_GIT_COMMIT", env.StringifyMacro(revision))])
