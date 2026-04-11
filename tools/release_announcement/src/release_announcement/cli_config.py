"""CLI parsing and backend configuration resolution for release_announcement."""

from __future__ import annotations


import argparse
import os
from dataclasses import dataclass, field

from .registry import BackendRegistry

# Default backend for BackendConfig instantiation
DEFAULT_BACKEND = "ollama"


@dataclass
class BackendCapabilities:
    """Runtime capability probe result for the selected backend/models."""

    supports_chat: bool
    supports_embeddings: bool = False


@dataclass
class ModelSelection:
    """Resolved model state for one model role (chat or embedding)."""

    model: str | None
    backend: str
    source: str


@dataclass
class BackendConfig:  # pylint: disable=too-many-instance-attributes
    """Resolved backend/model configuration and capability probe state."""

    backend: str = "ollama"
    chat_model: str | None = None
    embedding_model: str | None = None
    dry_run: bool = False
    pipeline_mode: str = "legacy"
    staged_mode: str | None = None
    capabilities: BackendCapabilities = field(
        default_factory=lambda: BackendCapabilities(supports_chat=False, supports_embeddings=False)
    )
    chat_model_backend: str = "ollama"
    embedding_model_backend: str = "ollama"
    chat_model_source: str = "default"
    embedding_model_source: str = "default"






def _parse_model_selector(raw: str, selected_backend: str | None = None) -> tuple[str, str | None]:
    """Parse optional BACKEND/model syntax, preserving backend-specific model tags.
    If selected_backend is None, uses DEFAULT_BACKEND."""
    backend = selected_backend if selected_backend is not None else DEFAULT_BACKEND
    if "/" not in raw:
        return backend, raw

    prefix, remainder = raw.split("/", 1)
    # Accept any prefix as backend, per plan (no explicit backend names)
    if remainder == "":
        return prefix, None
    return prefix, remainder


def _resolve_chat_selection(args: argparse.Namespace) -> ModelSelection:
    if args.chat_model is None:
        backend = args.backend if args.backend is not None else DEFAULT_BACKEND
        return ModelSelection(model=None, backend=backend, source="default")

    backend, model = _parse_model_selector(args.chat_model, args.backend)
    return ModelSelection(model=model, backend=backend, source="flag")


def _resolve_embedding_selection(args: argparse.Namespace) -> ModelSelection:
    if args.embedding_model is None:
        backend = args.backend if args.backend is not None else DEFAULT_BACKEND
        return ModelSelection(model=None, backend=backend, source="default")

    backend, model = _parse_model_selector(args.embedding_model, args.backend)
    return ModelSelection(model=model, backend=backend, source="flag")


def resolve_backend_config(args: argparse.Namespace) -> BackendConfig:
    """Resolve parsed CLI args into a full backend configuration object."""
    chat = _resolve_chat_selection(args)
    embedding = _resolve_embedding_selection(args)

    # Get the backend to probe for defaults if no model is provided
    backend_name = args.backend if args.backend is not None else DEFAULT_BACKEND
    backend = BackendRegistry.get_backend(backend_name)

    # If no chat model is provided, use the backend's default
    if chat.model is None and backend is not None:
        chat_model = backend._default_chat_model if hasattr(backend, "_default_chat_model") else None
    else:
        chat_model = chat.model

    # If no embedding model is provided, use the backend's default
    if embedding.model is None and backend is not None:
        embedding_model = backend._default_embedding_model if hasattr(backend, "_default_embedding_model") else None
    else:
        embedding_model = embedding.model

    return BackendConfig(
        backend=backend_name,
        chat_model=chat_model,
        embedding_model=embedding_model,
        dry_run=args.dry_run,
        pipeline_mode=args.pipeline,
        staged_mode=args.staged_mode,
        chat_model_backend=chat.backend,
        embedding_model_backend=embedding.backend,
        chat_model_source=chat.source if chat.model is not None else "backend-default",
        embedding_model_source=embedding.source if embedding.model is not None else "backend-default",
    )


def validate_cli_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    """Validate cross-argument invariants before startup probing or processing."""
    if args.staged_mode and args.pipeline != "staged":
        parser.error("--staged-mode is only valid when --pipeline staged is set.")

    if args.staged_mode == "chat-only" and args.embedding_model:
        parser.error("--staged-mode chat-only cannot be combined with --embedding-model.")


def build_arg_parser() -> argparse.ArgumentParser:
    """Build and return the command-line argument parser."""
    parser = argparse.ArgumentParser(description="Progressive Release Announcement Generator")
    parser.add_argument(
        "start",
        help="Starting boundary, or upper bound if end is omitted"
        " (e.g. pr3409 or v3.11.0)",
    )
    parser.add_argument(
        "end",
        nargs="?",
        help="Ending boundary (e.g. pr3500 or HEAD). Defaults to start if omitted.",
    )
    parser.add_argument("--file", required=True, help="Markdown file to update")
    parser.add_argument(
        "--prompt",
        default=".github/prompts/release-announcement.prompt.yml",
        help="YAML prompt template file",
    )
    parser.add_argument(
        "--chat-model",
        "--model",
        dest="chat_model",
        default=None,
        help=(
            "Chat model override (alias: --model). "
            "If omitted with --backend ollama, falls back to OLLAMA_MODEL or "
            "mistral-large-3:675b-cloud."
        ),
    )
    parser.add_argument(
        "--embedding-model",
        "--embed",
        dest="embedding_model",
        default=None,
        help=(
            "Embedding model override (alias: --embed). "
            "Used by trimming in GitHub/actions backends."
        ),
    )
    parser.add_argument(
        "--backend",
        help="LLM backend to use (actions: for GitHub Actions workflows)",
    )
    parser.add_argument(
        "--delay-secs",
        type=int,
        default=int(os.getenv("DELAY_SECS", "0")),
        help="Seconds to sleep before each PR is processed",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show which PRs would be processed/skipped without calling the LLM",
    )
    parser.add_argument(
        "--pipeline",
        default="legacy",
        choices=["legacy", "staged"],
        help="Preprocessing pipeline mode: legacy (default) or staged (Step 2 stubs).",
    )
    parser.add_argument(
        "--staged-mode",
        default=None,
        choices=["chat-only", "embedding-assisted"],
        help=(
            "Staged-mode execution preference. Valid only with --pipeline staged. "
            "If unset, mode is auto-selected from runtime capabilities."
        ),
    )
    return parser
