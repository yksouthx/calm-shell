use std::process::Command;

pub struct GitStatus {
    pub branch: String,
    pub clean: bool,
}

/// Returns None if the current directory isn't inside a git repo.
///
/// Deliberately a single subprocess spawn (`git status --porcelain --branch`)
/// rather than three separate `git` calls (is-inside-work-tree, rev-parse
/// HEAD, status --porcelain): this runs on every prompt render, so on
/// modest hardware three fork+execs per keystroke-to-prompt cycle is real,
/// perceptible lag. `--branch` prepends a `## ...` header line to the same
/// porcelain output, so one call gives us both branch and dirty state.
pub fn status() -> Option<GitStatus> {
    let output = Command::new("git")
        .args(["status", "--porcelain", "--branch"])
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let mut lines = stdout.lines();
    let branch = parse_branch_header(lines.next().unwrap_or(""));
    let clean = lines.next().is_none();

    Some(GitStatus { branch, clean })
}

/// Parses git's `## <info>` porcelain branch header, covering the shapes
/// git actually emits: a normal branch (optionally with `...upstream` and
/// an `[ahead N]`/`[behind N]` suffix), a brand new repo with no commits
/// yet, and detached HEAD.
fn parse_branch_header(line: &str) -> String {
    let rest = line.strip_prefix("## ").unwrap_or(line).trim();

    if let Some(stripped) = rest.strip_prefix("No commits yet on ") {
        return first_field(stripped);
    }
    if rest.starts_with("HEAD (no branch)") {
        return "detached".to_string();
    }

    first_field(rest)
}

/// Takes everything before the first `...` (upstream separator) or
/// whitespace (ahead/behind suffix), whichever comes first.
fn first_field(s: &str) -> String {
    let s = s.split("...").next().unwrap_or(s);
    let s = s.split_whitespace().next().unwrap_or(s);
    if s.is_empty() {
        "unknown".to_string()
    } else {
        s.to_string()
    }
}
