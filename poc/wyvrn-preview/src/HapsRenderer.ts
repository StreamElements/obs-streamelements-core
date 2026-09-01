/**
 * Renders a `HapsEffect` as a timeline: the amplitude envelope as a filled
 * curve, pitch as a line over it, transients as impulse markers, and a playhead.
 *
 * Timing comes from `SequenceClock`, the same one the body view uses, so the two
 * panels stay in step: one pass, then a rest, then repeat — or a single pass with
 * `isOneShot`. During the rest the playhead parks at the end and audio is silenced,
 * which is the timeline's equivalent of the body going dark.
 *
 * Optionally plays the vibration through WebAudio so the effect can be *felt*
 * approximately rather than only seen. That is off by default and, when on, is
 * the only genuinely async thing to release here: `AudioContext.close()` returns
 * a promise, which is exactly why `dispose()` is async in this codebase.
 */

import { Disposable } from "./Disposable.js";
import { HapsEffect } from "./HapsEffect.js";
import { SequenceClock } from "./SequenceClock.js";

export interface HapsRendererOptions {
	/** Rest between passes, in milliseconds. Default 1000. */
	pauseMs?: number;
	/** Play once and stop, instead of repeating. Default false. */
	isOneShot?: boolean;
	autoplay?: boolean;
	/** Render an audible approximation of the vibration. Default false. */
	audio?: boolean;
	/** Samples across the width when drawing the envelope. Default 600. */
	resolution?: number;
}

export class HapsRenderer extends Disposable {
	readonly effect: HapsEffect;

	#wrap: HTMLDivElement;
	#canvas: HTMLCanvasElement;
	#ctx: CanvasRenderingContext2D;
	#clock: SequenceClock;
	#rafHandle = 0;
	#playing = false;
	#resolution: number;
	#audioWanted: boolean;

	#audioCtx: AudioContext | null = null;
	#osc: OscillatorNode | null = null;
	#gain: GainNode | null = null;

	constructor(
		host: HTMLElement,
		effect: HapsEffect,
		options: HapsRendererOptions = {},
	) {
		super();

		this.effect = effect;
		this.#resolution = options.resolution ?? 600;
		this.#audioWanted = options.audio ?? false;
		this.#clock = new SequenceClock({
			pauseMs: options.pauseMs ?? 1000,
			isOneShot: options.isOneShot ?? false,
		});

		this.#wrap = document.createElement("div");
		this.#wrap.className = "wyvrn-haps";
		this.#wrap.style.cssText =
			"position:relative;width:100%;aspect-ratio:5 / 1;min-height:90px;";

		this.#canvas = document.createElement("canvas");
		this.#canvas.style.cssText = "position:absolute;inset:0;width:100%;height:100%;";
		this.#wrap.appendChild(this.#canvas);
		host.appendChild(this.#wrap);
		this.own(() => this.#wrap.remove());

		const ctx = this.#canvas.getContext("2d");
		if (!ctx) throw new Error("HapsRenderer: 2D canvas context unavailable");
		this.#ctx = ctx;

		const observer = new ResizeObserver(() => this.#resize());
		observer.observe(this.#wrap);
		this.own(() => observer.disconnect());

		this.#resize();

		if (options.autoplay ?? true) void this.play();
		else this.#draw(0);
	}

	get playing(): boolean {
		return this.#playing;
	}

	get isOneShot(): boolean {
		return this.#clock.isOneShot;
	}

	set isOneShot(value: boolean) {
		this.#clock.isOneShot = value;
	}

	/**
	 * Start, or restart from the beginning.
	 *
	 * Deliberately a re-trigger rather than a no-op when already running: firing
	 * an event that is already playing should restart it, which is also how
	 * `SetEventName` behaves.
	 */
	async play(): Promise<void> {
		this.assertNotDisposed();

		if (this.#audioWanted) await this.#startAudio();

		this.#clock.start();

		if (!this.#playing) {
			this.#playing = true;
			this.#tick();
		}
	}

	/** Halt where it is. `play()` restarts from the top. */
	pause(): void {
		if (!this.#playing) return;

		this.#playing = false;
		this.#cancelFrame();
		this.#silence();
	}

	/**
	 * Halt and reset to the beginning.
	 *
	 * Distinct from `pause()` in what it leaves on screen: pause freezes the
	 * current instant, stop returns to a resting state - playhead at the start,
	 * envelope dimmed, audio silenced - so a stopped preview looks stopped rather
	 * than looking like it is mid-effect.
	 */
	stop(): void {
		this.#playing = false;
		this.#cancelFrame();
		this.#silence();
		this.#clock.start();
		this.#draw(0, true);
	}

	async #startAudio(): Promise<void> {
		if (this.#audioCtx) return;

		const AudioCtor =
			globalThis.AudioContext ??
			(globalThis as { webkitAudioContext?: typeof AudioContext })
				.webkitAudioContext;

		if (!AudioCtor) return;

		const audioCtx = new AudioCtor();
		this.#audioCtx = audioCtx;

		// close() is async, so it is registered as an owned teardown rather than
		// fired and forgotten.
		this.own(async () => {
			try {
				await audioCtx.close();
			} catch {
				// Already closed, or the page is going away.
			}
		});

		const osc = audioCtx.createOscillator();
		const gain = audioCtx.createGain();

		osc.type = "sine";
		gain.gain.value = 0;
		osc.connect(gain).connect(audioCtx.destination);
		osc.start();

		this.own(() => {
			try {
				osc.stop();
			} catch {
				// stop() on an already-stopped node throws; harmless.
			}
			osc.disconnect();
			gain.disconnect();
		});

		this.#osc = osc;
		this.#gain = gain;
	}

	#silence(): void {
		if (this.#gain && this.#audioCtx) {
			this.#gain.gain.setTargetAtTime(0, this.#audioCtx.currentTime, 0.01);
		}
	}

	#cancelFrame(): void {
		if (this.#rafHandle) {
			cancelAnimationFrame(this.#rafHandle);
			this.#rafHandle = 0;
		}
	}

	#tick = (): void => {
		if (this.disposed || !this.#playing) return;

		const { time, resting, finished } = this.#clock.sample(
			this.effect.totalDurationSeconds,
		);

		if (finished) {
			this.#draw(time);
			this.pause();
			return;
		}

		if (resting) {
			this.#silence();
		} else if (this.#osc && this.#gain && this.#audioCtx) {
			const now = this.#audioCtx.currentTime;
			// Sub-100 Hz is felt rather than heard; lift it into an audible band
			// so the shape is perceivable through speakers.
			this.#osc.frequency.setTargetAtTime(
				Math.max(40, this.effect.frequencyAt(time)),
				now,
				0.02,
			);
			this.#gain.gain.setTargetAtTime(
				this.effect.amplitudeAt(time) * 0.2,
				now,
				0.02,
			);
		}

		this.#draw(time, resting);
		this.#rafHandle = requestAnimationFrame(this.#tick);
	};

	#resize(): void {
		if (this.disposed) return;

		const dpr = Math.min(window.devicePixelRatio || 1, 2);
		const rect = this.#wrap.getBoundingClientRect();
		if (rect.width === 0 || rect.height === 0) return;

		this.#canvas.width = Math.round(rect.width * dpr);
		this.#canvas.height = Math.round(rect.height * dpr);

		this.#draw(this.#clock.sample(this.effect.totalDurationSeconds).time);
	}

	#draw(seconds: number, resting = false): void {
		if (this.disposed) return;

		const ctx = this.#ctx;
		const { width, height } = this.#canvas;
		const total = this.effect.totalDurationSeconds || 1;
		const pad = height * 0.12;
		const usableH = height - pad * 2;

		ctx.clearRect(0, 0, width, height);

		// Baseline
		ctx.strokeStyle = "rgba(255,255,255,0.12)";
		ctx.lineWidth = Math.max(1, height * 0.008);
		ctx.beginPath();
		ctx.moveTo(0, height - pad);
		ctx.lineTo(width, height - pad);
		ctx.stroke();

		const n = this.#resolution;

		// The whole envelope dims during the rest, so the two panels agree at a
		// glance about whether anything is playing.
		ctx.globalAlpha = resting ? 0.35 : 1;

		const gradient = ctx.createLinearGradient(0, pad, 0, height - pad);
		gradient.addColorStop(0, "rgba(232,163,61,0.85)");
		gradient.addColorStop(1, "rgba(232,163,61,0.06)");

		ctx.beginPath();
		ctx.moveTo(0, height - pad);
		for (let i = 0; i <= n; i++) {
			const t = (i / n) * total;
			ctx.lineTo((i / n) * width, height - pad - this.effect.amplitudeAt(t) * usableH);
		}
		ctx.lineTo(width, height - pad);
		ctx.closePath();
		ctx.fillStyle = gradient;
		ctx.fill();

		// Pitch, as a line.
		ctx.beginPath();
		for (let i = 0; i <= n; i++) {
			const t = (i / n) * total;
			const y = height - pad - this.effect.pitchAt(t) * usableH;
			if (i === 0) ctx.moveTo(0, y);
			else ctx.lineTo((i / n) * width, y);
		}
		ctx.strokeStyle = "rgba(127,166,208,0.8)";
		ctx.lineWidth = Math.max(1, height * 0.012);
		ctx.stroke();

		// Transients: discrete taps, drawn as impulses.
		for (const tr of this.effect.transients) {
			const x = (tr.position / total) * width;
			const y = height - pad - tr.amplitude * usableH;

			ctx.strokeStyle = "rgba(232,234,240,0.55)";
			ctx.lineWidth = Math.max(1, height * 0.014);
			ctx.beginPath();
			ctx.moveTo(x, height - pad);
			ctx.lineTo(x, y);
			ctx.stroke();

			ctx.fillStyle = "rgba(232,234,240,0.95)";
			ctx.beginPath();
			ctx.arc(x, y, Math.max(2, height * 0.022), 0, Math.PI * 2);
			ctx.fill();
		}

		ctx.globalAlpha = 1;

		// Playhead. Parked at the end while resting.
		const px = (Math.min(seconds, total) / total) * width;
		ctx.strokeStyle = resting ? "rgba(255,255,255,0.25)" : "rgba(255,255,255,0.8)";
		ctx.lineWidth = Math.max(1, height * 0.01);
		ctx.beginPath();
		ctx.moveTo(px, 0);
		ctx.lineTo(px, height);
		ctx.stroke();
	}

	protected override async onDispose(): Promise<void> {
		this.#playing = false;
		this.#cancelFrame();
		this.#silence();
		// The AudioContext and its nodes were registered via own(); the base
		// class awaits close() after this returns.
	}
}
