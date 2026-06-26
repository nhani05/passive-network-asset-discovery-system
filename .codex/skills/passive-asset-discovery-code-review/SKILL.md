---
name: passive-asset-discovery-code-review
description: Review changes to the Passive Network Asset Discovery System for correctness, safety, maintainability, and delivery impact. Use for pull-request review, pre-merge review, and change-risk assessment.
---

# Code review

Review the diff against the requirement and existing contracts. Report findings first, ordered by severity, with file and line references. Distinguish blocking defects from optional improvements.

Check packet length and byte-order handling, ownership and error paths, asset identity/first-last-seen semantics, CLI compatibility, JSON validity, test adequacy, CMake portability, and Docker/documentation impact. Verify that changes do not depend on live capture privileges for ordinary tests.

If no blocking issue remains, state residual risks and validation gaps; do not claim certainty without evidence.
