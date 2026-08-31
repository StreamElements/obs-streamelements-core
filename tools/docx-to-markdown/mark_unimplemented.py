"""Mark documented API calls that the plug-in does not actually register.

The reference was maintained in Word for seven years with no way to check it
against the code, so it accumulated calls that do not exist. A reader cannot
tell those from the working ones, and finds out by calling one.

The list is DERIVED, not hard-coded, so re-running this after a code change
adds and removes markers by itself:

    python tools/docx-to-markdown/mark_unimplemented.py .

Two things are deliberately not flagged, both distinguished structurally rather
than by an allow-list:

  * Properties -- hostReady, apiMajorVersion and friends are values on
    window.host, not calls, and their headings carry no "(".
  * batchInvokeSeries, which is registered by calling
    RegisterIncomingApiCallHandler() directly instead of going through the
    API_HANDLER_BEGIN macro. Scanning only for the macro reports it as missing.

Handlers live in more than one file (StreamElementsBrowserDialog.cpp registers
the dialog ones), so the whole streamelements/ tree is scanned. Missing that
falsely flags endDialog, endModalDialog and endNonModalDialog.

Idempotent: an existing marker is replaced, never stacked.
"""

import io
import os
import re
import subprocess
import sys

REPO = sys.argv[1] if len(sys.argv) > 1 else '.'
SRC = os.path.join(REPO, 'streamelements')
HOST = os.path.join(REPO, 'docs', 'api', 'host')

MARK = '> ⚠️ **Not implemented.**'

# Documented under a name the code does not use, but the behaviour described
# does exist under the name given here. Kept as a note rather than renamed: the
# heading is what people copy, and which of the two names is meant to win is a
# question for whoever owns the API, not something to infer from the source.
ALIASES = {
    'setCurrentProfileById': 'setCurrentProfile',
}


def registered_names():
    names = set()
    pat = re.compile(
        r'(?:API_HANDLER_BEGIN|RegisterIncomingApiCallHandler)\(\s*"([^"]+)"')
    for root, _, files in os.walk(SRC):
        for f in files:
            if not f.endswith(('.cpp', '.hpp', '.mm', '.h')):
                continue
            text = io.open(os.path.join(root, f), encoding='utf-8',
                           errors='replace').read()
            names |= set(pat.findall(text))
    return names


REGISTERED = registered_names()


def removal_of(name):
    """The commit that took this handler out, or None if it never existed.

    Asking the code alone cannot tell "removed years ago" from "never built",
    and those deserve different warnings: the first is a stale document, the
    second is a specification that outran the implementation. git log -S over
    the full history separates them with evidence. Five of the six removals
    turn out to be the same commit -- the 2022 CEF/obs-browser ejection.
    """
    try:
        out = subprocess.check_output(
            ['git', 'log', '-S', name, '--all', '--date=short',
             '--format=%h\t%ad\t%s', '--', ':!docs', ':!tools'],
            cwd=REPO, stderr=subprocess.DEVNULL).decode('utf-8', 'replace')
    except Exception:
        return None

    lines = [l for l in out.split('\n') if l.strip()]
    if not lines:
        return None
    return lines[0].split('\t')          # newest touch == the removal


def note_for(name, deprecated):
    """The marker, worded for what is actually known about this call."""
    if name in ALIASES:
        return ('> ⚠️ **Name does not match the implementation.** The '
                'registered handler is `%s`, which does what is described '
                'here; no handler named `%s` has ever existed.'
                % (ALIASES[name], name))

    gone = removal_of(name)

    if gone:
        sha, date, subject = gone[0], gone[1], gone[2]
        return ('> ⚠️ **Removed.** Implemented once, but taken out in `%s` '
                '(%s, %s)%s. No handler by this name is registered any more, '
                'so calling it will not resolve.'
                % (sha, subject, date,
                   ' — and the entry below does not say so'
                   if not deprecated else ''))

    return ('%s No handler by this name appears anywhere in this repository, '
            'in any commit on any branch, and the history goes back to 2014. '
            'Documented but never built here.' % MARK)


def rewrite(path):
    text = io.open(path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(nl)

    out = []
    marked = []
    i = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        i += 1

        m = re.match(r'^## `(.+)`$', line)
        if not m:
            continue

        sig = m.group(1)

        # Drop any previous marker so a re-run refreshes rather than stacks,
        # and so a call that has since been implemented loses its warning.
        while i < len(lines) and (lines[i].startswith('> ⚠')
                                  or (lines[i] == '' and i + 1 < len(lines)
                                      and lines[i + 1].startswith('> ⚠'))):
            i += 1

        if '(' not in sig:          # a property, not a call
            continue

        name = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*)', sig)
        if not name or name.group(1) in REGISTERED:
            continue

        # Look ahead over this entry's own text, without consuming it, to see
        # whether it already declares itself deprecated.
        j = i
        deprecated = False
        while j < len(lines) and not lines[j].startswith('## '):
            low = lines[j].lower()
            # "Removed in API 3.0" as well as "Deprecated in API 3.0" -- both
            # forms occur, and matching only the first made the marker tell
            # reloadAllBrowserSources that its entry "says only that it was
            # deprecated" when the entry plainly says it was removed.
            if 'deprecat' in low or 'removed in api' in low:
                deprecated = True
                break
            j += 1

        out.append('')
        out.append(note_for(name.group(1), deprecated))
        marked.append((name.group(1), deprecated))

    io.open(path, 'w', encoding='utf-8', newline='').write(nl.join(out))
    return marked


def mark_index(names):
    """Flag the same calls in the front-page index, so a reader browsing the
    list of 217 sees which ones are dead before clicking into them."""
    p = os.path.join(REPO, 'docs', 'api', 'README.md')
    text = io.open(p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in text else '\n'

    legend = ('Calls marked ⚠️ are documented but **not registered by the '
              'plug-in** — deprecated and removed, or never implemented. Open '
              'one to see which.')

    out = []
    for line in text.split(nl):
        # Strip any existing marker BEFORE matching. Matching first would look
        # at a line whose name is preceded by the marker, fail, and then drop
        # the marker without putting it back -- a re-run would quietly unmark
        # everything.
        line = line.replace('| [`⚠️ ', '| [`')
        m = re.match(r'^\| \[`([A-Za-z_][A-Za-z0-9_]*)', line)
        if m and m.group(1) in names:
            line = line.replace('| [`', '| [`⚠️ ', 1)
        if line.strip() == legend:
            continue
        out.append(line)

    text = nl.join(out)
    anchor = '### All `window.host` calls, alphabetically' + nl
    if anchor in text and legend not in text:
        text = text.replace(anchor, anchor + nl + legend + nl, 1)

    io.open(p, 'w', encoding='utf-8', newline='').write(text)


total = []
for f in sorted(os.listdir(HOST)):
    if f.endswith('.md') and f != 'README.md':
        total += rewrite(os.path.join(HOST, f))

mark_index({n for n, _ in total})

print('registered handlers: %d' % len(REGISTERED))
print('marked %d call(s):' % len(total))
for n, dep in sorted(total):
    if n in ALIASES:
        kind = 'name mismatch -> %s' % ALIASES[n]
    else:
        gone = removal_of(n)
        kind = ('removed in %s (%s)' % (gone[0], gone[1]) if gone
                else 'NEVER existed in this repo')
    print('   %-44s %s' % (n, kind))
