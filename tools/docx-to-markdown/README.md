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
