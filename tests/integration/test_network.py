    wait(lambda:snap(0)==snap(1)==snap(2) and len(snap(0))==635,'one-way configured chain forwarding')
    for p in [a,b,c]:stop(p)
    print('PASS A -> B -> C forwarding without an A/C connection')
    master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,100,0,0));attrs=termios.tcgetattr(slave)
    remote=start(4,[3]);ui=start(3,[4],headless=False,stdin=slave,stdout=slave);time.sleep(.3)
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
