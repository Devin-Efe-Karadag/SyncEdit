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
