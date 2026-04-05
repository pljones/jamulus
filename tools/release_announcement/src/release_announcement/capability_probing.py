"""Backend capability probing and staged-mode validation logic."""

from __future__ import annotations

from .backends.base import BACKEND_REGISTRY, Backend, BackendCapabilities
from .cli_config import BackendConfig


def _create_backend(
    backend_name: str,
    chat_model: str | None,
    embedding_model: str | None,
) -> Backend:
    """Instantiate a backend from the registry with the given models."""
    cls = BACKEND_REGISTRY.get(backend_name)
    if cls is None:
        raise RuntimeError(
            f"Backend '{backend_name}' is not registered. "
            "Import its module to register it before use."
        )
    return cls(chat_model=chat_model, embedding_model=embedding_model)


def probe_capabilities(config: BackendConfig) -> BackendCapabilities:
    """Probe backend capabilities for the resolved model/backends."""
    should_probe_embeddings = not (
        config.pipeline_mode == "staged" and config.staged_mode == "chat-only"
    )

    if config.chat_model is None:
        raise RuntimeError("Chat model resolution failed before capability probing.")

    same_backend = config.chat_model_backend == config.embedding_model_backend
    chat_backend = _create_backend(
        config.chat_model_backend,
        chat_model=config.chat_model,
        embedding_model=config.embedding_model if same_backend else None,
    )
    supports_chat = chat_backend.probe_chat_capability()

    supports_embeddings = False
    if should_probe_embeddings:
        if same_backend:
            supports_embeddings = chat_backend.probe_embedding_capability()
        elif config.embedding_model and config.embedding_model_backend:
            embed_backend = _create_backend(
                config.embedding_model_backend,
                chat_model=None,
                embedding_model=config.embedding_model,
            )
            supports_embeddings = embed_backend.probe_embedding_capability()

    capabilities = BackendCapabilities(
        supports_chat=supports_chat,
        supports_embeddings=supports_embeddings,
    )
    config.capabilities = capabilities
    return capabilities


def validate_mode(config: BackendConfig) -> None:
    """Validate and reconcile pipeline/staged mode against probed capabilities."""
    if not config.capabilities.supports_chat:
        raise RuntimeError("Backend chat capability is required but unavailable.")

    if config.pipeline_mode == "legacy":
        return

    if config.pipeline_mode != "staged":
        raise RuntimeError(f"Unsupported pipeline mode: {config.pipeline_mode}")

    if config.staged_mode == "chat-only":
        config.capabilities.supports_embeddings = False
        return

    if config.staged_mode == "embedding-assisted":
        if not config.capabilities.supports_embeddings:
            print(
                "Warning: --staged-mode embedding-assisted requested but embeddings are "
                "unavailable; downgrading to chat-only."
            )
            config.staged_mode = "chat-only"
        return

    config.staged_mode = (
        "embedding-assisted" if config.capabilities.supports_embeddings else "chat-only"
    )
