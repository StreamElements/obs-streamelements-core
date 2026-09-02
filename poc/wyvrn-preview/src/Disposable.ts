/**
 * Disposal plumbing, in the shape the StreamElements client SDK uses: every
 * class that owns anything exposes `async dispose(): Promise<void>`, and calling
 * it releases everything that class acquired.
 *
 * Two properties matter more than they look:
 *
 *  - **Idempotent.** `dispose()` twice is a no-op the second time. Renderers get
 *    disposed by their owner and again by a page teardown, and neither should
 *    have to know about the other.
 *  - **Ordered and awaited.** Children are disposed in reverse acquisition
 *    order, and each is awaited. An `AudioContext.close()` is genuinely async;
 *    dropping the promise means "disposed" is a lie for a few milliseconds.
 */

export interface IDisposable {
	readonly disposed: boolean;
	dispose(): Promise<void>;
}

/** Anything we can own: our own classes, or a plain teardown function. */
export type Teardown = IDisposable | (() => void | Promise<void>);

export abstract class Disposable implements IDisposable {
	#disposed = false;
	#disposing: Promise<void> | null = null;
	#owned: Teardown[] = [];

	get disposed(): boolean {
		return this.#disposed;
	}

	/**
	 * Take ownership of something. Returns it, so it can wrap an expression:
	 *
	 *     this.ctx = this.own(new AudioContext());
	 */
	protected own<T extends Teardown>(resource: T): T {
		if (this.#disposed) {
			// Acquiring after disposal would leak silently, because nothing
			// will ever release it. Fail loudly instead.
			void Promise.resolve(runTeardown(resource));
			throw new Error(
				`${this.constructor.name}: cannot acquire resources after dispose()`,
			);
		}

		this.#owned.push(resource);
		return resource;
	}

	protected assertNotDisposed(): void {
		if (this.#disposed) {
			throw new Error(`${this.constructor.name}: already disposed`);
		}
	}

	/** Subclass teardown. Runs before owned resources are released. */
	protected async onDispose(): Promise<void> {}

	async dispose(): Promise<void> {
		// Re-entrant callers await the same in-flight disposal rather than
		// starting a second one.
		if (this.#disposing) return this.#disposing;
		if (this.#disposed) return;

		this.#disposing = (async () => {
			try {
				await this.onDispose();
			} finally {
				// Reverse order: the last thing acquired usually depends on
				// the things acquired before it.
				const owned = this.#owned.splice(0).reverse();

				for (const resource of owned) {
					try {
						await runTeardown(resource);
					} catch (err) {
						// One resource failing must not strand the rest.
						console.error(
							`${this.constructor.name}: dispose failed for an owned resource`,
							err,
						);
					}
				}

				this.#disposed = true;
			}
		})();

		return this.#disposing;
	}
}

async function runTeardown(resource: Teardown): Promise<void> {
	if (typeof resource === "function") {
		await resource();
		return;
	}

	await resource.dispose();
}

/**
 * A standalone bag for callers that are not themselves `Disposable` — a demo
 * page, a test. Same semantics.
 */
export class DisposableBag extends Disposable {
	add<T extends Teardown>(resource: T): T {
		return this.own(resource);
	}
}
