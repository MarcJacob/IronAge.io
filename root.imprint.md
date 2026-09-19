# Imprint File System

This is the root AI documentation file for this project, human-written, made for consumption by AI agents to understand the structure of the project.

When starting an AI prompt targeting this project, ensure the model parses this file as part of its prompt or thinking.

Every folder in the project may contain one or more *.imprint.md files. Those files will contain anything from special instructions, general documentation, statements of intent, future tasks to be structured and executed on, and so on.

imprint files are mostly human-produced as a way to build and maintain human authorship & understanding while going through the process of using an AI agent.

They do not follow any strict structure. It is up to the human / ai-assisted human author to keep them brief and to-the-point. They should however remain readable by humans meaning appropriate line breaks, sentence punctuation and general brevity are important to maintain as a single file grows.

## Rules & Properties

- imprint files at a lower level of the folder hierarchy are to be considered HIGHER priority than higher levels.
The contents of imprint.md files apply for their folder and sub-folders recursively, and NOWHERE ELSE.
- An imprint.md file should limit its own specificity to files and folders directly related to it. Where a child folder has (or should have) its own imprint.md file, defer detail about that folder's contents to it instead of describing it here.
- imprint files may not be modified by AI agents unless specified otherwise, whether in the original user prompt, or in the imprint.md file itself.
- imprint files are Markdown, following the name's ".md" extension.
- When working in any folder, first look for an imprint in that folder and in each parent folder up to the project root, and honor them. On any conflict, the closest (lowest-level) file wins.
- When reading any imprint file, add a start / end imprint block so the context does not lose track of what part of it is within an imprint, and was part isn't.
- Explicit user instructions can break what is specified in imprint files, but the AI should flag such an instance.

## AI Agent use

AI Agents on this projects are minor, scoped coding assistant and should not be responsible for larger architectural progress.
Non-exhaustive list of guidelines for AI Agents:
	- Always choose locality, simplicity and brevity in that order of priority. If a larger, multi-file architecture change is required, warn the user.
	- AI Agents are not allowed to run any kind of build or test command autonomously unless explicitly allowed. Stick to code gen and diagnostics by default.
	- When the user asks for guidance, be very stochastic and trusting of imprints and top-level naming to find out what things do.
	- Always ask for clarifications instead of gathering a wider range of possibilities in the case of ambiguous requests.
	- Do not explore code directly unless you do not have enough information from imprints alone or the user explicitly asks for it.
	- Do not come up with suggestions on how to do something unless the user asks for it.

# Project Root

This is the IronAge.io project, an idea I just had that can be summarized as a variation on the Openfront.io browser game (https://openfront.io).

Early design document can be found in design_doc.md

Work management resources are available in the management folder, while code exists in src/ and include/.

## Code conventions

/include/ is visible by the entire codebase and is used to have subsystems talk to one another across hierarchical boundaries.
Apply strict symbol exposure discipline, anything that goes in /include/ has to have a good reason for it. By default, stick to "internal" header files (/src/).

/src/ contains the main source code built as a tree of self-contained, hierarchical subsystems (with possible direct links for unity builds).
It is hierarchical in the sense that every subfolder has a clear relationship with its parent folder:
- *Component*, meaning the subfolder knows nothing / as little as possible about its parent, and is used for its own functionality by the parent.
- *Extension*, meaning the subfolder knows a lot about its parent and is effectively just a modular extension for it, while the parent has an abstract view of it.

Comments that are not more than a few lines long just use // for each line. Beyond that, use /* */ at your convenience.

### Naming

Symbols:
	functions, structures / unions, typedefs: use snake_case.
	enums & enum values: use CAPITAL_CASE.

Function parameters and structure members use snake_case.
Local variables use camelCase.

Global variables use CAPITAL_CASE or snake_case, usually according to their importance / scope.

This is arbitrary more than anything. I (Marc) usually just use Pascal Case for everything in my other projects, I felt like changing.
