# BACKLOG: Round to nearest in integer math

Input date: 2026-10-03

## Item

Integer math in game code should round to nearest (round half up) instead of
always rounding down, like float -> int conversion does.

## Description

Integer divisions and scaled values (percentages, ratios, rolls) currently
truncate. Introduce a convention / helper for rounding to nearest, e.g.
(a * b + d / 2) / d, and apply it where results are gameplay-visible.
Case by case: some spots (e.g. ceil on losses in attack resolution) are
deliberate and stay as they are.

## Why

Truncation biases results downward and makes small values collapse to 0.
Raised while reviewing the attack target fight resolution.
