"""Cross-link the data structures named in each API call's signature.

A signature like

    ## `getAllScenes(ResultCallback<SceneInfo[]>)`

names a type the reader almost always wants to look at next, and in the Word
document that meant scrolling to find it. The signature is a code span, and a
code span cannot contain a link, so the types are listed on their own line at
the end of the entry instead.

Idempotent: re-running replaces the generated line rather than stacking another
one, so this can be applied again after any edit.

    python tools/docx-to-markdown/link_types.py .
"""

import io
import os
import re
import sys

REPO = sys.argv[1] if len(sys.argv) > 1 else '.'
API = os.path.join(REPO, 'docs', 'api')
TYPES = os.path.join(API, 'types')

MARKER = '**Data structures:** '

known = sorted(
    (os.path.splitext(f)[0] for f in os.listdir(TYPES)
     if f.endswith('.md') and f != 'README.md'),
    key=len, reverse=True)

# Word boundaries matter: VideoCompositionInfo is a substring of
# SharedVideoCompositionInfo, and matching the short one inside the long one
# would link the wrong page. \b prevents that -- the character before is a word
# character, so the match fails.
PATTERN = re.compile(r'\b(%s)\b' % '|'.join(re.escape(t) for t in known))


def rewrite(path, depth):
    """depth: how many directories up types/ is from this file."""
    text = io.open(path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(nl)

    prefix = '../' * depth + 'types/'

    out = []
    i = 0
    added = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        i += 1

        m = re.match(r'^(#{2,3}) `(.+)`$', line)
        if not m:
            continue

        # Collect the entry body, dropping any previously generated line so a
        # re-run does not stack them.
        body = []
        while i < len(lines) and not re.match(r'^#{1,3} ', lines[i]):
            if not lines[i].startswith(MARKER):
                body.append(lines[i])
            i += 1

        while body and not body[-1].strip():
            body.pop()

        types = []
        for t in PATTERN.findall(m.group(2)):
            if t not in types:
                types.append(t)

        if types:
            body.append('')
            body.append(MARKER + ', '.join(
                '[`%s`](%s%s.md)' % (t, prefix, t) for t in types))
            added += 1

        body.append('')
        out.extend(body)

    io.open(path, 'w', encoding='utf-8', newline='').write(nl.join(out))
    return added


total = files = 0
for root, _, names in os.walk(API):
    if os.path.basename(root) == 'types':
        continue
    depth = 0 if os.path.abspath(root) == os.path.abspath(API) else 1
    for name in sorted(names):
        if not name.endswith('.md') or name == 'README.md':
            continue
        n = rewrite(os.path.join(root, name), depth)
        if n:
            files += 1
            total += n

print('linked data structures on %d entries across %d files' % (total, files))
