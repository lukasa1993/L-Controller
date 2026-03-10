# Quick Task 4: test ota update build next version with minimal changes and update with ota - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Task Boundary

Prepare the smallest practical firmware change that produces a newer OTA-eligible image, build it, and keep the task focused on the local upload/apply OTA path.

</domain>

<decisions>
## Implementation Decisions

### Version bump strategy
- Use an ad hoc version bump for this task rather than introducing a broader reusable version-management workflow.
- Keep the change minimal and repo-local so the next build produces a newer signed image than the current `0.0.0+0` default.

### OTA path focus
- Optimize this task for the local upload/apply OTA flow first.
- Do not expand the task into GitHub release automation unless the minimal local path requires it.

### Existing uncommitted edits
- Ignore the unrelated pending login and panel changes in the worktree.
- Do not fold them into this quick task unless the OTA task directly depends on the same files.

### Claude's Discretion
- Pick the exact minimal version increment that satisfies OTA's strictly-newer comparison.
- Decide whether the safest minimal implementation is a direct build config bump, a tiny helper, or a narrow doc note, as long as the task stays small and local-upload oriented.

</decisions>

<specifics>
## Specific Ideas

- The current build artifacts show `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="0.0.0+0"`, so the task can likely succeed with a narrow signing-version change plus a rebuild.
- Keep verification centered on producing a newer signed image and preserving the existing OTA upload/apply behavior.

</specifics>
