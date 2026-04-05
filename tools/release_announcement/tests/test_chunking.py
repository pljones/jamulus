"""Tests for ordered chunking and positional-selection fallback (Step 4 verif. items 2, 5)."""

from __future__ import annotations

from pathlib import Path

import pytest

from release_announcement.distillation import (
    MAINTAINER_DECISION_KEYWORDS,
    MAX_RANKING_CHUNKS,
    POSITIONAL_LAST_N,
    Chunk,
    _positional_selection,
    _select_chunks,
    ordered_chunk,
)
from tests.dummy_backend import DummyBackend


def _make_dummy_backend(
    fail_select: bool = False,
    fail_extract_after: int | None = None,
) -> DummyBackend:
    return DummyBackend(
        chat_model="dummy-chat",
        embedding_model="dummy-embeddings",
        fail_select=fail_select,
        fail_extract_after=fail_extract_after,
    )


# ─── ordered_chunk tests ──────────────────────────────────────────────────────


def _make_pr(body="PR body", comments=None, reviews=None):
    return {
        "body": body,
        "comments": comments if comments is not None else [],
        "reviews": reviews if reviews is not None else [],
    }


def test_ordered_chunk_body_first():
    chunks = ordered_chunk(_make_pr(body="intro"))
    assert len(chunks) == 1
    assert chunks[0].text == "intro"
    assert chunks[0].index == 0


def test_ordered_chunk_body_then_comments_then_reviews():
    pr = _make_pr(body="body", comments=["c1", "c2"], reviews=["r1"])
    chunks = ordered_chunk(pr)
    assert [c.text for c in chunks] == ["body", "c1", "c2", "r1"]
    assert [c.index for c in chunks] == [0, 1, 2, 3]


def test_ordered_chunk_skips_blank_entries():
    pr = _make_pr(body="", comments=["  ", "real"], reviews=[""])
    chunks = ordered_chunk(pr)
    assert len(chunks) == 1
    assert chunks[0].text == "real"


def test_ordered_chunk_dict_comments():
    pr = _make_pr(body="b", comments=[{"body": "dict comment"}])
    chunks = ordered_chunk(pr)
    assert chunks[1].text == "dict comment"


def test_ordered_chunk_preserves_index_sequence():
    pr = _make_pr(body="b", comments=[f"c{i}" for i in range(5)])
    chunks = ordered_chunk(pr)
    assert [c.index for c in chunks] == list(range(6))


# ─── positional_selection tests ───────────────────────────────────────────────


def _make_chunks(n: int) -> list[Chunk]:
    return [Chunk(text=f"chunk {i}", index=i) for i in range(n)]


def test_positional_selection_always_includes_first():
    chunks = _make_chunks(10)
    result = _positional_selection(chunks, last_n=1, maintainer_keywords=[])
    assert chunks[0] in result


def test_positional_selection_includes_last_n():
    chunks = _make_chunks(10)
    result = _positional_selection(chunks, last_n=3, maintainer_keywords=[])
    last_three = chunks[-3:]
    for c in last_three:
        assert c in result


def test_positional_selection_includes_keyword_chunks():
    chunks = [
        Chunk(text="normal text", index=0),
        Chunk(text="we agreed on this approach", index=1),
        Chunk(text="more normal text", index=2),
        Chunk(text="this is merged", index=3),
    ]
    result = _positional_selection(chunks, last_n=1, maintainer_keywords=["agreed", "merged"])
    indices = {c.index for c in result}
    assert 1 in indices
    assert 3 in indices


def test_positional_selection_preserves_original_order():
    chunks = _make_chunks(20)
    # Shuffle by using keyword to include non-contiguous items
    chunks[10] = Chunk(text="agreed on this", index=10)
    result = _positional_selection(chunks, last_n=2, maintainer_keywords=["agreed"])
    assert result == sorted(result, key=lambda c: c.index)


def test_positional_selection_empty_input():
    assert _positional_selection([], last_n=3, maintainer_keywords=[]) == []


# ─── _select_chunks ordered-subsequence tests ─────────────────────────────────


def test_select_chunks_output_is_subsequence_of_input():
    """Step 4 verif. item 2: output is a subsequence preserving original order."""
    n = 15
    chunks = _make_chunks(n)
    adapter = _make_dummy_backend()

    result = _select_chunks(
        chunks,
        adapter,
        use_embeddings=False,
        max_ranking_chunks=MAX_RANKING_CHUNKS,
        positional_last_n=POSITIONAL_LAST_N,
        maintainer_keywords=MAINTAINER_DECISION_KEYWORDS,
    )

    # Result must be in original discussion order (indices non-decreasing).
    assert result == sorted(result, key=lambda c: c.index)
    # All returned indices must come from the original set.
    original_indices = {c.index for c in chunks}
    for c in result:
        assert c.index in original_indices


def test_select_chunks_batches_when_over_threshold():
    """Step 4 verif. item 4: batches when len > max_ranking_chunks."""
    n = MAX_RANKING_CHUNKS + 1
    chunks = _make_chunks(n)
    adapter = _make_dummy_backend()

    result = _select_chunks(
        chunks,
        adapter,
        use_embeddings=False,
        max_ranking_chunks=MAX_RANKING_CHUNKS,
        positional_last_n=POSITIONAL_LAST_N,
        maintainer_keywords=MAINTAINER_DECISION_KEYWORDS,
    )

    # Adapter should have been called once per batch.
    expected_batches = -(-n // MAX_RANKING_CHUNKS)  # ceiling division
    assert len(adapter.select_calls) == expected_batches

    # Each batch should be ≤ max_ranking_chunks.
    for batch_chunks, _ in adapter.select_calls:
        assert len(batch_chunks) <= MAX_RANKING_CHUNKS

    # Output preserves original discussion order.
    assert result == sorted(result, key=lambda c: c.index)


def test_select_chunks_single_call_when_at_threshold():
    """No batching when exactly at max_ranking_chunks."""
    chunks = _make_chunks(MAX_RANKING_CHUNKS)
    adapter = _make_dummy_backend()

    _select_chunks(
        chunks,
        adapter,
        use_embeddings=False,
        max_ranking_chunks=MAX_RANKING_CHUNKS,
        positional_last_n=POSITIONAL_LAST_N,
        maintainer_keywords=MAINTAINER_DECISION_KEYWORDS,
    )
    assert len(adapter.select_calls) == 1


def test_select_chunks_positional_fallback_on_select_failure():
    """Step 4 verif. item 5: positional fallback when select_relevant_chunks raises."""
    chunks = _make_chunks(5)
    # Give one keyword chunk so it's retained by positional fallback.
    chunks[2] = Chunk(text="we agreed on this fix", index=2)
    adapter = _make_dummy_backend(fail_select=True)

    result = _select_chunks(
        chunks,
        adapter,
        use_embeddings=False,
        max_ranking_chunks=MAX_RANKING_CHUNKS,
        positional_last_n=POSITIONAL_LAST_N,
        maintainer_keywords=MAINTAINER_DECISION_KEYWORDS,
    )

    # Positional fallback must retain first, last POSITIONAL_LAST_N, and keyword chunks.
    result_indices = {c.index for c in result}
    assert 0 in result_indices          # first
    assert 4 in result_indices          # last (n-1)
    assert 2 in result_indices          # keyword "agreed"
    # Must be in original order.
    assert result == sorted(result, key=lambda c: c.index)


def test_select_chunks_positional_fallback_logs_warning(capsys):
    """Positional fallback emits a warning log when selection fails."""
    chunks = _make_chunks(3)
    adapter = _make_dummy_backend(fail_select=True)

    _select_chunks(
        chunks,
        adapter,
        use_embeddings=False,
        max_ranking_chunks=MAX_RANKING_CHUNKS,
        positional_last_n=POSITIONAL_LAST_N,
        maintainer_keywords=MAINTAINER_DECISION_KEYWORDS,
    )

    out = capsys.readouterr().out
    assert "[WARNING]" in out
    assert "positional" in out.lower()
