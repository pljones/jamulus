from __future__ import annotations

import os
import subprocess
import sys
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_BASELINE_DIR = REPO_ROOT / ".vscode" / "release-announcement-baseline"


@dataclass(frozen=True)
class MatrixScenario:
    backend: str
    label: str
    start: str
    end: str | None = None
    dry_run: bool = False


SCENARIOS = [
    MatrixScenario("ollama", "pr3429", "pr3429"),
    MatrixScenario("github", "pr3429", "pr3429"),
    MatrixScenario("ollama", "pr3625", "pr3625"),
    MatrixScenario("github", "pr3625", "pr3625"),
    MatrixScenario("ollama", "pr3502", "pr3502"),
    MatrixScenario("github", "pr3502", "pr3502"),
    MatrixScenario("ollama", "tag_r3_12_0beta4_to_r3_12_0beta5", "r3_12_0beta4", "r3_12_0beta5", True),
    MatrixScenario("github", "tag_r3_12_0beta4_to_r3_12_0beta5", "r3_12_0beta4", "r3_12_0beta5", True),
]


def _is_clean_tracked_worktree() -> bool:
    return subprocess.run(
        ["git", "diff", "--quiet", "--exit-code"], cwd=REPO_ROOT, check=False
    ).returncode == 0


def _git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=REPO_ROOT, text=True).strip()


@pytest.mark.integration
@pytest.mark.regression
def test_release_announcement_matrix_regression_against_baseline() -> None:
    if os.getenv("RA_RUN_MATRIX") != "1":
        pytest.skip("Set RA_RUN_MATRIX=1 to run full matrix regression")

    baseline_dir = Path(os.getenv("RA_BASELINE_DIR", str(DEFAULT_BASELINE_DIR)))
    if not baseline_dir.exists():
        pytest.skip(f"Baseline directory does not exist: {baseline_dir}")

    if not _is_clean_tracked_worktree():
        pytest.skip("Tracked files are dirty; matrix regression requires a clean tracked worktree")

    # Wording can drift across runs/models while behavior remains stable.
    # Keep strict byte-identity available behind an explicit opt-in flag.
    strict_output = os.getenv("RA_STRICT_OUTPUT", "0") == "1"

    py_env = os.environ.copy()
    src_path = REPO_ROOT / "tools" / "release_announcement" / "src"
    py_env["PYTHONPATH"] = f"{src_path}:{py_env.get('PYTHONPATH', '')}"

    run_root = (
        REPO_ROOT
        / "build"
        / "release_announcement"
        / "matrix_regression"
        / datetime.now(UTC).strftime("%Y%m%d_%H%M%S")
    )
    run_root.mkdir(parents=True, exist_ok=True)

    base_commit = _git("rev-parse", "HEAD")

    failures: list[str] = []

    try:
        for scenario in SCENARIOS:
            case_name = f"{scenario.backend}__{scenario.label}"
            baseline_case = baseline_dir / case_name
            if not baseline_case.exists():
                failures.append(f"missing baseline case directory: {baseline_case}")
                continue

            out_case = run_root / case_name
            out_case.mkdir(parents=True, exist_ok=True)

            cmd = [
                sys.executable,
                "-m",
                "release_announcement",
                "--delay-secs",
                "0",
                "--backend",
                scenario.backend,
                "--pipeline",
                "legacy",
                "--file",
                "ReleaseAnnouncement.md",
                scenario.start,
            ]
            if scenario.end:
                cmd.append(scenario.end)
            if scenario.dry_run:
                cmd.append("--dry-run")

            with (out_case / "stdout-stderr.log").open("w", encoding="utf-8") as handle:
                proc = subprocess.run(
                    cmd,
                    cwd=REPO_ROOT,
                    env=py_env,
                    check=False,
                    stdout=handle,
                    stderr=subprocess.STDOUT,
                    text=True,
                )

            (out_case / "exit_code.txt").write_text(f"{proc.returncode}\n", encoding="utf-8")
            (out_case / "ReleaseAnnouncement.after.md").write_text(
                (REPO_ROOT / "ReleaseAnnouncement.md").read_text(encoding="utf-8"),
                encoding="utf-8",
            )
            diff_text = subprocess.check_output(
                ["git", "--no-pager", "diff", "--", "ReleaseAnnouncement.md"],
                cwd=REPO_ROOT,
                text=True,
            )
            (out_case / "git-diff.txt").write_text(diff_text, encoding="utf-8")

            baseline_exit = (baseline_case / "exit_code.txt").read_text(encoding="utf-8").strip()
            if str(proc.returncode) != baseline_exit:
                failures.append(
                    f"{case_name}: exit code mismatch (new={proc.returncode}, baseline={baseline_exit})"
                )

            baseline_diff = (baseline_case / "git-diff.txt").read_text(encoding="utf-8")
            if diff_text != baseline_diff:
                failures.append(f"{case_name}: git-diff mismatch")

            # Always enforce behavioral parity for no-change vs changed outputs.
            baseline_changed = bool(baseline_diff.strip())
            new_changed = bool(diff_text.strip())
            if baseline_changed != new_changed:
                failures.append(
                    f"{case_name}: changed/no-change mismatch "
                    f"(new_changed={new_changed}, baseline_changed={baseline_changed})"
                )

            if strict_output:
                baseline_after = (baseline_case / "ReleaseAnnouncement.after.md").read_text(
                    encoding="utf-8"
                )
                new_after = (out_case / "ReleaseAnnouncement.after.md").read_text(encoding="utf-8")
                if baseline_after != new_after:
                    failures.append(f"{case_name}: ReleaseAnnouncement.after.md mismatch")

            subprocess.run(["git", "reset", "--hard", base_commit], cwd=REPO_ROOT, check=True)
    finally:
        subprocess.run(["git", "reset", "--hard", base_commit], cwd=REPO_ROOT, check=False)

    assert not failures, "\n".join(failures)
