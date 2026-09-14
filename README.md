# SyncEdit

SyncEdit is a terminal text editor where several copies of the same document
can edit together without a central server. Every running editor is a TCP peer.
If one peer goes offline, it can keep editing and exchange the missing changes
when it reconnects.

## Build it

SyncEdit currently runs on Linux and uses ncurses for its interface.

```sh
sudo apt install build-essential cmake libncurses-dev python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
```

The executable will be `build/syncedit`.

## Try two peers on one machine

Using separate data directories makes the two processes behave like different
users. Start the first editor in one terminal:

```sh
XDG_DATA_HOME=/tmp/syncedit-alice ./build/syncedit notes
```

Create a profile when the menu asks. The `notes` document then opens and the
status line shows the listening port, normally 9000. Press `Ctrl-P` at any time
to see the full connection details.

Open a second terminal and join the first peer:

```sh
XDG_DATA_HOME=/tmp/syncedit-bob \
  ./build/syncedit notes --join 127.0.0.1:9000
```

Create a different profile for this instance. Type in either terminal and the
same text should appear in the other. Close one editor with `Ctrl-Q`, continue
typing in the remaining editor, and start the closed peer again to see it catch
up.

When peers run on different machines, replace `127.0.0.1` with the listening
machine's reachable IPv4 address and allow the selected TCP port through its
firewall. SyncEdit tries ports 9000 through 9099 until it finds one available.

## Editor keys

| Key | Action |
| --- | --- |
| Arrow keys, Page Up/Down | Move around the document |
| Enter | Insert a newline |
| Backspace or Delete | Remove text |
| `Ctrl-S` | Export the current text |
| `Ctrl-P` | Show peers and connection details |
| `Ctrl-Q` | Close the document |
| `Ctrl-L` | Log out of the current profile |

## How synchronization works

SyncEdit sends operations rather than whole files or cursor positions. Each
inserted character receives a stable ID and points to another element in an
RGA-style sequence CRDT. That gives every peer the same ordering when edits
arrive in a different order. Deletes leave tombstones, and operations that
arrive before their dependencies wait until the missing operation appears.

Peers compare version vectors to find changes the other side has not seen.
New operations are forwarded immediately, and the comparison repeats
periodically to repair missed messages. A rope stores the visible text so
normal cursor movement does not have to scan the entire CRDT.

Accepted operations are written to a checksummed append-only log before they
are shared. Checkpoints make startup faster, but the document can always be
rebuilt from its operation log. Data is stored under `$XDG_DATA_HOME/syncedit`,
or `~/.local/share/syncedit` when `XDG_DATA_HOME` is not set.

## Tests and current limits

```sh
ctest --test-dir build --output-on-failure
```

The suite includes multi-process convergence, offline reconnects, reordered
delivery, persistence recovery, and randomized network faults.

SyncEdit currently supports printable ASCII and newlines. It is intended for
small files on trusted networks: connections are not encrypted or
authenticated, and there is no undo or shared cursor display yet.
