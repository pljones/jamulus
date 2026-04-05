from __future__ import annotations

from pathlib import Path

import pytest

from release_announcement import main as ra_main


def _sample_pr_data() -> dict:
    return {
        "number": 123,
        "title": "Sample PR",
        "body": "User visible change",
        "comments": ["Looks good", "Merged"],
        "reviews": ["Please rename field"],
    }


def test_prepare_pr_context_legacy_returns_none(capsys: pytest.CaptureFixture[str]) -> None:
    config = ra_main.BackendConfig(pipeline_mode="legacy")

    context = ra_main.prepare_pr_context(_sample_pr_data(), config, "legacy")

    assert context is None
    assert "staged.preprocessing" not in capsys.readouterr().out


def test_prepare_pr_context_staged_logs_all_stub_stages(
    capsys: pytest.CaptureFixture[str],
) -> None:
    config = ra_main.BackendConfig(backend="github", pipeline_mode="staged")

    context = ra_main.prepare_pr_context(_sample_pr_data(), config, "staged")

    assert context is None
    output = capsys.readouterr().out
    assert "staged.preprocessing.start" in output
    assert "staged.chunking.start" in output
    assert "staged.chunking.end" in output
    assert "staged.extraction.start" in output
    assert "staged.extraction.end" in output
    assert "staged.consolidation.start" in output
    assert "staged.consolidation.end" in output
    assert "staged.classification.start" in output
    assert "staged.classification.end" in output
    assert "staged.preprocessing.end context=none" in output


def test_process_single_pr_staged_falls_back_to_legacy_builder(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    ann_file = tmp_path / "ReleaseAnnouncement.md"
    ann_file.write_text("existing announcement", encoding="utf-8")

    pr_data = _sample_pr_data()
    build_calls: list[dict] = []

    monkeypatch.setattr(ra_main, "_fetch_pr_data", lambda *_args, **_kwargs: pr_data)

    def _capture_build(current_content: str, in_pr_data: dict, _template: dict) -> dict:
        build_calls.append({"content": current_content, "pr_data": in_pr_data})
        return {
            "messages": [{"role": "user", "content": "prompt"}],
            "model": "m",
            "modelParameters": {},
        }

    monkeypatch.setattr(ra_main, "_load_prompt_template", lambda *_args, **_kwargs: {"messages": []})
    monkeypatch.setattr(ra_main, "_build_ai_prompt", _capture_build)
    monkeypatch.setattr(ra_main, "_process_with_llm", lambda *_args, **_kwargs: "updated")
    monkeypatch.setattr(ra_main, "_write_and_check_announcement", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(ra_main, "_check_for_changes", lambda *_args, **_kwargs: "no_changes")

    result = ra_main.process_single_pr(
        pr_num=123,
        pr_title="Sample PR",
        announcement_file=str(ann_file),
        prompt_file="unused.yml",
        config=ra_main.BackendConfig(backend="ollama", pipeline_mode="staged"),
    )

    assert result == "no_changes"
    assert build_calls
    assert build_calls[0]["pr_data"] == pr_data

    output = capsys.readouterr().out
    assert "staged preprocessing returned no context, falling back to legacy mode" in output


@pytest.mark.parametrize("backend", ["ollama", "github", "actions"])
def test_process_single_pr_staged_calls_prepare_for_all_backends(
    backend: str,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    ann_file = tmp_path / "ReleaseAnnouncement.md"
    ann_file.write_text("existing announcement", encoding="utf-8")

    monkeypatch.setattr(ra_main, "_fetch_pr_data", lambda *_args, **_kwargs: _sample_pr_data())
    monkeypatch.setattr(ra_main, "_load_prompt_template", lambda *_args, **_kwargs: {"messages": []})
    monkeypatch.setattr(
        ra_main,
        "_build_ai_prompt",
        lambda *_args, **_kwargs: {
            "messages": [{"role": "user", "content": "prompt"}],
            "model": "m",
            "modelParameters": {},
        },
    )
    monkeypatch.setattr(ra_main, "_process_with_llm", lambda *_args, **_kwargs: "updated")
    monkeypatch.setattr(ra_main, "_write_and_check_announcement", lambda *_args, **_kwargs: None)
    monkeypatch.setattr(ra_main, "_check_for_changes", lambda *_args, **_kwargs: "no_changes")

    calls = {"count": 0}

    def _stub_prepare(*_args, **_kwargs):
        calls["count"] += 1
        return None

    monkeypatch.setattr(ra_main, "prepare_pr_context", _stub_prepare)

    result = ra_main.process_single_pr(
        pr_num=123,
        pr_title="Sample PR",
        announcement_file=str(ann_file),
        prompt_file="unused.yml",
        config=ra_main.BackendConfig(backend=backend, pipeline_mode="staged"),
    )

    assert result == "no_changes"
    assert calls["count"] == 1
