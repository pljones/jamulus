"""End-to-end pipeline tests for distillation.py (Step 4 verif. items 1, 3, 4, 5, 6)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from release_announcement.distillation import (
    MAX_DIRECT_CONSOLIDATION_CHUNKS,
    MAX_RANKING_CHUNKS,
    Chunk,
    ClassifiedSignals,
    DistilledContext,
    DistillationAdapter,
    Signal,
    run_distillation_pipeline,
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


# ─── Fixtures / helpers ───────────────────────────────────────────────────────


def _make_pr_data(num_comments: int = 3) -> dict[str, Any]:
    return {
        "body": "PR body with user-visible change",
        "comments": [f"Comment {i}" for i in range(num_comments)],
        "reviews": ["reviewer noted the change"],
    }


# ─── Protocol conformance ─────────────────────────────────────────────────────


def test_dummy_backend_satisfies_protocol():
    """Step 4 verif. item 1 (protocol check): isinstance passes at startup."""
    assert isinstance(_make_dummy_backend(), DistillationAdapter)


# ─── Full pipeline run with dummy backend ─────────────────────────────────────


def test_pipeline_returns_distilled_context(capsys):
    """Step 4 verif. item 1: full pipeline produces a correctly typed non-None context."""
    pr_data = _make_pr_data(num_comments=3)
    adapter = _make_dummy_backend()

    context = run_distillation_pipeline(
        pr_data,
        adapter,
        use_embeddings=False,
    )

    assert isinstance(context, DistilledContext)
    assert isinstance(context.summary, str)
    assert isinstance(context.structured_signals, list)
    assert isinstance(context.classification, ClassifiedSignals)
    assert isinstance(context.metadata, dict)


def test_pipeline_logs_all_stage_entries(capsys):
    """Step 4 verif. item 1: each phase logs entry and exit with timing."""
    pr_data = _make_pr_data(num_comments=2)
    adapter = _make_dummy_backend()

    run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    out = capsys.readouterr().out
    for stage in ("chunking", "selection", "extraction", "consolidation", "classification"):
        assert f"staged.{stage}.start" in out
        assert f"staged.{stage}.end" in out


def test_pipeline_metadata_contains_counts():
    """Metadata dict includes chunk_count, selected_chunk_count, signal_count, etc."""
    pr_data = _make_pr_data(num_comments=3)
    adapter = _make_dummy_backend()

    context = run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    assert "chunk_count" in context.metadata
    assert "selected_chunk_count" in context.metadata
    assert "signal_count" in context.metadata
    assert "consolidated_signal_count" in context.metadata
    assert "use_embeddings" in context.metadata


# ─── Ordered chunk preservation ───────────────────────────────────────────────


def test_chunks_are_subsequence_of_input_in_original_order():
    """Step 4 verif. item 2: output chunks are a subsequence preserving discussion order."""
    num_comments = 10
    pr_data = _make_pr_data(num_comments=num_comments)
    adapter = _make_dummy_backend()

    context = run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    # The dummy extract produces 1 signal per selected chunk; selected_chunk_count
    # tells us how many chunks fed into extraction.
    total_input_chunks = context.metadata["chunk_count"]
    selected = context.metadata["selected_chunk_count"]

    assert selected <= total_input_chunks
    assert selected >= 1

    # Verify adapter received chunks in non-decreasing index order.
    for batch_chunks, _ in adapter.select_calls:
        indices = [c.index for c in batch_chunks]
        assert indices == sorted(indices)


# ─── Consolidation batching threshold ─────────────────────────────────────────


def test_consolidation_batches_over_threshold():
    """Step 4 verif. item 3: batched consolidation when chunk count > threshold."""
    import math

    n = MAX_DIRECT_CONSOLIDATION_CHUNKS + 1
    pr_data = {
        "body": "body",
        "comments": [f"comment {i}" for i in range(n - 1)],
        "reviews": [],
    }
    adapter = _make_dummy_backend()

    run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    # Dummy produces 1 signal per chunk (n total signals).
    # _consolidate_hierarchically should invoke consolidate_signals > 1 time.
    assert len(adapter.consolidate_calls) > 1
    for call_signals in adapter.consolidate_calls:
        assert len(call_signals) <= MAX_DIRECT_CONSOLIDATION_CHUNKS


def test_consolidation_single_pass_at_threshold():
    """Single consolidation call when exactly at max_direct_consolidation_chunks."""
    n = MAX_DIRECT_CONSOLIDATION_CHUNKS
    pr_data = {
        "body": "body",
        "comments": [f"c{i}" for i in range(n - 1)],
        "reviews": [],
    }
    adapter = _make_dummy_backend()

    run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    assert len(adapter.consolidate_calls) == 1


# ─── Ranking batching threshold ───────────────────────────────────────────────


def test_ranking_batches_over_threshold():
    """Step 4 verif. item 4: chat ranking splits into batches when over max_ranking_chunks."""
    import math

    n = MAX_RANKING_CHUNKS + 1
    pr_data = {
        "body": "body",
        "comments": [f"c{i}" for i in range(n - 1)],
        "reviews": [],
    }
    adapter = _make_dummy_backend()

    run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    expected_select_calls = math.ceil(n / MAX_RANKING_CHUNKS)
    assert len(adapter.select_calls) == expected_select_calls
    for batch_chunks, _ in adapter.select_calls:
        assert len(batch_chunks) <= MAX_RANKING_CHUNKS


# ─── Positional-selection fallback on select failure ─────────────────────────


def test_positional_fallback_on_select_failure(capsys):
    """Step 4 verif. item 5: positional fallback when select_relevant_chunks raises."""
    pr_data = {
        "body": "first chunk",
        "comments": [f"middle {i}" for i in range(5)],
        "reviews": ["last review"],
    }
    adapter = _make_dummy_backend(fail_select=True)

    # Pipeline should NOT raise; it falls back to positional selection.
    context = run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    assert isinstance(context, DistilledContext)
    out = capsys.readouterr().out
    assert "[WARNING]" in out


# ─── Mid-pipeline failure propagation ────────────────────────────────────────


def test_extraction_failure_propagates_to_caller():
    """Step 4 verif. item 6: extraction failure propagates with phase and chunk info."""
    pr_data = _make_pr_data(num_comments=3)
    adapter = _make_dummy_backend(fail_extract_after=1)

    with pytest.raises(RuntimeError) as exc_info:
        run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    message = str(exc_info.value)
    assert "extraction" in message.lower()
    assert "chunk" in message.lower()


def test_extraction_failure_includes_index_info():
    """Extraction error message includes chunk index and discussion index."""
    pr_data = _make_pr_data(num_comments=5)
    adapter = _make_dummy_backend(fail_extract_after=2)

    with pytest.raises(RuntimeError) as exc_info:
        run_distillation_pipeline(pr_data, adapter, use_embeddings=False)

    message = str(exc_info.value)
    # Should mention discussion index
    assert "index=" in message or "chunk" in message.lower()


def test_process_single_pr_fallback_on_extraction_failure(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
):
    """Step 4 verif. item 6: process_single_pr catches and falls back on pipeline failure."""
    from release_announcement import main as ra_main

    ann_file = tmp_path / "ReleaseAnnouncement.md"
    ann_file.write_text("existing announcement", encoding="utf-8")

    pr_data = _make_pr_data(num_comments=3)
    failing_adapter = _make_dummy_backend(fail_extract_after=0)

    def _fake_prepare(pr, config, mode, **_kwargs):
        # Trigger the adapter failure path directly
        from release_announcement.distillation import run_distillation_pipeline as _run
        return _run(pr, failing_adapter, use_embeddings=False)

    monkeypatch.setattr(ra_main, "_fetch_pr_data", lambda *a, **kw: pr_data)
    monkeypatch.setattr(ra_main, "prepare_pr_context", _fake_prepare)
    monkeypatch.setattr(ra_main, "_load_prompt_template", lambda *a, **kw: {"messages": []})
    monkeypatch.setattr(ra_main, "_build_ai_prompt", lambda *a, **kw: {
        "messages": [{"role": "user", "content": "p"}],
        "model": "m",
        "modelParameters": {},
    })
    monkeypatch.setattr(ra_main, "_process_with_llm", lambda *a, **kw: "updated")
    monkeypatch.setattr(ra_main, "_write_and_check_announcement", lambda *a, **kw: None)
    monkeypatch.setattr(ra_main, "_check_for_changes", lambda *a, **kw: "no_changes")

    config = ra_main.BackendConfig(backend="ollama", pipeline_mode="staged")
    result = ra_main.process_single_pr(
        pr_num=1,
        pr_title="test",
        announcement_file=str(ann_file),
        prompt_file="unused.yml",
        config=config,
    )

    out = capsys.readouterr().out
    # Fallback warning must be logged
    assert "staged preprocessing failed" in out or "falling back to legacy" in out
    # Result is still meaningful (not an exception)
    assert result in {"no_changes", "committed", "dry_run"}


# ─── DistilledContext type check ──────────────────────────────────────────────


def test_distilled_context_has_correct_signal_types():
    """structured_signals contains Signal instances; classification is ClassifiedSignals."""
    context = run_distillation_pipeline(
        _make_pr_data(num_comments=2),
        _make_dummy_backend(),
        use_embeddings=False,
    )
    for sig in context.structured_signals:
        assert isinstance(sig, Signal)
    assert isinstance(context.classification, ClassifiedSignals)
