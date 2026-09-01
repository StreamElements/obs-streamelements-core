/**
 * The playback timing shared by both haptics previews.
 *
 * The timeline and the body view show the same effect two ways, so they have to
 * agree on *when* — otherwise the playhead is mid-sweep while the body has
 * already gone dark, and the two panels read as unrelated. Keeping the rule in
 * one place makes that agreement structural rather than a coincidence of two
 * copies staying in step.
 *
 * The cycle is: one pass of the effect, then a rest, then repeat.
 *
 *     |<-- duration -->|<-- pauseMs -->|<-- duration -->| ...
 *      playing           resting         playing
 *
 * The rest matters. Haptic effects are often a fraction of a second, and looped
 * back-to-back they read as a continuous buzz with no discernible shape; the gap
 * is what lets you see where one pass ends and the next begins.
 *
 * With `isOneShot` the clock plays once and reports `finished`, so a caller can
 * stop rather than idle in a rest forever.
 */

export interface SequenceClockOptions {
	/** Rest between passes, in milliseconds. Default 1000. */
	pauseMs?: number;
	/** Play once and stop, instead of repeating. Default false. */
	isOneShot?: boolean;
}

export interface SequenceSample {
	/** Position within the effect, in seconds. Clamped to the duration. */
	time: number;
	/** True during the gap between passes: nothing should be playing. */
	resting: boolean;
	/** True once a one-shot run is over. Always false when repeating. */
	finished: boolean;
}

export class SequenceClock {
	#pauseMs: number;
	#isOneShot: boolean;
	#startedAt = 0;

	constructor(options: SequenceClockOptions = {}) {
		this.#pauseMs = options.pauseMs ?? 1000;
		this.#isOneShot = options.isOneShot ?? false;
	}

	get pauseMs(): number {
		return this.#pauseMs;
	}

	get isOneShot(): boolean {
		return this.#isOneShot;
	}

	set isOneShot(value: boolean) {
		this.#isOneShot = value;
	}

	/** Restart from the top. */
	start(now = performance.now()): void {
		this.#startedAt = now;
	}

	/**
	 * Where the sequence stands at `now`, for an effect of `durationSeconds`.
	 *
	 * A zero-length effect never advances and never rests; it simply sits at 0,
	 * which is what an empty haps file should look like rather than dividing by
	 * a zero cycle.
	 */
	sample(durationSeconds: number, now = performance.now()): SequenceSample {
		if (durationSeconds <= 0) {
			return { time: 0, resting: false, finished: this.#isOneShot };
		}

		const elapsedMs = now - this.#startedAt;
		const durationMs = durationSeconds * 1000;

		if (this.#isOneShot) {
			if (elapsedMs >= durationMs) {
				return { time: durationSeconds, resting: false, finished: true };
			}
			return { time: elapsedMs / 1000, resting: false, finished: false };
		}

		const cycleMs = durationMs + this.#pauseMs;
		const intoCycle = ((elapsedMs % cycleMs) + cycleMs) % cycleMs;

		if (intoCycle >= durationMs) {
			// In the gap. Report the end of the effect rather than 0, so a
			// playhead parks where the pass finished instead of snapping back
			// before the rest has elapsed.
			return { time: durationSeconds, resting: true, finished: false };
		}

		return { time: intoCycle / 1000, resting: false, finished: false };
	}
}
