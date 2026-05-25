import os
import pathlib
import subprocess
import sys
import tempfile
import time

binary = os.path.abspath(sys.argv[1])
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    processes = []
