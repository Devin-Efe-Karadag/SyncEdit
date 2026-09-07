#!/usr/bin/env python3
"""Seeded four-process fault tests with a complete-operation-set oracle.

TCP proxies drop connections for partitions and delay/duplicate OP_BATCH frames.
They never invent operations. Successful local edits are captured from durable
logs before any crash, so convergence alone cannot hide lost acknowledged edits.
"""
import asyncio
import json
import os
from pathlib import Path
import random
import socket
import struct
import sys
import tempfile
import zlib


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def operations(path):
    if not path.exists():
        return {}
    data = path.read_bytes()
    assert data[:8] == b'CELOG002'
    offset, result = 16, {}
    while len(data) - offset >= 12:
        magic, length, crc = struct.unpack_from('!III', data, offset)
        assert magic == 0x43454c52 and zlib.crc32(data[offset:offset+8]) == crc
        if len(data) < offset + length + 16:
            break  # A concurrent append is not yet a complete durable record.
        payload = data[offset+12:offset+12+length]
        assert zlib.crc32(payload) == struct.unpack_from('!I', data, offset+12+length)[0]
        magic, version, kind, size = struct.unpack_from('!IHHI', payload)
        assert (magic, version, kind, size) == (0x43454454, 3, 3, len(payload)-12)
        count = struct.unpack_from('!H', payload, 12)[0]
        assert len(payload) == 14 + count * 42
        for i in range(count):
            op = struct.unpack_from('!BQQQQQB', payload, 14+i*42)
            key = op[1:3]
            assert key not in result or result[key] == op
            result[key] = op
        offset += length + 16
    return result


def oracle(ops):
    """Independent, deliberately simple tree traversal, no production rank index."""
    children, deleted = {}, set()
    for key, op in ops.items():
        if op[0] == 1:
            children.setdefault(op[3:5], []).append((op[5], key, op[6]))
        else:
            deleted.add(op[3:5])
    for siblings in children.values():
        siblings.sort(reverse=True)
    stack = list(reversed(children.get((0, 0), [])))
    text, visited = bytearray(), set()
    while stack:
        _, key, value = stack.pop()
        assert key not in visited
        visited.add(key)
        if key not in deleted:
            text.append(value)
        stack.extend(reversed(children.get(key, [])))
    assert len(visited) == sum(op[0] == 1 for op in ops.values())
    assert deleted <= visited
    return bytes(text)


class Link:
    def __init__(self, port, target, rng):
        self.port, self.target, self.rng = port, target, rng
        self.enabled = True
        self.writers, self.handlers = set(), set()
        self.duplicates = self.delays = 0

    async def accept(self, reader, writer):
        task = asyncio.current_task()
        self.handlers.add(task)
        pending = []
        peer = None
        try:
            if not self.enabled:
                return
            incoming, peer = await asyncio.open_connection('127.0.0.1', self.target)
            self.writers.update((writer, peer))

            async def send(target, frame, delay):
                await asyncio.sleep(delay)
                if self.enabled and not target.is_closing():
                    target.write(frame)
                    await target.drain()

            async def pipe(source, target):
                while True:
                    header = await source.readexactly(12)
                    _, _, kind, length = struct.unpack('!IHHI', header)
                    assert length <= 65536
                    frame = header + await source.readexactly(length)
                    if kind in (1, 2):
                        # Advertise proxy ports, otherwise introductions bypass the faults.
                        frame = bytearray(frame)
                        document_len = struct.unpack_from('!H', frame, 12)[0]
                        name_offset = 14 + document_len + 8
                        name_len = struct.unpack_from('!H', frame, name_offset)[0]
                        port_offset = name_offset + 2 + name_len
                        advertised = struct.unpack_from('!H', frame, port_offset)[0]
                        struct.pack_into('!H', frame, port_offset, self.routes[advertised])
                        frame = bytes(frame)
                    if kind == 3:
                        self.delays += 1
                        pending.append(asyncio.create_task(send(target, frame, self.rng.uniform(.005, .15))))
                        if self.rng.random() < .45:
                            self.duplicates += 1
                            pending.append(asyncio.create_task(send(target, frame, self.rng.uniform(.01, .2))))
                    else:
                        await send(target, frame, 0)

            pipes = [asyncio.create_task(pipe(reader, peer)), asyncio.create_task(pipe(incoming, writer))]
            _, remaining = await asyncio.wait(pipes, return_when=asyncio.FIRST_COMPLETED)
            for item in remaining:
                item.cancel()
            await asyncio.gather(*pipes, return_exceptions=True)
        except (OSError, asyncio.IncompleteReadError, ConnectionError):
            pass
        finally:
            for item in pending:
                item.cancel()
            await asyncio.gather(*pending, return_exceptions=True)
            for item in (writer, peer):
                if item:
                    self.writers.discard(item)
                    item.close()
                    try:
                        await item.wait_closed()
                    except OSError:
                        pass
            self.handlers.discard(task)

    def partition(self, enabled):
        self.enabled = enabled
        if not enabled:
            for writer in list(self.writers):
                writer.close()


async def scenario(binary, seed):
    rng = random.Random(seed)
    root = Path(tempfile.mkdtemp(prefix=f'syncedit-faults-{seed}-'))
    ports = [free_port() for _ in range(4)]
    links = [Link(free_port(), ports[(i+1) % 4], random.Random(seed*100+i)) for i in range(4)]
    for link in links:
        link.routes = {item.target: item.port for item in links}
    servers, peers, errors, trace, expected = [], [None]*4, [], [], {}

    async def wait(predicate, label, timeout=12):
        until = asyncio.get_running_loop().time()+timeout
        while asyncio.get_running_loop().time() < until:
            for p in peers:
                assert p is None or p.returncode is None, f'process exited: {p.returncode}'
            if predicate():
                return
            await asyncio.sleep(.03)
        raise AssertionError(f'{label}; seed={seed}, artifacts={root}')

    async def start(i):
        err = open(root/f'{i}.stderr', 'ab')
        errors.append(err)
        env = dict(os.environ, SYNCEDIT_TEST_HEADLESS='1',
                   SYNCEDIT_TEST_DATA_DIR=str(root/str(i)),
                   SYNCEDIT_TEST_LISTEN=f'127.0.0.1:{ports[i]}')
        peers[i] = await asyncio.create_subprocess_exec(
            binary, 'random', '--join', f'127.0.0.1:{links[i].port}',
            stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.DEVNULL, stderr=err, env=env)
        await wait(lambda: (root/str(i)/'identity').exists() and (root/str(i)/'operations.log').exists(), 'startup')

    async def command(i, text):
        peers[i].stdin.write((text+'\n').encode())
        await peers[i].stdin.drain()

    def local_counter(i):
        path=root/str(i)/'counter'
        return int(path.read_text()) if path.exists() else 0

    def capture():
        for i in range(4):
            for key, op in operations(root/str(i)/'operations.log').items():
                assert key not in expected or expected[key] == op
                expected[key] = op

    try:
        for link in links:
            servers.append(await asyncio.start_server(link.accept, '127.0.0.1', link.port))
        for i in range(4):
            await start(i)
        for turn in range(24):
            i = rng.randrange(4)
            rid = int((root/str(i)/'identity').read_text())
            before = operations(root/str(i)/'operations.log')
            counter = max((key[1] for key in before if key[0] == rid), default=0)
            token = f'{seed}:{turn};'
            await command(i, 'insert 0 '+token)
            await command(i, 'save')
            await wait(lambda: local_counter(i) >= counter+len(token)+1, 'durable local edit')
            if turn % 3 == 0:
                await command(i, 'erase 0')
                await wait(lambda: local_counter(i) >= counter+len(token)+2, 'durable delete')
            capture()
            if turn in (4, 5):
                for link in links:
                    link.partition(False)
                trace.append([turn, 'full partition'])
            else:
                link = rng.choice(links)
                link.partition(rng.random() > .35)
                trace.append([turn, 'link', links.index(link), link.enabled])
            if turn % 5 == 2:
                victim = rng.randrange(4)
                await command(victim, 'checkpoint')
                await asyncio.sleep(rng.uniform(0, .01))
                peers[victim].kill()
                await peers[victim].wait()
                peers[victim] = None
                await start(victim)
                trace.append([turn, 'restart', victim])
            await asyncio.sleep(.08)
        capture()
        for link in links:
            link.partition(True)
        def all_operations():
            return all(operations(root/str(i)/'operations.log') == expected for i in range(4))
        await wait(all_operations, 'complete operation-set convergence', 45)
        target = oracle(expected)
        for i in range(4):
            await command(i, 'save')
        await wait(lambda: all((root/str(i)/'snapshot.txt').exists() and (root/str(i)/'snapshot.txt').read_bytes() == target for i in range(4)), 'oracle text')
        assert sum(link.duplicates for link in links) > 0
        # A clean checkpoint restart must also preserve the exact operation set.
        for i in range(4):
            await command(i, 'quit')
        for p in peers:
            await asyncio.wait_for(p.wait(), 5)
            assert p.returncode == 0
        peers = [None]*4
        for i in range(4):
            await start(i)
            await command(i, 'save')
        await wait(all_operations, 'post-checkpoint operation sets')
        await wait(lambda: all((root/str(i)/'snapshot.txt').read_bytes() == target for i in range(4)), 'post-checkpoint text')
        print(f'PASS seed={seed}, {len(expected)} operations, four peers, partitions/reordering/duplicates/crashes/checkpoints; {root}', flush=True)
    finally:
        (root/'trace.json').write_text(json.dumps(trace, indent=2))
        for p in peers:
            if p and p.returncode is None:
                p.kill()
                await p.wait()
        for server in servers:
            server.close()
            await server.wait_closed()
        for link in links:
            link.partition(False)
        await asyncio.gather(*(task for link in links for task in list(link.handlers)), return_exceptions=True)
        for err in errors:
            err.close()
        for path in root.glob('*.stderr'):
            if path.stat().st_size:
                print(path.read_text(), file=sys.stderr)


async def main():
    for seed in (7, 71, 2026):
        await scenario(os.path.abspath(sys.argv[1]), seed)

if __name__ == '__main__':
    asyncio.run(main())
