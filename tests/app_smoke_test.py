#!/usr/bin/env python3
"""Launch a build or installed binary, check QML loading and second-instance activation.

On Windows run this only in a disposable CI account: QStandardPaths uses that user's
AppData. Linux runs with an isolated XDG configuration directory.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as config, tempfile.TemporaryFile(mode="w+") as log:
    env = {**os.environ, "XDG_CONFIG_HOME": config, "QT_QPA_PLATFORM": "windows" if os.name == "nt" else "offscreen",
           "QT_QUICK_BACKEND": "software"}
    first = subprocess.Popen([binary, "--autostart"], env=env, stdout=log, stderr=log)
    try:
        time.sleep(2)
        if first.poll() is not None:
            raise RuntimeError(f"Primary exited: {first.returncode}")
        for args in (["--autostart"], []):
            subprocess.run([binary, *args], env=env, stdout=log, stderr=log, check=True, timeout=15)
        if first.poll() is not None:
            raise RuntimeError("Primary exited after activation")
    finally:
        first.terminate()
        try:
            first.wait(timeout=5)
        except subprocess.TimeoutExpired:
            first.kill()
            first.wait()
        log.seek(0)
        output = log.read()
        if any(error in output for error in ("failed to load component", "ReferenceError:",
                                              "TypeError:", "is not a type", "Cannot assign")):
            raise RuntimeError(output)
print("Application startup, QML loading and activation passed")
