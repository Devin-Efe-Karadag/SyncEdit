# SyncEdit

SyncEdit is a small ncurses editor that can share a document directly with
another copy of itself. There is no central document server: every editor is a
TCP peer, and disconnected peers can keep typing and catch up later.

## See it work locally

Build the Linux executable:

```sh
sudo apt install build-essential cmake libncurses-dev python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
```

Two data directories let one computer pretend to be two users. In terminal A:

```sh
XDG_DATA_HOME=/tmp/syncedit-alice ./build/syncedit notes
```

Create a profile when prompted. The editor normally listens on port 9000;
`Ctrl-P` shows the actual address.

In terminal B:

```sh
XDG_DATA_HOME=/tmp/syncedit-bob \
  ./build/syncedit notes --join 127.0.0.1:9000
```

Give this copy a different profile. Text typed in either window should appear
in the other. To try the offline case, close Bob with `Ctrl-Q`, edit with
Alice, then run Bob's command again.

For two real machines, replace `127.0.0.1` with the first machine's reachable
IPv4 address and allow its listening port through the firewall. If 9000 is
occupied, SyncEdit tries the rest of the range through 9099.

## Controls

- Arrow keys and Page Up/Page Down move around.
- Enter inserts a line; Backspace and Delete remove text.
- `Ctrl-S` exports the current document.
- `Ctrl-P` opens connection details and the peer list.
- `Ctrl-Q` closes the document. `Ctrl-L` logs out.

## Why simultaneous edits do not overwrite each other

Peers exchange edit operations instead of whole file versions. An inserted
character receives a stable ID and a place in an RGA-style sequence CRDT.
Concurrent inserts can therefore arrive in different orders and still settle
into the same document. Deletes leave tombstones, and an operation waits if
the character it depends on has not arrived yet.

A version vector tells two peers which operations each is missing. Fresh edits
are forwarded immediately, and periodic comparisons repair missed messages.
The visible characters also live in a rope so routine cursor movement does not
walk the entire CRDT.

## Files on disk

An accepted operation reaches a checksummed append-only log before it is sent
to peers. Checkpoints shorten startup, but the log is enough to rebuild the
document. Profiles and documents are kept in `$XDG_DATA_HOME/syncedit`, or
`~/.local/share/syncedit` when that variable is unset.

```sh
ctest --test-dir build --output-on-failure
```

Those tests include concurrent processes, reconnects, reordered delivery,
recovery and randomized network faults.

The current editor is aimed at small files on trusted networks. It handles
printable ASCII and newlines; encryption, authentication, undo and shared
cursor display are not implemented.
