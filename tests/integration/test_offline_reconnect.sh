#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
# One shared scenario verifies forwarding, partitions, replay and malformed peers together.
