# Chapter 2 — Staging and Making Commits

## Objectives
- Create files and see status changes
- Stage and unstage changes
- Create atomic, well-written commits

## Steps

1) Create a file and check status
   ```bash
   echo "Hello Git" > README.md
   git status
   ```
   You should see `README.md` as an untracked file.

2) Stage the change
   ```bash
   git add README.md
   git status
   ```
   Now `README.md` is in the staged changes list.

3) Commit with a meaningful message
   ```bash
   git commit -m "Add README with greeting"
   ```

4) Make another change
   ```bash
   echo "\nMore content." >> README.md
   git diff
   ```
   Review what changed before staging.

5) Stage and commit selectively
   ```bash
   git add -p README.md   # interactively stage hunks
   git commit -m "Expand README with details"
   ```

6) Unstage or discard if needed
   ```bash
   git restore --staged README.md   # unstage
   git restore README.md            # discard unstaged changes
   ```

## ASCII Diagram — File State Transitions
```
Untracked --> Tracked (unmodified) --edit--> Modified --add--> Staged --commit--> Committed
                 ^                         ^                ^
                 |                         |                |
                 +--------- restore -------+---- reset -----+
```

## Commit Message Tips
- Use imperative mood: "Add", "Fix", "Refactor"
- Keep subject line ≤ 72 chars; add details in body if needed
- Commit logically related changes together

## Next
Proceed to Chapter 3 to create and work with branches.