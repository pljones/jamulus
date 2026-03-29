#!/usr/bin/env python3
##############################################################################
# Copyright (c) 2026
#
# Author(s):
#  The Jamulus Development Team
#
##############################################################################
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
#
##############################################################################

"""
Given a range of git history, iterate over PR merges and
update the specified release announcement file.

This script supports multiple LLM backends:
- Ollama (local models, --backend ollama)
- GitHub Models API for CLI use (--backend github, resolves token via env or 'gh auth token')
- GitHub Models API for workflow use (--backend actions, requires GITHUB_TOKEN in step env)
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from typing import Any, NamedTuple

import yaml  # pylint: disable=import-error  # type: ignore[import]
from release_announcement_backends.github_backend import (
    call_github_models_api as call_github_models_api_backend,
)
from release_announcement_backends.ollama_backend import call_ollama_model

GITHUB_MODELS_DEFAULT_CHAT_MODEL = "openai/gpt-4o"
GITHUB_MODELS_DEFAULT_EMBEDDING_MODEL = "openai/text-embedding-3-small"
GITHUB_MODELS_CHAT_ENDPOINT = "https://models.github.ai/inference/chat/completions"
GITHUB_MODELS_EMBEDDINGS_ENDPOINT = "https://models.github.ai/inference/embeddings"
GITHUB_MODELS_CHAT_TOKEN_LIMIT = 7500
"""Conservative token budget for a single GitHub Models chat-completions call."""

_PR_MERGE_RE = re.compile(r"^Merge pull request #(\d+) from ")
_CHANGELOG_SKIP_RE = re.compile(r"(?m)^CHANGELOG:\s*SKIP\s*$", re.IGNORECASE)
_upstream_repo_cache: dict = {}


def normalize_github_token(raw_token: str) -> str:
    """Normalize token text from environment variables or gh output."""
    # Handle common shell patterns where a trailing newline sneaks in.
    token = raw_token.replace("\r", "").replace("\n", "").strip()
    if token.lower().startswith("bearer "):
        token = token[7:].strip()

    # Tokens must be a single header-safe value.
    if any(ch.isspace() for ch in token) or any(ord(ch) < 32 or ord(ch) == 127 for ch in token):
        print(
            "Error: GitHub token contains whitespace/control characters after normalization. "
            "Set GH_TOKEN/GITHUB_TOKEN to the raw token value."
        )
        sys.exit(1)

    if not token:
        print("Error: GitHub token is empty after normalization.")
        sys.exit(1)

    return token


def resolve_github_token() -> str:
    """Resolve GitHub token from env first, then gh auth token as fallback."""
    raw_token = os.getenv("GH_TOKEN") or os.getenv("GITHUB_TOKEN")
    if raw_token:
        return normalize_github_token(raw_token)

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

    return normalize_github_token(gh_token)


def get_gh_auth_env() -> dict[str, str]:
    """Get subprocess env with normalized token for gh CLI calls if provided."""
    env = os.environ.copy()
    raw_token = os.getenv("GH_TOKEN") or os.getenv("GITHUB_TOKEN")
    if not raw_token:
        return env

    token = normalize_github_token(raw_token)
    # Set both vars so gh uses a clean value consistently.
    env["GH_TOKEN"] = token
    env["GITHUB_TOKEN"] = token
    return env


def get_universal_timestamp(identifier: str) -> str:
    """
    Resolves an identifier to an ISO8601 timestamp.
    Supports: pr123, Tags, SHAs, and Branches.
    """
    target = resolve_identifier_to_git_target(identifier)

    # 2. Get authoritative Git timestamp for the target
    try:
        return subprocess.check_output(
            ["git", "show", "-s", "--format=%cI", target], text=True
        ).strip()
    except subprocess.CalledProcessError:
        print(f"Error: Could not resolve '{target}' as a Git object.")
        sys.exit(1)


def _get_upstream_repo() -> str | None:
    """
    Return the parent repository's 'owner/name' string for the current repo,
    or None if the current repo has no parent (i.e. it IS the canonical upstream).
    Result is cached after the first call.
    """
    if "value" not in _upstream_repo_cache:
        try:
            raw = subprocess.check_output(
                ["gh", "repo", "view", "--json", "parent"],
                text=True,
                env=get_gh_auth_env(),
            )
            parent = json.loads(raw).get("parent")
            if parent:
                owner = parent["owner"]["login"]
                name = parent["name"]
                _upstream_repo_cache["value"] = f"{owner}/{name}"
            else:
                _upstream_repo_cache["value"] = None
        except (subprocess.CalledProcessError, json.JSONDecodeError, KeyError):
            _upstream_repo_cache["value"] = None
    return _upstream_repo_cache["value"]


def resolve_identifier_to_git_target(identifier: str) -> str:
    """
    Resolve an identifier to a git object SHA or ref.

    Non-pr identifiers (tags, SHAs, branches) are passed straight through to
    git — no network call needed.

    pr<N> identifiers refer to a merged PR in the upstream repository.  The
    upstream is discovered automatically via 'gh repo view --json parent'; if
    the current repository has no parent it is itself the upstream.
    """
    if identifier.lower().startswith("pr"):
        pr_id = identifier[2:]
        upstream = _get_upstream_repo()
        repo_desc = f"upstream {upstream}" if upstream else "current repo"
        print(f"   > Resolving PR #{pr_id} via {repo_desc} …")
        try:
            cmd = ["gh", "pr", "view", pr_id, "--json", "mergeCommit", "--jq", ".mergeCommit.oid"]
            if upstream:
                cmd += ["--repo", upstream]
            target = subprocess.check_output(cmd, text=True, env=get_gh_auth_env()).strip()
            if not target or target == "null":
                print(f"Error: PR #{pr_id} has no merge commit (is it merged?)")
                sys.exit(1)
            return target
        except subprocess.CalledProcessError:
            print(f"Error: Failed to fetch PR #{pr_id} from {repo_desc}.")
            sys.exit(1)

    return identifier


def get_previous_commit_timestamp(identifier: str) -> str:
    """Get the timestamp of the commit immediately before the resolved target."""
    target = resolve_identifier_to_git_target(identifier)
    previous_target = f"{target}~"

    try:
        return subprocess.check_output(
            ["git", "show", "-s", "--format=%cI", previous_target], text=True
        ).strip()
    except subprocess.CalledProcessError:
        print(f"Error: Could not resolve previous commit for '{identifier}'.")
        sys.exit(1)


def parse_iso_datetime(value: str) -> datetime:
    """Parse ISO8601 timestamps from Git/GitHub into timezone-aware datetime."""
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def get_ordered_pr_list(start_iso: str, end_iso: str) -> list:
    """
    Find PRs merged in (start_iso, end_iso] from the local git log.
    Parses 'Merge pull request #N' merge commits so it works correctly
    regardless of which repository the workflow runs in (no GitHub API used).
    Returns list of dicts with 'number', 'title', 'mergedAt', oldest-first.
    """
    sep = "\x1e"  # ASCII record-separator; safe as git log field delimiter
    log_output = subprocess.check_output(
        ["git", "log", "--merges", f"--format=tformat:{sep}%cI%n%s%n%b"], text=True
    )

    start_dt = parse_iso_datetime(start_iso)
    end_dt = parse_iso_datetime(end_iso)

    prs = []
    for record in log_output.split(sep):
        record = record.strip()
        if not record:
            continue
        lines = record.splitlines()
        if len(lines) < 2:
            continue

        committed_at = lines[0].strip()
        subject = lines[1].strip()
        match = _PR_MERGE_RE.match(subject)
        if not match:
            continue

        try:
            committed_dt = parse_iso_datetime(committed_at)
        except ValueError:
            continue

        if committed_dt <= start_dt or committed_dt > end_dt:
            continue

        pr_num = int(match.group(1))
        # GitHub merge commits: body line 1 is the PR title; fall back to subject.
        title = next((line.strip() for line in lines[2:] if line.strip()), subject)
        prs.append({"number": pr_num, "title": title, "mergedAt": committed_at})

    return sorted(prs, key=lambda x: x["mergedAt"])


def sanitize_pr_data(raw_json: str) -> dict[str, Any]:
    """Strips metadata and GitHub noise to save tokens."""
    data = json.loads(raw_json)
    # Remove "(Fixes #123)" and "(Closes #123)"
    clean_body = re.sub(
        r"\(?(Fixes|Closes) #\d+\)?", "", data.get("body", ""), flags=re.IGNORECASE
    )

    return {
        "number": data.get("number"),
        "title": data.get("title"),
        "body": clean_body,
        "comments": [c.get("body") for c in data.get("comments", []) if c.get("body")],
        "reviews": [r.get("body") for r in data.get("reviews", []) if r.get("body")],
    }


def has_changelog_skip(pr_data: dict[str, Any]) -> bool:
    """Return True if the PR body or any comment contains a CHANGELOG: SKIP directive."""
    text_fields = [pr_data.get("body") or ""] + list(pr_data.get("comments", []))
    return any(_CHANGELOG_SKIP_RE.search(field) for field in text_fields if field)


def load_prompt_template(prompt_file: str) -> dict[str, Any]:
    """Load the prompt template from YAML file."""
    try:
        with open(prompt_file, "r", encoding="utf-8") as f:
            return yaml.safe_load(f)
    except FileNotFoundError:
        print(f"Error: Prompt file '{prompt_file}' not found.")
        sys.exit(1)
    except yaml.YAMLError as err:
        print(f"Error: Failed to parse prompt file: {err}")
        sys.exit(1)


def build_ai_prompt(
    current_announcement: str, pr_data: dict[str, Any], prompt_template: dict[str, Any]
) -> dict[str, Any]:
    """Build the complete AI prompt from template and data."""
    system_prompt = next(
        m["content"] for m in prompt_template["messages"] if m["role"] == "system"
    )

    user_content = (
        "Current working announcement:\n====\n"
        + current_announcement
        + "\n====\n\nNewly merged pull request:\n"
        + json.dumps(pr_data, indent=2)
        + "\n====\n\nUpdate the Release Announcement to include any user-relevant "
        + "changes from this PR.\nReturn the complete updated Markdown document only."
    )
    return {
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_content},
        ],
        "model": prompt_template.get("model", GITHUB_MODELS_DEFAULT_CHAT_MODEL),
        "modelParameters": prompt_template.get("modelParameters", {}),
    }


def call_github_models_api(
    prompt: dict[str, Any],
    chat_model_override: str | None = None,
    embedding_model_override: str | None = None,
) -> str:
    """Call the extracted GitHub backend with common token/config resolution from main."""
    return call_github_models_api_backend(
        prompt=prompt,
        resolve_token=resolve_github_token,
        chat_model_override=chat_model_override,
        embedding_model_override=embedding_model_override,
        backend_config={
            "default_chat_model": GITHUB_MODELS_DEFAULT_CHAT_MODEL,
            "default_embedding_model": GITHUB_MODELS_DEFAULT_EMBEDDING_MODEL,
            "chat_endpoint": GITHUB_MODELS_CHAT_ENDPOINT,
            "embeddings_endpoint": GITHUB_MODELS_EMBEDDINGS_ENDPOINT,
            "token_limit": GITHUB_MODELS_CHAT_TOKEN_LIMIT,
        },
    )

def strip_markdown_fences(text: str) -> str:
    """
    Removes ```markdown ... ``` or ``` ... ``` wrappers
    that LLMs often add to their responses.
    """
    # Remove the opening fence (with or without 'markdown' tag)
    text = re.sub(r"^```(markdown)?\n", "", text, flags=re.IGNORECASE)
    # Remove the closing fence
    text = re.sub(r"\n```$", "", text)
    return text.strip()


class BackendConfig(NamedTuple):
    """LLM backend selection and model name."""

    backend: str = "ollama"
    chat_model: str | None = None
    embedding_model: str | None = None
    dry_run: bool = False


def process_single_pr(
    pr_num: int,
    pr_title: str,
    announcement_file: str,
    prompt_file: str,
    config: BackendConfig = BackendConfig(),
) -> str:
    """
    Process a single PR and optionally update the announcement file.
    Returns one of:
      "committed"  – LLM updated the file with user-facing changes.
      "no_changes" – LLM ran but produced no diff.
      "skipped"    – PR carries a CHANGELOG: SKIP directive; not sent to LLM.
      "dry_run"    – dry-run mode; PR would have been processed (no LLM called).
    """
    print(f"--- Processing PR #{pr_num}: {pr_title} ---")

    # Get PR data
    pr_data = _fetch_pr_data(pr_num, config)
    if pr_data is None:
        return "dry_run"

    # Check for changelog skip
    if has_changelog_skip(pr_data):
        return "skipped"

    if config.dry_run:
        return "dry_run"

    # Load and process announcement
    current_content = _load_announcement_content(announcement_file)
    prompt_template = _load_prompt_template(prompt_file)
    ai_prompt = _build_ai_prompt(current_content, pr_data, prompt_template)

    # Process with LLM backend
    updated_ra = _process_with_llm(ai_prompt, config)

    # Write and check changes
    _write_and_check_announcement(updated_ra, announcement_file)

    return _check_for_changes(announcement_file)


# Get PR data, routing to upstream when running from a fork so that upstream
# PR numbers resolve correctly regardless of where the script runs.
def _fetch_pr_data(pr_num: int, config: BackendConfig) -> dict[str, Any] | None:
    """Fetch PR data from GitHub API."""
    upstream = _get_upstream_repo()
    repo_flag = ["--repo", upstream] if upstream else []
    try:
        update_text = subprocess.check_output(
            ["gh", "pr", "view", str(pr_num), "--json", "number,title,body,comments,reviews"]
            + repo_flag,
            text=True,
            env=get_gh_auth_env(),
        )
        return sanitize_pr_data(update_text)
    except subprocess.CalledProcessError:
        if config.dry_run:
            return None  # PR not accessible; skip metadata fetch in dry-run.
        raise


def _load_announcement_content(announcement_file: str) -> str:
    """Load current announcement content."""
    with open(announcement_file, "r", encoding="utf-8") as f:
        return f.read()


def _load_prompt_template(prompt_file: str) -> dict[str, Any]:
    """Load prompt template."""
    return load_prompt_template(prompt_file)


def _build_ai_prompt(
    current_content: str, pr_data: dict[str, Any], prompt_template: dict[str, Any]
) -> dict[str, Any]:
    """Build AI prompt from template and data."""
    return build_ai_prompt(current_content, pr_data, prompt_template)


def _process_with_llm(ai_prompt: dict[str, Any], config: BackendConfig) -> str:
    """Process with appropriate LLM backend."""
    if config.backend == "ollama":
        if config.embedding_model:
            print(
                "   > Note: --embedding-model/--embed is ignored for --backend ollama "
                "(chat model only)."
            )
        updated_ra = call_ollama_model(ai_prompt, config.chat_model, config.embedding_model)
    elif config.backend in {"github", "actions"}:
        updated_ra = call_github_models_api(
            ai_prompt,
            chat_model_override=config.chat_model,
            embedding_model_override=config.embedding_model,
        )
    else:
        print(f"Error: Unknown backend '{config.backend}'")
        sys.exit(1)
    return strip_markdown_fences(updated_ra)


def _write_and_check_announcement(updated_ra: str, announcement_file: str) -> None:
    """Write updated announcement and check for changes."""
    with open(announcement_file, "w", encoding="utf-8") as f:
        f.write(updated_ra)


def _check_for_changes(announcement_file: str) -> str:
    """Check if announcement file has changes."""
    diff_check = subprocess.run(
        ["git", "diff", "-w", "--exit-code", announcement_file],
        capture_output=True,
        check=False,
    )
    return "no_changes" if diff_check.returncode == 0 else "committed"


def _setup_backend_token(backend: str) -> None:
    """Resolve and pin the GitHub token for the chosen backend."""
    if backend == "github":
        token = resolve_github_token()
        os.environ["GH_TOKEN"] = token
        os.environ["GITHUB_TOKEN"] = token
    elif backend == "actions":
        raw = os.getenv("GITHUB_TOKEN") or os.getenv("GH_TOKEN")
        if not raw:
            print(
                "Error: --backend actions requires GITHUB_TOKEN to be set.\n"
                "Add this to your workflow step:\n"
                "  env:\n"
                "    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}"
            )
            sys.exit(1)
        token = normalize_github_token(raw)
        os.environ["GH_TOKEN"] = token
        os.environ["GITHUB_TOKEN"] = token


def _build_arg_parser() -> argparse.ArgumentParser:
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
        default="ollama",
        choices=["ollama", "github", "actions"],
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
    return parser


def main():
    parser = _build_arg_parser()
    args = parser.parse_args()

    if args.delay_secs < 0:
        print("Error: --delay-secs must be >= 0")
        sys.exit(1)

    # Resolve and pin the token before any subprocess calls that need it.
    _setup_backend_token(args.backend)

    # 1. Resolve Timeframes
    if args.end is None:
        lower_bound_label = f"{args.start}~"
        upper_bound_label = args.start
        start_ts = get_previous_commit_timestamp(args.start)
        end_ts = get_universal_timestamp(args.start)
    else:
        lower_bound_label = args.start
        upper_bound_label = args.end
        start_ts = get_universal_timestamp(args.start)
        end_ts = get_universal_timestamp(args.end)

    print(f"--- Resolving boundaries: {lower_bound_label} -> {upper_bound_label} ---")

    # 2. Fetch and Filter
    todo_prs = get_ordered_pr_list(start_ts, end_ts)

    if not todo_prs:
        print("No new PRs found to process.")
        return

    print(f"--- Found {len(todo_prs)} PRs to merge oldest-to-newest ---")

    # 3. Progressive Merge Loop
    processed = skipped = no_changes = 0
    for pr in todo_prs:
        pr_num = pr["number"]
        pr_title = pr["title"]

        if args.delay_secs > 0 and not args.dry_run:
            print(f"--- Sleeping {args.delay_secs}s before PR #{pr_num} ---")
            time.sleep(args.delay_secs)

        result = process_single_pr(
            pr_num,
            pr_title,
            args.file,
            args.prompt,
            BackendConfig(args.backend, args.chat_model, args.embedding_model, args.dry_run),
        )

        if result == "committed":
            subprocess.run(["git", "add", args.file], check=True)
            subprocess.run(
                ["git", "commit", "-m", f"[bot] RA: Merge #{pr_num}: {pr_title}"],
                check=True,
            )
            print("   Successfully committed.")
            processed += 1
        elif result == "no_changes":
            print(f"   > No user-facing changes for #{pr_num}. Skipping commit.")
            no_changes += 1
        elif result == "skipped":
            print(f"   > Skipping PR #{pr_num} (CHANGELOG: SKIP).")
            skipped += 1
        elif result == "dry_run":
            print(f"   [DRY RUN] Would process PR #{pr_num}.")
            processed += 1

    if args.dry_run:
        print(
            f"\n--- [DRY RUN] Done. {len(todo_prs)} PRs found: "
            f"{processed} would process, {skipped} would skip (CHANGELOG: SKIP). ---"
        )
    else:
        print(
            f"\n--- Done! {len(todo_prs)} PRs: "
            f"{processed} committed, {no_changes} no changes, {skipped} skipped. ---"
        )


if __name__ == "__main__":
    main()
