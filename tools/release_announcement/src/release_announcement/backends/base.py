"""Abstract backend interface and shared types for release-announcement generation."""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Any, ClassVar


@dataclass
class BackendCapabilities:
    """Runtime capability probe result for the selected backend/models."""

    supports_chat: bool
    supports_embeddings: bool = False


BACKEND_REGISTRY: dict[str, type[Backend] | None] = {}
"""Map from backend name to its Backend subclass.

Production backends (ollama, github, actions) register themselves at module
load.  The dummy test backend is absent until ``tests.dummy_backend`` is
explicitly imported, so it never appears in production choices.

A ``None`` value marks a backend as known-but-disabled; validation rejects it.
"""


def register_backend(name: str, cls: type[Backend] | None) -> None:
    """Add or update a backend name in the registry."""
    BACKEND_REGISTRY[name] = cls


class Backend(ABC):
    """Abstract base for all LLM backends.

    Each concrete backend declares its own default model names as class
    attributes and implements ``probe_capabilities`` to report what the
    backend actually supports at runtime.
    """

    # --- Staged distillation hooks ---
    #
    # Backends may override these when staged distillation support is added.
    # The default implementations fail explicitly so staged mode degrades
    # cleanly rather than silently doing the wrong thing.

    def select_relevant_chunks(
        self, chunks: list[Any], use_embeddings: bool
    ) -> list[Any]:
        raise NotImplementedError(
            f"{type(self).__name__}.select_relevant_chunks is not implemented"
        )

    def extract_chunk_signals(self, chunk: Any, extraction_prompt: str) -> list[Any]:
        raise NotImplementedError(
            f"{type(self).__name__}.extract_chunk_signals is not implemented"
        )

    def consolidate_signals(
        self, signals: list[Any], consolidation_prompt: str
    ) -> list[Any]:
        raise NotImplementedError(
            f"{type(self).__name__}.consolidate_signals is not implemented"
        )

    def classify_signals(self, signals: list[Any], classification_prompt: str) -> Any:
        raise NotImplementedError(
            f"{type(self).__name__}.classify_signals is not implemented"
        )

    def render_final_context(self, classified: Any, metadata: dict[str, Any]) -> Any:
        raise NotImplementedError(
            f"{type(self).__name__}.render_final_context is not implemented"
        )

    DEFAULT_CHAT_MODEL: ClassVar[str]
    DEFAULT_EMBEDDING_MODEL: ClassVar[str | None] = None

    @abstractmethod
    def probe_chat_capability(self) -> bool:
        """Return True when this backend can fulfil a chat completion request.

        May raise ``RuntimeError`` if the backend is fundamentally misconfigured
        (e.g. missing credentials), allowing the caller to abort early.
        """

    @abstractmethod
    def probe_embedding_capability(self) -> bool:
        """Return True when this backend can produce embedding vectors.

        Returns False (with a logged warning) on soft failures so callers can
        downgrade gracefully; only raises on hard configuration errors.
        """

    @abstractmethod
    def call_chat(self, prompt: dict[str, Any]) -> str | None:
        """Run a chat completion and return the response text.

        Returns ``None`` to signal that no LLM output is available (e.g. the
        dummy backend), which callers treat as a no-op / legacy-mode fallback.
        """
