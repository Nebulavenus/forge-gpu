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

## Important: local and PR reviews differ

PR reviews and CLI reviews will differ, even if run on the same code. CLI
reviews optimize for immediate feedback during active development, while PR
reviews provide comprehensive team collaboration context and broader
repository analysis.

**A clean local review does not guarantee a clean PR review.** Use the local
review as a pre-filter to catch obvious issues (unchecked returns, doc/code
mismatches, style problems) before pushing — but always request a full
`@coderabbitai review` on the PR afterward.

## After a clean local review

A clean local review means the obvious issues are handled. Push and request
a full PR review:

1. **Push** the commits
2. **Request a PR review** — comment `@coderabbitai review` on the PR
3. **Wait for the PR review** — it may find additional issues the local
   review missed (repo-wide patterns, learnings from past PRs, cross-file
   architectural concerns)

## What it cannot do

- Interact with existing PR feedback threads
- Resolve or dismiss CodeRabbit conversations on a PR
- Update the GitHub approval state
- Access CodeRabbit's learnings from past PR interactions
- Perform the broader repository analysis that PR reviews include
