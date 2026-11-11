#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Build paste-ready Confluence pages from docs/direct-rialto/*.md.

For every page this writes two files into the output directory, plus
PUBLISH_ORDER.txt with the Confluence page titles and hierarchy:

  <page>.wiki  Confluence wiki markup.  In the Confluence editor use
               Insert (+) > Markup > "Confluence Wiki", paste, Insert.
               Diagrams become {plantuml} macros.
  <page>.md    Markdown with each diagram embedded as a ```plantuml block,
               for a Markdown macro / review.

A diagram placeholder in a page is a line of the form
    [[diagram:direct-rialto-foo-sequence]]
which is replaced by the contents of docs/diagrams/<name>.puml
(copyright header and the @startuml/@enduml lines are stripped for wiki
output because the macro adds them).

Usage:
    make_confluence_pages.py [--out DIR] [--macro NAME]

Limitations: the converter handles the Markdown subset these pages use
(headings, paragraphs, tables, nested lists, code spans and fences, bold,
italic, links).  Check each page in the Confluence preview after pasting.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIAGRAMS = ROOT.parent / "diagrams"
PLACEHOLDER = re.compile(r"^\[\[diagram:([A-Za-z0-9_.-]+)\]\]\s*$")
PAGE_TITLES = [
    "Overview", "Architecture and class model", "Lifecycle and state machine",
    "Media data path", "Flush and seek", "Threading model and locking", "DRM",
    "Subtitles and TTML", "Monitoring and diagnostics",
    "Integration, configuration and build", "Testing",
    "Workarounds and improvement register", "Troubleshooting and glossary",
]
# "Direct Rialto - <Title>", optionally already in [brackets] in the source.
PAGE_REF = re.compile(r"\[?(Direct Rialto - (?:%s))\]?" % "|".join(
    re.escape(t) for t in sorted(PAGE_TITLES, key=len, reverse=True)))


MISSING = []


def load_diagram(name, strip_markers):
    path = DIAGRAMS / (name + ".puml")
    if not path.exists():
        MISSING.append(name)
        return "title MISSING DIAGRAM %s" % name
    lines = path.read_text(encoding="utf-8").splitlines()
    # Drop the leading apostrophe-comment licence header.
    while lines and (lines[0].startswith("'") or not lines[0].strip()):
        lines.pop(0)
    if strip_markers:
        lines = [ln for ln in lines
                 if not ln.startswith("@startuml") and
                 not ln.startswith("@enduml")]
    return "\n".join(lines).strip("\n")


def inline_markdown(text):
    out = []
    for line in text.splitlines():
        match = PLACEHOLDER.match(line)
        if match:
            body = load_diagram(match.group(1), strip_markers=False)
            out.append("```plantuml\n%s\n```" % body)
        else:
            out.append(line)
    return "\n".join(out) + "\n"


def convert_inline(text):
    # Stash code spans first so their contents are not touched.
    spans = []

    def stash(match):
        body = re.sub(r"([{}\[\]|])", r"\\\1",
                      match.group(1).replace("\\|", "|"))
        spans.append("{{%s}}" % body)
        return "\x00%d\x00" % (len(spans) - 1)

    def stash_link(link):
        spans.append(link)
        return "\x00%d\x00" % (len(spans) - 1)

    text = re.sub(r"`([^`]+)`", stash, text)
    # Cross-page references become Confluence page links (same space).
    text = PAGE_REF.sub(
        lambda m: stash_link("[%s]" % m.group(1)), text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"[\1|\2]", text)
    text = re.sub(r"\*\*([^*]+)\*\*", "\x01\\1\x01", text)
    text = re.sub(r"(?<![\w*])\*([^*\s][^*]*)\*(?![\w*])", r"_\1_", text)
    text = text.replace("\x01", "*")
    # Escape literal braces that would otherwise start a macro.
    text = text.replace("{", "\\{").replace("}", "\\}")
    return re.sub(r"\x00(\d+)\x00", lambda m: spans[int(m.group(1))], text)


BLOCK_START = re.compile(
    r"^(#{1,6}\s|\||[-*]\s|\d+\.\s|```|\[\[diagram:)")


def unwrap(text):
    """Join hard-wrapped paragraph and list-item lines.

    Markdown treats a single newline as a space, but Confluence wiki markup
    treats it as a line break (and ends a list item), so soft-wrapped lines
    must be joined before conversion.
    """
    out = []
    in_code = False
    can_join = False
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("```"):
            in_code = not in_code
            out.append(line)
            can_join = False
            continue
        if in_code:
            out.append(line)
            continue
        if not stripped:
            out.append(line)
            can_join = False
            continue
        start = BLOCK_START.match(stripped)
        if start:
            out.append(line)
            kind = start.group(1)
            can_join = kind[0] in "-*" or kind[0].isdigit()
            continue
        if can_join:
            out[-1] = out[-1].rstrip() + " " + stripped
        else:
            out.append(line)
            can_join = True
    return "\n".join(out)


def to_wiki(text, macro):
    out = []
    lines = unwrap(text).splitlines()
    list_stack = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if not re.match(r"^\s*([-*]|\d+\.)\s+", line):
            list_stack = []
        match = PLACEHOLDER.match(line)
        if match:
            body = load_diagram(match.group(1), strip_markers=True)
            out.append("{%s}\n%s\n{%s}" % (macro, body, macro))
            i += 1
            continue
        fence = re.match(r"^```(\w*)\s*$", line)
        if fence:
            lang = fence.group(1)
            i += 1
            block = []
            while i < len(lines) and not lines[i].startswith("```"):
                block.append(lines[i])
                i += 1
            i += 1
            opener = "{code:%s}" % lang if lang else "{code}"
            out.append("%s\n%s\n{code}" % (opener, "\n".join(block)))
            continue
        heading = re.match(r"^(#{1,6})\s+(.*)$", line)
        if heading:
            out.append("h%d. %s" % (len(heading.group(1)),
                                    convert_inline(heading.group(2))))
            i += 1
            continue
        if line.lstrip().startswith("|") and line.rstrip().endswith("|"):
            header = True
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                row = lines[i].strip().strip("|")
                if re.match(r"^[\s:|-]+$", row):
                    i += 1
                    continue
                cells = [convert_inline(c.strip()) or " "
                         for c in re.split(r"(?<!\\)\|", row)]
                sep = "||" if header else "|"
                out.append(sep + sep.join(cells) + sep)
                header = False
                i += 1
            continue
        bullet = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", line)
        if bullet:
            depth = len(bullet.group(1).replace("\t", "  ")) // 2 + 1
            mark = "#" if bullet.group(2)[0].isdigit() else "*"
            # Confluence nests mixed lists by prefix, for example "#*".
            list_stack = list_stack[:depth - 1]
            while len(list_stack) < depth - 1:
                list_stack.append("*")
            list_stack.append(mark)
            out.append("%s %s" % ("".join(list_stack),
                                  convert_inline(bullet.group(3))))
            i += 1
            continue
        out.append(convert_inline(line))
        i += 1
    return "\n".join(out) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out",
                        default=str(ROOT.parents[1] / "build" / "confluence"),
                        help="output directory (default: build/confluence, "
                        "which git ignores)")
    parser.add_argument("--macro", default="plantuml",
                        help="Confluence PlantUML macro name")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    pages = sorted(ROOT.glob("[0-9][0-9]-*.md"))
    if not pages:
        sys.exit("no pages found in %s" % ROOT)
    order = ["Create the pages in this order. Page 00 is the parent;",
             "every other page is a child of it.", ""]
    for page in pages:
        text = page.read_text(encoding="utf-8")
        title_match = re.match(r"^#\s+(.*)\n+", text)
        title = title_match.group(1).strip() if title_match else page.stem
        # Confluence shows the page title itself, so drop the H1.
        body = text[title_match.end():] if title_match else text
        (out_dir / (page.stem + ".md")).write_text(
            inline_markdown(text), encoding="utf-8")
        (out_dir / (page.stem + ".wiki")).write_text(
            to_wiki(body, args.macro), encoding="utf-8")
        parent = "(top page)" if page.stem.startswith("00-") \
            else "child of page 00"
        order.append("%-44s title: %s  [%s]" %
                     (page.stem + ".wiki", title, parent))
        print("wrote", page.stem)
    (out_dir / "PUBLISH_ORDER.txt").write_text("\n".join(order) + "\n",
                                               encoding="utf-8")
    if MISSING:
        sys.exit("missing diagrams: %s" % ", ".join(sorted(set(MISSING))))


if __name__ == "__main__":
    main()
