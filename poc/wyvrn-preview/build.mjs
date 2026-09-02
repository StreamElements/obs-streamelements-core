/**
 * Build: TypeScript -> browser ESM, and a single self-contained demo page.
 *
 * Uses Node's own `module.stripTypeScriptTypes()` rather than a toolchain, so
 * this runs on a clean machine with no `npm install`. The source is therefore
 * restricted to *erasable* TypeScript — type annotations, interfaces, `type`
 * imports. No enums, no parameter properties, no namespaces, since stripping
 * cannot emit runtime code for those.
 *
 *   node build.mjs                       # -> dist/
 *   node build.mjs --demo <chroma> <haps>  # also inlines those files into the demo
 */

import { stripTypeScriptTypes } from "node:module";
import { readFile, writeFile, mkdir, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import path from "node:path";

const root = path.dirname(fileURLToPath(import.meta.url));
const srcDir = path.join(root, "src");
const distDir = path.join(root, "dist");

await mkdir(distDir, { recursive: true });

const files = (await readdir(srcDir)).filter((f) => f.endsWith(".ts"));
const modules = new Map();

for (const file of files) {
	const ts = await readFile(path.join(srcDir, file), "utf8");
	const js = stripTypeScriptTypes(ts, { mode: "strip" });
	const outName = file.replace(/\.ts$/, ".js");

	await writeFile(path.join(distDir, outName), js, "utf8");
	modules.set(outName, js);
}

console.log(`built ${modules.size} module(s) -> dist/`);

// ---------------------------------------------------------------- demo page

const args = process.argv.slice(2);
const manifestIndex = args.indexOf("--manifest");

let embedded = null;

if (manifestIndex !== -1) {
	// A manifest rather than a file list, because the body view needs each haps
	// source paired with the body regions it targets - and that pairing lives in
	// the WYVRN config, not in the .haps file itself.
	const manifest = JSON.parse(await readFile(args[manifestIndex + 1], "utf8"));

	const chroma = [];
	for (const p of manifest.chroma ?? []) {
		const buf = await readFile(p);
		chroma.push({
			name: path.basename(p),
			url: `data:application/octet-stream;base64,${buf.toString("base64")}`,
		});
	}

	const haps = [];
	for (const entry of manifest.haps ?? []) {
		const buf = await readFile(entry.path);
		haps.push({
			name: path.basename(entry.path),
			url: `data:application/json;base64,${buf.toString("base64")}`,
			targeting: entry.targeting ?? [],
		});
	}

	embedded = { chroma, haps, title: manifest.title ?? null };
	console.log(
		`embedded ${chroma.length} .chroma and ${haps.length} .haps source(s) into the demo`,
	);
}

// Flatten the modules into one script: the demo has to work from file:// and
// from inside a sandboxed page, where cross-file ESM imports are awkward.
const order = [
	"Disposable.js",
	"SequenceClock.js",
	"deviceSvgs.generated.js",
	"ChromaDevices.js",
	"ChromaEffect.js",
	"HapsEffect.js",
	"ChromaRenderer.js",
	"HapsRenderer.js",
	"HapticBodyRenderer.js",
];

const bundle = order
	.map((name) => {
		const js = modules.get(name);
		if (!js) throw new Error(`build: missing module ${name}`);
		// Strip module syntax; everything ends up in one scope.
		return js
			.replace(/^\s*import[^;]+;$/gm, "")
			.replace(/^export\s+/gm, "")
			.replace(/^\s*export\s*\{[^}]*\}\s*;?\s*$/gm, "");
	})
	.join("\n\n");

const template = await readFile(path.join(root, "demo", "index.html"), "utf8");

const html = template
	.replace("/*__BUNDLE__*/", bundle)
	.replace("/*__EMBEDDED__*/", JSON.stringify(embedded));

await writeFile(path.join(distDir, "demo.html"), html, "utf8");
console.log("wrote dist/demo.html");
