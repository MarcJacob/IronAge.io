# BACKLOG: Web client overhaul

Input date: 2026-10-23

## Item

Explore a new web client platform called **Dynamic Web Game Client**. The initial implementation should be introduced incrementally within specific modules of the current **Web Game Client**, rather than immediately creating a separate parallel client. If the approach proves viable across the client, it can later become the basis for replacing and eventually retiring the current implementation. At that point, the replacement should lose the “Dynamic” qualifier and become the Web Game Client.

The platform should support multiple pages and lifecycle stages, including a main menu, lobby, and in-game view.

## Concept

The project already has a working static implementation of the application in TypeScript and WebAssembly. The Dynamic Web Game Client should build on that implementation rather than replace its page lifecycle, connection handling, or general browser runtime immediately.

The first useful application of dynamic generation is the collection of TypeScript types and related access code that must currently be kept in sync with C++ types. The C++ game code should become the source from which the corresponding browser-facing representation is generated, while the existing TypeScript/WebAssembly application continues to provide the stable host around it.

The Dynamic Web Game Client would therefore combine:

- The existing static TypeScript and WebAssembly resources, which provide the application shell, visual foundation, WebSocket lifecycle, and major events such as joining a lobby or match.
- Dynamically generated TypeScript or JavaScript modules produced from the C++ game code. These modules would expose generated representations of selected C++ types and use interfaces supplied by the existing client to add or update game-specific elements and behavior.

The static resources remain the toolbox and stable browser-side host, while generated resources become the type-accurate usage of that toolbox for selected game modules. This allows the approach to be introduced one type family or module at a time instead of requiring a second complete client.

For example, static code could construct and place background and foreground canvases in the DOM, assign them stable names, and handle their invariant lifecycle. Generated code could assume those named interfaces exist, obtain the corresponding canvases, and generate rendering and interaction logic directly from the C++ types available to the builder.

This boundary must remain deliberate. Static code should not quietly become a second game client, and generated code should not depend on undocumented details of the static implementation. The interface between them needs explicit names, lifecycle rules, and versioning or compatibility checks.

## Proposed builder architecture

The C++ builder would be a reusable service rather than code tied only to the game server. It would include:

- A Web-to-Binary parser that reads the static web resources and builds an in-memory representation of their DOM tree and supported code insertion points.
- C++ builder APIs that add or modify DOM elements and modules, and attach JavaScript snippets to defined leaf elements such as functions or static elements. Most application logic would remain expressible in C++, while JavaScript would be treated as generated content where the browser requires it.
- A Binary-to-Web parser that serializes the completed in-memory representation back into HTML and JavaScript resources for the browser.

The Game Server would implement this service to generate the resources it serves. The web resource system may need a small abstraction so resources can be generated on demand per request, or generated once and cached at server initialization when the inputs are known to be unchanged.

“Binary” here means the builder’s in-memory/intermediate representation, not necessarily a persisted or wire-level binary format. The first exploration should avoid committing to a complex binary file format unless it provides a clear benefit.

## Initial transition strategy

Until no new dependent code needs to be written directly in TypeScript, use a simple manually invoked translation program rather than making the Game Server generate client resources at runtime.

The translator would inspect or otherwise encode the relevant C++ game data types and emit a specific JavaScript module containing the browser-facing representations and access/serialization helpers needed by the existing TypeScript client. The generated file would be written to a known client source location and copied along with the compiled client resources during the normal client build.

The developer would manually rerun the translator when a C++ change affects a client dependency, such as adding a command type, game data structure, entity, or entity property. This keeps the early workflow explicit and easy to debug while avoiding premature per-request server generation.

The translator should be designed as the first version of the eventual server-side builder. Later, once dependent code has moved away from direct TypeScript ownership of these structures, the same translation logic can be invoked by the Game Server to generate the dynamic resource as part of serving or preparing the client. The output format and browser-facing API should remain compatible across this transition where practical.

This staged approach should also generate TypeScript declaration information, or an equivalent TypeScript-visible interface, so the language server can understand the generated JavaScript module. The generated artifact is the source consumed by TypeScript; it should not be hand-edited.

## Exploration scope

- Dynamically generate the first type family or module that currently requires manual C++/TypeScript mirroring.
- Decide whether the generated browser-facing output should be TypeScript, JavaScript, declaration files, or a combination. Generated declaration files could preserve TypeScript language-server support even when implementation code is generated.
- Define how generated types are imported by existing TypeScript modules without requiring the whole client to migrate at once.
- Document the development trade-off that directly consuming generated runtime types without generated declarations will reduce language-server help, autocomplete, and static checking in those TypeScript files.
- Define the static host interface, generated module interfaces, DOM naming rules, lifecycle callbacks, and page transitions for main menu, lobby, and game pages.
- Prototype one narrow generated type or module inside the existing Web Game Client before generating complete page content.
- Determine whether the intermediate representation should model only generated type/module boundaries, or also DOM elements and selected JavaScript structures.
- Define resource generation and caching behavior in the Game Server’s web resource system.
- Establish criteria for expanding the approach from selected mirrored types to additional modules, or abandoning it without destabilizing the existing client.
