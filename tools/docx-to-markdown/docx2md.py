"""Convert 'JavaScript OBS API Version 6.6.docx' into a Markdown tree.

The source is a Word document with a completely regular shape, which is what
makes a faithful conversion possible:

    Title / Subtitle            -- "JavaScript OBS API" / "Version 6.6"
    Heading2                    -- front-matter sections, plus "Endpoints and
                                   Data Structures"
      Heading3                  -- `window`, `window.host`, or a data-structure
                                   type name
        Heading4                -- a functional group under window.host
                                   ("Scenes", "Docking widgets", ...)
          Heading5              -- ONE API CALL. The heading text is the
                                   signature itself, e.g.
                                   getAllScenes({ videoCompositionId },
                                                ResultCallback<SceneInfo[]>)
            Heading6            -- a sub-note (used once)

Three Word idioms have to be decoded rather than transcribed:

  * a 1-column table is a code box, not data -> fenced code block
  * a 3-column table is nearly always "Property | Type | Description"
    -> a real Markdown table
  * runs are split mid-word wherever the document was edited, so "1.21" is
    stored as "1." + "21" and a naive pass emits "**1.****21**". Adjacent runs
    carrying identical formatting are merged before anything is rendered.

Signatures are emitted as inline code (## `getAllScenes(...)`) rather than bare
text. That is both the modern convention and the thing that makes escaping a
non-issue: `ResultCallback<SceneInfo[]>` in a plain heading would be eaten as
raw HTML.
"""

import os
import re
import xml.etree.ElementTree as ET
import zipfile

W = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
R = '{http://schemas.openxmlformats.org/officeDocument/2006/relationships}'

# A .docx is a zip. Read out of it directly rather than requiring an unpacked
# copy on disk, so this stays runnable exactly as committed. Point SE_API_DOCX
# at a newer revision to re-run the conversion.
DOCX = os.environ.get('SE_API_DOCX', 'JavaScript OBS API Version 6.6.docx')

_zip = zipfile.ZipFile(DOCX)


def part(name):
    return _zip.read(name)


def document():
    return ET.fromstring(part('word/document.xml')).find(W + 'body')


RELS = {el.get('Id'): el.get('Target')
        for el in ET.fromstring(part('word/_rels/document.xml.rels'))}


# --------------------------------------------------------------- helpers

def style_of(p):
    pr = p.find(W + 'pPr')
    if pr is None:
        return None
    st = pr.find(W + 'pStyle')
    return st.get(W + 'val') if st is not None else None


def is_listitem(p):
    pr = p.find(W + 'pPr')
    return pr is not None and pr.find(W + 'numPr') is not None


# Word wraps field results in directional marks. They are invisible in Word,
# meaningless in Markdown, and they corrupt word matching -- the stale
# cross-reference "<LTR>33MessageBusMessageInfo" is one of these.
INVISIBLE = dict.fromkeys(map(ord, u'‎‏​﻿'), None)


def run_text(r):
    out = []
    for n in r:
        if n.tag == W + 't':
            out.append(n.text or '')
        elif n.tag == W + 'tab':
            out.append('    ')
        elif n.tag == W + 'br':
            out.append('\n')
    return ''.join(out).translate(INVISIBLE)


def run_format(r):
    pr = r.find(W + 'rPr')
    if pr is None:
        return (False, False, False)
    mono = False
    f = pr.find(W + 'rFonts')
    if f is not None and (f.get(W + 'ascii') or '').startswith('Courier'):
        mono = True
    return (pr.find(W + 'b') is not None,
            pr.find(W + 'i') is not None,
            mono)


# Only what actually breaks CommonMark/GFM in this document's prose.
# `_`, `*`, `[`, `]`, `{`, `}` are deliberately NOT escaped: intra-word
# underscores and asterisks do not emphasise in GFM, and the document is full of
# identifiers like game_capture and SceneInfo[] that escaping would make
# unreadable to anyone editing the file by hand.
def esc(s):
    s = s.replace('\\', '\\\\').replace('`', '\\`')
    s = s.replace('<', '\\<')
    s = re.sub(r'^(\s*)([#>])', r'\1\\\2', s)
    return s


def merged_runs(container):
    """(text, bold, italic, mono) tuples, adjacent same-format runs merged."""
    seq = []
    for node in container:
        if node.tag == W + 'hyperlink':
            rid = node.get(R + 'id')
            url = RELS.get(rid) if rid else None
            inner = ''.join(run_text(r) for r in node.iter(W + 'r'))
            if inner:
                seq.append(('link', inner, url))
            continue
        if node.tag != W + 'r':
            continue
        t = run_text(node)
        if not t:
            continue
        fmt = run_format(node)
        if seq and seq[-1][0] == 'run' and seq[-1][2] == fmt:
            seq[-1] = ('run', seq[-1][1] + t, fmt)
        elif (seq and seq[-1][0] == 'run'
              and re.search(r'\w$', seq[-1][1]) and re.match(r'\w', t)):
            # Different formatting, but the split falls INSIDE a word, which is
            # always an editing artifact and never intent. Without this,
            # "sharedVideoCompositionId" (italic) + "s" (plain) renders as
            # *sharedVideoCompositionId*s. Keep the first run's formatting and
            # absorb the fragment.
            seq[-1] = ('run', seq[-1][1] + t, seq[-1][2])
        else:
            seq.append(('run', t, fmt))
    return seq


# Word cross-references that lost their target bake the page number into the
# text, giving "is a 33MessageBusMessageInfo data structure". A bare number
# fused to a CamelCase type name is never legitimate, so it is dropped -- three
# occurrences: 33MessageBusMessageInfo twice, 35ReleaseGroupInfo once.
STALE_REF = re.compile(
    r'\b\d{1,3}(?=[A-Z][A-Za-z]*(?:Info|Properties|Args|Content)\b)')


def images_of(container):
    """Relationship targets of any pictures anchored in this paragraph."""
    out = []
    for blip in container.iter(
            '{http://schemas.openxmlformats.org/drawingml/2006/main}blip'):
        rid = blip.get(R + 'embed')
        if rid and rid in RELS:
            out.append(os.path.basename(RELS[rid]))
    return out


def inline(container):
    parts = []
    for item in merged_runs(container):
        if item[0] == 'link':
            _, text, url = item
            parts.append('[%s](%s)' % (esc(text), url) if url else esc(text))
            continue

        _, t, (bold, italic, mono) = item
        if not t.strip():
            parts.append(t)
            continue

        lead = t[:len(t) - len(t.lstrip())]
        trail = t[len(t.rstrip()):]
        core = t.strip()

        if mono:
            parts.append('%s`%s`%s' % (lead, core, trail))
            continue

        core = esc(core)
        if bold:
            core = '**%s**' % core
        if italic:
            core = '*%s*' % core
        parts.append(lead + core + trail)

    return ''.join(parts)


def cell_lines(tc):
    lines = []
    for p in tc.findall(W + 'p'):
        lines.append(''.join(run_text(r) for r in p.iter(W + 'r')))
    flat = []
    for l in lines:
        flat.extend(l.split('\n'))
    return flat


def cell_md(tc):
    """A cell rendered for use inside a Markdown table row.

    A newline anywhere in a cell ends the row, so BOTH sources of one have to
    become <br>: the paragraph boundaries, and any <w:br/> inside a paragraph.
    Missing the second corrupted the ObsPropertyInfo controlType row, whose
    cell is a single paragraph holding "checkbox<br/>number".
    """
    parts = []
    for p in tc.findall(W + 'p'):
        s = inline(p).strip()
        if s:
            parts.extend(x for x in s.split('\n') if x.strip())
    return '<br>'.join(parts).replace('|', '\\|')


# --------------------------------------------------------------- blocks

def table_to_md(tbl):
    rows = tbl.findall(W + 'tr')
    if not rows:
        return ''

    ncols = max(len(r.findall(W + 'tc')) for r in rows)

    if ncols == 1:
        lines = []
        for r in rows:
            for tc in r.findall(W + 'tc'):
                lines.extend(cell_lines(tc))
        while lines and not lines[0].strip():
            lines.pop(0)
        while lines and not lines[-1].strip():
            lines.pop()
        body = '\n'.join(lines)
        lang = 'js' if re.search(
            r'\b(window|function|=>|var |let |const )', body) else ''
        return '```%s\n%s\n```' % (lang, body)

    out = []
    head = [cell_md(tc) for tc in rows[0].findall(W + 'tc')]
    head += [''] * (ncols - len(head))
    out.append('| ' + ' | '.join(head) + ' |')
    out.append('|' + '|'.join([' --- '] * ncols) + '|')
    for r in rows[1:]:
        cells = [cell_md(tc) for tc in r.findall(W + 'tc')]
        cells += [''] * (ncols - len(cells))
        out.append('| ' + ' | '.join(cells) + ' |')
    return '\n'.join(out)


def heading_md(text, level, as_code):
    if as_code:
        return '%s `%s`' % ('#' * level, text.replace('`', ''))
    return '%s %s' % ('#' * level, esc(text))


def render(blocks, demote=0, code_headings=(5,)):
    """Render blocks to Markdown.

    `demote` shifts Word heading levels so each file starts at `#`.
    `code_headings` are the Word levels whose text is a signature and is
    therefore emitted as inline code.
    """
    out = []
    for b in blocks:
        if b.tag == W + 'tbl':
            md = table_to_md(b)
            if md:
                out.append(md)
            continue
        if b.tag != W + 'p':
            continue

        st = style_of(b) or ''
        if st.startswith('TOC'):
            continue

        if st.startswith('Heading'):
            lvl = int(st.replace('Heading', ''))
            txt = ''.join(run_text(r) for r in b.iter(W + 'r')).strip()
            if not txt:
                continue
            out.append(heading_md(txt, max(1, lvl - demote),
                                  lvl in code_headings))
            continue

        for img in images_of(b):
            out.append('![%s](images/%s)' % (os.path.splitext(img)[0], img))

        s = inline(b)
        if s.strip():
            out.append(('- ' + s.strip()) if is_listitem(b) else s.strip())

    text = '\n\n'.join(out)
    text = STALE_REF.sub('', text)
    return re.sub(r'\n{3,}', '\n\n', text).strip() + '\n'


# --------------------------------------------------------------- slugs

def slug(s):
    s = s.strip().lower()
    s = re.sub(r'[^a-z0-9]+', '-', s)
    return s.strip('-') or 'section'


def sig_name(sig):
    m = re.match(r'\s*([A-Za-z_][A-Za-z0-9_.]*)', sig)
    return m.group(1) if m else sig.strip()
