#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
# One shared scenario verifies forwarding, partitions, replay and malformed peers together.
exec python3 "$root/tests/integration/test_network.py" "${1:-$root/build/syncedit}"
