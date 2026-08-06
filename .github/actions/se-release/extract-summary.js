#!/usr/bin/env node
//
// Pull the final assistant text out of claude-code-base-action's execution file.
//
// The released base action (v0.0.63) predates the `claude_args` /
// `structured_output` interface that only exists on main: its outputs are
// `conclusion` and `execution_file` and nothing else. So there is no JSON schema
// to constrain the reply and no structured field to read -- the blurb is simply
// the model's final text, and this recovers it from the transcript.
//
// argv: <execution file> <output file>
//
// Never fails the build. A missing file, malformed JSON, or an errored run all
// produce an empty output file, and the release is then composed from notes and
// changelog alone.

const fs = require('fs');

const [, , execPath, outPath] = process.argv;
const warn = (m) => console.log(`::warning title=Release summary unavailable::${m}`);

function extract(entries) {
	// Prefer the terminal result event; it is what the CLI reports as the answer.
	for (let i = entries.length - 1; i >= 0; i--) {
		const e = entries[i];
		if (e && e.type === 'result') {
			if (e.is_error) {
				warn(`Claude reported an error: ${String(e.result ?? '').slice(0, 300)}`);
				return '';
			}
			if (typeof e.result === 'string' && e.result.trim()) return e.result;
		}
	}
	// Fall back to the last assistant turn if no result event was written.
	for (let i = entries.length - 1; i >= 0; i--) {
		const e = entries[i];
		if (!e || e.type !== 'assistant' || !Array.isArray(e.message?.content)) continue;
		const text = e.message.content
			.filter((b) => b && b.type === 'text' && typeof b.text === 'string')
			.map((b) => b.text)
			.join('\n')
			.trim();
		if (text) return text;
	}
	warn('No assistant text found in the execution transcript.');
	return '';
}

let summary = '';
try {
	if (!execPath || !fs.existsSync(execPath)) {
		warn('The Claude step produced no execution file.');
	} else {
		const parsed = JSON.parse(fs.readFileSync(execPath, 'utf8'));
		summary = extract(Array.isArray(parsed) ? parsed : [parsed]);
	}
} catch (err) {
	warn(`Could not read the execution transcript: ${err.message}`);
}

// Strip any code fence the model wrapped the text in despite being asked not to.
summary = summary.replace(/^\s*```[a-z]*\n?/i, '').replace(/\n?```\s*$/, '').trim();

fs.mkdirSync(require('path').dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, summary ? `${summary}\n` : '');
console.log(
	summary
		? `::notice::Recovered a ${summary.length}-character release summary.`
		: '::notice::No release summary; the release will be composed without one.',
);
