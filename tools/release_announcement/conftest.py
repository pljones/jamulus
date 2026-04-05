"""Pytest configuration for the release_announcement test suite.

Adds the project root (tools/release_announcement/) to ``sys.path`` so that
``import tests.dummy_backend`` works when production code under
``src/release_announcement/`` imports it at runtime (e.g. during
``--backend dummy`` staging).
"""

from __future__ import annotations

import sys
from pathlib import Path

# tools/release_announcement/ — the package root that contains both src/ and tests/
_PROJECT_ROOT = Path(__file__).resolve().parent

if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))
