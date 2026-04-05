"""Shared staged-distillation pipeline for release announcement generation.

This module owns ordered chunking, embeddings-aware relevance selection, phase
sequencing, retry/fallback policy, schema validation, hierarchical
consolidation for large chunk sets, and final distilled-summary assembly.

Backend-specific adapters supply only the five provider calls defined in the
``DistillationAdapter`` protocol; all sequencing and fallback logic lives here.
"""

from __future__ import annotations

import json
import re
import time
from dataclasses import dataclass, field
from typing import Any, Protocol, runtime_checkable

import yaml  # type: ignore[import]
from pydantic import BaseModel, ValidationError  # type: ignore[import]

# ─── Configuration constants ──────────────────────────────────────────────────

MAX_DIRECT_CONSOLIDATION_CHUNKS: int = 20
"""Maximum number of signals to pass to a single consolidation call."""

MAX_RANKING_CHUNKS: int = 30
"""Maximum number of chunks to rank in a single chat-ranking call."""

POSITIONAL_LAST_N: int = 3
"""Number of trailing chunks always included in positional-selection fallback."""

MAINTAINER_DECISION_KEYWORDS: list[str] = [
    "merged",
    "agreed",
    "closing",
    "decided",
    "approved",
    "lgtm",
    "going with",
    "will go",
    "as discussed",
    "settled on",
    "resolves",
    "addressed",
]

DEFAULT_EXTRACTION_PROMPT: str = ".github/prompts/extraction.prompt.yml"
DEFAULT_CONSOLIDATION_PROMPT: str = ".github/prompts/consolidation.prompt.yml"
DEFAULT_CLASSIFICATION_PROMPT: str = ".github/prompts/classification.prompt.yml"

# ─── Data models ──────────────────────────────────────────────────────────────


@dataclass
class Chunk:
    """One ordered fragment of a PR discussion."""

    text: str
    index: int
    relevance_score: float = 0.0
    metadata: dict = field(default_factory=dict)


class Signal(BaseModel):
    """One user-visible change extracted from a PR discussion chunk."""

    change: str
    impact: str
    users_affected: str
    confidence: float
    final_outcome: bool


class ClassifiedSignals(BaseModel):
    """Classification result grouping signals into release-note categories."""

    internal: list[Signal] = []
    minor: list[Signal] = []
    targeted: list[Signal] = []
    major: list[Signal] = []
    no_user_facing_changes: bool = False


@dataclass
class DistilledContext:
    """Structured staged-preprocessing output passed to prompt builders."""

    summary: str
    structured_signals: list[Signal]
    classification: ClassifiedSignals
    metadata: dict[str, Any]


# ─── Shared parsing helpers ────────────────────────────────────────────────────


def _extract_json_content(content: str) -> str:
    """Strip markdown code fences from LLM text when present."""
    stripped = content.strip()
    match = re.search(r"```(?:json)?\s*([\s\S]*?)```", stripped)
    return match.group(1).strip() if match else stripped


def _parse_signal_list(content: str) -> list[Signal]:
    """Parse an LLM text response into a list of Signal objects.

    Callers pass this the content string extracted from a provider response
    envelope; the function handles markdown fences, top-level arrays, and
    ``{"signals": [...]}`` wrappers.

    Raises:
        json.JSONDecodeError: if the content is not valid JSON.
        pydantic.ValidationError: if any item does not match the Signal schema.
        ValueError: if the JSON does not contain a recognisable list structure.
    """
    raw = _extract_json_content(content)
    data = json.loads(raw)

    if isinstance(data, list):
        return [Signal.model_validate(item) for item in data]

    if isinstance(data, dict):
        for key in ("signals", "items", "changes", "results"):
            if key in data and isinstance(data[key], list):
                return [Signal.model_validate(item) for item in data[key]]

    raise ValueError(
        f"Expected a JSON array of signal objects; got {type(data).__name__}: {raw[:200]}"
    )


def _parse_classified_signals(content: str) -> ClassifiedSignals:
    """Parse an LLM text response into a ClassifiedSignals object.

    Raises:
        json.JSONDecodeError: if the content is not valid JSON.
        pydantic.ValidationError: if the data does not match ClassifiedSignals.
    """
    raw = _extract_json_content(content)
    data = json.loads(raw)
    return ClassifiedSignals.model_validate(data)


# ─── DistillationAdapter protocol ─────────────────────────────────────────────


@runtime_checkable
class DistillationAdapter(Protocol):
    """Abstract interface that all backend-specific adapters must implement.

    The shared pipeline calls these five methods; all sequencing, batching, and
    fallback logic lives in this module.  No adapter contains prompt text.
    """

    def select_relevant_chunks(
        self, chunks: list[Chunk], use_embeddings: bool
    ) -> list[Chunk]:
        """Rank and return chunks (subset or all) with relevance metadata.

        When ``use_embeddings`` is True the adapter uses its embeddings API.
        When False it performs a single chat ranking call for the supplied
        batch; the caller is responsible for splitting large inputs into
        batches before calling this method.
        """
        ...

    def extract_chunk_signals(
        self, chunk: Chunk, extraction_prompt: str
    ) -> list[Signal]:
        """Send the chunk text to the provider and return extracted signals.

        The adapter sends ``[system: extraction_prompt, user: chunk.text]``,
        unwraps the provider response envelope, and passes the content string
        to ``_parse_signal_list``.
        """
        ...

    def consolidate_signals(
        self, signals: list[Signal], consolidation_prompt: str
    ) -> list[Signal]:
        """Merge and deduplicate a list of signals.

        The adapter sends ``[system: consolidation_prompt, user: json(signals)]``,
        unwraps the response, and passes the content to ``_parse_signal_list``.
        """
        ...

    def classify_signals(
        self, signals: list[Signal], classification_prompt: str
    ) -> ClassifiedSignals:
        """Assign each signal to a release-note category.

        The adapter sends ``[system: classification_prompt, user: json(signals)]``,
        unwraps the response, and passes the content to
        ``_parse_classified_signals``.  An empty result is valid.
        """
        ...

    def render_final_context(
        self, classified: ClassifiedSignals, metadata: dict[str, Any]
    ) -> DistilledContext:
        """Assemble and return a DistilledContext from the classification result.

        No provider call is required.
        """
        ...


# ─── Chunking ─────────────────────────────────────────────────────────────────


def ordered_chunk(pr_data: dict[str, Any]) -> list[Chunk]:
    """Produce an ordered list of Chunk objects from sanitised PR data.

    Order: PR body first, then comments (timeline + inline review), then
    top-level review submission bodies.  This mirrors the original discussion
    thread order produced by ``_fetch_pr_data``.
    """
    texts: list[str] = []

    body = pr_data.get("body")
    if body and isinstance(body, str) and body.strip():
        texts.append(body.strip())

    for item in pr_data.get("comments", []):
        if isinstance(item, str):
            text = item.strip()
        elif isinstance(item, dict):
            text = (item.get("body") or item.get("text") or "").strip()
        else:
            continue
        if text:
            texts.append(text)

    for item in pr_data.get("reviews", []):
        if isinstance(item, str):
            text = item.strip()
        elif isinstance(item, dict):
            text = (item.get("body") or item.get("text") or "").strip()
        else:
            continue
        if text:
            texts.append(text)

    return [Chunk(text=t, index=i) for i, t in enumerate(texts)]


# ─── Relevance selection ───────────────────────────────────────────────────────


def _positional_selection(
    chunks: list[Chunk],
    last_n: int,
    maintainer_keywords: list[str],
) -> list[Chunk]:
    """Positional fallback: first chunk, last N chunks, and keyword chunks."""
    if not chunks:
        return []

    n = len(chunks)
    selected: set[int] = {0}
    selected.update(range(max(0, n - last_n), n))

    kw = [k.lower() for k in maintainer_keywords]
    for i, chunk in enumerate(chunks):
        if any(k in chunk.text.lower() for k in kw):
            selected.add(i)

    return [chunks[i] for i in sorted(selected)]


def _select_chunks(
    chunks: list[Chunk],
    adapter: DistillationAdapter,
    use_embeddings: bool,
    max_ranking_chunks: int,
    positional_last_n: int,
    maintainer_keywords: list[str],
) -> list[Chunk]:
    """Select relevant chunks via the adapter, with batching and fallback."""
    if not chunks:
        return []

    if use_embeddings:
        try:
            selected = adapter.select_relevant_chunks(chunks, use_embeddings=True)
            return sorted(selected, key=lambda c: c.index)
        except Exception as err:
            print(
                f"   [WARNING] staged.selection embeddings failed ({err}); "
                "falling back to positional selection"
            )
            return _positional_selection(chunks, positional_last_n, maintainer_keywords)

    # Chat-only ranking with batching
    if len(chunks) <= max_ranking_chunks:
        try:
            selected = adapter.select_relevant_chunks(chunks, use_embeddings=False)
            return sorted(selected, key=lambda c: c.index)
        except Exception as err:
            print(
                f"   [WARNING] staged.selection chat ranking failed ({err}); "
                "falling back to positional selection"
            )
            return _positional_selection(chunks, positional_last_n, maintainer_keywords)

    # Batch ranking: split into batches, rank each, merge preserving order
    batches = [
        chunks[i : i + max_ranking_chunks] for i in range(0, len(chunks), max_ranking_chunks)
    ]
    top_chunks: list[Chunk] = []
    try:
        for batch in batches:
            batch_result = adapter.select_relevant_chunks(batch, use_embeddings=False)
            top_chunks.extend(batch_result)
        return sorted(top_chunks, key=lambda c: c.index)
    except Exception as err:
        print(
            f"   [WARNING] staged.selection batch ranking failed ({err}); "
            "falling back to positional selection"
        )
        return _positional_selection(chunks, positional_last_n, maintainer_keywords)


# ─── Consolidation ────────────────────────────────────────────────────────────


def _run_consolidation_batches(
    signals: list[Signal],
    consolidation_prompt: str,
    adapter: DistillationAdapter,
    max_batch: int,
) -> list[Signal]:
    """Run one level of batched consolidation and return the combined result."""
    if len(signals) <= max_batch:
        return adapter.consolidate_signals(signals, consolidation_prompt)

    results: list[Signal] = []
    for start in range(0, len(signals), max_batch):
        batch = signals[start : start + max_batch]
        results.extend(adapter.consolidate_signals(batch, consolidation_prompt))
    return results


def _consolidate_hierarchically(
    signals: list[Signal],
    consolidation_prompt: str,
    adapter: DistillationAdapter,
    max_batch: int,
) -> list[Signal]:
    """Merge signals with hierarchical batching when input exceeds max_batch.

    When the number of signals exceeds ``max_batch``, the function runs a first
    pass of batched consolidation calls followed by a second consolidation pass
    over the batch results.  If the adapter reduces the count, the function
    recurses until it fits in a single call.  When the adapter does not reduce
    the count (e.g. a test identity adapter) the function performs exactly two
    levels of batching to avoid infinite recursion.
    """
    if len(signals) <= max_batch:
        return adapter.consolidate_signals(signals, consolidation_prompt)

    # First pass: one consolidation call per batch
    first_pass = _run_consolidation_batches(signals, consolidation_prompt, adapter, max_batch)

    if len(first_pass) < len(signals):
        # Adapter reduced the count; recurse until within threshold
        return _consolidate_hierarchically(first_pass, consolidation_prompt, adapter, max_batch)

    # Count not reduced (identity adapter): do exactly one more batched pass
    return _run_consolidation_batches(first_pass, consolidation_prompt, adapter, max_batch)


# ─── Prompt loading ────────────────────────────────────────────────────────────


def _load_stage_prompt(prompt_path: str) -> str:
    """Load a stage prompt YAML and return the system message content string."""
    try:
        with open(prompt_path, encoding="utf-8") as fh:
            data = yaml.safe_load(fh) or {}
    except FileNotFoundError:
        print(f"   [WARNING] Stage prompt file '{prompt_path}' not found; using empty prompt.")
        return ""
    except yaml.YAMLError as err:
        print(f"   [WARNING] Failed to parse stage prompt '{prompt_path}': {err}; using empty.")
        return ""

    for msg in data.get("messages", []):
        if isinstance(msg, dict) and msg.get("role") == "system":
            content = msg.get("content", "")
            return content.strip() if isinstance(content, str) else ""
    return ""


# ─── Main pipeline orchestrator ───────────────────────────────────────────────


def run_distillation_pipeline(
    pr_data: dict[str, Any],
    adapter: DistillationAdapter,
    use_embeddings: bool,
    stage_prompt_paths: dict[str, str] | None = None,
    max_direct_consolidation_chunks: int = MAX_DIRECT_CONSOLIDATION_CHUNKS,
    max_ranking_chunks: int = MAX_RANKING_CHUNKS,
    positional_last_n: int = POSITIONAL_LAST_N,
    maintainer_decision_keywords: list[str] | None = None,
) -> DistilledContext:
    """Run the full staged distillation pipeline and return a DistilledContext.

    Stages:
    1. Ordered chunking of PR discussion.
    2. Relevance selection (embeddings-assisted or chat-ranking, with fallback).
    3. Per-chunk signal extraction.
    4. Hierarchical consolidation.
    5. Classification.
    6. Final context assembly.

    Args:
        pr_data: Sanitised PR data dict from ``_fetch_pr_data``.
        adapter: Backend adapter implementing ``DistillationAdapter``.
        use_embeddings: Whether to use the embeddings path for selection.
        stage_prompt_paths: Optional overrides for the three stage prompt paths.
        max_direct_consolidation_chunks: Batch threshold for consolidation.
        max_ranking_chunks: Batch threshold for chat-ranking.
        positional_last_n: Number of trailing chunks kept in positional fallback.
        maintainer_decision_keywords: Additional keywords for positional fallback.

    Raises:
        AssertionError: if ``adapter`` does not implement ``DistillationAdapter``.
    """
    assert isinstance(adapter, DistillationAdapter), (
        f"adapter must implement DistillationAdapter; got {type(adapter)}"
    )

    kw = list((maintainer_decision_keywords or []) + MAINTAINER_DECISION_KEYWORDS)
    paths = stage_prompt_paths or {}
    extraction_prompt = _load_stage_prompt(
        paths.get("extraction", DEFAULT_EXTRACTION_PROMPT)
    )
    consolidation_prompt = _load_stage_prompt(
        paths.get("consolidation", DEFAULT_CONSOLIDATION_PROMPT)
    )
    classification_prompt = _load_stage_prompt(
        paths.get("classification", DEFAULT_CLASSIFICATION_PROMPT)
    )

    # Stage 1 — Ordered chunking
    t0 = time.perf_counter()
    print("   [INFO] staged.chunking.start")
    chunks = ordered_chunk(pr_data)
    elapsed = (time.perf_counter() - t0) * 1000
    print(f"   [INFO] staged.chunking.end chunks={len(chunks)} elapsed_ms={elapsed:.2f}")

    # Stage 2 — Relevance selection
    t0 = time.perf_counter()
    print(
        f"   [INFO] staged.selection.start "
        f"chunks={len(chunks)} use_embeddings={use_embeddings}"
    )
    selected_chunks = _select_chunks(
        chunks,
        adapter,
        use_embeddings=use_embeddings,
        max_ranking_chunks=max_ranking_chunks,
        positional_last_n=positional_last_n,
        maintainer_keywords=kw,
    )
    elapsed = (time.perf_counter() - t0) * 1000
    print(
        f"   [INFO] staged.selection.end "
        f"selected={len(selected_chunks)} elapsed_ms={elapsed:.2f}"
    )

    # Stage 3 — Per-chunk structured extraction
    t0 = time.perf_counter()
    print(f"   [INFO] staged.extraction.start chunks={len(selected_chunks)}")
    all_signals: list[Signal] = []
    for i, chunk in enumerate(selected_chunks):
        try:
            signals = adapter.extract_chunk_signals(chunk, extraction_prompt)
            all_signals.extend(signals)
        except Exception as err:
            raise RuntimeError(
                f"staged.extraction failed at chunk {i} (discussion index={chunk.index}): {err}"
            ) from err
    elapsed = (time.perf_counter() - t0) * 1000
    print(
        f"   [INFO] staged.extraction.end "
        f"signals={len(all_signals)} elapsed_ms={elapsed:.2f}"
    )

    # Stage 4 — Hierarchical consolidation
    t0 = time.perf_counter()
    print(f"   [INFO] staged.consolidation.start signals={len(all_signals)}")
    consolidated = _consolidate_hierarchically(
        all_signals,
        consolidation_prompt,
        adapter,
        max_batch=max_direct_consolidation_chunks,
    )
    elapsed = (time.perf_counter() - t0) * 1000
    print(
        f"   [INFO] staged.consolidation.end "
        f"signals={len(consolidated)} elapsed_ms={elapsed:.2f}"
    )

    # Stage 5 — Classification
    t0 = time.perf_counter()
    print(f"   [INFO] staged.classification.start signals={len(consolidated)}")
    classified = adapter.classify_signals(consolidated, classification_prompt)
    elapsed = (time.perf_counter() - t0) * 1000
    print(f"   [INFO] staged.classification.end elapsed_ms={elapsed:.2f}")

    # Stage 6 — Final context assembly
    metadata: dict[str, Any] = {
        "chunk_count": len(chunks),
        "selected_chunk_count": len(selected_chunks),
        "signal_count": len(all_signals),
        "consolidated_signal_count": len(consolidated),
        "use_embeddings": use_embeddings,
    }
    return adapter.render_final_context(classified, metadata)
