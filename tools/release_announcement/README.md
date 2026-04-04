# release_announcement

Python package for generating and updating `ReleaseAnnouncement.md` from merged PRs.

## Entry Point

Run from the repository root after installing the package:

```bash
python -m release_announcement START [END] --file ReleaseAnnouncement.md [options]
```

Examples:

```bash
python -m release_announcement HEAD~20 HEAD --file ReleaseAnnouncement.md --backend ollama --dry-run
python -m release_announcement pr3500 --file ReleaseAnnouncement.md --backend actions
```

## CLI Flags

- Positional: `start` (required), `end` (optional)
- `--file` (required): Markdown file to update.
- `--prompt`: Prompt template file path. Default: `.github/prompts/release-announcement.prompt.yml`.
- `--chat-model`, `--model`: Chat model override.
- `--embedding-model`, `--embed`: Embedding model override (GitHub/actions backends).
- `--backend`: One of `ollama`, `github`, `actions`.
- `--delay-secs`: Delay before each PR is processed.
- `--dry-run`: Discover and report work without calling the LLM.

## Install

From repository root:

```bash
python3 -m pip install ./tools/release_announcement
```

For development/test dependencies:

```bash
python3 -m pip install -e ./tools/release_announcement[dev]
```

## Test Suite

Run from the package root:

```bash
cd tools/release_announcement
python -m pytest
```
