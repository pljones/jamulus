"""Backend capability probing and staged-mode validation logic."""

from __future__ import annotations

import os
from typing import Callable

from .cli_config import BackendCapabilities, BackendConfig


def probe_capabilities(  # pylint: disable=too-many-arguments,too-many-positional-arguments
    config: BackendConfig,
    resolve_github_token: Callable[[], str],
    probe_github_backend_capabilities: Callable[..., dict[str, bool]],
    probe_ollama_backend_capabilities: Callable[..., dict[str, bool]],
    github_chat_endpoint: str,
    github_embeddings_endpoint: str,
) -> BackendCapabilities:
    """Probe backend capabilities for the resolved model/backends."""
    should_probe_embeddings = not (
        config.pipeline_mode == "staged" and config.staged_mode == "chat-only"
    )

    if config.chat_model is None:
        raise RuntimeError("Chat model resolution failed before capability probing.")

    if config.chat_model_backend == "ollama":
        chat_probe = probe_ollama_backend_capabilities(
            chat_model=config.chat_model,
            embedding_model=config.embedding_model,
            probe_embeddings=should_probe_embeddings,
        )
    elif config.chat_model_backend in {"github", "actions"}:
        token = resolve_github_token()
        chat_probe = probe_github_backend_capabilities(
            token=token,
            chat_model=config.chat_model,
            embedding_model=config.embedding_model,
            chat_endpoint=os.getenv("MODELS_ENDPOINT", github_chat_endpoint),
            embeddings_endpoint=github_embeddings_endpoint,
            probe_embeddings=should_probe_embeddings,
        )
    else:
        raise RuntimeError(
            f"Unsupported chat model backend for probing: {config.chat_model_backend}"
        )

    supports_embeddings = False
    if should_probe_embeddings:
        if config.embedding_model_backend == config.chat_model_backend:
            supports_embeddings = bool(chat_probe.get("supports_embeddings"))
        elif config.embedding_model and config.embedding_model_backend == "ollama":
            embed_probe = probe_ollama_backend_capabilities(
                chat_model=config.chat_model,
                embedding_model=config.embedding_model,
                probe_embeddings=True,
            )
            supports_embeddings = bool(embed_probe.get("supports_embeddings"))
        elif config.embedding_model and config.embedding_model_backend in {"github", "actions"}:
            token = resolve_github_token()
            embed_probe = probe_github_backend_capabilities(
                token=token,
                chat_model=config.chat_model,
                embedding_model=config.embedding_model,
                chat_endpoint=os.getenv("MODELS_ENDPOINT", github_chat_endpoint),
                embeddings_endpoint=github_embeddings_endpoint,
                probe_embeddings=True,
            )
            supports_embeddings = bool(embed_probe.get("supports_embeddings"))

    capabilities = BackendCapabilities(
        supports_chat=bool(chat_probe.get("supports_chat")),
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
