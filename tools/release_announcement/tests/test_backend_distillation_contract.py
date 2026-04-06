from __future__ import annotations

import json

import pytest

from release_announcement.backends.base import BACKEND_REGISTRY
from release_announcement.distillation import Chunk, Signal


def _instantiate_backend(name: str):
    cls = BACKEND_REGISTRY[name]
    assert cls is not None
    # Minimal constructor args used across current backends
    return cls(chat_model=None, embedding_model=None)  # type: ignore[misc, call-arg]


@pytest.mark.parametrize("backend_name", ["github", "actions"])
def test_non_ollama_backends_fail_cleanly_for_staged_methods(backend_name: str) -> None:
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


def test_ollama_backend_supports_staged_methods_with_mocked_calls(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Ollama backend implements staged adapter methods used by distillation."""
    from release_announcement.backends.ollama_backend import OllamaBackend

    backend = OllamaBackend(chat_model="mock-chat", embedding_model=None)

    def _fake_chat(*_args, **kwargs):
        messages = kwargs.get("messages", [])
        operation = messages[0]["content"] if messages else ""
        if "selected_indices" in operation:
            content = json.dumps({"selected_indices": [0]})
        elif messages and messages[0]["role"] == "system" and messages[1]["role"] == "user":
            user_content = messages[1]["content"]
            if user_content.startswith("["):
                if "internal" in operation or "classification" in operation.lower():
                    content = json.dumps(
                        {
                            "internal": [],
                            "minor": [
                                {
                                    "change": "mock classified change",
                                    "impact": "minor",
                                    "users_affected": "all",
                                    "confidence": 0.8,
                                    "final_outcome": True,
                                }
                            ],
                            "targeted": [],
                            "major": [],
                            "no_user_facing_changes": False,
                        }
                    )
                else:
                    content = json.dumps(
                        [
                            {
                                "change": "mock consolidated change",
                                "impact": "minor",
                                "users_affected": "all",
                                "confidence": 0.8,
                                "final_outcome": True,
                            }
                        ]
                    )
            else:
                content = json.dumps(
                    [
                        {
                            "change": "mock extracted change",
                            "impact": "minor",
                            "users_affected": "all",
                            "confidence": 0.8,
                            "final_outcome": True,
                        }
                    ]
                )
        else:
            content = json.dumps({"selected_indices": [0]})
        return {"message": {"content": content}}

    monkeypatch.setattr("release_announcement.backends.ollama_backend.ollama.chat", _fake_chat)

    selected = backend.select_relevant_chunks([Chunk(text="x", index=0)], use_embeddings=False)
    assert len(selected) == 1

    extracted = backend.extract_chunk_signals(Chunk(text="x", index=0), "extract")
    assert extracted

    consolidated = backend.consolidate_signals(extracted, "consolidate")
    assert consolidated

    classified = backend.classify_signals(consolidated, "classification")
    context = backend.render_final_context(classified, {"meta": 1})
    assert context.summary
    assert isinstance(context.metadata, dict)


def test_ollama_embeddings_selection_works_with_mocked_dummy_vectors(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Embeddings-assisted selection path works when embeddings are available."""
    from release_announcement.backends.ollama_backend import OllamaBackend

    backend = OllamaBackend(chat_model="mock-chat", embedding_model="dummy-embed")

    def _fake_embeddings(*_args, **kwargs):
        prompt = kwargs.get("prompt", "")
        if "Select discussion chunks" in prompt:
            return {"embedding": [1.0, 0.0]}
        if "important" in prompt:
            return {"embedding": [0.9, 0.1]}
        return {"embedding": [0.1, 0.9]}

    monkeypatch.setattr(
        "release_announcement.backends.ollama_backend.ollama.embeddings",
        _fake_embeddings,
    )

    chunks = [
        Chunk(text="important final maintainer decision", index=0),
        Chunk(text="minor side chat", index=1),
    ]
    selected = backend.select_relevant_chunks(chunks, use_embeddings=True)

    assert selected
    assert selected[0].index == 0


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
