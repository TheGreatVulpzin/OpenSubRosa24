"""Start the original server of a recorder folder and watch it in the console of bot.py.

  python -X utf8 instrumentation/recording/live_view.py [server folder] [bot.py arguments]

Without a folder it uses build/testserver (a copy of an original server folder, set to the World mode).

The server runs without a window. The bot joins it, goes into the base of company 0 and stays until you
press Q (the keys are in the header of bot.py). The server stops when the console ends. Run it in a real
terminal, without -I, so that numpy loads and the terminal can answer the sixel question.
"""
import os
import subprocess
import sys
import time
from pathlib import Path


def main():
    rest = sys.argv[1:]
    if rest and not rest[0].startswith("-"):
        folder, rest = rest[0], rest[1:]
    else:
        folder = str(Path(__file__).resolve().parents[2] / "build" / "testserver")
    exe = os.path.join(folder, "subrosadedicated024d.exe")
    if not os.path.exists(exe):
        print("no subrosadedicated024d.exe in " + folder)
        return 1
    server = subprocess.Popen([exe], cwd=folder, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              creationflags=0x08000000)
    try:
        time.sleep(4)
        bot = Path(__file__).with_name("bot.py")
        args = [sys.executable, str(bot), "--pid", str(server.pid), "--console", "--company", "0",
                "--stay", "100000", "--leave"] + rest
        return subprocess.run(args).returncode
    finally:
        server.kill()


if __name__ == "__main__":
    sys.exit(main())
