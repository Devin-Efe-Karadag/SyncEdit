import os
import pathlib
import subprocess
import sys
import tempfile
import time

binary = os.path.abspath(sys.argv[1])
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    processes = []

    def start(label, port, *args):
        env = dict(os.environ, SYNCEDIT_TEST_HEADLESS='1',
                   SYNCEDIT_TEST_DATA_DIR=str(root / label),
                   SYNCEDIT_TEST_LISTEN='127.0.0.1:' + str(port))
        p = subprocess.Popen([binary, 'notes', *args], env=env, stdin=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True)
        processes.append(p)
        lines = [p.stderr.readline() for _ in range(3)]
        assert p.poll() is None, lines
        return p, root / label

    def command(p, text):
        p.stdin.write(text + '\n')
        p.stdin.flush()

    def stop(p):
        command(p, 'quit')
        assert p.wait(timeout=10) == 0, p.stderr.read()

    def wait_for(paths, text):
        deadline = time.monotonic() + 15
