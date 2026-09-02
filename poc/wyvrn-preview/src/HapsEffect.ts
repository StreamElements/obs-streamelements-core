/**
 * Parser for Interhaptics `.haps` files — the haptics half of a WYVRN event.
 *
 * Unlike `.chroma` these are JSON, and the schema below was read off real
 * assets rather than documentation:
 *
 *     {
 *       description, gain, version,
 *       vibration: {
 *         frequency_range: { min, max },
 *         gain, loop,
 *         melodies:   [ { gain, mute, notes: [ Note ] } ],
 *         transients: [ { position, amplitude, pitch } ]
 *       }
 *     }
 *
 *     Note = {
 *       position, length, m_gain,
 *       amplitude: { gain, interpolation_function, keyframes: [ { position, value, slope? } ] },
 *       pitch:     { gain, interpolation_function, keyframes: [ ... ] }
 *     }
 *
 * Two kinds of content, and a file may carry either or both:
 *
 *  - **Transients** — discrete taps at a point in time. Amplitude and pitch are
 *    normalised 0..1.
 *  - **Melodies** — continuous vibration. Each note has amplitude and pitch
 *    envelopes as keyframe lists, with positions relative to the note's own
 *    start. Keyframe positions can be slightly negative in real files, so
 *    envelope evaluation clamps rather than assuming a sorted 0..length range.
 *
 * `pitch` is normalised too: map it through `frequency_range` for Hz.
 */

import { Disposable } from "./Disposable.js";

export interface HapsKeyframe {
	readonly position: number;
	readonly value: number;
	readonly slope?: number;
}

export interface HapsEnvelope {
	readonly gain: number;
	readonly interpolationFunction: string;
	readonly keyframes: readonly HapsKeyframe[];
}

export interface HapsNote {
	readonly position: number;
	readonly length: number;
	readonly gain: number;
	readonly amplitude: HapsEnvelope;
	readonly pitch: HapsEnvelope;
}

export interface HapsMelody {
	readonly gain: number;
	readonly mute: boolean;
	readonly notes: readonly HapsNote[];
}

export interface HapsTransient {
	readonly position: number;
	readonly amplitude: number;
	readonly pitch: number;
}

export interface HapsLoadOptions {
	signal?: AbortSignal;
	fetchImpl?: typeof fetch;
}

/**
 * How long a transient stays audible/visible after its instant. Transients
 * carry no duration in the file; without a window they are unsamplable.
 */
const TRANSIENT_DECAY_SECONDS = 0.08;

export class HapsParseError extends Error {
	constructor(message: string) {
		super(message);
		this.name = "HapsParseError";
	}
}

function envelope(raw: any): HapsEnvelope {
	const keyframes: HapsKeyframe[] = Array.isArray(raw?.keyframes)
		? raw.keyframes
				.filter((k: any) => typeof k?.position === "number")
				.map((k: any) => ({
					position: k.position,
					value: typeof k.value === "number" ? k.value : 0,
					slope: typeof k.slope === "number" ? k.slope : undefined,
				}))
				// Real files are ordered, but nothing guarantees it.
				.sort((a: HapsKeyframe, b: HapsKeyframe) => a.position - b.position)
		: [];

	return {
		gain: typeof raw?.gain === "number" ? raw.gain : 1,
		interpolationFunction: raw?.interpolation_function ?? "Linear",
		keyframes,
	};
}

/** Sample an envelope at `t` (seconds, relative to the note), clamped at both ends. */
export function sampleEnvelope(env: HapsEnvelope, t: number): number {
	const kf = env.keyframes;
	if (kf.length === 0) return 0;
	if (t <= kf[0].position) return kf[0].value * env.gain;
	if (t >= kf[kf.length - 1].position)
		return kf[kf.length - 1].value * env.gain;

	for (let i = 1; i < kf.length; i++) {
		const b = kf[i];
		if (t <= b.position) {
			const a = kf[i - 1];
			const span = b.position - a.position;
			// Coincident keyframes are a step, not a divide-by-zero.
			const f = span <= 0 ? 1 : (t - a.position) / span;
			return (a.value + (b.value - a.value) * f) * env.gain;
		}
	}

	return kf[kf.length - 1].value * env.gain;
}


/**
 * Rewrite an `m_`-prefixed haps document into the plain shape.
 *
 * Only the fields this renderer consumes are translated; anything else
 * (`m_stiffness`, `m_texture`, priorities) is dropped deliberately rather than
 * carried as dead weight.
 */
function normalizeMSchema(raw: any): any {
	// Nominal range for the m_ schema, which does not declare one.
	const FREQ_MAX = 1000;

	// The two modulation curves are NOT in the same units, which is easy to miss
	// and produces silently absurd output: amplitude keyframes are normalised
	// 0..1, but frequency keyframes are absolute Hz (observed up to ~242 across
	// the 202 GenericEvent files). Treating frequency as normalised yields
	// 50 kHz readings. `scale` converts it to the 0..1 pitch the model expects.
	const curve = (mod: any, scale = 1) => ({
		gain: 1,
		interpolation_function: "Linear",
		keyframes: Array.isArray(mod?.m_keyframes)
			? mod.m_keyframes.map((k: any) => ({
					position: typeof k?.m_time === "number" ? k.m_time : 0,
					value: (typeof k?.m_value === "number" ? k.m_value : 0) * scale,
				}))
			: [],
	});

	const vib = raw.m_vibration ?? {};

	return {
		description: raw.m_description ?? "",
		gain: typeof raw.m_gain === "number" ? raw.m_gain : 1,
		version: String(raw.m_version ?? ""),
		vibration: {
			gain: typeof vib.m_gain === "number" ? vib.m_gain : 1,
			loop: Boolean(vib.m_loop),
			// The m_ schema carries no explicit range; this matches what the
			// plain-schema files declare, and is what `scale` normalises against.
			frequency_range: { min: 0, max: FREQ_MAX },
			transients: [],
			melodies: Array.isArray(vib.m_melodies)
				? vib.m_melodies.map((m: any) => ({
						gain: typeof m?.m_gain === "number" ? m.m_gain : 1,
						mute: Boolean(m?.m_mute),
						notes: Array.isArray(m?.m_notes)
							? m.m_notes.map((n: any) => {
									const fx = n?.m_hapticEffect ?? {};
									return {
										position: n?.m_startingPoint ?? 0,
										length: n?.m_length ?? 0,
										m_gain: n?.m_gain ?? 1,
										amplitude: curve(fx.m_amplitudeModulation),
										pitch: curve(
											fx.m_frequencyModulation,
											1 / FREQ_MAX,
										),
									};
								})
							: [],
					}))
				: [],
		},
	};
}

export class HapsEffect extends Disposable {
	readonly description: string;
	readonly gain: number;
	readonly version: string;
	readonly loop: boolean;
	readonly frequencyRange: { min: number; max: number };
	readonly melodies: readonly HapsMelody[];
	readonly transients: readonly HapsTransient[];
	readonly totalDurationSeconds: number;
	readonly sourceName: string;

	private constructor(init: {
		description: string;
		gain: number;
		version: string;
		loop: boolean;
		frequencyRange: { min: number; max: number };
		melodies: HapsMelody[];
		transients: HapsTransient[];
		sourceName: string;
	}) {
		super();
		Object.assign(this, init);

		const melodyEnd = init.melodies.reduce(
			(max, m) =>
				m.notes.reduce((n, note) => Math.max(n, note.position + note.length), max),
			0,
		);
		const transientEnd = init.transients.reduce(
			// Include the decay tail, otherwise the final tap is clipped off the
			// end of the timeline.
			(max, t) => Math.max(max, t.position + TRANSIENT_DECAY_SECONDS),
			0,
		);

		this.totalDurationSeconds = Math.max(melodyEnd, transientEnd);
	}

	static async fromUrl(
		url: string,
		options: HapsLoadOptions = {},
	): Promise<HapsEffect> {
		const doFetch = options.fetchImpl ?? globalThis.fetch;

		const controller = new AbortController();
		const onAbort = () => controller.abort(options.signal?.reason);
		options.signal?.addEventListener("abort", onAbort, { once: true });

		try {
			const response = await doFetch(url, { signal: controller.signal });

			if (!response.ok) {
				throw new HapsParseError(
					`${url}: HTTP ${response.status} ${response.statusText}`,
				);
			}

			return HapsEffect.fromJson(await response.text(), url.split("/").pop() ?? url);
		} finally {
			options.signal?.removeEventListener("abort", onAbort);
		}
	}

	static fromJson(json: string, sourceName = "(memory)"): HapsEffect {
		let raw: any;

		try {
			raw = JSON.parse(json);
		} catch (err) {
			throw new HapsParseError(`${sourceName}: not valid JSON - ${String(err)}`);
		}

		// Two schemas exist in the wild and neither is documented. Files under
		// hapticFolders/<Game>/ use plain keys (`vibration.melodies[].notes[]`
		// with `amplitude.keyframes`), while the 202 files under
		// hapticFolders/GenericEvent/ - the same set the Effects Library serves -
		// use an `m_`-prefixed shape with `m_startingPoint`, `m_length` and
		// modulation curves keyed by `m_time`/`m_value`.
		//
		// They describe the same thing, so the `m_` form is normalised into the
		// plain one here and everything downstream sees a single model.
		if (raw && raw.m_vibration && !raw.vibration) {
			raw = normalizeMSchema(raw);
		}

		const vibration = raw?.vibration ?? {};

		const melodies: HapsMelody[] = Array.isArray(vibration.melodies)
			? vibration.melodies.map((m: any) => ({
					gain: typeof m?.gain === "number" ? m.gain : 1,
					mute: Boolean(m?.mute),
					notes: Array.isArray(m?.notes)
						? m.notes.map((n: any) => ({
								position: typeof n?.position === "number" ? n.position : 0,
								length: typeof n?.length === "number" ? n.length : 0,
								gain: typeof n?.m_gain === "number" ? n.m_gain : 1,
								amplitude: envelope(n?.amplitude),
								pitch: envelope(n?.pitch),
							}))
						: [],
				}))
			: [];

		const transients: HapsTransient[] = Array.isArray(vibration.transients)
			? vibration.transients
					.filter((t: any) => typeof t?.position === "number")
					.map((t: any) => ({
						position: t.position,
						amplitude: typeof t.amplitude === "number" ? t.amplitude : 0,
						pitch: typeof t.pitch === "number" ? t.pitch : 0,
					}))
					.sort((a: HapsTransient, b: HapsTransient) => a.position - b.position)
			: [];

		return new HapsEffect({
			description: typeof raw?.description === "string" ? raw.description : "",
			gain: typeof raw?.gain === "number" ? raw.gain : 1,
			version: String(raw?.version ?? ""),
			loop: Boolean(vibration.loop),
			frequencyRange: {
				min: vibration?.frequency_range?.min ?? 0,
				max: vibration?.frequency_range?.max ?? 1000,
			},
			melodies,
			transients,
			sourceName,
		});
	}

	/**
	 * Combined amplitude at `seconds`, 0..1. Overlapping contributions take the
	 * strongest rather than summing, since two effects at 0.6 is not 1.2 of
	 * anything.
	 *
	 * **Transients count too.** They are discrete taps with no duration of their
	 * own, and an earlier version of this method looked only at melody notes -
	 * so a transient-only file reported zero amplitude for its whole length and
	 * appeared to do nothing. That is not a rare shape: `Interact_Environment`
	 * ships with two transients and no notes at all.
	 *
	 * A tap is given a short decay window so it registers as a spike rather than
	 * an infinitely thin event that sampling would almost always miss.
	 */
	amplitudeAt(seconds: number): number {
		this.assertNotDisposed();

		let peak = 0;

		for (const melody of this.melodies) {
			if (melody.mute) continue;

			for (const note of melody.notes) {
				const local = seconds - note.position;
				if (local < 0 || local > note.length) continue;

				peak = Math.max(
					peak,
					sampleEnvelope(note.amplitude, local) * note.gain * melody.gain,
				);
			}
		}

		for (const transient of this.transients) {
			const local = seconds - transient.position;
			if (local < 0 || local > TRANSIENT_DECAY_SECONDS) continue;

			// Linear decay: sharp attack, quick fall.
			const decay = 1 - local / TRANSIENT_DECAY_SECONDS;
			peak = Math.max(peak, transient.amplitude * decay);
		}

		return Math.min(1, peak * this.gain);
	}

	/** Normalised pitch 0..1 at `seconds`; multiply through `frequencyRange` for Hz. */
	pitchAt(seconds: number): number {
		this.assertNotDisposed();

		for (const melody of this.melodies) {
			if (melody.mute) continue;

			for (const note of melody.notes) {
				const local = seconds - note.position;
				if (local < 0 || local > note.length) continue;

				return sampleEnvelope(note.pitch, local);
			}
		}

		return 0;
	}

	frequencyAt(seconds: number): number {
		const { min, max } = this.frequencyRange;
		return min + (max - min) * this.pitchAt(seconds);
	}

	protected override async onDispose(): Promise<void> {
		(this as { melodies: readonly HapsMelody[] }).melodies = [];
		(this as { transients: readonly HapsTransient[] }).transients = [];
	}
}
