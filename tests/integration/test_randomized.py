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
            else:
                link.partition(rng.random() > .35)
            if turn % 5 == 2:
                await command(victim, 'checkpoint')
                peers[victim].kill()
                peers[victim] = None
                trace.append([turn, 'restart', victim])
        capture()
            link.partition(True)
            return all(operations(root/str(i)/'operations.log') == expected for i in range(4))
        target = oracle(expected)
            await command(i, 'save')
        assert sum(link.duplicates for link in links) > 0
        for i in range(4):
        for p in peers:
            assert p.returncode == 0
        for i in range(4):
            await command(i, 'save')
        await wait(lambda: all((root/str(i)/'snapshot.txt').read_bytes() == target for i in range(4)), 'post-checkpoint text')
    finally:
        for p in peers:
                p.kill()
        for server in servers:
            await server.wait_closed()
            link.partition(False)
        for err in errors:
        for path in root.glob('*.stderr'):
                print(path.read_text(), file=sys.stderr)
    for seed in (7, 71, 2026):
if __name__ == '__main__':
