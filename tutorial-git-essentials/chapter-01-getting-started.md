# Chapter 1 — Getting Started with Git

## Objectives
- Install Git and verify version
- Configure your identity
- Initialize or clone a repository
- Understand the basic working areas

## Prerequisites
- Terminal access
- A GitHub, GitLab, or similar remote account (optional for now)

## Steps

1) Verify Git is installed
   - macOS: `brew install git`
   - Linux: `sudo apt-get update && sudo apt-get install -y git`
   - Windows: Install Git for Windows (`https://git-scm.com/download/win`)
   - Confirm installation:
     ```bash
     git --version
     ```

2) Set your identity (first time only)
   ```bash
   git config --global user.name "Your Name"
   git config --global user.email "you@example.com"
   git config --global init.defaultBranch main
   ```

3) Create a new repo locally
   ```bash
   mkdir hello-git && cd hello-git
   git init
   ```

4) Or clone an existing repo
   ```bash
   git clone git@github.com:your-org/your-repo.git
   cd your-repo
   ```

5) Explore the repo status
   ```bash
   git status
   ```

## ASCII Diagram — Git Areas Overview
```
+-------------------+      add/stage      +------------------+       commit       +------------------+
|   Working Tree    |  ---------------->  |   Staging Area   |  -------------->  |   Local Repo     |
|  (your files)     |                     |  (index/cache)   |                   |   (.git)         |
+-------------------+                     +------------------+                   +------------------+
         ^                                                                                 |
         |                                                                                 | push/pull
         +---------------------------------------------------------------------------------v
                                                                            +------------------+
                                                                            |   Remote Repo    |
                                                                            | (e.g., GitHub)   |
                                                                            +------------------+
```

## Quick Checks
- `git --version` prints a version number
- `git config --global -l` shows your name and email
- `git init` created a `.git` directory or `git clone` made a new folder

## Next
Proceed to Chapter 2 to create, stage, and commit changes.