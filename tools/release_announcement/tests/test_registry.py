"""Tests for the backend registry."""

import os

# Set the GITHUB_TOKEN environment variable before importing the registry module
os.environ["GITHUB_TOKEN"] = "test_token"

from release_announcement.registry import registry, ModelNotFoundError


def test_registry_register_and_get():
    """Test that the registry can register and retrieve a backend."""
    # Create a minimal backend that implements the protocol
    class MinimalBackend:
        def probe_chat(self, model: str | None) -> bool:
            return True
        
        def probe_embeddings(self, model: str | None) -> bool:
            return False
        
        def call_chat(self, prompt: dict) -> str:
            return "response"
    
    backend = MinimalBackend()
    registry.register("testBackend", backend)
    assert registry.get("testBackend") is backend


def test_registry_get_unknown():
    """Test that the registry returns None for unknown backends."""
    assert registry.get("unknownBackend") is None
