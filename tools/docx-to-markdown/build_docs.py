"""Split the converted document into a Markdown tree under docs/api/.

Heading levels: each output file is titled with `#`, and the Word levels below
it are shifted so the file reads as a standalone document. A host group file is
titled with its group name, so the Heading5 signatures inside it become `##`.
"""

import collections
import io
import os
import re
import shutil
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from docx2md import (W, style_of, run_text, render, slug, sig_name,
                     document, part)

REPO = sys.argv[1]
OUT = os.path.join(REPO, 'docs', 'api')

body = document()
BLOCKS = [b for b in body if b.tag in (W + 'p', W + 'tbl')]


def htext(b):
    return ''.join(run_text(r) for r in b.iter(W + 'r')).strip()


def hlevel(b):
    if b.tag != W + 'p':
        return None
    st = style_of(b) or ''
    if st == 'Title':
        return 0
    if st.startswith('Heading'):
        return int(st.replace('Heading', ''))
    return None


def split_at(blocks, level):
    out, cur, title = [], [], None
    for b in blocks:
        if hlevel(b) == level:
            if title is not None or cur:
                out.append((title, cur))
            title, cur = htext(b), []
        else:
            cur.append(b)
    if title is not None or cur:
        out.append((title, cur))
    return [(t, c) for t, c in out if t is not None]


def write(relpath, text):
    p = os.path.join(OUT, relpath)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
    return relpath


# ------------------------------------------------------------------ parse

VERSION = None
for b in BLOCKS:
    if style_of(b) == 'Subtitle':
        VERSION = htext(b).replace('Version', '').strip()
        break

sections = dict(split_at(BLOCKS, 2))
written = []

FRONT = [
    ('Abstract', 'abstract.md'),
    ('Status of this Document', 'status.md'),
    ('Revision History', 'revision-history.md'),
    ('Goals and Considerations', 'goals-and-considerations.md'),
    ('Versioning', 'versioning.md'),
    ('General guidelines', 'general-guidelines.md'),
    ('Web Authentication', 'web-authentication.md'),
    ('Conventions', 'conventions.md'),
]

for title, fname in FRONT:
    if title not in sections:
        print('  !! missing front-matter section: %s' % title)
        continue
    # The H2 is the file title, so H3 inside becomes ##. Nothing at this level
    # is a signature.
    md = '# %s\n\n%s' % (title, render(sections[title], demote=1,
                                       code_headings=()))
    written.append(write(fname, md))

api = sections['Endpoints and Data Structures']

host_files = []
type_files = []
window_calls = []

for name, blocks in split_at(api, 3):
    if not name:
        continue

    if name == 'window':
        # H3 is the title -> H4 (Events/Methods) becomes ##, H5 signatures ###
        md = '# `window`\n\n' + render(blocks, demote=2)
        written.append(write('window.md', md))
        window_calls = [htext(b) for b in blocks
                        if hlevel(b) == 5 and htext(b)]
        continue

    if name == 'window.host':
        for gname, gblocks in split_at(blocks, 4):
            if not gname:
                continue
            rel = 'host/%s.md' % slug(gname)
            sigs = [htext(b) for b in gblocks if hlevel(b) == 5 and htext(b)]
            # H4 is the title -> H5 signatures become ##
            md = ('# %s\n\n`window.host`\n\n%s'
                  % (gname, render(gblocks, demote=3)))
            written.append(write(rel, md))
            host_files.append((gname, rel, sigs))
        continue

    rel = 'types/%s.md' % name
    md = '# %s\n\n%s' % (name, render(blocks, demote=2, code_headings=()))
    written.append(write(rel, md))
    type_files.append((name, rel))


# ------------------------------------------------------------- the indexes

def anchor_for(sig):
    """GitHub's anchor for '## `signature`'.

    Backticks are dropped, the rest lowercased, runs of non-alphanumerics
    become single hyphens, leading/trailing hyphens trimmed.
    """
    s = sig.replace('`', '').strip().lower()
    s = re.sub(r'[^a-z0-9 _-]', '', s)
    return re.sub(r'[ _]+', '-', s).strip('-')


rows = []
for gname, rel, sigs in host_files:
    # Overloads repeat a signature verbatim within one page -- getAllScenes has
    # a 1.21 form and a 6.0 form. Python-Markdown gives the second heading the
    # slug with "_1" appended, so the anchors have to be assigned in document
    # order here too, or both index rows land on the first overload. Anchor
    # validation cannot catch that: the anchor it points at does exist.
    seen = collections.Counter()
    for s in sigs:
        base = anchor_for(s)
        n = seen[base]
        seen[base] += 1
        rows.append((sig_name(s), s, gname, rel,
                     base if n == 0 else '%s_%d' % (base, n)))

# Stable, so equal signatures keep document order and the un-suffixed anchor
# stays with the first of them.
rows.sort(key=lambda r: (r[0].lower(), r[1]))

index = ['| Call | Group |', '| --- | --- |']
for bare, s, gname, rel, anchor in rows:
    index.append('| [`%s`](%s#%s) | [%s](%s) |'
                 % (s.replace('|', '\\|'), rel, anchor, gname, rel))
index = '\n'.join(index)

readme = """# JavaScript OBS API

**API version %(version)s** — the host API SE.Live exposes to page JavaScript
running inside OBS.

Converted from `JavaScript OBS API Version %(version)s.docx` — last recorded
API revision 2025-10-07, file last saved 2025-10-09, Word revision 117 of a
document started 2018-05-22. That document is no longer the source of truth;
this tree is.

## Start here

| | |
| --- | --- |
| [Abstract](abstract.md) | what this API is |
| [Status of this Document](status.md) | how stable the spec is |
| [Conventions](conventions.md) | naming, callbacks, error handling |
| [General guidelines](general-guidelines.md) | how to use the API well |
| [Versioning](versioning.md) | how API versions are numbered |
| [Web Authentication](web-authentication.md) | authenticating a page |
| [Goals and Considerations](goals-and-considerations.md) | design rationale |
| [Revision History](revision-history.md) | every change, 1.0 → %(version)s |

## Reference

- [`window`](window.md) — %(nwindow)d events and methods on the page's own `window`
- [`window.host`](host/README.md) — %(nmethods)d calls across %(ngroups)d groups
- [Data structures](types/README.md) — %(ntypes)d object types

### `window.host` by group

%(grouplist)s

### All `window.host` calls, alphabetically

%(index)s

## Adding a new API call

The tree mirrors the shape of the API, so a new call touches one file:

1. Add it to the group file under `host/` covering its area — `host/scenes.md`,
   `host/docking-widgets.md`, and so on. Copy a neighbouring entry: a `##`
   heading holding the **full signature in backticks**, then an
   `Available since API version X` line, then the description.
2. If it takes or returns a new object type, add `types/<TypeName>.md` with a
   `Property | Type | Description` table.
3. Add a row to [revision-history.md](revision-history.md), newest last.
4. Add a row to the alphabetical index above.

A new *group* is a new file under `host/`, plus a line in the group list and in
[host/README.md](host/README.md).

Signatures are the headings, so keep them exact — the index links to them by
anchor, and a changed signature silently breaks that link.
""" % {
    'version': VERSION,
    'nwindow': len(window_calls),
    'nmethods': len(rows),
    'ngroups': len(host_files),
    'ntypes': len(type_files),
    'grouplist': '\n'.join('- [%s](%s) — %d call%s'
                           % (g, r, len(s), '' if len(s) == 1 else 's')
                           for g, r, s in host_files),
    'index': index,
}
written.append(write('README.md', readme))

written.append(write('types/README.md',
                     '# Data structures\n\n'
                     'Objects passed to and returned from '
                     '[`window.host`](../README.md) calls.\n\n'
                     + '\n'.join('- [%s](%s)' % (n, os.path.basename(r))
                                 for n, r in sorted(type_files,
                                                    key=lambda x: x[0].lower()))
                     + '\n'))

written.append(write('host/README.md',
                     '# `window.host`\n\n'
                     'The host API, grouped by area. See the '
                     '[alphabetical index]'
                     '(../README.md#all-windowhost-calls-alphabetically).\n\n'
                     + '\n'.join('- [%s](%s) — %d call%s'
                                 % (g, os.path.basename(r), len(s),
                                    '' if len(s) == 1 else 's')
                                 for g, r, s in host_files)
                     + '\n'))

# ------------------------------------------------------------------ media

# Images are emitted as images/<name>, so the media has to sit beside whichever
# file references it. Only RecordingSettings carries one today, but copying per
# referencing directory keeps that from being a special case.
copied = 0
for rel in written:
    full = os.path.join(OUT, rel)
    text = io.open(full, encoding='utf-8').read()
    for name in re.findall(r'!\[[^\]]*\]\(images/([^)]+)\)', text):
        dst = os.path.join(os.path.dirname(full), 'images', name)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, 'wb') as fh:
            fh.write(part('word/media/' + name))
        copied += 1

print('media files: %d' % copied)
print('version: %s' % VERSION)
print('files:   %d' % len(written))
print('  front matter:   %d' % len(FRONT))
print('  host groups:    %d  (%d calls)' % (len(host_files), len(rows)))
print('  data types:     %d' % len(type_files))
print('  window entries: %d' % len(window_calls))
