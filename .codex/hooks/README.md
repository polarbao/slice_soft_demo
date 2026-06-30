# Codex Hooks

Hooks are disabled in `.codex/config.toml`.

If hooks are added later, they must be non-destructive by default and must not:

- change git history;
- delete user files;
- enable OpenVDB by default;
- change RGBWSV protocol semantics;
- touch production data or hardware devices.
