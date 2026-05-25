# SyncEdit

SyncEdit is a terminal text editor I wrote to experiment with collaborative
editing without a server. Every instance is both an ncurses editor and a TCP
peer. You can keep typing while disconnected, then exchange the missing edits
when the connection comes back.

## Trying it

Build it on Linux and open the profile/document menu:

```sh
sudo apt install build-essential cmake libncurses-dev python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/syncedit
```

To open a document directly:

```sh
./build/syncedit notes
```

Press `Ctrl-P` to see the address the editor is listening on. A second peer can
join with:

```sh
./build/syncedit notes --join peer.example.com:9000
```

The address is remembered, so it is normally only needed the first time.

## What is being synchronized

Edits do not contain character offsets. Each inserted character gets a stable
ID and points to another element in an RGA-style sequence CRDT. Deletes leave a
tombstone, concurrent inserts are sorted the same way by every peer, and an
operation that arrives before its dependency waits until that dependency turns
up. The visible characters are kept in a rope so the editor does not have to
walk the entire CRDT for normal cursor movement.

Peers compare version vectors and send batches of operations the other side is
missing. New operations are also forwarded to connected peers, and the version
check repeats periodically to repair missed updates. This is what lets several
copies converge after offline editing or a process restart.

Accepted operations go into a checksummed append-only log before they are
shared. Snapshots are plain-text exports; checkpoints make startup faster but
can be discarded and rebuilt from the log. Files live under
`$XDG_DATA_HOME/syncedit`, or `~/.local/share/syncedit` when that variable
is not set.

The editor keys are close to nano: arrows and Page Up/Down move, Enter inserts
a newline, Backspace/Delete remove text, `Ctrl-S` saves, `Ctrl-P` shows peers,
`Ctrl-Q` closes the document, and `Ctrl-L` logs out.

Run the test suite with:

```sh
ctest --test-dir build --output-on-failure
```

SyncEdit handles printable ASCII and newlines and is intended for small files
on trusted networks. There is no encryption, authentication, undo, or shared
cursor display.
