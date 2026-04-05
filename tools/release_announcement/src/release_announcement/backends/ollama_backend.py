"""Ollama backend for release-announcement generation."""

import json
import os
from typing import Any

import ollama  # pylint: disable=import-error  # type: ignore[import]

from .base import Backend, register_backend


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
        or os.getenv("OLLAMA_MODEL", OllamaBackend.DEFAULT_CHAT_MODEL)
    )
    response = ollama.chat(model=model, messages=prompt["messages"])
    return response["message"]["content"].strip()


class OllamaBackend(Backend):
    """Ollama backend instance with resolved model configuration."""

    DEFAULT_CHAT_MODEL = "mistral-large-3:675b-cloud"
    DEFAULT_EMBEDDING_MODEL = None

    def __init__(self, chat_model: str | None, embedding_model: str | None) -> None:
        self.chat_model = chat_model or self.DEFAULT_CHAT_MODEL
        self.embedding_model = embedding_model or os.getenv("OLLAMA_EMBEDDING_MODEL")

    def probe_chat_capability(self) -> bool:
        """Probe Ollama chat support for the configured chat model."""
        chat_payload = {
            "model": self.chat_model,
            "messages": [{"role": "user", "content": "Reply with 'ok'."}],
        }
        try:
            response = ollama.chat(model=self.chat_model, messages=chat_payload["messages"])
            content = response.get("message", {}).get("content", "")
            if not isinstance(content, str) or not content.strip():
                raise RuntimeError(
                    "Ollama chat probe returned an empty or invalid message content. "
                    f"request_payload={json.dumps(chat_payload, ensure_ascii=True)} "
                    f"response={json.dumps(response, ensure_ascii=True)}"
                )
        except Exception as err:  # pylint: disable=broad-except
            raise RuntimeError(
                "Ollama chat capability probe failed. "
                f"request_payload={json.dumps(chat_payload, ensure_ascii=True)} "
                f"error={err}"
            ) from err
        return True

    def probe_embedding_capability(self) -> bool:
        """Probe Ollama embedding support for the configured embedding model."""
        if not self.embedding_model:
            return False
        embed_payload = {"model": self.embedding_model, "input": "test"}
        try:
            response = ollama.embeddings(model=self.embedding_model, prompt="test")
            vector = response.get("embedding")
            if not isinstance(vector, list) or not vector:
                raise RuntimeError(
                    "Ollama embeddings probe returned no embedding vector. "
                    f"request_payload={json.dumps(embed_payload, ensure_ascii=True)} "
                    f"response={json.dumps(response, ensure_ascii=True)}"
                )
            return True
        except Exception as err:  # pylint: disable=broad-except
            print(
                "Warning: Ollama embedding capability probe failed; "
                "continuing with chat-only staged mode. "
                f"request_payload={json.dumps(embed_payload, ensure_ascii=True)} "
                f"error={err}"
            )
            return False

    def call_chat(self, prompt: dict[str, Any]) -> str | None:
        response = ollama.chat(model=self.chat_model, messages=prompt["messages"])
        return response["message"]["content"].strip()


register_backend("ollama", OllamaBackend)
