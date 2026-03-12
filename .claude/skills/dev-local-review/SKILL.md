---
name: dev-local-review
description: Run a local CodeRabbit review before pushing to catch issues early
argument-hint: "[--base branch]"
disable-model-invocation: false
---

Run a local CodeRabbit code review via the Claude Code plugin. Use this
as a pre-push gate to catch issues before they hit the PR and burn a
review round.

**This is not a substitute for the GitHub PR review.** The local review
runs the same analysis but cannot interact with PR feedback threads,
resolve conversations, or update approval state. It is a fast, local
check only.

## Prerequisites

### 1. Install the CodeRabbit CLI

```bash
curl -fsSL https://cli.coderabbit.ai/install.sh | sh
```

### 2. Authenticate

Interactive OAuth (opens browser):

```bash
coderabbit auth login
```

Verify:

```bash
coderabbit auth status
```

### 3. Install the Claude Code plugin

In Claude Code:

```text
/plugin install coderabbit
```

### 4. Verify

```text
/coderabbit:review --base main
```

If this returns findings or "No findings", the setup is complete.

## Usage

Run a review against main (default):

```text
/coderabbit:review --base main
```

**Important:** Always pass the project's `.coderabbit.yaml` config so the
local review applies the same rules as the GitHub bot:

```text
/coderabbit:review --base main -c .coderabbit.yaml
```

Review only committed or uncommitted changes:

```text
/coderabbit:review committed
/coderabbit:review uncommitted
```

Compare against a different base branch:

```text
/coderabbit:review --base develop
```

## When to use

- Before pushing fixes during `/dev-review-pr` — catch new issues locally
  instead of waiting for the GitHub bot
- Before creating a PR with `/dev-create-pr` — clean up obvious issues
  before the first review
- Any time you want a quick code quality check without pushing

## After a clean local review

When the local review returns **no findings**, the code matches what
CodeRabbit would see on GitHub. In this case:

1. **Push** the commits
2. **Request a resolve** — comment `@coderabbitai resolve` on the PR to
   dismiss the old feedback threads
3. **Do NOT request a new review** — the local review already confirmed the
   code is clean, so `@coderabbitai review` would just repeat that work
4. **Wait for merge** — the resolved state plus passing CI should be
   sufficient for merge

This only applies when the local review is clean and no additional code
changes were made after the review. If you changed code after the local
review, run the local review again before pushing.

## What it cannot do

- Interact with existing PR feedback threads
- Resolve or dismiss CodeRabbit conversations on a PR
- Update the GitHub approval state
- Replace the `@coderabbitai review` request after pushing
