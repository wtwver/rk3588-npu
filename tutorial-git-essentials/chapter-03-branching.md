# Chapter 3 — Branching and Navigation

## Objectives
- Create and switch branches
- List, rename, and delete branches
- Understand HEAD and branch pointers

## Steps

1) Create and switch to a new branch
   ```bash
   git switch -c feature/login
   # or: git checkout -b feature/login
   ```

2) Make changes and commit
   ```bash
   echo "Login WIP" > login.txt
   git add login.txt
   git commit -m "Start login feature"
   ```

3) List branches and see the current one
   ```bash
   git branch
   ```

4) Switch back and forth
   ```bash
   git switch main
   git switch feature/login
   ```

5) Rename and delete branches
   ```bash
   git branch -m feature/auth-login   # rename current branch
   git branch -d old-branch           # delete fully merged branch
   git branch -D wip/throwaway        # force delete
   ```

## ASCII Diagram — Branch Pointers and HEAD
```
            commit A ---- commit B ---- commit C (main)
               \                          ^
                \                         |
                 \--- commit D --- commit E (feature/login)

HEAD -> points to the current branch name, which points to a commit.
When you commit, the branch pointer moves forward to the new commit.
```

## Tips
- Keep branch names descriptive (e.g., `feature/`, `bugfix/`, `hotfix/` prefixes)
- One branch per feature; merge via Pull Request for review

## Next
Proceed to Chapter 4 to merge branches and handle conflicts.