"""Exercise actual profile sessions through a PTY, without menu bypass hooks."""
import os
import pathlib
import pty
import select
import subprocess
import sys
import tempfile
import time
import fcntl

with tempfile.TemporaryDirectory() as directory:
    env = {k: v for k, v in os.environ.items() if not k.startswith('SYNCEDIT_TEST_')}
    env.update(XDG_DATA_HOME=directory, TERM='xterm-256color')
    master, slave = pty.openpty()
    p = subprocess.Popen([sys.argv[1]], stdin=slave, stdout=slave, stderr=slave, env=env)
    pending = b''

    def expect(text):
        global pending
        end = time.monotonic() + 10
        target = text.encode()
        while target not in pending:
            assert time.monotonic() < end, (text, pending)
            assert p.poll() is None, pending
            if select.select([master], [], [], .1)[0]:
                pending += os.read(master, 65536)
        pending = pending.split(target, 1)[1]

    def send(text):
        os.write(master, text.encode())

    try:
        expect('Create a profile before opening documents.')
        send('1\n')
        expect('Choose a listed number or N.')
        send('n\n')
        expect('New profile name:')
        send('alice\n')
        expect('Documents for alice')
        assert list(pathlib.Path(directory).rglob('profile_name'))
        send('n\n')
        expect('Document name:')
        send('notes\n')
        expect('Ctrl-L log out')
        send('hello')
        time.sleep(.2)
        send('\x11')
        expect('Documents for alice')
        assert next(pathlib.Path(directory).rglob('snapshot.txt')).read_text() == 'hello'
        send('1\n')
        expect('Ctrl-L log out')
        send('\x0c')
        expect('Logged out.')
        expect('Profiles')
        send('n\n')
        expect('New profile name:')
        send('bob\n')
        expect('Documents for bob')
        send('l\n')
        expect('Logged out.')
