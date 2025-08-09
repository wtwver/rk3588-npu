# Chapter 4 — Merge, Rebase, and Conflict Resolution

## Objectives
- Merge a feature branch into main
- Rebase a branch to clean history
- Resolve merge conflicts

## Steps

1) Update `main` and your feature branch
   ```bash
   git switch main
   git pull --ff-only
   git switch feature/login
   git fetch origin
   ```

2) Rebase feature branch on latest `main` (optional but common)
   ```bash
   git rebase main
   ```
   If conflicts occur, Git pauses for you to resolve them.

3) Resolve conflicts
   - Open the conflicted files and keep the desired changes
   - Mark resolved files as staged:
     ```bash
     git add path/to/conflicted-file
     ```
   - Continue the rebase or merge:
     ```bash
     git rebase --continue   # during rebase
     # or
     git merge --continue    # during merge
     ```

4) Merge into `main`
   ```bash
   git switch main
   git merge --no-ff feature/login -m "Merge feature/login"
   ```

5) If merge introduced conflicts, resolve like step 3 and finish merge

## ASCII Diagram — Merge vs Rebase
```
Merge (preserves branch structure):

main:    A --- B --------- E --- F
                  \              \
feature:           C --- D ------- G (merge commit)

Rebase (linear history):

main:    A --- B --- E --- F
                \
feature:         C' -- D' -- G'   (rebased on top of F)
```

## Guidance
- Use merge to preserve context of collaboration
- Use rebase to keep a tidy linear history (avoid rebasing shared branches)

## Next
Proceed to Chapter 5 to work with remotes and share your code.