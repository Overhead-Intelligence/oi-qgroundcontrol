# Contributing

This is Overhead Intelligence's fork of QGroundControl. The workflow (branches,
PRs, releases, how CI builds the installer for you) is described in the
repository [README](../README.md#working-on-this-repo) and, for Claude Code
sessions, in [CLAUDE.md](../CLAUDE.md).

Short version: branch off `development`, open a PR to `development` with a
`CHANGELOG.md` entry, wait for the CI installer, a human merges.

Changes that belong in QGroundControl itself should go to
[mavlink/qgroundcontrol](https://github.com/mavlink/qgroundcontrol) and come
back here through an upstream sync.
