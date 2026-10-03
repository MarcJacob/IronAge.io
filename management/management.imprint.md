# MANAGEMENT FOLDER

This is a general project management folder. It exists for both humans and AI agents as
a shared reference of the work that has been done and the work that remains to be done
on this project. It is not source code and holds no build-relevant files.

Key file: dev-plan.md holds the high-level development plan and the stated technical
architecture for the project. It should be kept up to date as architecture decisions are
made or phases of work complete, and consulted whenever the shape of upcoming work is
unclear.

The /backlog/ folder holds one file per backlog item.

The /work_units/ folder contains all current work unit files, indexed from dev-plan.md's Active Work Units section.

The /archive/ folder holds finished work units.

The /artisan_skills/ folder holds one file per content pipeline: a step-by-step recipe an artisan agent follows
to derive new content (e.g. a new command) from existing precedents.

## Work unit lifecycle

1. Backlog: the item is a file in /backlog/.
2. Start: when work begins on it, move the file to /work_units/ and reference it in dev-plan.md's Active Work Units.
   New work units that did not come from the backlog start here directly.
3. Ongoing: progress and decisions are recorded in the file.
4. Complete: record the outcome in the file, mark it complete in the plan. It stays in /work_units/ for a time.
5. After a time, move it to /archive/ and update the plan's reference.
Backlog items are moved, never deleted.

## AI Agent use

AI agents working in this project are allowed to assign work to developers when asked
to do so (e.g. by a human requesting a task breakdown, a work item, or a hand-off).
When doing so:

- The agent may state time budgets or other constraints on the assigned work if it has
  been given them or if they are apparent from context (e.g. a deadline, a scope limit,
  available tooling).
- Whenever time or other constraints are part of an assignment, the agent MUST consult
  dev-plan.md first, so the assigned work is scoped and sequenced consistently with
  the stated development phases and architecture rather than invented ad hoc.
- Work assignments should reference the relevant phase(s) and/or architecture section of
  dev-plan.md where applicable, and should flag when requested work falls outside the
  plan's current sequencing (per root.imprint.md, larger architectural changes should be
  flagged to the user rather than assumed).
- Whenever work is done, or when the user explicitly request it, the dev-plan.md file should be updated
  to add / remove [WIP] or [DONE] flags after the relevant plan items, and if necessary add sub-items
  as the tasks are better understood and broken down, and offer to split away from the plan into a work unit
  when a coherent set of tasks comes together.
