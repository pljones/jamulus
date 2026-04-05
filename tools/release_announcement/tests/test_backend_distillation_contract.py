from __future__ import annotations

import pytest

from release_announcement.backends.base import BACKEND_REGISTRY
from release_announcement.distillation import Chunk, Signal


def _instantiate_backend(name: str):
    cls = BACKEND_REGISTRY[name]
    assert cls is not None
    # Minimal constructor args used across current backends
    return cls(chat_model=None, embedding_model=None)  # type: ignore[misc, call-arg]


@pytest.mark.parametrize("backend_name", ["ollama", "github", "actions"])
def test_non_dummy_backends_fail_cleanly_for_staged_methods(backend_name: str) -> None:
    """Backends without staged implementation raise NotImplementedError cleanly."""
    backend = _instantiate_backend(backend_name)

    with pytest.raises(NotImplementedError):
        backend.select_relevant_chunks([Chunk(text="x", index=0)], use_embeddings=False)

    with pytest.raises(NotImplementedError):
        backend.extract_chunk_signals(Chunk(text="x", index=0), "prompt")

    with pytest.raises(NotImplementedError):
        backend.consolidate_signals(
            [
                Signal(
                    change="c",
                    impact="minor",
                    users_affected="all",
                    confidence=0.9,
                    final_outcome=True,
                )
            ],
            "prompt",
        )

    with pytest.raises(NotImplementedError):
        backend.classify_signals([], "prompt")

    with pytest.raises(NotImplementedError):
        backend.render_final_context(None, {})


def test_dummy_backend_supports_staged_methods() -> None:
    """Dummy backend implements staged methods and can be used as an adapter."""
    from tests.dummy_backend import DummyBackend

    backend = DummyBackend(chat_model="dummy-chat", embedding_model="dummy-embeddings")

    selected = backend.select_relevant_chunks([Chunk(text="x", index=0)], use_embeddings=False)
    assert len(selected) == 1

    extracted = backend.extract_chunk_signals(Chunk(text="x", index=0), "prompt")
    assert extracted

    consolidated = backend.consolidate_signals(extracted, "prompt")
    assert consolidated

    classified = backend.classify_signals(consolidated, "prompt")
    context = backend.render_final_context(classified, {"meta": 1})
    assert context.summary
    assert isinstance(context.metadata, dict)
