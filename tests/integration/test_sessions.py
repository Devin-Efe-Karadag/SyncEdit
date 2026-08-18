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
        send('1\n')
        expect('Documents for alice')
        expect('1. notes')
        profile_path = pathlib.Path(directory) / 'syncedit/profiles' / 'alice'.encode().hex()
        with open(profile_path / 'profile.lock', 'a') as busy:
            fcntl.flock(busy, fcntl.LOCK_SH | fcntl.LOCK_NB)
            send('d\n')
            expect('Export plain text first?')
            send('n\n')
            expect('Type REMOVE')
            send('REMOVE\n')
            expect('profile is in use')
            expect('Documents for alice')
            assert profile_path.exists()
        send('r\n')
        expect('Document number to remove:')
        send('1\n')
        expect('Export plain text first?')
        send('n\n')
        expect('Type REMOVE')
        expect('Removed locally.')
        root = pathlib.Path(directory) / 'syncedit'
        assert not list((root / 'profiles' / 'alice'.encode().hex()).rglob('document'))
        expect('Document name:')
        expect('Ctrl-L log out')
        time.sleep(.2)
        expect('Documents for alice')
        expect('Export plain text first?')
        expect('Type REMOVE')
        expect('Removed locally.')
        assert not (root / 'profiles' / 'alice'.encode().hex()).exists()
        assert (root / 'profiles' / 'bob'.encode().hex()).exists()
        assert sorted(p.read_text() for p in (root / 'exports').rglob('document.txt')) == ['hello', 'keep this too']
        send('q\n')
        assert p.wait(timeout=10) == 0
        assert len(list((root / 'profiles').rglob('profile_name'))) == 1
        result = subprocess.run([sys.argv[1], '--as', 'alice'], env=env,
                                capture_output=True, timeout=10)
        assert result.returncode != 0
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
        os.close(master)
        os.close(slave)
print('Profile creation, document close/reopen, logout, switching and isolation passed')
