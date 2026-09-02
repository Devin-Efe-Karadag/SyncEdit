#!/usr/bin/env python3
"""Real processes, TCP fault injection, crash recovery, and ncurses PTY smoke test."""
import os, sys, tempfile, subprocess, socket, time, struct, pathlib, signal, pty, fcntl, termios
BIN=os.path.abspath(sys.argv[1]); root=pathlib.Path(tempfile.mkdtemp(prefix='syncedit-integration-'))
processes=[]
def port():
    with socket.socket() as s: s.bind(('127.0.0.1',0)); return s.getsockname()[1]
ports=[port() for _ in range(6)]
def start(i, peers=(), document='notes', headless=True, stdin=None, stdout=None):
    args=[BIN, document]
    for j in peers: args += ['--join',f'127.0.0.1:{ports[j]}']
    err=open(root/f'{i}.stderr','ab')
    env={**os.environ,'TERM':'xterm-256color','SYNCEDIT_TEST_DATA_DIR':str(root/str(i)),
         'SYNCEDIT_TEST_LISTEN':f'127.0.0.1:{ports[i]}'}
    if headless: env['SYNCEDIT_TEST_HEADLESS']='1'
    p=subprocess.Popen(args,stdin=subprocess.PIPE if stdin is None else stdin,stdout=subprocess.DEVNULL if stdout is None else stdout,stderr=err,env=env)
    processes.append(p); return p

def command(p,s): p.stdin.write((s+'\n').encode()); p.stdin.flush()
def snap(i):
    p=root/str(i)/'snapshot.txt'; return p.read_bytes() if p.exists() else None

def wait(predicate, label, timeout=15):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if predicate(): return
        time.sleep(.05)
    raise AssertionError(label+' timed out; snapshots='+repr([snap(i) for i in range(3)]))
def stop(p,kill=False):
    if p.poll() is None:
        p.kill() if kill else p.terminate()
        p.wait(5)
    if not kill: assert p.returncode==0, p.returncode

def frame(t,payload=b''): return struct.pack('!IHHI',0x43454454,3,t,len(payload))+payload
def hello(doc='notes',replica=123):
    b=doc.encode(); name=b'injector'
    return struct.pack('!H',len(b))+b+struct.pack('!QH',replica,len(name))+name+struct.pack('!HH',9000,0)
def op(rep,cnt,parent_rep,parent_cnt,stamp,ch,insert=True):
    return struct.pack('!BQQQQQB',1 if insert else 2,rep,cnt,parent_rep,parent_cnt,stamp,ord(ch) if ch else 0)
def inject(ops):
    with socket.create_connection(('127.0.0.1',ports[0])) as s:
        s.sendall(frame(1,hello()));time.sleep(.1)
        payload=frame(3,struct.pack('!H',len(ops))+b''.join(ops))
        # Fragment across headers and operations, exercise partial reads.
        for k in range(0,len(payload),7): s.sendall(payload[k:k+7])
        time.sleep(.3)
try:
    a=start(0,[1,2]);b=start(1,[0,2]);c=start(2,[0,1])
    for p,text in [(a,'ALPHA'),(b,'BRAVO'),(c,'CHARLIE')]:command(p,'insert 0 '+text)
    wait(lambda:snap(0) is not None and snap(0)==snap(1)==snap(2) and len(snap(0))==17,'three concurrent peers')
    original=snap(0); print('PASS three-peer concurrent convergence')
    stop(c,True)
    old=ports[2];ports[2]=port();c=start(2)
    command(c,'insert 0 OFFLINE');command(a,'insert 0 ONLINE');command(b,'erase 0')
    wait(lambda:snap(2) is not None and b'OFFLINE' in snap(2),'offline edits')
    stop(c,True);ports[2]=old;c=start(2,[0,1])
    wait(lambda:snap(0)==snap(1)==snap(2) and snap(0) and b'OFFLINE' in snap(0) and len(snap(0))==29,'offline reconnect')
    print('PASS offline editing, reconnect, and SIGKILL log recovery')
    # Delete-before-insert and child-before-parent, then duplicate entire batch.
    operations=[op(123,3,123,1,3,'',False),op(123,2,123,1,2,'Y'),op(123,1,0,0,1,'X')]
    inject(operations)
    wait(lambda:snap(0)==snap(1)==snap(2) and snap(0) and b'Y' in snap(0) and len(snap(0))==30,'reordered dependency forwarding')
    before=snap(0);inject(operations);time.sleep(1.3);assert snap(0)==snap(1)==snap(2)==before
    print('PASS reordered dependencies and duplicate forwarding across three processes')
    with socket.create_connection(('127.0.0.1',ports[0])) as s:
        s.settimeout(3);s.sendall(frame(1,hello('wrong-document',456)));received=b''
        while True:
            data=s.recv(4096)
            if not data:break
            received+=data
        assert frame(8,b'invalid peer message') in received
    print('PASS mismatched document rejected with ERROR')
    for bad in [struct.pack('!IHHI',0,3,6,0),struct.pack('!IHHI',0x43454454,99,6,0),struct.pack('!IHHI',0x43454454,3,3,65537)]:
        with socket.create_connection(('127.0.0.1',ports[0])) as s:
            s.settimeout(3);s.sendall(bad)
            while s.recv(4096):pass
        assert a.poll() is None
    print('PASS live malformed magic, version and oversized frame rejection')
    stop(b,True);b=start(1,[0,2]);wait(lambda:snap(1)==before,'restart replay')
    stop(c)
    command(a,'insert 0 '+('q'*600))
    wait(lambda:snap(0)==snap(1) and len(snap(0))==630,'edits while C is absent')
    c=start(2,[0,1])
    wait(lambda:snap(0)==snap(1)==snap(2) and len(snap(0))==630,'bounded multi-batch transfer')
    print('PASS transfer exceeding one operation batch')
    for p in [a,b,c]:stop(p)
    a=start(0,[1]);b=start(1,[2]);c=start(2)
    command(a,'insert 0 CHAIN')
    wait(lambda:snap(0)==snap(1)==snap(2) and len(snap(0))==635,'one-way configured chain forwarding')
    for p in [a,b,c]:stop(p)
    print('PASS A -> B -> C forwarding without an A/C connection')
    master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,100,0,0));attrs=termios.tcgetattr(slave)
    remote=start(4,[3]);ui=start(3,[4],headless=False,stdin=slave,stdout=slave);time.sleep(.3)
    os.write(master,b'abc\x1bODX\x7f\x1b[3~\rsecond\x13');time.sleep(.4)
    # abc -> cursor before c -> abXc -> abc -> ab -> ab newline second
    wait(lambda:snap(3)==b'ab\nsecond','ncurses keyboard actions')
    wait(lambda:snap(4)==b'ab\nsecond','remote initial sync')
    command(remote,'insert 0 PRE')
    wait(lambda:snap(3)==snap(4)==b'PREab\nsecond','remote edit before cursor')
    os.write(master,b'!\x13')
    wait(lambda:snap(3)==b'PREab\nsecond!','stable cursor after remote prefix')
    wait(lambda:snap(4)==b'PREab\nsecond!','remote receives anchored edit')
    command(remote,'erase '+str(len(b'PREab\nsecond!')-1))
    wait(lambda:snap(3)==snap(4)==b'PREab\nsecond','remote deletes cursor anchor')
    command(remote,'insert '+str(len(b'PREab\nsecond'))+' R')
    wait(lambda:snap(3)==snap(4)==b'PREab\nsecondR','insert before tombstoned anchor')
    os.write(master,b'Z\x13')
    wait(lambda:snap(3)==b'PREab\nsecondRZ','cursor retains tombstone anchor across redraws')
    os.write(master,b'\x10hidden typing\r\x7f\x13');time.sleep(.2)
    assert snap(3)==b'PREab\nsecondRZ', 'peer panel must not edit hidden text'
    os.write(master,b'\x10')
    os.write(master,b'\x10\x10\x1bOA\x1bOB\x1b[5~\x1b[6~');fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',30,110,0,0));ui.send_signal(signal.SIGWINCH);time.sleep(.2)
    os.write(master,b'\x11');ui.wait(5);assert ui.returncode==0;assert termios.tcgetattr(slave)==attrs
    # Capture actual ncurses rendering for a reproducible terminal transcript.
    os.set_blocking(master,False);capture=b''
    try:
        while True:capture+=os.read(master,65536)
    except BlockingIOError:pass
    (root/'terminal.ansi').write_bytes(capture)
    assert b'syncedit' in capture and b'Ctrl-S' in capture
    stop(remote)
    ui=start(3,headless=False,stdin=slave,stdout=slave);time.sleep(.3);stop(ui);assert termios.tcgetattr(slave)==attrs
    os.close(master);os.close(slave)
    print('PASS real ncurses PTY editing, resize, clean quit, SIGTERM terminal restoration')
    print('Artifacts:',root)
finally:
    for p in processes:
        if p.poll() is None:p.kill();p.wait()
    for path in root.glob('*.stderr'):
        data=path.read_text()
        if data: print(path.name,data,file=sys.stderr)
