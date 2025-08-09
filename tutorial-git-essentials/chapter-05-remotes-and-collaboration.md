# Chapter 5 — Remotes and Collaboration

## Objectives
- Add and inspect remotes
- Push, pull, and fetch
- Open pull requests and review changes

## Steps

1) Inspect remotes
   ```bash
   git remote -v
   ```

2) Add a remote
   ```bash
   git remote add origin git@github.com:your-org/your-repo.git
   ```

3) Push the default branch
   ```bash
   git push -u origin main
   ```

4) Push a feature branch
   ```bash
   git push -u origin feature/login
   ```

5) Pull updates into your local branch
   ```bash
   git pull --ff-only
   ```

6) Open a Pull Request (PR)
   - Go to your repository hosting service
   - Open a PR from `feature/login` into `main`
   - Request reviews, respond to feedback, and update commits as needed

7) Fetch and review others' work without merging
   ```bash
   git fetch origin
   git log --oneline --graph --decorate --all | head -n 30
   ```

## ASCII Diagram — Typical Team Workflow
```
Developer Local Repo                     Remote (Shared)

   [feature/login] -- push -->  origin/feature/login  -- PR -->  main
          |                                                          |
          \-- rebase/merge <--  origin/main   <-- pull/fetch --------/
```

## Tips
- Prefer `--ff-only` pulls to avoid unintended merge commits
- Protect `main` with required reviews and checks
- Delete feature branches after merging to keep the repo tidy

## Congratulations
You have completed Git Essentials! Continue by exploring advanced topics like submodules, bisect, cherry-pick, and CI/CD integration.