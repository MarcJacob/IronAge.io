// Typed subscribable events of a UI module (new-style modules, see ui_element_login.ts).
// The module declares its events as a type mapping each event name to its callback signature, and emits them on user actions.
// Modes subscribe with on(); the returned function unsubscribes (call it in on_exit).

type EventMap = { [name: string]: (...args: any[]) => void };

export class UiEvents<Events extends EventMap>
{
    private listeners: { [K in keyof Events]?: Events[K][] } = {};

    on<K extends keyof Events>(event: K, callback: Events[K]): () => void
    {
        const list = (this.listeners[event] ??= []);
        list.push(callback);
        return () => { const index = list.indexOf(callback); if (index >= 0) list.splice(index, 1); };
    }

    emit<K extends keyof Events>(event: K, ...args: Parameters<Events[K]>): void
    {
        // Copy: a callback may unsubscribe (e.g. by switching modes) while the event is dispatched.
        for (const callback of (this.listeners[event] ?? []).slice()) callback(...args);
    }
}
