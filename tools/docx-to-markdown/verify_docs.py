"""Prove the Markdown tree lost nothing from the .docx.

Compares word multisets. The output legitimately has MORE words than the source
(indexes, the "how to add a call" section), so the test is that the source is a
subset of the output -- any word the document had that the tree does not is a
real loss.

Also counts the structural elements independently: headings, tables, code boxes.
"""

import collections
import io
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from docx2md import W, style_of, run_text, STALE_REF, document

REPO = sys.argv[1]
OUT = os.path.join(REPO, 'docs', 'api')

body = document()


def words(s):
    # The converter strips the page numbers that broken Word cross-references
    # baked into the text; normalise the source the same way so the comparison
    # is between like and like.
    s = STALE_REF.sub('', s)
    s = s.replace('’', "'").replace('‘', "'")
    s = s.replace('“', '"').replace('”', '"')
    s = s.replace('–', '-').replace('—', '-').replace(' ', ' ')
    return re.findall(r"[A-Za-z0-9_]+", s)


# ------------------------------------------------------------ source side

src_words = collections.Counter()
n_head = n_tbl = n_code = 0

for b in body:
    if b.tag == W + 'tbl':
        rows = b.findall(W + 'tr')
        cols = max((len(r.findall(W + 'tc')) for r in rows), default=0)
        if cols == 1:
            n_code += 1
        else:
            n_tbl += 1
        # Concatenate per paragraph before tokenizing: Word splits runs
        # mid-word after edits ("b" + "ool"), and counting <w:t> elements
        # separately would invent losses that are not there.
        for p in b.iter(W + 'p'):
            src_words.update(words(''.join(run_text(r) for r in p.iter(W + 'r'))))
        continue

    if b.tag != W + 'p':
        continue

    st = style_of(b) or ''
    if st.startswith('TOC'):
        continue          # auto-generated, regenerated as README indexes
    if st.startswith('Heading') and ''.join(
            run_text(r) for r in b.iter(W + 'r')).strip():
        n_head += 1

    src_words.update(words(''.join(run_text(r) for r in b.iter(W + 'r'))))

# The TOC heading itself and its page-number noise are dropped deliberately.
for junk in ('Table', 'of', 'Contents'):
    pass

# ------------------------------------------------------------ output side

out_words = collections.Counter()
o_head = o_tbl = o_code = 0
nfiles = 0

for root, _, files in os.walk(OUT):
    for f in sorted(files):
        if not f.endswith('.md'):
            continue
        nfiles += 1
        text = io.open(os.path.join(root, f), encoding='utf-8').read()

        infence = False
        for line in text.split('\n'):
            if line.startswith('```'):
                if not infence:
                    o_code += 1
                infence = not infence
                continue
            if infence:
                continue
            if re.match(r'^#{1,6} ', line):
                o_head += 1
            if re.match(r'^\|\s*---', line):
                o_tbl += 1

        out_words.update(words(text))

# ------------------------------------------------------------------ report

# Two headings exist only to contain other headings, and the tree replaces both:
# "Table of Contents" becomes the generated indexes, and "Endpoints and Data
# Structures" becomes the directory layout itself. Their words are the only
# content deliberately not carried over.
EXPECTED_DROPPED = {'Table', 'Contents', 'Endpoints', 'Structures'}

missing = collections.Counter()
for w, n in src_words.items():
    if out_words[w] < n and w not in EXPECTED_DROPPED:
        missing[w] = n - out_words[w]

# --------------------------------------------------- table integrity

# A newline inside a cell silently ends the row, so a malformed table passes a
# word-count check while rendering as garbage. Every row must carry the same
# number of columns as its header separator.
ragged = []
for root, _, files in os.walk(OUT):
    for f in sorted(files):
        if not f.endswith('.md'):
            continue
        path = os.path.join(root, f)
        lines = io.open(path, encoding='utf-8').read().split('\n')
        infence = False
        i = 0
        while i < len(lines):
            if lines[i].startswith('```'):
                infence = not infence
            elif not infence and re.match(r'^\|\s*---', lines[i]) and i > 0:
                ncols = lines[i].count('|') - 1
                j = i - 1
                while j < len(lines) and lines[j].startswith('|'):
                    got = (lines[j].count('|') - lines[j].count('\\|')) - 1
                    if got != ncols:
                        ragged.append((os.path.relpath(path, OUT), j + 1,
                                       ncols, got))
                    j += 1
                i = j
                continue
            i += 1

print('=== structure ===')
print('  %-22s source %5d   output %5d' % ('headings', n_head, o_head))
print('  %-22s source %5d   output %5d' % ('data tables', n_tbl, o_tbl))
print('  %-22s source %5d   output %5d' % ('code boxes', n_code, o_code))
print('  %-22s               %5d' % ('markdown files', nfiles))
if ragged:
    print()
    print('  %d RAGGED TABLE ROW(S) -- a cell newline broke the row:' % len(ragged))
    for rel, line, want, got in ragged[:20]:
        print('     %s:%d  expected %d cols, got %d' % (rel, line, want, got))
else:
    print('  %-22s               %5s' % ('ragged table rows', 'none'))
print()
print('=== text ===')
print('  source word tokens : %d (%d distinct)'
      % (sum(src_words.values()), len(src_words)))
print('  output word tokens : %d (%d distinct)'
      % (sum(out_words.values()), len(out_words)))
print()

if not missing:
    print('  OK: every source word occurs in the output at least as often')
else:
    print('  %d distinct words are under-represented (%d occurrences):'
          % (len(missing), sum(missing.values())))
    for w, n in missing.most_common(40):
        print('     -%-4d %s' % (n, w))
    sys.exit(1)
