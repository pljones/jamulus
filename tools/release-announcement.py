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
import subprocess
import json
import argparse
import sys
import re
import os
import time
import urllib.request
import urllib.error
from datetime import datetime
from typing import Dict, Any, NamedTuple
import yaml

# Try to import ollama, but don't fail if it's not available
try:
    import ollama  # pylint:disable=E0401
    OLLAMA_AVAILABLE = True
except ImportError:
    OLLAMA_AVAILABLE = False


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
            ["gh", "auth", "token"],
            text=True,
            stderr=subprocess.STDOUT
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


def get_gh_auth_env() -> Dict[str, str]:
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
            ["git", "show", "-s", "--format=%cI", target],
            text=True
        ).strip()
    except subprocess.CalledProcessError:
        print(f"Error: Could not resolve '{target}' as a Git object.")
        sys.exit(1)


def resolve_identifier_to_git_target(identifier: str) -> str:
    """Resolve a PR/tag/sha/branch identifier to a git object name or SHA."""
    if identifier.lower().startswith("pr"):
        pr_id = identifier[2:]
        try:
            print(f"   > Resolving PR #{pr_id} to its merge commit...")
            cmd = f"gh pr view {pr_id} --json mergeCommit -q .mergeCommit.oid"
            target = subprocess.check_output(
                cmd, shell=True, text=True, env=get_gh_auth_env()
            ).strip()

            if not target or target == "null":
                print(f"Error: PR #{pr_id} has no merge commit (is it merged?)")
                sys.exit(1)
            return target
        except subprocess.CalledProcessError:
            print(f"Error: Failed to fetch PR #{pr_id} from GitHub.")
            sys.exit(1)

    return identifier


def get_previous_commit_timestamp(identifier: str) -> str:
    """Get the timestamp of the commit immediately before the resolved target."""
    target = resolve_identifier_to_git_target(identifier)
    previous_target = f"{target}~"

    try:
        return subprocess.check_output(
            ["git", "show", "-s", "--format=%cI", previous_target],
            text=True
        ).strip()
    except subprocess.CalledProcessError:
        print(f"Error: Could not resolve previous commit for '{identifier}'.")
        sys.exit(1)


def parse_iso_datetime(value: str) -> datetime:
    """Parse ISO8601 timestamps from Git/GitHub into timezone-aware datetime."""
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def get_ordered_pr_list(start_iso: str, end_iso: str) -> list:
    """
    Fetch PRs merged in the range (start, end], ordered oldest to newest.
    Lower bound is exclusive; upper bound is inclusive.
    """
    cmd = (
        'gh pr list --state merged '
        f'--search "merged:<={end_iso}" '
        '--limit 1000 '
        '--json number,title,mergedAt'
    )
    prs = json.loads(
        subprocess.check_output(cmd, shell=True, text=True, env=get_gh_auth_env())
    )

    start_dt = parse_iso_datetime(start_iso)
    end_dt = parse_iso_datetime(end_iso)

    filtered_prs = [
        pr for pr in prs
        if start_dt < parse_iso_datetime(pr['mergedAt']) <= end_dt
    ]

    return sorted(filtered_prs, key=lambda x: x['mergedAt'])


def sanitize_pr_data(raw_json: str) -> Dict[str, Any]:
    """Strips metadata and GitHub noise to save tokens."""
    data = json.loads(raw_json)
    # Remove "(Fixes #123)" and "(Closes #123)"
    clean_body = re.sub(
        r'\(?(Fixes|Closes) #\d+\)?', '', data.get("body", ""), flags=re.IGNORECASE
    )

    return {
        "number": data.get("number"),
        "title": data.get("title"),
        "body": clean_body,
        "comments": [c.get("body") for c in data.get("comments", []) if c.get("body")],
        "reviews": [r.get("body") for r in data.get("reviews", []) if r.get("body")]
    }


def load_prompt_template(prompt_file: str) -> Dict[str, Any]:
    """Load the prompt template from YAML file."""
    try:
        with open(prompt_file, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f)
    except FileNotFoundError:
        print(f"Error: Prompt file '{prompt_file}' not found.")
        sys.exit(1)
    except yaml.YAMLError as e:
        print(f"Error: Failed to parse prompt file: {e}")
        sys.exit(1)


def build_ai_prompt(current_announcement: str, pr_data: Dict[str, Any],
                   prompt_template: Dict[str, Any]) -> Dict[str, Any]:
    """Build the complete AI prompt from template and data."""
    system_prompt = next(
        m['content'] for m in prompt_template['messages'] if m['role'] == 'system'
    )

    user_content = (
        'Current working announcement:\n====\n'
        + current_announcement
        + '\n====\n\nNewly merged pull request:\n'
        + json.dumps(pr_data, indent=2)
        + '\n====\n\nUpdate the Release Announcement to include any user-relevant '
        + 'changes from this PR.\nReturn the complete updated Markdown document only.'
    )

    return {
        'messages': [
            {'role': 'system', 'content': system_prompt},
            {'role': 'user', 'content': user_content}
        ],
        'model': prompt_template.get('model', 'openai/gpt-4o'),
        'modelParameters': prompt_template.get('modelParameters', {})
    }


def call_ollama_model(prompt: Dict[str, Any], model: str) -> str:
    """Call Ollama API for local model inference."""
    if not OLLAMA_AVAILABLE:
        print("Error: Ollama is not available. Please install it or use a different backend.")
        sys.exit(1)

    response = ollama.chat(model=model, messages=prompt['messages'])
    return response['message']['content'].strip()


def call_github_models_api(prompt: Dict[str, Any]) -> str:
    """
    Call GitHub Models API directly.
    """
    token = resolve_github_token()

    endpoint = os.getenv(
        "MODELS_ENDPOINT",
        "https://models.github.ai/inference/chat/completions"
    )

    model_parameters = prompt.get("modelParameters", {})
    payload = {
        "model": prompt.get("model", "openai/gpt-4o"),
        "messages": prompt["messages"]
    }

    # Map prompt-template key to API key expected by GitHub Models endpoint.
    if "maxCompletionTokens" in model_parameters:
        payload["max_completion_tokens"] = model_parameters["maxCompletionTokens"]
    if "temperature" in model_parameters:
        payload["temperature"] = model_parameters["temperature"]

    req = urllib.request.Request(
        endpoint,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
        },
        method="POST"
    )

    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            body = resp.read().decode("utf-8")
    except urllib.error.HTTPError as err:
        error_body = err.read().decode("utf-8", errors="replace")
        print(f"Error: GitHub Models API returned HTTP {err.code}: {error_body}")
        sys.exit(1)
    except urllib.error.URLError as err:
        print(f"Error: Failed to call GitHub Models API: {err}")
        sys.exit(1)

    try:
        data = json.loads(body)
        content = data.get("choices", [{}])[0].get("message", {}).get("content", "")
    except (json.JSONDecodeError, IndexError, TypeError) as err:
        print(f"Error: Invalid response from GitHub Models API: {err}")
        sys.exit(1)

    if not content:
        print("Error: GitHub Models API returned an empty response.")
        sys.exit(1)

    return content.strip()


def strip_markdown_fences(text: str) -> str:
    """
    Removes ```markdown ... ``` or ``` ... ``` wrappers
    that LLMs often add to their responses.
    """
    # Remove the opening fence (with or without 'markdown' tag)
    text = re.sub(r'^```(markdown)?\n', '', text, flags=re.IGNORECASE)
    # Remove the closing fence
    text = re.sub(r'\n```$', '', text)
    return text.strip()


class BackendConfig(NamedTuple):
    """LLM backend selection and model name."""
    backend: str = "ollama"
    model: str = "mistral-large-3:675b-cloud"


def process_single_pr(pr_num: int, pr_title: str, announcement_file: str,
                     prompt_file: str,
                     config: BackendConfig = BackendConfig()) -> bool:
    """
    Process a single PR and update the announcement file.
    Returns True if changes were made, False otherwise.
    """
    # Load current announcement
    with open(announcement_file, 'r', encoding='utf-8') as f:
        current_content = f.read()

    print(f"--- Processing PR #{pr_num}: {pr_title} ---")

    # Get PR data
    update_text = subprocess.check_output(
        f'gh pr view {pr_num} --json number,title,body,comments,reviews',
        shell=True, text=True, env=get_gh_auth_env()
    )
    pr_data = sanitize_pr_data(update_text)

    # Load prompt template
    prompt_template = load_prompt_template(prompt_file)

    # Build AI prompt
    ai_prompt = build_ai_prompt(current_content, pr_data, prompt_template)

    # Call appropriate LLM backend
    if config.backend == "ollama":
        updated_ra = call_ollama_model(ai_prompt, config.model)
    elif config.backend in ("github", "actions"):
        updated_ra = call_github_models_api(ai_prompt)
    else:
        print(f"Error: Unknown backend '{config.backend}'")
        sys.exit(1)

    # Clean the output
    updated_ra = strip_markdown_fences(updated_ra)

    # Write updated announcement
    with open(announcement_file, 'w', encoding='utf-8') as f:
        f.write(updated_ra)

    # Check if it actually changed
    diff_check = subprocess.run(
        ["git", "diff", "-w", "--exit-code", announcement_file],
        capture_output=True, check=False
    )

    if diff_check.returncode == 0:
        print(f"   > Result: No user-facing changes for #{pr_num}. Skipping commit.")
        return False

    return True


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


def main():
    default_delay_secs = int(os.getenv("DELAY_SECS", "0"))
    default_model = os.getenv("OLLAMA_MODEL", "mistral-large-3:675b-cloud")

    parser = argparse.ArgumentParser(
        description="Progressive Release Announcement Generator"
    )
    parser.add_argument("start",
                       help="Starting boundary, or upper bound if end is omitted"
                            " (e.g. pr3409 or v3.11.0)")
    parser.add_argument(
        "end",
        nargs="?",
        help="Ending boundary (e.g. pr3500 or HEAD). Defaults to start if omitted."
    )
    parser.add_argument("--file", required=True, help="Markdown file to update")
    parser.add_argument("--prompt",
                       default=".github/prompts/release-announcement.prompt.yml",
                       help="YAML prompt template file")
    parser.add_argument("--model", default=default_model,
                       help="Model to use (for Ollama backend)")
    parser.add_argument("--backend", default="ollama",
                       choices=["ollama", "github", "actions"],
                       help="LLM backend to use (actions: for GitHub Actions workflows)")
    parser.add_argument("--delay-secs", type=int, default=default_delay_secs,
                       help="Seconds to sleep before each PR is processed")
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
    for pr in todo_prs:
        pr_num = pr['number']
        pr_title = pr['title']

        if args.delay_secs > 0:
            print(f"--- Sleeping {args.delay_secs}s before PR #{pr_num} ---")
            time.sleep(args.delay_secs)

        if process_single_pr(pr_num, pr_title, args.file, args.prompt,
                      BackendConfig(args.backend, args.model)):
            # Commit changes
            subprocess.run(["git", "add", args.file], check=True)
            subprocess.run(
                ["git", "commit", "-m", f"[bot] RA: Merge #{pr_num}: {pr_title}"],
                check=True
            )
            print("   Successfully committed.")
        else:
            print(f"   > Result: No user-facing changes for #{pr_num}. Skipping commit.")

    print(f"\n--- Done! Processed {len(todo_prs)} PRs. ---")


if __name__ == "__main__":
    main()
