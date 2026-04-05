"""Dummy backend and DistillationAdapter for unit-testing the staged pipeline framework.

This module is intentionally kept under ``tests/`` and must never be imported
from production code.  Importing this module is the explicit act that registers
the ``dummy`` backend name — it will not appear in ``BACKEND_REGISTRY`` until
then, so ``--backend dummy`` is rejected by the CLI unless tests explicitly
import it (e.g. via conftest.py or the test file itself).

Model-name-encoded behaviour
-----------------------------
``DummyBackend`` reads the configured model name to drive specific test flows:

- ``chat_model="fail-probe"`` → ``probe_chat_capability`` returns ``False``.
- ``embedding_model=None`` (or omitted) → ``probe_embedding_capability`` returns
  ``False``.
- ``call_chat`` always returns ``None``, signalling "no LLM output" and
  triggering the legacy-mode fallback in callers.
"""

from __future__ import annotations

from typing import Any

from release_announcement.backends.base import (
    Backend,
    register_backend,
)
from release_announcement.distillation import (
    Chunk,
    ClassifiedSignals,
    DistilledContext,
    DistillationAdapter,
    Signal,
)


class DummyDistillationAdapter:
    """Hardcoded adapter; no LLM calls are made.

    Attributes:
        fail_select: If True, raise RuntimeError from ``select_relevant_chunks``.
        fail_extract_after: If set, raise RuntimeError from
            ``extract_chunk_signals`` once that many successful calls have
            been made (0 = fail on the first call).
        select_calls: Record of each (chunks, use_embeddings) argument pair.
        extract_calls: Record of each chunk passed to ``extract_chunk_signals``.
        consolidate_calls: Record of each signals list passed to
            ``consolidate_signals``.
        classify_calls: Record of each signals list passed to
            ``classify_signals``.
    """

    def __init__(
        self,
        fail_select: bool = False,
        fail_extract_after: int | None = None,
    ) -> None:
        self.fail_select = fail_select
        self.fail_extract_after = fail_extract_after
        self._extract_call_count = 0

        self.select_calls: list[tuple[list[Chunk], bool]] = []
        self.extract_calls: list[Chunk] = []
        self.consolidate_calls: list[list[Signal]] = []
        self.classify_calls: list[list[Signal]] = []

    # ── DistillationAdapter implementation ────────────────────────────────────

    def select_relevant_chunks(
        self, chunks: list[Chunk], use_embeddings: bool
    ) -> list[Chunk]:
        """Return all input chunks with dummy relevance scores."""
        self.select_calls.append((list(chunks), use_embeddings))
        if self.fail_select:
            raise RuntimeError("DummyDistillationAdapter: select_relevant_chunks failed")
        return [
            Chunk(text=c.text, index=c.index, relevance_score=0.9, metadata=dict(c.metadata))
            for c in chunks
        ]

    def extract_chunk_signals(
        self, chunk: Chunk, extraction_prompt: str  # noqa: ARG002
    ) -> list[Signal]:
        """Return a single hardcoded Signal per chunk."""
        self.extract_calls.append(chunk)
        if (
            self.fail_extract_after is not None
            and self._extract_call_count >= self.fail_extract_after
        ):
            raise RuntimeError(
                f"DummyDistillationAdapter: extract_chunk_signals failed "
                f"at call {self._extract_call_count}"
            )
        self._extract_call_count += 1
        return [
            Signal(
                change=f"Dummy change from chunk {chunk.index}",
                impact="minor",
                users_affected="all users",
                confidence=0.9,
                final_outcome=True,
            )
        ]

    def consolidate_signals(
        self, signals: list[Signal], consolidation_prompt: str  # noqa: ARG002
    ) -> list[Signal]:
        """Return the input list unchanged (identity consolidation)."""
        self.consolidate_calls.append(list(signals))
        return list(signals)

    def classify_signals(
        self, signals: list[Signal], classification_prompt: str  # noqa: ARG002
    ) -> ClassifiedSignals:
        """Classify every signal as ``minor``."""
        self.classify_calls.append(list(signals))
        return ClassifiedSignals(minor=list(signals))

    def render_final_context(
        self, classified: ClassifiedSignals, metadata: dict[str, Any]
    ) -> DistilledContext:
        """Assemble a DistilledContext from the dummy classification result."""
        user_facing = (
            classified.minor + classified.targeted + classified.major
        )
        summary = (
            "; ".join(s.change for s in user_facing)
            if user_facing
            else "No user-facing changes."
        )
        return DistilledContext(
            summary=summary,
            structured_signals=list(user_facing),
            classification=classified,
            metadata=dict(metadata),
        )


# Runtime-checkable protocol verification (fail fast if protocol drifted)
assert isinstance(DummyDistillationAdapter(), DistillationAdapter), (
    "DummyDistillationAdapter does not satisfy DistillationAdapter protocol"
)


class DummyBackend(Backend):
    """Test-only backend that never issues real LLM calls.

    Model names encode test behaviour:
    - ``chat_model="fail-probe"`` → probe reports ``supports_chat=False``.
    - ``embedding_model=None`` → probe reports ``supports_embeddings=False``.
    - ``call_chat`` always returns ``None`` (triggers legacy-mode fallback).
    """

    DEFAULT_CHAT_MODEL = "dummy-chat"
    DEFAULT_EMBEDDING_MODEL = "dummy-embeddings"

    def __init__(
        self,
        chat_model: str | None,
        embedding_model: str | None,
        fail_select: bool = False,
        fail_extract_after: int | None = None,
    ) -> None:
        self.chat_model = chat_model
        self.embedding_model = embedding_model
        self._adapter = DummyDistillationAdapter(
            fail_select=fail_select,
            fail_extract_after=fail_extract_after,
        )

        # Expose adapter telemetry for tests that assert call behavior.
        self.select_calls = self._adapter.select_calls
        self.extract_calls = self._adapter.extract_calls
        self.consolidate_calls = self._adapter.consolidate_calls
        self.classify_calls = self._adapter.classify_calls

    def probe_chat_capability(self) -> bool:
        return self.chat_model != "fail-probe"

    def probe_embedding_capability(self) -> bool:
        return bool(self.embedding_model)

    def call_chat(self, prompt: dict[str, Any]) -> str | None:
        return None

    # DistillationAdapter hooks (explicitly overridden for staged-mode tests)

    def select_relevant_chunks(
        self, chunks: list[Chunk], use_embeddings: bool
    ) -> list[Chunk]:
        return self._adapter.select_relevant_chunks(chunks, use_embeddings)

    def extract_chunk_signals(
        self, chunk: Chunk, extraction_prompt: str
    ) -> list[Signal]:
        return self._adapter.extract_chunk_signals(chunk, extraction_prompt)

    def consolidate_signals(
        self, signals: list[Signal], consolidation_prompt: str
    ) -> list[Signal]:
        return self._adapter.consolidate_signals(signals, consolidation_prompt)

    def classify_signals(
        self, signals: list[Signal], classification_prompt: str
    ) -> ClassifiedSignals:
        return self._adapter.classify_signals(signals, classification_prompt)

    def render_final_context(
        self, classified: ClassifiedSignals, metadata: dict[str, Any]
    ) -> DistilledContext:
        return self._adapter.render_final_context(classified, metadata)


register_backend("dummy", DummyBackend)
