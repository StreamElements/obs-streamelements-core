/**
 * Plays a haptic sequence across a seated figure, front and side view, so the
 * *where* of an effect is visible and not just the *when*.
 *
 * The histogram beside it answers "what does this feel like over time". This
 * answers "where on the body does it land", which the timeline cannot show:
 * WYVRN events target body regions, and one event routinely drives several at
 * once. From a real config:
 *
 *     "Targeting": [
 *       { "Gain": 1, "Spatialization": "Global", "Target": "Chest" },
 *       { "Gain": 1, "Spatialization": "Global", "Target": "Hand"  },
 *       { "Gain": 1, "Spatialization": "Global", "Target": "Waist" }
 *     ]
 *
 * So this renderer takes **multiple sources**, each a haps effect plus the
 * regions it targets, and lights each region with the strongest contribution at
 * that instant.
 *
 * Playback is one pass, then a 1000 ms rest, then repeat - the pause is what
 * makes a short effect readable, since a tight loop reads as continuous buzz.
 */

import { Disposable } from "./Disposable.js";
import { HapsEffect } from "./HapsEffect.js";
import { SequenceClock } from "./SequenceClock.js";

/** Regions the artwork can light. */
export type BodyRegion =
	| "head"
	| "chest"
	| "waist"
	| "handLeft"
	| "handRight"
	| "legLeft"
	| "legRight";

const ALL_REGIONS: readonly BodyRegion[] = [
	"head",
	"chest",
	"waist",
	"handLeft",
	"handRight",
	"legLeft",
	"legRight",
];

export interface HapticTargeting {
	/** `Target` from the config: Head, Chest, Waist, Hand, Leg, All, Top, Down. */
	target: string;
	/** `Spatialization`: Global, Left or Right. */
	spatialization?: string;
	gain?: number;
}

export interface HapticSource {
	effect: HapsEffect;
	targeting: HapticTargeting[];
	/** Shown in the source list; defaults to the effect's filename. */
	label?: string;
}

export interface HapticBodyRendererOptions {
	/** Rest between passes, in milliseconds. Default 1000. */
	pauseMs?: number;
	/** Play once and stop, instead of repeating. Default false. */
	isOneShot?: boolean;
	autoplay?: boolean;
}

/**
 * Config `Target` -> regions.
 *
 * Case is normalised and `Wasit` is accepted, because both appear in shipped
 * configs (25 lowercase `waist`, 22 misspelled). Dropping them would silently
 * unlight a region that the effect really does drive.
 */
function regionsFor(targeting: HapticTargeting): BodyRegion[] {
	const raw = (targeting.target ?? "").trim().toLowerCase();
	const target = raw === "wasit" ? "waist" : raw;
	const side = (targeting.spatialization ?? "Global").trim().toLowerCase();

	const paired = (left: BodyRegion, right: BodyRegion): BodyRegion[] => {
		if (side === "left") return [left];
		if (side === "right") return [right];
		return [left, right];
	};

	switch (target) {
		case "head":
			return ["head"];
		case "chest":
			return ["chest"];
		case "waist":
			return ["waist"];
		case "hand":
			return paired("handLeft", "handRight");
		case "leg":
			return paired("legLeft", "legRight");
		case "top":
			return ["head", "chest"];
		case "down":
			return ["waist", "legLeft", "legRight"];
		case "all":
			return [...ALL_REGIONS];
		default:
			return [];
	}
}

export class HapticBodyRenderer extends Disposable {
	#wrap: HTMLDivElement;
	#sources: HapticSource[] = [];
	#regionEls = new Map<BodyRegion, SVGElement[]>();
	#rafHandle = 0;
	#playing = false;
	#clock: SequenceClock;

	constructor(host: HTMLElement, options: HapticBodyRendererOptions = {}) {
		super();

		// The same clock the timeline uses, so the two panels cannot drift apart.
		this.#clock = new SequenceClock({
			pauseMs: options.pauseMs ?? 1000,
			isOneShot: options.isOneShot ?? false,
		});

		this.#wrap = document.createElement("div");
		this.#wrap.className = "wyvrn-body";
		this.#wrap.style.cssText =
			"display:flex;gap:8px;align-items:flex-end;justify-content:center;width:100%;";
		this.#wrap.innerHTML = FRONT_VIEW + SIDE_VIEW;

		host.appendChild(this.#wrap);
		this.own(() => this.#wrap.remove());

		// Both views contribute elements for the same region, so a region maps to
		// a list rather than a single node.
		for (const region of ALL_REGIONS) {
			const els = Array.from(
				this.#wrap.querySelectorAll<SVGElement>(`[data-region="${region}"]`),
			);
			this.#regionEls.set(region, els);
		}

		this.#setAll(0);

		if (options.autoplay ?? true) void this.play();
	}

	/** Total sequence length, the longest source. */
	get durationSeconds(): number {
		return this.#sources.reduce(
			(max, s) => Math.max(max, s.effect.totalDurationSeconds),
			0,
		);
	}

	get sources(): readonly HapticSource[] {
		return this.#sources;
	}

	/**
	 * Add a source. The renderer does **not** take ownership of the effect: the
	 * caller may be showing the same one in the timeline beside this, and
	 * disposing it here would pull it out from under them.
	 */
	addSource(source: HapticSource): void {
		this.assertNotDisposed();
		this.#sources.push(source);
	}

	clearSources(): void {
		this.#sources = [];
		this.#setAll(0);
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
	 * Async only so both haptics previews present the same signature - there is
	 * nothing to await here, unlike the timeline, which may be opening an
	 * AudioContext. A caller driving both should not have to remember which is
	 * which.
	 */
	async play(): Promise<void> {
		this.assertNotDisposed();

		this.#clock.start();

		if (!this.#playing) {
			this.#playing = true;
			this.#tick();
		}
	}

	/** Halt where it is, leaving whatever is currently lit. */
	pause(): void {
		this.#playing = false;
		this.#cancelFrame();
	}

	/** Halt, reset to the beginning, and go dark. */
	stop(): void {
		this.#playing = false;
		this.#cancelFrame();
		this.#clock.start();
		this.#setAll(0);
	}

	#cancelFrame(): void {
		if (this.#rafHandle) {
			cancelAnimationFrame(this.#rafHandle);
			this.#rafHandle = 0;
		}
	}

	#tick = (): void => {
		if (this.disposed || !this.#playing) return;

		const { time, resting, finished } = this.#clock.sample(this.durationSeconds);

		// Dark during the rest, and dark once a one-shot run is over: in both
		// cases nothing is being felt, so nothing should be lit.
		if (resting || finished) this.#setAll(0);
		else this.#applyAt(time);

		if (finished) {
			this.pause();
			return;
		}

		this.#rafHandle = requestAnimationFrame(this.#tick);
	};

	#setAll(level: number): void {
		for (const region of ALL_REGIONS) this.#setRegion(region, level);
	}

	#setRegion(region: BodyRegion, level: number): void {
		const els = this.#regionEls.get(region);
		if (!els) return;

		const clamped = Math.max(0, Math.min(1, level));

		for (const el of els) {
			// Opacity carries intensity; the blur filter on the group turns it
			// into a glow rather than a flat wash.
			el.style.opacity = String(clamped);
		}
	}

	#applyAt(seconds: number): void {
		const level = new Map<BodyRegion, number>();

		for (const source of this.#sources) {
			const amplitude = source.effect.amplitudeAt(seconds);
			if (amplitude <= 0) continue;

			for (const targeting of source.targeting) {
				const gain = targeting.gain ?? 1;
				const value = amplitude * gain;

				for (const region of regionsFor(targeting)) {
					// Overlapping sources take the strongest rather than summing:
					// two effects at 0.6 is not 1.2 of anything.
					level.set(region, Math.max(level.get(region) ?? 0, value));
				}
			}
		}

		for (const region of ALL_REGIONS) {
			this.#setRegion(region, level.get(region) ?? 0);
		}
	}

	protected override async onDispose(): Promise<void> {
		this.#playing = false;
		this.#cancelFrame();
		this.#regionEls.clear();
		// Sources are borrowed, not owned - see addSource.
		this.#sources = [];
	}
}

/*
 * Artwork.
 *
 * A seated figure in headphones on an angular gaming chair, front and side.
 * Each glowing area is a shape carrying `data-region`, drawn twice - once into a
 * blurred group for the bloom, once crisp on top - so raising opacity reads as
 * light coming *from* the body rather than a sticker stuck on it.
 *
 * Deliberately schematic: it has to stay legible around 180px tall and read
 * instantly as "a person in a chair", which rules out anatomical detail. The
 * chair is drawn first and darkest so the body separates from it, and the
 * headphones are exaggerated because they are the one prop that tells you at a
 * glance which way the figure faces.
 */

const GLOW = "#b06bff";
const CHAIR = "#191d26";
const CHAIR_EDGE = "#2f3648";
const BODY = "#333b4d";
const GEAR = "#454f66";

function view(id: string, label: string, body: string): string {
	return `
<figure style="margin:0;display:flex;flex-direction:column;align-items:center;gap:8px;flex:1 1 0;min-width:0;">
  <svg viewBox="0 0 200 250" style="width:100%;height:auto;max-height:210px;display:block;" aria-label="Haptics on the body, ${label} view">
    <defs>
      <filter id="glow-${id}" x="-70%" y="-70%" width="240%" height="240%">
        <feGaussianBlur stdDeviation="6" result="b"/>
        <feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="b"/></feMerge>
      </filter>
    </defs>
    ${body}
  </svg>
</figure>`;
}

function layered(
	id: string,
	label: string,
	chair: string,
	figure: string,
	regions: string,
): string {
	return view(
		id,
		label,
		`<g>${chair}</g>
		 <g>${figure}</g>
		 <g filter="url(#glow-${id})" fill="${GLOW}">${regions}</g>
		 <g fill="${GLOW}">${regions}</g>`,
	);
}

const FRONT_REGIONS = `
  <ellipse data-region="head"      opacity="0" cx="100" cy="48"  rx="17" ry="19"/>
  <rect    data-region="chest"     opacity="0" x="80"  y="80"  width="40" height="34" rx="12"/>
  <rect    data-region="waist"     opacity="0" x="83"  y="116" width="34" height="22" rx="10"/>
  <ellipse data-region="handLeft"  opacity="0" cx="62"  cy="146" rx="10" ry="9"/>
  <ellipse data-region="handRight" opacity="0" cx="138" cy="146" rx="10" ry="9"/>
  <rect    data-region="legLeft"   opacity="0" x="79"  y="158" width="17" height="52" rx="8"/>
  <rect    data-region="legRight"  opacity="0" x="104" y="158" width="17" height="52" rx="8"/>`;

const FRONT_VIEW = layered(
	"front",
	"front",
	// Wraparound backrest with raised wings, headrest above, seat, pedestal.
	`<path d="M52 62 Q52 30 74 26 L126 26 Q148 30 148 62 L148 150 L52 150 Z"
	       fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M52 68 Q42 76 45 110 L58 110 L58 72 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M148 68 Q158 76 155 110 L142 110 L142 72 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <rect x="80" y="16" width="40" height="20" rx="9" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M58 148 L142 148 L136 178 L64 178 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <rect x="95" y="178" width="10" height="28" fill="${CHAIR_EDGE}"/>
	 <path d="M66 226 L134 226 L142 236 L58 236 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M100 206 L70 228 M100 206 L130 228" stroke="${CHAIR_EDGE}" stroke-width="3" fill="none"/>`,
	// Figure: shoulders, torso, head, headphones, arms, thighs.
	`<path d="M84 68 L116 68 L124 84 L76 84 Z" fill="${BODY}"/>
	 <rect x="78" y="78" width="44" height="64" rx="16" fill="${BODY}"/>
	 <ellipse cx="100" cy="48" rx="17" ry="19" fill="${BODY}"/>
	 <path d="M79 45 A21 21 0 0 1 121 45" fill="none" stroke="${GEAR}" stroke-width="6" stroke-linecap="round"/>
	 <rect x="71" y="40" width="13" height="22" rx="6.5" fill="${GEAR}"/>
	 <rect x="116" y="40" width="13" height="22" rx="6.5" fill="${GEAR}"/>
	 <rect x="60" y="88" width="15" height="54" rx="7.5" fill="${BODY}"/>
	 <rect x="125" y="88" width="15" height="54" rx="7.5" fill="${BODY}"/>
	 <ellipse cx="62" cy="146" rx="10" ry="9" fill="${BODY}"/>
	 <ellipse cx="138" cy="146" rx="10" ry="9" fill="${BODY}"/>
	 <rect x="79" y="150" width="17" height="58" rx="8" fill="${BODY}"/>
	 <rect x="104" y="150" width="17" height="58" rx="8" fill="${BODY}"/>`,
	FRONT_REGIONS,
);

const SIDE_REGIONS = `
  <ellipse data-region="head"      opacity="0" cx="98"  cy="46"  rx="18" ry="19"/>
  <rect    data-region="chest"     opacity="0" x="80"  y="76"  width="36" height="34" rx="12"/>
  <rect    data-region="waist"     opacity="0" x="80"  y="112" width="40" height="22" rx="10"/>
  <ellipse data-region="handLeft"  opacity="0" cx="148" cy="118" rx="9" ry="8"/>
  <ellipse data-region="handRight" opacity="0" cx="148" cy="118" rx="9" ry="8"/>
  <rect    data-region="legLeft"   opacity="0" x="112" y="136" width="52" height="17" rx="8"/>
  <rect    data-region="legRight"  opacity="0" x="112" y="136" width="52" height="17" rx="8"/>`;

const SIDE_VIEW = layered(
	"side",
	"side",
	// Profile: raked backrest and headrest, seat pan, armrest, gas lift, base.
	`<path d="M64 152 L60 62 Q60 32 80 26 L96 22 L100 40 L84 48 Q78 62 80 152 Z"
	       fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M72 26 Q88 16 104 20 L100 40 L76 42 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M64 150 L168 144 L170 166 L66 172 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M106 106 L154 102 L156 114 L108 118 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <rect x="96" y="170" width="10" height="30" fill="${CHAIR_EDGE}"/>
	 <path d="M64 224 L142 224 L150 234 L56 234 Z" fill="${CHAIR}" stroke="${CHAIR_EDGE}" stroke-width="2"/>
	 <path d="M101 200 L70 226 M101 200 L134 226" stroke="${CHAIR_EDGE}" stroke-width="3" fill="none"/>`,
	// Profile figure facing right: torso reclined, upper arm down, forearm out to
	// a desk, thigh horizontal, shin down, foot forward.
	`<path d="M84 64 L110 64 L118 82 L80 82 Z" fill="${BODY}"/>
	 <rect x="78" y="74" width="40" height="64" rx="16" fill="${BODY}"/>
	 <ellipse cx="98" cy="46" rx="18" ry="19" fill="${BODY}"/>
	 <path d="M81 43 A20 20 0 0 1 114 38" fill="none" stroke="${GEAR}" stroke-width="6" stroke-linecap="round"/>
	 <ellipse cx="94" cy="50" rx="8" ry="11" fill="${GEAR}"/>
	 <rect x="106" y="86" width="46" height="15" rx="7.5" transform="rotate(10 106 86)" fill="${BODY}"/>
	 <ellipse cx="148" cy="118" rx="9" ry="8" fill="${BODY}"/>
	 <rect x="106" y="134" width="60" height="18" rx="9" fill="${BODY}"/>
	 <rect x="154" y="146" width="17" height="44" rx="8" fill="${BODY}"/>
	 <rect x="152" y="186" width="32" height="10" rx="5" fill="${BODY}"/>`,
	SIDE_REGIONS,
);
