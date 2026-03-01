---
name: review-pr
description: Handle automated PR review feedback and merge when ready
argument-hint: "[pr-number]"
disable-model-invocation: false
---

Check GitHub Actions status, review feedback from automated reviewers (Code Rabbit,
Claude), implement or respond to feedback, and merge the PR when all feedback is
resolved.

The user provides:

- **PR number**: the pull request number to review (optional - infers from current branch)

## Workflow

### 1. Check GitHub Actions status

Use `gh pr checks <pr-number>` to see if automated reviews are complete.

**Important:** This command returns exit code 8 if any checks are pending, and
outputs them to stderr. Handle both stdout and stderr to capture all check statuses.

Output format: `check-name  status  duration  url`

**If any checks are still running (pending):**

- Report which checks are pending
- Exit with message: "GitHub Actions still running. Run this skill again when checks complete."
- DO NOT sleep or wait — let the user run the skill again later

**If checks failed:**

- Report which checks failed
- Show the failure URL from the output
- **If markdown linting failed**, offer to run it locally and fix issues (see step 1.5)
- Otherwise exit and ask user to investigate

**If all checks passed:**

- Proceed to step 2 (fetch review comments)

### 1.5. Fix markdown linting failures (if needed)

If the "Markdown Lint" check failed:

1. **Run locally to see errors:**

   ```bash
   npx markdownlint-cli2 "**/*.md"
   ```

2. **Attempt auto-fix:**

   ```bash
   npx markdownlint-cli2 --fix "**/*.md"
   ```

3. **Manually fix remaining errors** (especially MD040 - missing language tags)

4. **Verify all errors resolved:**

   ```bash
   npx markdownlint-cli2 "**/*.md"
   ```

5. **Commit and push fixes:**

   ```bash
   git add <fixed-files>
   git commit -m "Fix markdown linting errors

   Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
   git push
   ```

6. **Exit and wait for checks to re-run** — user should invoke `/review-pr` again after checks pass

### 1.7. Check for paused CodeRabbit reviews

After checks pass, check whether CodeRabbit has paused its reviews (this happens
when many commits are pushed in rapid succession). Fetch the PR's general comments
and look at the **first** comment from `coderabbitai[bot]`:

```bash
gh api repos/{owner}/{repo}/issues/{pr-number}/comments \
  | jq '[.[] | select(.user.login == "coderabbitai[bot]")][0].body' -r \
  | head -20
```

**If the comment body contains "Reviews paused":**

- Report to the user: "CodeRabbit reviews are paused due to rapid commits."
- Post **two** comments — one to do a full review, one to resume future reviews:

  ```bash
  gh pr comment <pr-number> --body "@coderabbitai full review"
  gh pr comment <pr-number> --body "@coderabbitai resume reviews"
  ```

- Exit with message: "Requested CodeRabbit full review and resumed reviews.
  Run this skill again when the review completes."
- DO NOT proceed to fetch review comments — CodeRabbit hasn't reviewed yet

**If the comment body contains "Currently processing new changes":**

- Report to the user: "CodeRabbit is currently processing new changes."
- Exit with message: "CodeRabbit is still reviewing. Run this skill again when
  the review completes."
- DO NOT proceed to fetch review comments — CodeRabbit hasn't finished yet

**If reviews are not paused and not processing:**

- Proceed to step 2

### 2. Fetch review comments

**Important:** Inline review comments (the "tasks" users see in the GitHub UI)
are NOT returned by `gh pr view --json comments`. You must use the API directly:

```bash
gh api repos/{owner}/{repo}/pulls/{pr-number}/comments
```

This returns an array of review comment objects with:

- `path` — file path
- `line` / `start_line` — line numbers
- `body` — comment body (markdown, may include severity, suggestions)
- `html_url` — link to view the conversation
- `user.login` — reviewer (e.g., "coderabbitai[bot]", "claude-code[bot]")

**CodeRabbit comment format:**

- Body starts with severity: `_⚠️ Potential issue_ | _🟠 Major_`, `_🟡 Minor_`, or other markers
- Includes suggested fixes in `<details><summary>Suggested fix</summary>` blocks
- May include committable suggestions between `<!-- suggestion_start -->` markers
- **Important:** CodeRabbit may leave nitpick/style comments alongside major issues—fetch ALL comments, not just the first few

Parse and categorize feedback by:

- Severity (🟠 Major, 🟡 Minor, nitpick/style)
- File and line number
- Reviewer (CodeRabbit, Claude, human)

**Present ALL comments to the user** sorted by severity (Major → Minor → Nitpick) so security/critical issues are addressed first.

### 3. Present feedback summary

**Sort comments by severity:** Major → Minor → Nitpick/Style, so critical issues (especially security) are addressed first.

Show the user:

```text
## PR Review Feedback Summary

### Pending conversations: X

**Major issues (🟠):**
1. **[CodeRabbit]** file.c:123
   > Security: Anyone can trigger this workflow
   [View](https://github.com/...)

**Minor issues (🟡):**
2. **[CodeRabbit]** README.md:45
   > Style: Add language tag to code block
   [View](https://github.com/...)

**Nitpicks/Style:**
3. **[Claude]** main.c:100
   > Consider adding a comment here
   [View](https://github.com/...)

### Resolved conversations: Y
```

If no pending conversations, skip to step 6.

### 4. Handle each piece of feedback

For each pending conversation, ask the user:

```text
Feedback from [Reviewer] on file.c:123:
> [feedback text]

How should I handle this?
1. Implement the suggestion (I'll make the code changes)
2. Respond and resolve (you explain why it's not applicable)
3. Skip for now (handle later)
```

Use AskUserQuestion with options:

- "Implement" - Make the code changes (do NOT commit/push yet — batch all changes)
- "Respond and resolve" - Ask user for response text, post comment, resolve conversation
- "Skip" - Move to next feedback

**Important: Batch all changes into a single commit+push.** Do NOT commit and
push after each individual feedback item. Process ALL feedback first, make all
code changes, then do one commit and one push at the end. Pushing multiple
times in quick succession causes CodeRabbit to pause reviews, which creates
a frustrating loop of re-triggering and waiting.

### 5. After implementing changes

After processing ALL feedback items, if any code changes were made:

- Stage all changed files
- Create a single commit: "Address PR feedback: [summary of all changes]"
- Push once to the PR branch
- Re-run checks with `gh pr checks` (don't wait, just show status)
- Exit with: "Changes pushed. Run this skill again after checks complete."

**On next run after CodeRabbit re-reviews:**

- Check if comments are auto-resolved (CodeRabbit detects implemented fixes)
- If still unresolved after implementing the suggested fix:
  - Mention CodeRabbit in a comment: "@coderabbitai I've implemented your suggestion in commit ABC123. Can you verify and resolve this conversation?"
  - OR if CodeRabbit's check is complete (not currently running): "@coderabbitai Why hasn't this been auto-resolved? Is there an issue with my implementation?"
  - If CodeRabbit is still checking: wait for it to finish before asking

### 6. Check if ready to merge

Check two things: the PR-level review decision AND individual reviews.

```bash
# PR-level decision (may lag behind individual reviews)
gh pr view <pr-number> --json reviewDecision,statusCheckRollup

# Individual reviews — check the LATEST review from each reviewer
gh api repos/{owner}/{repo}/pulls/{pr-number}/reviews \
  | jq '[.[] | {user: .user.login, state: .state, date: .submitted_at}]
        | group_by(.user)
        | map(sort_by(.date) | last)'
```

**Important:** `reviewDecision` can show `CHANGES_REQUESTED` even after a
reviewer has submitted a newer `APPROVED` review. This happens because GitHub
tracks the *earliest unresolved* review, not the latest. Always check the
latest review from each reviewer — if their most recent review is `APPROVED`,
the PR is approved regardless of what `reviewDecision` says.

Verify:

- All status checks pass
- The latest review from each reviewer is `APPROVED` (or `COMMENTED` — only
  `CHANGES_REQUESTED` blocks)
- No unresolved conversations that need action

**If not ready:**

- Report what's blocking (conversations, checks, approval)
- Exit with instructions

**If ready:**

- Proceed to step 7

### 7. Merge the PR

Ask user for confirmation:

```text
✅ All feedback resolved, all checks passing, PR approved.

Ready to merge PR #X: "Title"
Branch: lesson-03-uniforms-and-motion → main

Merge method:
1. Squash and merge (recommended for lessons)
2. Merge commit
3. Rebase and merge
```

After user confirmation:

```bash
gh pr merge <pr-number> --squash --delete-branch
```

Show success message with merged commit SHA.

## GitHub CLI commands reference

```bash
# Check PR status (returns exit code 8 if checks are pending)
gh pr checks <pr-number>

# Get inline review comments (the "tasks" shown in GitHub UI)
# IMPORTANT: This is the correct way to fetch review feedback
gh api repos/{owner}/{repo}/pulls/{pr-number}/comments

# View PR details (does NOT include inline review comments)
gh pr view <pr-number> --json reviewDecision,statusCheckRollup

# View specific workflow run
gh run view <run-id>

# Reply to a specific review comment thread (CORRECT - creates nested reply)
# Use this to respond to CodeRabbit/Claude feedback on specific lines
# IMPORTANT: Must include PR number in path - repos/{owner}/{repo}/pulls/{pr-number}/comments/{comment-id}/replies
# Example: gh api repos/RosyGameStudio/forge-gpu/pulls/1/comments/2807683534/replies
gh api repos/{owner}/{repo}/pulls/{pr-number}/comments/{comment-id}/replies \
  -f body="response text"

# Post a general PR comment (AVOID - doesn't thread properly with review feedback)
# This creates a standalone comment, not a reply to a review thread
gh pr comment <pr-number> --body "response text"

# Ask CodeRabbit to verify/resolve after implementing a fix (reply to the comment thread)
# IMPORTANT: Must include PR number in the path (not just comment ID)
gh api repos/{owner}/{repo}/pulls/{pr-number}/comments/{comment-id}/replies \
  -f body="@coderabbitai I've implemented your suggestion in commit ABC123. Can you verify and resolve this conversation?"

# Resolve a review thread manually
# Note: GitHub CLI doesn't directly support this - may need gh api or manual UI resolution
# Usually let CodeRabbit auto-resolve when it re-reviews

# Merge PR
gh pr merge <pr-number> --squash --delete-branch
```

## Implementation notes

- **Never sleep or wait** — if actions are running, exit and tell user to re-run
- Can be run multiple times — each run picks up where it left off
- Commit messages for feedback changes should reference the PR number
- If Code Rabbit or Claude give conflicting advice, prioritize project conventions from CLAUDE.md
- Always show the user what will be changed before making code modifications
- Use the same commit message format as publish-lesson (with Co-Authored-By line)
- **The "X out of Y pending tasks"** shown in GitHub UI are the unresolved review comment threads—fetch them via `gh api repos/{owner}/{repo}/pulls/{pr-number}/comments`
- **CodeRabbit paused/processing reviews:** When many commits are pushed quickly, CodeRabbit pauses reviews and adds "Reviews paused" to its initial PR comment. It may also show "Currently processing new changes" while actively re-reviewing. Always check for both statuses before fetching review comments — if paused, post both `@coderabbitai full review` AND `@coderabbitai resume reviews` (the second is needed to unpause future reviews); if processing, wait for it to finish. In either case, exit and tell the user to re-run the skill later.
- **CodeRabbit auto-resolution:** CodeRabbit automatically resolves conversations when it detects the suggested fix was implemented in a new commit. Let it auto-resolve; only ask it to resolve manually if it doesn't detect your fix after re-review.
- **Reply to comment threads:** Always use `gh api repos/{owner}/{repo}/pulls/{pr-number}/comments/{comment-id}/replies` to reply to specific review comments. This keeps conversations threaded. Do NOT use `gh pr comment` for replies—it creates unthreaded general comments.
  - **Common mistake:** Forgetting to include the PR number in the path (e.g., `repos/{owner}/{repo}/pulls/comments/{id}/replies` won't work—must be `pulls/{pr-number}/comments/{id}/replies`)

## Error handling

- If `gh` CLI is not installed or not authenticated, provide clear setup instructions
- If PR doesn't exist, report error
- If branch has conflicts, report and exit (user must resolve manually)
- If merge is blocked by branch protection rules, report the blocking rules

## Example interaction

```text
Running review-pr for PR #1...

✓ All GitHub Actions checks passed
✓ Found 2 pending conversations

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Feedback 1/2 - Code Rabbit on main.c:382

> Consider adding error handling after SDL_calloc.
> If allocation fails, the function continues with a
> NULL pointer.

How should I handle this?
[User selects: Implement]

Making changes to main.c...
✓ Added NULL check after SDL_calloc
✓ Changes committed and pushed

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Feedback 2/2 - Claude on README.md:45

> Consider mentioning that aspect ratio correction
> prevents stretching on non-square windows.

How should I handle this?
[User selects: Respond and resolve]

Response: "Good catch! This is already mentioned in
the Key Concepts section at line 82. I'll resolve
this as a duplicate."

✓ Comment posted and conversation resolved

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Changes pushed. Waiting for checks...

Run this skill again after checks complete.
```

## Future enhancements

- Auto-detect if feedback is trivial (typos, formatting) and batch implement
- Learn from resolved conversations to avoid similar issues in future lessons
- Generate summary of what was changed for the PR merge commit message
