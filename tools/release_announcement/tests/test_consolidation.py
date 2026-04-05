"""Tests for hierarchical consolidation logic (Step 4 verif. item 3)."""

from __future__ import annotations

from pathlib import Path

import pytest

from release_announcement.distillation import (
    MAX_DIRECT_CONSOLIDATION_CHUNKS,
    Signal,
    _consolidate_hierarchically,
    _run_consolidation_batches,
)
from tests.dummy_backend import DummyBackend


def _make_dummy_backend() -> DummyBackend:
    return DummyBackend(chat_model="dummy-chat", embedding_model="dummy-embeddings")


# ─── Helpers ──────────────────────────────────────────────────────────────────


def _make_signals(n: int) -> list[Signal]:
    return [
        Signal(
            change=f"Change {i}",
            impact="minor",
            users_affected="all users",
            confidence=0.9,
            final_outcome=True,
        )
        for i in range(n)
    ]


# ─── Single-pass tests ────────────────────────────────────────────────────────


def test_single_pass_when_at_threshold():
    """Step 4 verif. item 3: single consolidation call when count == max_batch."""
    signals = _make_signals(MAX_DIRECT_CONSOLIDATION_CHUNKS)
    adapter = _make_dummy_backend()

    result = _consolidate_hierarchically(
        signals, "prompt", adapter, MAX_DIRECT_CONSOLIDATION_CHUNKS
    )

    assert len(adapter.consolidate_calls) == 1
    assert len(result) == len(signals)


def test_single_pass_when_below_threshold():
    """Single consolidation call when count < max_batch."""
    signals = _make_signals(MAX_DIRECT_CONSOLIDATION_CHUNKS - 1)
    adapter = _make_dummy_backend()

    _consolidate_hierarchically(signals, "prompt", adapter, MAX_DIRECT_CONSOLIDATION_CHUNKS)

    assert len(adapter.consolidate_calls) == 1


# ─── Batched-pass tests ───────────────────────────────────────────────────────


def test_batched_when_over_threshold():
    """Step 4 verif. item 3: two-level consolidation when count exceeds max_batch."""
    n = MAX_DIRECT_CONSOLIDATION_CHUNKS + 1
    signals = _make_signals(n)
    adapter = _make_dummy_backend()

    _consolidate_hierarchically(signals, "prompt", adapter, MAX_DIRECT_CONSOLIDATION_CHUNKS)

    # Expect at least 3 calls: 2 first-pass batches + at least 1 second pass.
    assert len(adapter.consolidate_calls) >= 3


def test_first_pass_batch_sizes_within_limit():
    """Each first-pass batch must be ≤ max_batch."""
    n = MAX_DIRECT_CONSOLIDATION_CHUNKS * 2 + 1
    signals = _make_signals(n)
    adapter = _make_dummy_backend()

    _consolidate_hierarchically(signals, "prompt", adapter, MAX_DIRECT_CONSOLIDATION_CHUNKS)

    for call_signals in adapter.consolidate_calls:
        assert len(call_signals) <= MAX_DIRECT_CONSOLIDATION_CHUNKS


def test_second_pass_occurs_after_first_pass():
    """Call pattern: first pass batches → second pass batch(es)."""
    n = MAX_DIRECT_CONSOLIDATION_CHUNKS + 1
    signals = _make_signals(n)
    adapter = _make_dummy_backend()

    _consolidate_hierarchically(signals, "prompt", adapter, MAX_DIRECT_CONSOLIDATION_CHUNKS)

    # First pass: ceil(n / max_batch) calls; each <= max_batch size.
    import math
    first_pass_count = math.ceil(n / MAX_DIRECT_CONSOLIDATION_CHUNKS)
    # With n = 21, max_batch = 20: first_pass_count = 2.
    # The dummy returns input unchanged so count doesn't reduce;
    # we expect exactly 2 more second-pass calls (same batching of 21 = 20+1).
    total_first_plus_second = first_pass_count * 2  # two equal levels of batching
    assert len(adapter.consolidate_calls) == total_first_plus_second


def test_run_consolidation_batches_single_call_within_limit():
    """_run_consolidation_batches issues a single call when input fits in one batch."""
    signals = _make_signals(5)
    adapter = _make_dummy_backend()

    _run_consolidation_batches(signals, "prompt", adapter, max_batch=10)
    assert len(adapter.consolidate_calls) == 1


def test_run_consolidation_batches_multiple_calls_over_limit():
    """_run_consolidation_batches issues ceil(n/batch) calls when over limit."""
    import math
    n = 7
    batch = 3
    signals = _make_signals(n)
    adapter = _make_dummy_backend()

    _run_consolidation_batches(signals, "prompt", adapter, max_batch=batch)
    assert len(adapter.consolidate_calls) == math.ceil(n / batch)


def test_consolidation_prompt_forwarded_to_adapter():
    """The consolidation prompt string is passed unchanged to the adapter."""
    signals = _make_signals(2)
    received_prompts: list[str] = []

    class CapturingAdapter:
        def consolidate_signals(self, sigs, prompt):
            received_prompts.append(prompt)
            return sigs

    _consolidate_hierarchically(signals, "my-special-prompt", CapturingAdapter(), max_batch=10)
    assert all(p == "my-special-prompt" for p in received_prompts)
