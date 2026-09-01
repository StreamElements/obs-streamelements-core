/**
 * Verifies the parsers against real assets. The parsers are DOM-free by design,
 * so this runs in plain Node - no browser, no jsdom.
 *
 *   node test.mjs "C:\\Program Files (x86)\\Interhaptics\\hapticFolders"
 */

import { readFile, readdir } from "node:fs/promises";
import path from "node:path";
import { ChromaEffect } from "./dist/ChromaEffect.js";
import { HapsEffect, sampleEnvelope } from "./dist/HapsEffect.js";
import { CHROMA_DEVICES } from "./dist/ChromaDevices.js";
import { Disposable, DisposableBag } from "./dist/Disposable.js";
import { SequenceClock } from "./dist/SequenceClock.js";

const root =
	process.argv[2] ?? "C:\\Program Files (x86)\\Interhaptics\\hapticFolders";

let pass = 0;
let fail = 0;

function check(cond, label, detail = "") {
	if (cond) {
		pass++;
	} else {
		fail++;
		console.log(`  FAIL  ${label}${detail ? " - " + detail : ""}`);
	}
}

// ---------------------------------------------------------------- disposal

{
	const order = [];

	class Child extends Disposable {
		constructor(name) {
			super();
			this.name = name;
		}
		async onDispose() {
			order.push(this.name);
		}
	}

	const bag = new DisposableBag();
	bag.add(new Child("first"));
	bag.add(new Child("second"));
	bag.add(() => order.push("fn"));

	await bag.dispose();
	check(
		order.join(",") === "fn,second,first",
		"disposal is reverse-order",
		order.join(","),
	);
	check(bag.disposed, "disposed flag set");

	await bag.dispose();
	check(order.length === 3, "dispose is idempotent", `ran ${order.length} teardowns`);

	// Concurrent dispose must not run teardown twice.
	const bag2 = new DisposableBag();
	let count = 0;
	bag2.add(() => {
		count++;
	});
	await Promise.all([bag2.dispose(), bag2.dispose(), bag2.dispose()]);
	check(count === 1, "concurrent dispose runs teardown once", `count=${count}`);

	// A failing teardown must not strand the others.
	const bag3 = new DisposableBag();
	let after = false;
	bag3.add(() => {
		after = true;
	});
	bag3.add(() => {
		throw new Error("boom");
	});
	const errs = [];
	const realError = console.error;
	console.error = (...a) => errs.push(a);
	await bag3.dispose();
	console.error = realError;
	check(after, "a throwing teardown does not strand the rest");
	check(errs.length === 1, "the failure is reported", `${errs.length} logged`);

	// Acquiring after disposal must fail loudly rather than leak.
	const bag4 = new DisposableBag();
	await bag4.dispose();
	let threw = false;
	try {
		bag4.add(() => {});
	} catch {
		threw = true;
	}
	check(threw, "acquiring after dispose throws");
}


// ------------------------------------------------------------ sequence clock

{
	// Both haptics previews share this, so a wrong rule desynchronises the
	// timeline from the body view rather than merely looking odd in one panel.
	const D = 0.4;              // 400 ms effect
	const PAUSE = 1000;
	const clock = new SequenceClock({ pauseMs: PAUSE });
	clock.start(0);

	const at = (ms) => clock.sample(D, ms);

	check(at(0).time === 0 && !at(0).resting, "clock starts at 0, playing");
	check(Math.abs(at(200).time - 0.2) < 1e-9, "clock advances in seconds");
	check(!at(399).resting, "still playing just before the end");
	check(at(400).resting, "rests exactly at the duration");
	check(at(1399).resting, "still resting just before the cycle ends");

	// Wraps into the next pass, not into the middle of one.
	const wrapped = at(1400);
	check(!wrapped.resting && wrapped.time < 1e-6, "wraps to the start of the next pass");
	check(Math.abs(at(1600).time - 0.2) < 1e-9, "second pass tracks like the first");

	// The playhead parks at the end during the rest rather than snapping to 0.
	check(Math.abs(at(900).time - D) < 1e-9, "playhead parks at the end while resting");

	// Never reports finished while repeating, however long it runs.
	check(!at(60_000).finished, "a repeating clock never finishes");

	// One-shot.
	const once = new SequenceClock({ pauseMs: PAUSE, isOneShot: true });
	once.start(0);
	check(!once.sample(D, 200).finished, "one-shot is not finished mid-pass");
	check(!once.sample(D, 200).resting, "one-shot never rests");
	check(once.sample(D, 400).finished, "one-shot finishes at the duration");
	check(once.sample(D, 5000).finished, "one-shot stays finished");
	check(
		Math.abs(once.sample(D, 5000).time - D) < 1e-9,
		"a finished one-shot reports the end, not a wrapped time",
	);

	// A zero-length effect must not divide by a zero cycle.
	const empty = new SequenceClock();
	empty.start(0);
	const e = empty.sample(0, 1234);
	check(e.time === 0 && !e.resting, "a zero-length effect sits at 0 without resting");

	// start() re-triggers rather than continuing.
	const re = new SequenceClock({ pauseMs: PAUSE });
	re.start(0);
	re.start(1000);
	check(Math.abs(re.sample(D, 1200).time - 0.2) < 1e-9, "start() restarts the cycle");

	// oneShot is switchable after construction, since the demo toggles it.
	const sw = new SequenceClock({ pauseMs: PAUSE });
	sw.start(0);
	sw.isOneShot = true;
	check(sw.sample(D, 900).finished, "isOneShot can be set after construction");
}

// ------------------------------------------------------------ real assets

// Recurse: GenericEvent nests two levels deep (GenericEvent/Idle/Idle_Rainbow),
// and a single-level walk silently found zero .haps files there - which is
// exactly where the m_-prefixed schema lives, so it went untested.
async function walk(dir, depth = 0) {
	if (depth > 4) return [];
	let entries;
	try {
		entries = await readdir(dir, { withFileTypes: true });
	} catch {
		return [];
	}
	const out = [];
	for (const e of entries) {
		const full = path.join(dir, e.name);
		if (e.isDirectory()) out.push(...(await walk(full, depth + 1)));
		else out.push(full);
	}
	return out;
}

const allFiles = await walk(root);
const folders = await readdir(root, { withFileTypes: true });
let chromaTested = 0;
let hapsTested = 0;
const deviceSeen = new Map();
const unknownPairs = new Map();
let mSchema = 0;

for (const full of allFiles) {
	const name = path.basename(full);
	const shortName = path.relative(root, full);

	if (name.toLowerCase().endsWith(".chroma") && chromaTested < 900) {
		const buf = await readFile(full);
		const ab = buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);

		try {
			const fx = ChromaEffect.fromArrayBuffer(ab, name);
			deviceSeen.set(fx.device.key, (deviceSeen.get(fx.device.key) ?? 0) + 1);
			chromaTested++;

			if (chromaTested === 1) {
				check(fx.frames.length > 0, "chroma has frames");
				check(fx.totalDurationSeconds > 0, "chroma has a duration");
				check(fx.frameAt(0) === fx.frames[0], "frameAt(0) is the first frame");
				check(
					fx.frameAt(fx.totalDurationSeconds * 2 + 0.0001) !== undefined,
					"frameAt wraps past the end",
				);
				check(fx.frameAt(-1) !== undefined, "frameAt handles negative time");

				const c = ChromaEffect.decodeColor(0x00123456);
				check(
					c.r === 0x56 && c.g === 0x34 && c.b === 0x12,
					"COLORREF decodes low byte as red",
					JSON.stringify(c),
				);
			}
		} catch (err) {
			fail++;
			const dv = new DataView(ab);
			const frames = dv.getUint32(6, true);
			unknownPairs.set(
				`deviceType=${dv.getUint8(4)} device=${dv.getUint8(5)}`,
				`implies ${frames > 0 ? (ab.byteLength - 10) / frames / 4 - 1 : NaN} LEDs/frame`,
			);
			console.log(`  FAIL  ${shortName}: ${err.message}`);
		}
	} else if (name.toLowerCase().endsWith(".haps") && hapsTested < 900) {
		const json = await readFile(full, "utf8");

		try {
			const fx = HapsEffect.fromJson(json, name);
			hapsTested++;
			if (/"m_vibration"/.test(json)) mSchema++;

			const a = fx.amplitudeAt(fx.totalDurationSeconds / 2);
			check(a >= 0 && a <= 1, "haps amplitude stays in 0..1", `${name} -> ${a}`);

			const hz = fx.frequencyAt(0);
			check(
				hz >= fx.frequencyRange.min && hz <= fx.frequencyRange.max,
				"haps frequency stays in range",
				`${name} -> ${hz}`,
			);
		} catch (err) {
			fail++;
			console.log(`  FAIL  ${shortName}: ${err.message}`);
		}
	}
}

// Envelope edge cases the real files exercise: a negative first keyframe, and
// coincident keyframes.
{
	const env = {
		gain: 1,
		interpolationFunction: "Linear",
		keyframes: [
			{ position: -0.003, value: 0 },
			{ position: 0.5, value: 1 },
			{ position: 0.5, value: 0.25 },
			{ position: 1, value: 0 },
		],
	};
	check(sampleEnvelope(env, -10) === 0, "envelope clamps before the first keyframe");
	check(sampleEnvelope(env, 10) === 0, "envelope clamps after the last keyframe");
	check(
		Number.isFinite(sampleEnvelope(env, 0.5)),
		"coincident keyframes do not divide by zero",
	);
}

// Malformed input must throw a typed error, not crash.
{
	try {
		ChromaEffect.fromArrayBuffer(new ArrayBuffer(4), "tiny.chroma");
		check(false, "short chroma throws");
	} catch (err) {
		check(err.name === "ChromaParseError", "short chroma throws ChromaParseError");
	}

	try {
		HapsEffect.fromJson("{not json", "bad.haps");
		check(false, "bad json throws");
	} catch (err) {
		check(err.name === "HapsParseError", "bad json throws HapsParseError");
	}

	const empty = HapsEffect.fromJson("{}", "empty.haps");
	check(empty.melodies.length === 0, "an empty haps parses to zero melodies");
	check(empty.totalDurationSeconds === 0, "an empty haps has zero duration");
}

console.log("");
console.log(`  .chroma parsed : ${chromaTested}`);
console.log(`  .haps parsed   : ${hapsTested}  (${mSchema} using the m_ schema)`);
console.log(
	`  devices seen   : ${[...deviceSeen]
		.sort()
		.map(([k, n]) => `${k} x${n}`)
		.join(", ")}`,
);

if (unknownPairs.size) {
	console.log("  UNKNOWN devices:");
	for (const [k, v] of unknownPairs) console.log(`     ${k}  ${v}`);
}

console.log(
	`  device table   : ${CHROMA_DEVICES.length} entries (${CHROMA_DEVICES.map(
		(d) => d.ledCount,
	).join("/")} LEDs)`,
);
console.log("");
console.log(`  ${pass} passed, ${fail} failed`);

process.exit(fail === 0 ? 0 : 1);
