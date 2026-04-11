"""Backend registry and protocol definitions for release_announcement."""

from __future__ import annotations

import os
import subprocess
import sys
from typing import Protocol, runtime_checkable


class ModelNotFoundError(Exception):
    """Raised when the backend reports the model name is invalid."""
    pass


@runtime_checkable
class BackendProtocol(Protocol):
    """Abstract interface that all backends must implement."""

    def probe_chat(self, model: str | None) -> bool:
        """Send a minimal chat request to verify the backend is reachable with this model.
        
        Args:
            model: The model name to probe. If None, the backend uses its own internal default.
            
        Returns:
            True on success.
            
        Raises:
            ModelNotFoundError: When the backend reports the model name is invalid.
            Any other exception (network, auth, etc.) propagates as-is.
        """
        ...

    def probe_embeddings(self, model: str | None) -> bool:
        """Send a minimal embedding request to verify embedding support for this model.
        
        Args:
            model: The model name to probe. If None, the backend uses its own internal default.
            
        Returns:
            True on success, False when the backend does not support embeddings as a capability.
            
        Raises:
            ModelNotFoundError: When the model name is invalid.
            Any other exception (network, auth, etc.) propagates as-is.
        """
        ...

    def call_chat(self, prompt: dict) -> str:
        """Call the chat model with the given prompt.
        
        Args:
            prompt: The prompt to send to the chat model.
            
        Returns:
            The response from the chat model.
        """
        ...


class LegacyOllamaBackend(BackendProtocol):
    """Legacy wrapper for the Ollama backend."""

    def __init__(self) -> None:
        self._default_chat_model = os.getenv("OLLAMA_MODEL", "mistral-large-3:675b-cloud")
        self._default_embedding_model = None

    def probe_chat(self, model: str | None) -> bool:
        """Probe the chat model."""
        from .backends.ollama_backend import probe_capabilities as probe_ollama_capabilities
        
        chat_model = model if model is not None else self._default_chat_model
        embedding_model = self._default_embedding_model
        result = probe_ollama_capabilities(chat_model, embedding_model, probe_embeddings=False)
        return result.get("supports_chat", False)

    def probe_embeddings(self, model: str | None) -> bool:
        """Probe the embedding model."""
        from .backends.ollama_backend import probe_capabilities as probe_ollama_capabilities
        
        embedding_model = model if model is not None else self._default_embedding_model
        if embedding_model is None:
            return False
        result = probe_ollama_capabilities(None, embedding_model, probe_embeddings=True)
        return result.get("supports_embeddings", False)

    def call_chat(self, prompt: dict) -> str:
        """Call the chat model."""
        from .backends.ollama_backend import call_ollama_model
        
        return call_ollama_model(prompt, self._default_chat_model, self._default_embedding_model)


class LegacyGitHubBackend(BackendProtocol):
    """Legacy wrapper for the GitHub backend."""

    def __init__(self) -> None:
        self._default_chat_model = "openai/gpt-4o"
        self._default_embedding_model = "openai/text-embedding-3-small"
        self._token = self._resolve_github_token()

    def _resolve_github_token(self) -> str:
        """Resolve GitHub token from env first, then gh auth token as fallback."""
        raw_token = os.getenv("GH_TOKEN") or os.getenv("GITHUB_TOKEN")
        if raw_token:
            return raw_token.replace("\r", "").replace("\n", "").strip()

        try:
            gh_token = subprocess.check_output(
                ["gh", "auth", "token"], text=True, stderr=subprocess.STDOUT
            )
        except FileNotFoundError:
            print(
                "Error: GH_TOKEN/GITHUB_TOKEN is not set and GitHub CLI ('gh') is not installed. "
                "Install gh or set GH_TOKEN/GITHUB_TOKEN."
            )
            sys.exit(1)
        except subprocess.CalledProcessError as err:
            details = (err.output or "").strip()
            if details:
                print(
                    "Error: GH_TOKEN/GITHUB_TOKEN is not set and failed to run 'gh auth token'.\n"
                    f"gh output: {details}"
                )
            else:
                print("Error: GH_TOKEN/GITHUB_TOKEN is not set and failed to run 'gh auth token'.")
            sys.exit(1)

        return gh_token.strip()

    def probe_chat(self, model: str | None) -> bool:
        """Probe the chat model."""
        from .backends.github_backend import probe_capabilities as probe_github_capabilities
        
        chat_model = model if model is not None else self._default_chat_model
        result = probe_github_capabilities(
            self._token,
            chat_model,
            self._default_embedding_model,
            "https://models.github.ai/inference/chat/completions",
            "https://models.github.ai/inference/embeddings",
            probe_embeddings=False,
        )
        return result.get("supports_chat", False)

    def probe_embeddings(self, model: str | None) -> bool:
        """Probe the embedding model."""
        from .backends.github_backend import probe_capabilities as probe_github_capabilities
        
        embedding_model = model if model is not None else self._default_embedding_model
        result = probe_github_capabilities(
            self._token,
            self._default_chat_model,
            embedding_model,
            "https://models.github.ai/inference/chat/completions",
            "https://models.github.ai/inference/embeddings",
            probe_embeddings=True,
        )
        return result.get("supports_embeddings", False)

    def call_chat(self, prompt: dict) -> str:
        """Call the chat model."""
        from .backends.github_backend import call_github_models_api as call_github_chat_model
        
        return call_github_chat_model(
            prompt,
            chat_model_override=self._default_chat_model,
            embedding_model_override=self._default_embedding_model,
        )


class LegacyActionsBackend(BackendProtocol):
    """Legacy wrapper for the Actions backend."""

    def __init__(self) -> None:
        self._default_chat_model = "openai/gpt-4o"
        self._default_embedding_model = "openai/text-embedding-3-small"
        self._token = self._resolve_github_token()

    def _resolve_github_token(self) -> str:
        """Resolve GitHub token from env."""
        raw_token = os.getenv("GITHUB_TOKEN") or os.getenv("GH_TOKEN")
        if not raw_token:
            raise RuntimeError(
                "Error: --backend actions requires GITHUB_TOKEN to be set.\n"
                "Add this to your workflow step:\n"
                "  env:\n"
                "    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}"
            )
        return raw_token.replace("\r", "").replace("\n", "").strip()

    def probe_chat(self, model: str | None) -> bool:
        """Probe the chat model."""
        from .backends.github_backend import probe_capabilities as probe_github_capabilities
        
        chat_model = model if model is not None else self._default_chat_model
        result = probe_github_capabilities(
            self._token,
            chat_model,
            self._default_embedding_model,
            "https://models.github.ai/inference/chat/completions",
            "https://models.github.ai/inference/embeddings",
            probe_embeddings=False,
        )
        return result.get("supports_chat", False)

    def probe_embeddings(self, model: str | None) -> bool:
        """Probe the embedding model."""
        from .backends.github_backend import probe_capabilities as probe_github_capabilities
        
        embedding_model = model if model is not None else self._default_embedding_model
        result = probe_github_capabilities(
            self._token,
            self._default_chat_model,
            embedding_model,
            "https://models.github.ai/inference/chat/completions",
            "https://models.github.ai/inference/embeddings",
            probe_embeddings=True,
        )
        return result.get("supports_embeddings", False)

    def call_chat(self, prompt: dict) -> str:
        """Call the chat model."""
        from .backends.github_backend import call_github_models_api as call_github_chat_model
        
        return call_github_chat_model(
            prompt,
            chat_model_override=self._default_chat_model,
            embedding_model_override=self._default_embedding_model,
        )


class BackendRegistry:
    """Registry for backends implementing BackendProtocol."""

    def __init__(self) -> None:
        self._backends: dict[str, BackendProtocol] = {}

    def register(self, name: str, backend: BackendProtocol) -> None:
        """Register a backend with the given name."""
        self._backends[name] = backend

    def get(self, name: str) -> BackendProtocol | None:
        """Get a backend by name."""
        return self._backends.get(name)


# Global registry instance
registry = BackendRegistry()


def _get_ollama_backend() -> LegacyOllamaBackend:
    """Lazy initialization for Ollama backend."""
    return LegacyOllamaBackend()


def _get_github_backend() -> LegacyGitHubBackend:
    """Lazy initialization for GitHub backend."""
    return LegacyGitHubBackend()


def _get_actions_backend() -> LegacyActionsBackend:
    """Lazy initialization for Actions backend."""
    return LegacyActionsBackend()


# Register legacy backends lazily
registry.register("ollama", _get_ollama_backend())
registry.register("github", _get_github_backend())

# Actions backend requires GITHUB_TOKEN, so only register if it's set
if os.getenv("GITHUB_TOKEN") is not None:
    registry.register("actions", _get_actions_backend())
