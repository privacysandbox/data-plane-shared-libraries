# Privacy Sandbox Common

Common repository for shared across Privacy Sandbox teams.

## Getting Started

When you first pull the repo, run 'builders/tools/pre-commit install' to copy the pre-commit hook
file from the build-system/ repo locally.

Populate the submodule directories by running:

```sh
git submodule update --init --remote --force
```

To install a hook that automatically appends commit messages with Gerrit `Change-Id`, run:

```sh
f="$(git rev-parse --git-dir)"/hooks/commit-msg
mkdir -p "$(dirname $f)"
curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg
chmod +x $f
```
