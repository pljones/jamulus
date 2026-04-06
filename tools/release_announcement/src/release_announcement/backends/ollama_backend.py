"""Ollama backend for release-announcement generation."""

import json
import os
from typing import Any

import ollama  # pylint: disable=import-error  # type: ignore[import]

from ..distillation import (
    Chunk,
    ClassifiedSignals,
    DistilledContext,
    Signal,
    _parse_classified_signals,
    _parse_signal_list,
)

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
        except Exception as err:
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
        except Exception as err:
            print(
                "Warning: Ollama embedding capability probe failed; "
                "continuing with chat-only staged mode. "
                f"request_payload={json.dumps(embed_payload, ensure_ascii=True)} "
                f"error={err}"
            )
            return False

    def _chat_messages(self, messages: list[dict[str, str]], purpose: str) -> str:
        """Execute a chat call and return text content with rich diagnostics on failure."""
        request_payload = {"model": self.chat_model, "messages": messages}
        try:
            response = ollama.chat(model=self.chat_model, messages=messages)
            content = response.get("message", {}).get("content", "")
            if not isinstance(content, str) or not content.strip():
                raise RuntimeError(
                    "Ollama chat call returned empty or invalid content. "
                    f"operation={purpose} "
                    f"request_payload={json.dumps(request_payload, ensure_ascii=True)} "
                    f"response_body={json.dumps(response, ensure_ascii=True)}"
                )
            return content.strip()
        except RuntimeError:
            raise
        except Exception as err:
            raise RuntimeError(
                "Ollama chat call failed. "
                f"operation={purpose} "
                f"request_payload={json.dumps(request_payload, ensure_ascii=True)} "
                f"response_error={err}"
            ) from err

    @staticmethod
    def _cosine_similarity(a: list[float], b: list[float]) -> float:
        dot = sum(x * y for x, y in zip(a, b))
        mag_a = sum(x * x for x in a) ** 0.5
        mag_b = sum(x * x for x in b) ** 0.5
        return dot / (mag_a * mag_b) if mag_a and mag_b else 0.0

    def _embed_text(self, text: str, purpose: str) -> list[float]:
        """Return one embedding vector with rich diagnostics on failure."""
        if not self.embedding_model:
            raise RuntimeError(
                "Ollama embedding model is required for embeddings-assisted selection. "
                f"operation={purpose} request_payload={json.dumps({'model': None, 'input': text[:120]}, ensure_ascii=True)}"
            )

        request_payload = {"model": self.embedding_model, "input": text}
        try:
            response = ollama.embeddings(model=self.embedding_model, prompt=text)
            vector = response.get("embedding")
            if not isinstance(vector, list) or not vector:
                raise RuntimeError(
                    "Ollama embeddings call returned no embedding vector. "
                    f"operation={purpose} "
                    f"request_payload={json.dumps(request_payload, ensure_ascii=True)} "
                    f"response_body={json.dumps(response, ensure_ascii=True)}"
                )
            return [float(v) for v in vector]
        except RuntimeError:
            raise
        except Exception as err:
            raise RuntimeError(
                "Ollama embeddings call failed. "
                f"operation={purpose} "
                f"request_payload={json.dumps(request_payload, ensure_ascii=True)} "
                f"response_error={err}"
            ) from err

    def select_relevant_chunks(
        self, chunks: list[Chunk], use_embeddings: bool
    ) -> list[Chunk]:
        if not chunks:
            return []

        if use_embeddings:
            query = (
                "Select discussion chunks that best capture final, user-visible "
                "release-note changes, prioritizing maintainer decisions and outcomes."
            )
            query_vector = self._embed_text(query, "staged.selection.embeddings.query")

            scored: list[Chunk] = []
            for chunk in chunks:
                chunk_vector = self._embed_text(
                    chunk.text,
                    "staged.selection.embeddings.chunk",
                )
                score = self._cosine_similarity(query_vector, chunk_vector)
                scored.append(
                    Chunk(
                        text=chunk.text,
                        index=chunk.index,
                        relevance_score=score,
                        metadata=dict(chunk.metadata),
                    )
                )

            keep = max(1, min(len(scored), (len(scored) + 1) // 2))
            top = sorted(scored, key=lambda c: c.relevance_score, reverse=True)[:keep]
            return top

        ranking_prompt = (
            "Rank the provided discussion chunks by relevance to final user-facing "
            "release notes. Return strict JSON only in the form "
            '{"selected_indices":[0,2,...]}. Keep at least one index.'
        )
        chunks_payload = [
            {"index": i, "discussion_index": c.index, "text": c.text}
            for i, c in enumerate(chunks)
        ]
        content = self._chat_messages(
            [
                {"role": "system", "content": ranking_prompt},
                {"role": "user", "content": json.dumps(chunks_payload, ensure_ascii=True)},
            ],
            "staged.selection.chat_ranking",
        )

        try:
            payload = json.loads(content)
            indices = payload.get("selected_indices", []) if isinstance(payload, dict) else []
            selected_indices = {
                int(i) for i in indices if isinstance(i, int) and 0 <= i < len(chunks)
            }
        except Exception as err:
            raise RuntimeError(
                "Ollama chat ranking returned invalid JSON. "
                "operation=staged.selection.chat_ranking "
                f"response_body={content} "
                f"response_error={err}"
            ) from err

        if not selected_indices:
            selected_indices = {0}

        return [
            Chunk(
                text=chunks[i].text,
                index=chunks[i].index,
                relevance_score=1.0,
                metadata=dict(chunks[i].metadata),
            )
            for i in sorted(selected_indices)
        ]

    def extract_chunk_signals(self, chunk: Chunk, extraction_prompt: str) -> list[Signal]:
        content = self._chat_messages(
            [
                {"role": "system", "content": extraction_prompt},
                {"role": "user", "content": chunk.text},
            ],
            "staged.extraction",
        )
        try:
            return _parse_signal_list(content)
        except Exception as err:
            raise RuntimeError(
                "Ollama extraction response failed schema validation. "
                f"operation=staged.extraction response_body={content} error={err}"
            ) from err

    def consolidate_signals(
        self, signals: list[Signal], consolidation_prompt: str
    ) -> list[Signal]:
        payload = [sig.model_dump() for sig in signals]
        content = self._chat_messages(
            [
                {"role": "system", "content": consolidation_prompt},
                {"role": "user", "content": json.dumps(payload, ensure_ascii=True)},
            ],
            "staged.consolidation",
        )
        try:
            return _parse_signal_list(content)
        except Exception as err:
            raise RuntimeError(
                "Ollama consolidation response failed schema validation. "
                f"operation=staged.consolidation response_body={content} error={err}"
            ) from err

    def classify_signals(
        self, signals: list[Signal], classification_prompt: str
    ) -> ClassifiedSignals:
        payload = [sig.model_dump() for sig in signals]
        content = self._chat_messages(
            [
                {"role": "system", "content": classification_prompt},
                {"role": "user", "content": json.dumps(payload, ensure_ascii=True)},
            ],
            "staged.classification",
        )
        try:
            return _parse_classified_signals(content)
        except Exception as err:
            raise RuntimeError(
                "Ollama classification response failed schema validation. "
                f"operation=staged.classification response_body={content} error={err}"
            ) from err

    def render_final_context(
        self, classified: ClassifiedSignals, metadata: dict[str, Any]
    ) -> DistilledContext:
        user_facing = classified.minor + classified.targeted + classified.major
        summary = (
            "No user-facing changes."
            if classified.no_user_facing_changes or not user_facing
            else "\n".join(f"- {signal.change}" for signal in user_facing)
        )

        metadata_out = dict(metadata)
        metadata_out.setdefault("backend", "ollama")

        return DistilledContext(
            summary=summary,
            structured_signals=list(user_facing),
            classification=classified,
            metadata=metadata_out,
        )

    def call_chat(self, prompt: dict[str, Any]) -> str | None:
        response = ollama.chat(model=self.chat_model, messages=prompt["messages"])
        return response["message"]["content"].strip()


register_backend("ollama", OllamaBackend)
