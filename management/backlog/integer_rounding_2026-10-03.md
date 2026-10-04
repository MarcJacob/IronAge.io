# BACKLOG: Round to nearest in integer math

Input date: 2026-10-03

## Item

Game integer math (percentages, ratios, rolls) should round to nearest instead
of truncating, e.g. `(a * b + d / 2) / d`. Add a helper / convention, apply
where results are gameplay-visible, case by case (deliberate ceil on losses in
attack resolution stays).

## Why

Truncation biases results down and collapses small values to 0. Raised while
reviewing attack fight resolution.
