# Project IronAge.io
*README START - HUMAN-ORIENTED DOCUMENT, AI AGENTS SHOULD USE THE IMPRINT SYSTEM TO UNDERSTAND THE PROJECT*

IronAge.io seeks to be an alternative take on the genre of games currently most well-known through www.openfront.io, which we could call "online browser-based massively multiplayer strategy games" (mouthfully).

A rough description of my ideas can be found in the design document in the *management* folder (though it needs a rewrite badly at the time of writing this).

In short, IronAge is an online browser game (with loose plans for a future native client) where players exist as feudal rulers in competition for power and wealth, through conquest and economic development.

What makes IronAge stand out beyond its specific mechanics is one central design idea: *there are multiple ways to win*, and being the "sovereign" victor (which is equivalent to Openfront's sole victory condition of owning 80% of the map) is only one of them, and the riskiest as well. "Winning" means being *the first among the winners* and triggering the game's end, at which point the game's *winners* are decided from how close they were to each victory condition, and by overall score.

This is the first half of the idea: the game acknowledges that different players can have different "game vibes" they're going for, like "trader" or "mitochondria" players on OF.

The other half is a diplomacy system that is more advanced than other similar games. Beyond the simple alliance, players can also differentiate between War and Neutral status, exercize "soft" forms of warfare through the economy and through other offensive actions than conquest like raiding / banditry, and finally form *vassalization* relationships where a player explicitly acknowledges that they are now tied to another player's sovereignty, in exchange for protection. That way, a conquering player has a choice between *annexing* territory or having the other player bend the knee.

Those two game aspects taken together form interesting combinations: you may get vassalized quite early on, but it doesn't stop you from trying to win by ruling over the greatest share of the population (in which case you will win before your liege does), or through pure economic might (in which case finding a good liege that can cover you militarily can actually be *good* news !).

# AI capabilities

*To learn more about AI usage on this project, look further below*.

This project is setup to facilitate the usage of agentic AI using a system I've coined the *Imprint files system*. If you clone this repository, setup an agentic workspace over it and ask your agent to read the root.imprint.md file at the start of a session, it will be ready to answer any of your questions efficiently should the rest of this document prove unsufficient.

In fact, if you wish to get a full overview of the project as it exists currently, I recommend you load a relatively low / medium size model (for efficiency, at most something like GPT Luna) and ask them to explain it to you. The imprint files will allow the AI to quickly get an overview of the entire thing without having to parse all the code / contents until you ask for details.

# Building & Running

This project uses a hand-built, simple command-file-based build system. I initially started with CMake but I don't like how it makes things more complicated by generating a ton of files in its cache and making it annoying to manage multiple build chains in parallel which this project obviously needs.

The most straightforward way to build this project on Windows (the only supported platform currently) is to use the relevant "build all" script in the *scripts* folder and starting the game server in *build/Win32_Game_server/*. Then connect two tabs to localhost:8000 and you'll have a working test session. Running the server over an actual network and connecting through IP and port forwarding is for now untested.

The build scripts you use directly are present in the */scripts/* folder, while *they* make use of in-place build scripts located next to their associated *platform app main file*. Every app and sub-component (such as the server platform-independent code) is built as a single translation unit, so the building logic is actually relatively simple.

For actual development, I use Neovim with a local project-specific config you will find in */.nvim/*. If you have Neovim on your Windows system, you can simply start the dev environment with the root folder command file. It will automatically start Neovim using the local project settings which first will load your local config, so unless you have conflicting keymappings it should be relatively smooth to get started, and you will have access to environment variables and pre-mapped hotkeys to start builds / tests handy.

## Technical architecture (High level overview)

The project's code is currently split into 3:
- The Game Server, implemented through a Win32 app + a platform-independent hot-reloadable layer.
- The Game Client, implemented through a Web app + a platform-independent layer.
- The Game Common code, present on both sides.


The Server accepts connections through whatever protocol is supported (currently HTTP -> WebSocket) and manages a set of Match Slots within which players are placed in order to play together over the same map and share all inputs into the game simulation.

The simulation itself lives in the Game Common code, and happens independently on all sides, leaving the server with the one responsibility of gathering and broadcasting all inputs applied for a specific tick, which it does by receiving them from all connected clients individually. This is a classic "lockstep" architecture.

### Tech stack

The main programming language used to build this is C++C / C++ of my own style (basically C with some cherry-picked features from C++ including operator & function overloading, simple templates and adding methods to structures here and there).

The Win32 Game Server is fully hand-rolled and makes no use of the standard library outside the Win32 / platform layer.
The Web Game Client has a front half made with Typescript and no other dependencies, and a back half split into a WASM "platform" and a platform-independent fully hand-rolled client backend.

Both rely on the Game Common Code to simulate match, input commands and communicate over the network which is also fully hand-rolled C / C++.

# On the usage of AI

This project is developped with active usage of AI (Claude, mostly). What does it look like and what does it mean in terms of code quality and overall project organization ?

First, let's establish that I see myself as an "AI skeptic", in the sense that my goal with AI is not to embrace it thoroughly and have it replace as much of my capabilities and work as possible. Rather, I seek to find a healthy relationship with it that allows me to focus on what I've always cared about (learning new stuff, experimenting and being in full ownership of my code), lower the relative importance of my mental or technical weaknesses, and perform grunt work I consider to be not worth my time.

So the first and only **!!disclaimer!!** I will give is this: **This project is not vibe-coded. The vast majority of the code is written by me, by hand, and any AI-generated code exists in one of the three "acceptable categories" listed below, and is, no-matter-what, always thoroughly reviewed such that I am never a stranger to the code or how it works down to a low level of detail.**

*I lied, second disclaimer: this README is fully hand-written !*

Now, let's get into how I use it.

## AI as a coding assistant

Let's begin with the most salient use in the eyes of a developer reading through this.

Some amount of the code on this project is started (and sometimes mostly left as) AI-generated code with three possible purposes:
- "Soft" code that changes shape often because the underlying architecture is still in too much flux.
    - Typescript User interface handling, byte payload construction from WASM to Frontend...
    - Game Input Command handling for specific placeholder game features.
- Placeholder code that stays until the final hand-crafted feature comes to replace it (usually with a full architecture expansion around it).
- "Content" code that follows a well-defined, well-understood shape and whose implementation details are very predictable.
    - Currently this mostly includes game input command handling code. While not in its final shape, it will stay relatively static for a while and it's simple enough that I could explain it in "recipe" form (see "artisan skills") and get a very predictable result from AI. The AI essentially becomes a content generation pipeline with little to no "artistic agency" left to be had anyway.

Regardless of the nature of the generated code, it is always reviewed and very often iterated on multiple times, sometimes by hand, and usually replaced at some point.

A fourth category you *could* count is the performing of fastidious changes due to, for example, changing where a variable is located or how some piece of memory is managed. In that case, the AI is effectively an advanced rename / refactor tool but I personally do not consider the resulting code as "AI generated", more "AI assisted" which is subtly, but critically, different.

## AI as a guide / teacher

The personal projects I work on always involve some sort of technical challenge I have never approached before.

In times past, solving such challenges would have required a mix of blind experimentation, and research on the web looking for answers must then be related to my specific set of problems / circumstances. Nothing wrong with that, but I have been saving a lot of time by letting AI help me approach new topics by leveraging its ability to use the project's context to relate new and old concepts together and give me a clear set of choices on how to proceed.

Obviously this is not a safe process: *you don't know what you don't know*, and like with any source of teaching, this means you can be in a poor position to think critically about what is presented to you. When asking AI to guide you through how to do something you don't know how to do, it is VERY important to understand the level of abstraction you are in, and thus how likely it is that for whatever the AI presents to you, alternatives exist, sometimes better, sometimes worse. In my experience AI is relatively good at giving you the full picture here, but it can always make mistakes by omission or miss-judging what your actual intents and preferences are.

When asking AI to help there, it is thus critical to not only ask "What next ?" but also ensuring that it knows that:
- You value understanding from first-principles, so pulling out huge libraries / dependencies is not a commonly-favored approach.
- You value simplicity, so it is better to start with something small and discover abstractions later.
- You have specific plans for the project and that it should start from those instead of what it sees as "common practice" for similar projects.
... and so on.

## As a middle manager

On this project, I hold two roles: *lead developper*, and *director*. The role of *project manager* it held by AI. How does this work, and why ?

The *director* role is obvious: This is my project, my idea, my decisions. I hold complete responsibility for this entire repo's content and the quality of the final product, and it follows my design vision. An AI will *never* dictate to me what is the correct way of designing a game or be the final decider on the technical architecture, although it may contribute advice on topics I know too little about.

The *lead developer* role is also obvious: The vast majority of the code is authored by me, and it is up to me to own the code in a way that allows me to efficiently iterate on new features and keep the complexity under control. Of course, if ever anyone else participates, I would be in charge of making sure they can do so comfortably and efficiently.

This leaves *project management*, which in this trio ties into the following tasks:
- Keeping track of advancement in relation to an overall plan and specific development tasks / subjects.
- Guiding work sessions: what to do next, suggestions on how, what makes sense to do first or to do according to a desired length of time spent working.
- Keeping development and developers (yes, myself) on track: rabbit holes can be plunged into later, there are features to complete and the AI is oddly good at keeping me accountable.

When I work, most of the time I have a Claude Code session open, configured on a specific agent type I have authored to help with exactly that as efficiently as possible. It has write permissions *solely* over the *management* folder, and can only acquire understanding of the codebase by:
- Reading *imprint* files.
- Dispatching a sub-agent to produce a report.

This is by far the way I use AI that has increased my productivity the most compared to past projects where I would unfortunately often wander off into random directions, inevitably stall visible advancement of the project and, in that way, break the dopaminergic cycle of work -> results -> happy feelies -> more work.

## The core of the system: Imprint files

All over the project (mostly the codebase) you will find Markdown files called *imprints*, which are small, in-place pieces of "soft" documentation that allow a human developer to leave behind various descriptions, explanations and advice about the contents of the folder and all of its sub-folders recursively.

In a (mostly) tree-shaped codebase, this leads to those files serving as a technical documentation of sort, but that is not their role. You will not find a listing of all functions / structures / types there - the code contains them, doesn't it ? No, you will instead find about the same stuff I would say to you were you a new member on the team and asking me about it.

The point of those files is to allow a new agentic session to get a high-level, hierarchical overview of the project, starting from most abstract down to most specific (from root folder to leaf folders) as needed, without first needing to load all the code into its context.

Instead, the AI is instructed (explicitly, told to read the root.imprint.md file through the Claude Code hooks) to take heed of this system at the start of ANY session / task, and to limit its search to a heuristic based on those files before, if needed, going through any code / content file, and even then only the strict minimum.

Doing this, both on this project and elsewhere (including my day job), I have observed significant efficiency increases especially for smaller models, which leads to my high usage efficiency compared to some of my peers.

Good luck not doing that when the price of AI reaches its real cost and goes x100 (:

As with the project itself, there's no better way of learning about this system than asking AI itself !

As of writing this, I would say the system isn't fully, properly deployed on this project - some folders would deserve their own imprint, and existing one can be a little light on some details. I still need to formalize some things about the process, such as how and when to update the files and how much detail / complexity of a subfolder warrants creating a new one.
