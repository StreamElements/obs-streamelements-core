/**
 * Renders a `ChromaEffect` the way the WYVRN Effects Library does: by colouring
 * the shaped LED elements inside the real device SVG.
 *
 * This is not a stylistic choice, it is the whole difference between a preview
 * that reads as hardware and one that reads as a spreadsheet. An earlier version
 * of this file painted a uniform grid of squares onto a canvas, which is
 * *arithmetically* the same data and looks nothing like the gallery: a keyboard
 * came out as an 8x24 block of identical cells rather than as keys.
 *
 * The gallery's approach, ported:
 *
 *   1. Inline the device SVG, which carries one shaped element per LED, classed
 *      `led` (and `ledkeys` on the keypad).
 *   2. Build an ordered map from LED index to SVG element. The order is
 *      per-device and hand-tuned - see `buildDeviceMap`.
 *   3. Per frame, walk the colours in order and set `style.fill` on the mapped
 *      element.
 *
 * Frames advance on a wall-clock timer rather than one rAF per frame, because
 * effects declare their own per-frame duration (typically 1/30s) and a display
 * refreshing at 60 or 144 Hz must not play them at its own rate.
 */

import { Disposable } from "./Disposable.js";
import { ChromaEffect } from "./ChromaEffect.js";
import { DEVICE_SVGS } from "./deviceSvgs.generated.js";

/** The gallery's `h-28` wrapper: one height for every device. */
export const CHROMA_BOX_HEIGHT = 112;

/**
 * Per-device width bounds, converted from the gallery's Tailwind classes
 * (`min-w-[10rem] max-w-[14rem] flex-grow` on the keyboard, and so on). Only the
 * keyboard grows, which is what makes it the dominant object in the row.
 */
const DEVICE_BOX: Record<string, { min: number; max: number; grow?: boolean }> = {
	keyboard: { min: 160, max: 224, grow: true },
	mousepad: { min: 80, max: 112 },
	chromalink: { min: 80, max: 112 },
	headset: { min: 64, max: 96 },
	mouse: { min: 48, max: 80 },
	keypad: { min: 48, max: 80 },
};

export interface ChromaRendererOptions {
	/** Loop playback. Default true, as the gallery does. */
	loop?: boolean;
	/** Start immediately. Default true. */
	autoplay?: boolean;
	/** Colour for an LED the effect leaves unlit. Default a dark grey. */
	unlitFill?: string;
}

export class ChromaRenderer extends Disposable {
	readonly effect: ChromaEffect;

	#wrap: HTMLDivElement;
	#svg: SVGElement | null = null;
	#map: SVGElement[] = [];
	#rafHandle = 0;
	#startedAt = 0;
	#pausedAt = 0;
	#playing = false;
	#lastFrame = -1;
	#options: Required<ChromaRendererOptions>;

	constructor(
		host: HTMLElement,
		effect: ChromaEffect,
		options: ChromaRendererOptions = {},
	) {
		super();

		this.effect = effect;
		this.#options = {
			loop: options.loop ?? true,
			autoplay: options.autoplay ?? true,
			unlitFill: options.unlitFill ?? "#1b1f27",
		};

		// Sizing mirrors the gallery exactly: a fixed-height box per device, the
		// SVG filling that height at its natural aspect. Devices are different
		// shapes, so a uniform width would stretch some and shrink others; a
		// common height is what makes a keyboard read as wide and a mouse as
		// small, side by side.
		const bounds = DEVICE_BOX[effect.device.svgKey] ?? { min: 48, max: 112 };

		this.#wrap = document.createElement("div");
		this.#wrap.className = "wyvrn-chroma";
		this.#wrap.style.cssText =
			`height:${CHROMA_BOX_HEIGHT}px;padding:8px;` +
			`min-width:${bounds.min}px;max-width:${bounds.max}px;` +
			(bounds.grow ? "flex-grow:1;" : "") +
			"display:flex;align-items:center;justify-content:center;";

		const svgText = DEVICE_SVGS[effect.device.svgKey];

		if (svgText) {
			this.#wrap.innerHTML = svgText;
			const svg = this.#wrap.querySelector("svg");

			if (svg) {
				this.#svg = svg;
				svg.removeAttribute("width");
				svg.removeAttribute("height");
				svg.setAttribute(
					"style",
					"height:100%;width:auto;max-width:100%;display:block;",
				);
				this.#map = buildDeviceMap(svg, effect.deviceType, effect.deviceId);
			}
		}

		host.appendChild(this.#wrap);
		this.own(() => this.#wrap.remove());

		this.#paintUnlit();

		if (this.#options.autoplay) this.play();
		else this.#drawAt(0);
	}

	/** How many LED elements the artwork actually offers. */
	get mappedLedCount(): number {
		return this.#map.length;
	}

	get playing(): boolean {
		return this.#playing;
	}

	get currentTime(): number {
		if (!this.#playing) return this.#pausedAt;
		return (performance.now() - this.#startedAt) / 1000;
	}

	play(): void {
		this.assertNotDisposed();
		if (this.#playing) return;

		this.#playing = true;
		this.#startedAt = performance.now() - this.#pausedAt * 1000;
		this.#tick();
	}

	/** Halt on the current frame. */
	pause(): void {
		if (!this.#playing) return;

		this.#pausedAt = this.currentTime;
		this.#playing = false;
		this.#cancelFrame();
	}

	/**
	 * Halt, rewind, and darken every LED.
	 *
	 * Present so all three renderers answer to the same three verbs; a caller
	 * driving a whole event should not have to stop the haptics and separately
	 * remember that the lighting only knows how to pause.
	 */
	stop(): void {
		this.#playing = false;
		this.#cancelFrame();
		this.#pausedAt = 0;
		this.#lastFrame = -1;
		this.#paintUnlit();
	}

	seek(seconds: number): void {
		this.assertNotDisposed();

		this.#pausedAt = Math.max(0, seconds);
		if (this.#playing) this.#startedAt = performance.now() - this.#pausedAt * 1000;
		this.#drawAt(this.#pausedAt);
	}

	#cancelFrame(): void {
		if (this.#rafHandle) {
			cancelAnimationFrame(this.#rafHandle);
			this.#rafHandle = 0;
		}
	}

	#tick = (): void => {
		if (this.disposed || !this.#playing) return;

		const t = this.currentTime;
		const total = this.effect.totalDurationSeconds;

		if (!this.#options.loop && total > 0 && t >= total) {
			this.#drawAt(total);
			this.pause();
			return;
		}

		this.#drawAt(t);
		this.#rafHandle = requestAnimationFrame(this.#tick);
	};

	#paintUnlit(): void {
		for (const el of this.#map) {
			el.style.fill = this.#options.unlitFill;
		}
	}

	#drawAt(seconds: number): void {
		if (this.disposed || this.#map.length === 0) return;

		const index = this.effect.frameIndexAt(seconds);
		// Setting fill on up to 278 elements every animation frame is wasted
		// work when the effect only advances 30 times a second.
		if (index === this.#lastFrame) return;
		this.#lastFrame = index;

		const frame = this.effect.frames[index];
		if (!frame) return;

		const colors = frame.colors;
		const n = Math.min(colors.length, this.#map.length);

		for (let i = 0; i < n; i++) {
			const c = colors[i];
			// Same decode the gallery uses: COLORREF, low byte red.
			this.#map[i].style.fill =
				`rgb(${c & 0xff}, ${(c >> 8) & 0xff}, ${(c >> 16) & 0xff})`;
		}
	}

	protected override async onDispose(): Promise<void> {
		this.#playing = false;
		this.#cancelFrame();
		this.#map = [];
		this.#svg = null;
	}
}

/**
 * LED index -> SVG element, ported from the gallery's `ChromaSvgMapper`.
 *
 * The reorderings below are the gallery's own. They are not derivable from the
 * artwork: they encode which physical LED each index corresponds to, and the
 * SVGs happen to declare their elements in a different order.
 */
function buildDeviceMap(
	svg: SVGElement,
	deviceType: number,
	device: number,
): SVGElement[] {
	const find = (className: string): SVGElement[] =>
		Array.from(svg.querySelectorAll<SVGElement>(`.${className}`));

	if (deviceType === 0) {
		// 1D strips.
		switch (device) {
			case 0: {
				// ChromaLink
				const leds = find("led");
				return leds.length >= 5
					? [leds[4], leds[0], leds[1], leds[2], leds[3]]
					: leds;
			}
			case 1: {
				// Headset
				const leds = find("led");
				return leds.length >= 5
					? [leds[3], leds[4], leds[1], leds[2], leds[0]]
					: leds;
			}
			case 2:
				// Mousepad runs the other way round from its declaration order.
				return find("led").reverse();
		}
	} else if (deviceType === 1) {
		// 2D grids.
		switch (device) {
			case 0:
			case 3: {
				// Keyboard, standard and extended, share one artwork. The
				// gallery concatenates the plain LEDs with every key-ish
				// element and fills sequentially - approximate, but it is what
				// produces the look, so it is reproduced rather than improved.
				const leds = find("led");
				const keys = Array.from(
					svg.querySelectorAll<SVGElement>('[class*="key"]'),
				);
				return [...leds, ...keys];
			}
			case 1: {
				// Keypad: its 20 cells are classed `ledkeys`.
				const keys = find("ledkeys");
				keys.push(...find("led"));
				return keys;
			}
			case 2:
				return find("led");
		}
	}

	return find("led");
}
