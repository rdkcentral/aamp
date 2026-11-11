# direct-rialto documentation set

Source for the "Direct Rialto" Confluence page tree. Thirteen pages document the `direct-rialto/` component: architecture, lifecycle, data path, flush and seek, threading, DRM, subtitles, monitoring, integration, testing, a register of workarounds and improvements, and a troubleshooting guide. The pages are written so that an engineer can understand the component without reading the code.

## Layout

| Path | Content |
|---|---|
| `NN-*.md` | One file per Confluence page. Page `00` is the parent; every other page is its child. |
| `../diagrams/direct-rialto-*.puml` | PlantUML sources. Pages embed them with a `[[diagram:<name>]]` line. |
| `_register/<TAG>.md` | Workaround and improvement register fragments, one per area. |
| `_register/_shortlist.md` | Hand-written prioritised shortlist for the register page. |
| `tools/build_register_page.py` | Generates `11-workarounds-and-improvement-register.md` from `_register/`. Do not edit page 11 by hand. |
| `tools/make_confluence_pages.py` | Converts the pages into paste-ready Confluence wiki markup with PlantUML macros. |

## Publishing to Confluence

1. Regenerate page 11 if any register fragment changed:
   `python3 docs/direct-rialto/tools/build_register_page.py`
2. Build the Confluence output (written to `build/confluence/`, which git ignores):
   `python3 docs/direct-rialto/tools/make_confluence_pages.py`
   The script fails and lists any `[[diagram:...]]` placeholder without a matching `.puml` file. Use `--macro <name>` if the PlantUML plugin's macro is not called `plantuml`.
3. Create the pages in the order and with the titles in `build/confluence/PUBLISH_ORDER.txt`. Make page 00 the parent and every other page a child of it.
4. For each page, in the Confluence editor choose Insert (+) > Markup > "Confluence Wiki", paste the contents of `NN-*.wiki`, check the preview, then insert.
5. Cross-references such as `[Direct Rialto - Flush and seek]` become page links. They resolve once all pages exist in the same space with these exact titles.

Things to check in the preview:

- PlantUML macros render. Large class diagrams may need the plugin's layout engine; all diagrams use core syntax only.
- Code-like text such as `PlayHold::PositionPending` is not turned into emoticons. Some Confluence versions convert `:P`, `(i)` and similar sequences. If that happens, disable emoticon conversion for the page or space.

The `NN-*.md` files in the same output folder have each diagram embedded as a fenced `plantuml` block, for review or for a Markdown macro.

## Keeping the documentation current

- When you change direct-rialto behaviour, update the page that describes it and any affected diagram in the same pull request.
- Reference code as `File.cpp:Symbol`, not by line number.
- Add new workarounds or known issues to the right `_register/<TAG>.md` fragment with a new ID. Never reuse an ID; mark fixed items as fixed.
- Validate a diagram with `java -jar plantuml.jar -syntax < file.puml`.
