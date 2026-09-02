/**
 * Parser for Razer `.chroma` effect files.
 *
 * Layout, little-endian throughout:
 *
 *     uint32   version
 *     uint8    deviceType      0 = linear strip, 1 = grid
 *     uint8    device          index within that type class
 *     uint32   frameCount
 *     repeat frameCount:
 *         float32  duration    seconds
 *         uint32   color[ledCount]
 *
 * `ledCount` is **not** in the file; it comes from the device table. See
 * ChromaDevices.ts for how those were derived and checked.
 *
 * Colours are Win32 COLORREF — `0x00BBGGRR`, so the low byte is red and the top
 * byte is always zero. Confirmed against real assets, where the top byte is zero
 * in every sample examined.
 */

import { Disposable } from "./Disposable.js";
import { findDevice, deviceFromFileName, type ChromaDevice } from "./ChromaDevices.js";

export interface ChromaFrame {
	/** Frame duration in seconds — typically 1/30. */
	readonly durationSeconds: number;
	/** Raw COLORREF values, `ledCount` of them, row-major for grid devices. */
	readonly colors: Uint32Array;
}

export interface ChromaLoadOptions {
	/** Abort an in-flight load. The effect also aborts on `dispose()`. */
	signal?: AbortSignal;
	/** Injectable for tests. */
	fetchImpl?: typeof fetch;
	/**
	 * Override device detection. Needed only when the header's `(deviceType,
	 * device)` pair is not in the table.
	 */
	device?: ChromaDevice;
}

export class ChromaParseError extends Error {
	constructor(message: string) {
		super(message);
		this.name = "ChromaParseError";
	}
}

export class ChromaEffect extends Disposable {
	readonly version: number;
	readonly deviceType: number;
	readonly deviceId: number;
	readonly device: ChromaDevice;
	readonly frames: readonly ChromaFrame[];
	readonly totalDurationSeconds: number;
	readonly sourceName: string;

	private constructor(init: {
		version: number;
		deviceType: number;
		deviceId: number;
		device: ChromaDevice;
		frames: ChromaFrame[];
		sourceName: string;
	}) {
		super();
		this.version = init.version;
		this.deviceType = init.deviceType;
		this.deviceId = init.deviceId;
		this.device = init.device;
		this.frames = init.frames;
		this.sourceName = init.sourceName;
		this.totalDurationSeconds = init.frames.reduce(
			(sum, f) => sum + f.durationSeconds,
			0,
		);
	}

	static async fromUrl(
		url: string,
		options: ChromaLoadOptions = {},
	): Promise<ChromaEffect> {
		const doFetch = options.fetchImpl ?? globalThis.fetch;

		// Our own controller so dispose()/timeout can abort, chained to any
		// caller-supplied signal.
		const controller = new AbortController();
		const onAbort = () => controller.abort(options.signal?.reason);
		options.signal?.addEventListener("abort", onAbort, { once: true });

		try {
			const response = await doFetch(url, { signal: controller.signal });

			if (!response.ok) {
				throw new ChromaParseError(
					`${url}: HTTP ${response.status} ${response.statusText}`,
				);
			}

			const buffer = await response.arrayBuffer();
			const name = url.split("/").pop() ?? url;

			return ChromaEffect.fromArrayBuffer(buffer, name, options);
		} finally {
			options.signal?.removeEventListener("abort", onAbort);
		}
	}

	static fromArrayBuffer(
		buffer: ArrayBuffer,
		sourceName = "(memory)",
		options: ChromaLoadOptions = {},
	): ChromaEffect {
		if (buffer.byteLength < 10) {
			throw new ChromaParseError(
				`${sourceName}: too short to be a .chroma file (${buffer.byteLength} bytes)`,
			);
		}

		const view = new DataView(buffer);
		let offset = 0;

		const version = view.getUint32(offset, true);
		offset += 4;
		const deviceType = view.getUint8(offset);
		offset += 1;
		const deviceId = view.getUint8(offset);
		offset += 1;
		const frameCount = view.getUint32(offset, true);
		offset += 4;

		const device =
			options.device ??
			findDevice(deviceType, deviceId) ??
			deviceFromFileName(sourceName);

		if (!device) {
			throw new ChromaParseError(
				`${sourceName}: unknown device (deviceType=${deviceType}, device=${deviceId}); ` +
					`cannot determine LED count, which is not stored in the file`,
			);
		}

		const ledCount = device.ledCount;
		const expected = 10 + frameCount * (4 + ledCount * 4);

		if (buffer.byteLength !== expected) {
			// Not fatal by itself, but it almost always means the wrong device
			// geometry, which renders as noise rather than failing. Say so.
			throw new ChromaParseError(
				`${sourceName}: size mismatch — ${buffer.byteLength} bytes, but ` +
					`${frameCount} frames x ${ledCount} LEDs (${device.key}) implies ${expected}. ` +
					`The device table is probably wrong for (deviceType=${deviceType}, device=${deviceId}).`,
			);
		}

		const frames: ChromaFrame[] = [];

		for (let i = 0; i < frameCount; i++) {
			const durationSeconds = view.getFloat32(offset, true);
			offset += 4;

			const colors = new Uint32Array(ledCount);
			for (let led = 0; led < ledCount; led++) {
				colors[led] = view.getUint32(offset, true);
				offset += 4;
			}

			frames.push({ durationSeconds, colors });
		}

		return new ChromaEffect({
			version,
			deviceType,
			deviceId,
			device,
			frames,
			sourceName,
		});
	}

	/**
	 * Index of the frame playing at `seconds`, honouring each frame's own
	 * declared duration and wrapping. Returns -1 when there are no frames.
	 *
	 * Renderers want the index rather than the frame so they can skip repainting
	 * when the effect has not advanced: at 30 fps on a 144 Hz display, four out
	 * of five animation frames show the same thing.
	 */
	frameIndexAt(seconds: number): number {
		this.assertNotDisposed();

		if (this.frames.length === 0) return -1;

		const total = this.totalDurationSeconds;
		if (total <= 0) return 0;

		let t = seconds % total;
		if (t < 0) t += total;

		for (let i = 0; i < this.frames.length; i++) {
			if (t < this.frames[i].durationSeconds) return i;
			t -= this.frames[i].durationSeconds;
		}

		return this.frames.length - 1;
	}

	/** The frame playing at `seconds`. */
	frameAt(seconds: number): ChromaFrame | undefined {
		const i = this.frameIndexAt(seconds);
		return i < 0 ? undefined : this.frames[i];
	}

	/** COLORREF `0x00BBGGRR` -> `{ r, g, b }`. */
	static decodeColor(colorref: number): { r: number; g: number; b: number } {
		return {
			r: colorref & 0xff,
			g: (colorref >> 8) & 0xff,
			b: (colorref >> 16) & 0xff,
		};
	}

	protected override async onDispose(): Promise<void> {
		// Frames are plain typed arrays; dropping the reference is the whole
		// job. Kept explicit so a future addition has an obvious home.
		(this as { frames: readonly ChromaFrame[] }).frames = [];
	}
}
