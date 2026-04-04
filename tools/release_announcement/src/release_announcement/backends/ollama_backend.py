"""Ollama backend for release-announcement generation."""

import os
from typing import Any

import ollama  # pylint: disable=import-error  # type: ignore[import]


def call_ollama_model(
    prompt: dict[str, Any],
    chat_model_override: str | None = None,
    embedding_model_override: str | None = None,
    model_override: str | None = None,
) -> str:
    """Call Ollama API for local model inference."""
    del embedding_model_override  # Ollama backend currently uses chat model only.
    model = (
        chat_model_override
        or model_override
        or os.getenv("OLLAMA_MODEL", "mistral-large-3:675b-cloud")
    )
    response = ollama.chat(model=model, messages=prompt["messages"])
    return response["message"]["content"].strip()
