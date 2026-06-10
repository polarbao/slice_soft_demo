# Slice Document State Rules

Use A/B/C/D evidence classification.

## Evidence levels

```text
A = current code/config/tests; safe implementation basis
B = formal target design; direction only
C = historical reference; background only
D = deprecated/conflicting; not implementation basis
```

## Rules

- Current code wins over old docs when checking implemented behavior.
- Stage reports are evidence of reported validation, not automatic proof unless commands and results are listed.
- PRD/DEV/TASK documents describe target direction, not implementation status.
- Historical chat logs may explain rationale, but should not be used as direct implementation truth.
- If a source is missing, stale, or expired, state that clearly.

## Required state split

For architecture/refactor/protocol questions, output:

```text
Current State
Target State
Historical State
Pending Confirmation
```
