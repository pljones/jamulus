from __future__ import annotations

import argparse

import pytest

from release_announcement import main as ra_main


def _parse(cli_args: list[str]) -> argparse.Namespace:
    parser = ra_main._build_arg_parser()
    return parser.parse_args(cli_args + ["--file", "ReleaseAnnouncement.md", "pr3429"])


def _resolve(cli_args: list[str]) -> ra_main.BackendConfig:
    args = _parse(cli_args)
    return ra_main._resolve_backend_config(args)


def test_defaults_to_ollama_chat_and_no_embedding(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("OLLAMA_MODEL", raising=False)
    config = _resolve([])

    assert config.backend == "ollama"
    assert config.chat_model == "mistral-large-3:675b-cloud"
    assert config.embedding_model is None


def test_backend_ollama_defaults(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("OLLAMA_MODEL", raising=False)
    config = _resolve(["--backend", "ollama"])

    assert config.chat_model == "mistral-large-3:675b-cloud"
    assert config.embedding_model is None


def test_backend_github_defaults() -> None:
    config = _resolve(["--backend", "github"])

    assert config.chat_model == "openai/gpt-4o"
    assert config.embedding_model == "openai/text-embedding-3-small"


def test_ollama_chat_model_override(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("OLLAMA_MODEL", raising=False)
    config = _resolve(["--backend", "ollama", "--chat-model", "mistral-large"])

    assert config.chat_model == "mistral-large"
    assert config.embedding_model is None


def test_ollama_embedding_model_override_with_prefix(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("OLLAMA_MODEL", raising=False)
    config = _resolve(["--backend", "ollama", "--embedding-model", "ollama:nomic-embed-text"])

    assert config.embedding_model == "nomic-embed-text"
    assert config.chat_model == "mistral-large-3:675b-cloud"


def test_github_embedding_model_override_from_ollama_backend(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("OLLAMA_MODEL", raising=False)
    config = _resolve(
        ["--backend", "ollama", "--embedding-model", "github:text-embedding-3-small"]
    )

    assert config.embedding_model == "text-embedding-3-small"
    assert config.chat_model == "mistral-large-3:675b-cloud"


def test_github_backend_with_ollama_chat_override() -> None:
    config = _resolve(["--backend", "github", "--chat-model", "ollama:mistral-large"])

    assert config.chat_model == "mistral-large"
    assert config.embedding_model == "openai/text-embedding-3-small"


def test_no_backend_with_chat_and_embedding_flags() -> None:
    config = _resolve(["--chat-model", "mistral-large", "--embedding-model", "nomic-embed-text"])

    assert config.backend == "ollama"
    assert config.chat_model == "mistral-large"
    assert config.embedding_model == "nomic-embed-text"


def test_no_backend_with_prefixed_models() -> None:
    config = _resolve(
        [
            "--chat-model",
            "ollama:mistral-large",
            "--embedding-model",
            "github:text-embedding-3-small",
        ]
    )

    assert config.backend == "ollama"
    assert config.chat_model == "mistral-large"
    assert config.embedding_model == "text-embedding-3-small"


def test_chat_prefix_only_uses_backend_default() -> None:
    config = _resolve(["--chat-model", "github:"])

    assert config.chat_model == "openai/gpt-4o"
    assert config.chat_model_backend == "github"


def test_embedding_prefix_only_uses_backend_default() -> None:
    config = _resolve(["--embedding-model", "ollama:"])

    assert config.embedding_model == "nomic-embed-text"
    assert config.embedding_model_backend == "ollama"


def test_pipeline_staged_ollama_embedding_model_is_resolved() -> None:
    config = _resolve(
        [
            "--pipeline",
            "staged",
            "--backend",
            "ollama",
            "--embedding-model",
            "ollama:nomic-embed-text",
        ]
    )

    assert config.embedding_model == "nomic-embed-text"


def test_pipeline_legacy_stores_embedding_model() -> None:
    config = _resolve(["--pipeline", "legacy", "--embedding-model", "nomic-embed-text"])

    assert config.embedding_model == "nomic-embed-text"


def test_staged_mode_requires_staged_pipeline() -> None:
    parser = ra_main._build_arg_parser()
    args = parser.parse_args([
        "--staged-mode",
        "chat-only",
        "--file",
        "ReleaseAnnouncement.md",
        "pr3429",
    ])

    with pytest.raises(SystemExit):
        ra_main.validate_cli_args(parser, args)


def test_chat_only_cannot_be_combined_with_embedding_model() -> None:
    parser = ra_main._build_arg_parser()
    args = parser.parse_args([
        "--pipeline",
        "staged",
        "--staged-mode",
        "chat-only",
        "--embedding-model",
        "nomic-embed-text",
        "--file",
        "ReleaseAnnouncement.md",
        "pr3429",
    ])

    with pytest.raises(SystemExit):
        ra_main.validate_cli_args(parser, args)


def test_chat_only_skips_embedding_probe(monkeypatch: pytest.MonkeyPatch) -> None:
    config = _resolve(["--pipeline", "staged", "--staged-mode", "chat-only", "--backend", "ollama"])
    calls: list[bool] = []

    def _fake_probe(**kwargs):
        calls.append(bool(kwargs["probe_embeddings"]))
        return {"supports_chat": True, "supports_embeddings": False}

    monkeypatch.setattr(ra_main, "probe_ollama_backend_capabilities", _fake_probe)

    caps = ra_main.probe_capabilities(config)

    assert calls == [False]
    assert caps.supports_embeddings is False
    assert config.capabilities.supports_embeddings is False
