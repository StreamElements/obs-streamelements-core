# docx → Markdown, for the JavaScript OBS API reference

One-shot migration of `JavaScript OBS API Version 6.6.docx` into
[`docs/api/`](../../docs/api/). **The Markdown tree is now the source of truth**
— this tool exists so the conversion can be audited or re-run, not so the Word
document can keep being edited.

```sh
export SE_API_DOCX="/path/to/JavaScript OBS API Version 6.6.docx"
python tools/docx-to-markdown/build_docs.py  .
python tools/docx-to-markdown/verify_docs.py .
```

| | |
| --- | --- |
| `docx2md.py` | reads the .docx (a zip) and renders WordprocessingML to Markdown |
| `build_docs.py` | routes the document's sections into the file tree and writes the indexes |
| `verify_docs.py` | proves nothing was lost; exits non-zero if anything was |

## What the conversion had to decide

The document's shape is regular, which is what made a faithful conversion
possible: `Heading5` is one API call and its text is the **signature itself**.
Four Word idioms had to be decoded rather than transcribed:

- **A 1-column table is a code box**, not data. 10 of them become fenced blocks;
  the 3-column ones become real Markdown tables.
- **Runs are split mid-word** wherever the document was edited, so "1.21" is
  stored as `1.` + `21`. Rendered naively that yields `**1.****21**`, and where
  the two halves carry *different* formatting it yields
  `*sharedVideoCompositionId*s`. Adjacent runs are merged before rendering,
  including across a formatting change when the split falls inside a word.
- **Newlines inside table cells end the row.** Both sources of one — paragraph
  boundaries and `<w:br/>` — become `<br>`. Missing the second corrupted the
  `ObsPropertyInfo.controlType` row, which is why `verify_docs.py` checks every
  table for ragged rows.
- **Broken cross-references baked page numbers into the text**, giving
  "is a 33MessageBusMessageInfo data structure". Three of these are dropped.

## What is deliberately not carried over

Two headings existed only to contain other headings, and the tree replaces both:
"Table of Contents" (now generated indexes) and "Endpoints and Data Structures"
(now the directory layout). `verify_docs.py` allows exactly these and nothing
else.

Signatures are emitted as inline code — ``## `getAllScenes(...)` `` — which is
both the modern convention and the thing that keeps
`ResultCallback<SceneInfo[]>` from being eaten as raw HTML.

## Corrections made after conversion

The tree is the source of truth, so these were fixed in the Markdown, not the
Word document. **Re-running `build_docs.py` would revert them**, and would also
drop the type cross-links; run `link_types.py` afterwards and re-apply these by
hand.

- **`getAllScenes`** — the document listed the same signature twice, once under
  API 1.21 and once under 6.0. The implementation takes the argument as
  optional (`if (args->GetSize()) input = args->GetValue(0); else
  input->SetNull();`, and `GetVideoComposition()` falls back to the native
  composition), so the 1.21 form is `getAllScenes(ResultCallback<SceneInfo[]>)`
  with no argument — exactly as the neighbouring `getCurrentScene` pair is
  already written.
- **`getShowBuiltInMenuItems`** — documented as `getShowBuil**d**InMenuItems`.
  No such handler exists; the setter beside it was already spelled correctly.

## Verifying the docs against the implementation

The handlers are registered in two files, not one -- missing the second reports
`endDialog`, `endModalDialog` and `endNonModalDialog` as undocumented:

```sh
grep -rho 'API_HANDLER_BEGIN("[^"]*"' streamelements/ | sed 's/.*("//;s/"//' | sort -u
```

Diffing that against the `##` signatures under `docs/api/host/` is what found
both corrections above. It also lists calls documented but absent from the code
(`dispatchKeyboardEvent`, `dispatchMouseEvent`, `getHostCapabilities`,
`reloadAllBrowserSources`, `getContainerForeignPopupWindowsProperties`,
`setContainerForeignPopupWindowsProperties`, `setCurrentProfileById`), one
registered but undocumented (`getCurrentSceneCollectionProperties`), and three
debug-only handlers that are undocumented on purpose (`crashProgram`,
`crashProgramFastFail`, `deadlockProgram`). Those are now **marked in the docs themselves** by
`mark_unimplemented.py`, which derives the list rather than hard-coding it, so
re-running it after a code change adds and removes markers on its own. It
asks `git log -S` over the full history (back to 2014) so it can tell a call
that was **removed** from one that was **never built** -- the code alone cannot.
It distinguishes three cases: deprecated-and-removed (four calls, which say so in
their own text), a name that does not match the implementation
(`setCurrentProfileById` -> `setCurrentProfile`), and genuinely unexplained
(`getHostCapabilities`, `reloadAllBrowserSources` -- neither deprecated nor
ever built).

`getCurrentSceneCollectionProperties` was reported as registered but
undocumented. It was in fact documented all along -- its heading had lost its
Heading5 style in Word, so it was body text, absent from the document’s own
table of contents, and invisible to any structural pass. Promoted to a real
heading, and its behaviour written up from the source.

Nothing registered is undocumented now except three debug-only handlers
(`crashProgram`, `crashProgramFastFail`, `deadlockProgram`) and
`setCurrentProfile`, which is the real name behind the `setCurrentProfileById`
entry and is marked there.
