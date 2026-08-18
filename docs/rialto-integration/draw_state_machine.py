#!/usr/bin/env python3
"""
draw_state_machine.py
---------------------
Generate a directed graph of the AampRialtoPlayer state machine (GoF State
pattern, implemented in direct-rialto/AampPlayerStateMachine.cpp).

Each node is a player state.  Each edge is labelled with the event name and
any action performed during the transition.

Dependencies
~~~~~~~~~~~~
    pip install graphviz          # Python bindings (not needed for plantuml)
    apt install graphviz          # or: brew install graphviz (macOS)

Usage
~~~~~
    python3 docs/rialto-integration/draw_state_machine.py
    python3 docs/rialto-integration/draw_state_machine.py --format plantuml

Outputs
~~~~~~~
    docs/rialto-integration/player_state_machine.svg   (default)
    docs/rialto-integration/player_state_machine.png   (--format png)
    docs/rialto-integration/player_state_machine.puml  (--format plantuml)

Optional flags
~~~~~~~~~~~~~~
    --format  dot|svg|png|pdf|plantuml   (default: svg)
    --output  <path>                     override output file path
    --view                               open the rendered image after generation
    --no-reconfigure                     omit onReconfigure arcs
"""

import argparse
import sys

# ---------------------------------------------------------------------------
# Graphviz is optional — only needed for non-plantuml formats
# ---------------------------------------------------------------------------
def _require_graphviz():
    try:
        import graphviz
        return graphviz
    except ImportError:
        sys.exit(
            "ERROR: 'graphviz' Python package not found.\n"
            "Install it with:  pip install graphviz\n"
            "and make sure the Graphviz binaries are also installed:\n"
            "  Linux:  sudo apt install graphviz\n"
            "  macOS:  brew install graphviz"
        )

# ---------------------------------------------------------------------------
# State machine definition
# ---------------------------------------------------------------------------
# Each entry in TRANSITIONS is:
#   (from_state, event, action, to_state)
#
# 'event'  — the IPlayerState virtual method name (human-readable label)
# 'action' — what the player does during this transition (may be empty)

STATES = [
    ("IDLE",              "Constructed; pipeline not yet created"),
    ("PIPELINE_CREATED",  "IMediaPipeline::load() succeeded"),
    ("SOURCES_ATTACHING", "Waiting for all attachSource() calls"),
    ("SOURCES_ATTACHED",  "allSourcesAttached() sent to Rialto server"),
    ("PLAYING",           "Server confirmed PLAYING"),
    ("PAUSED",            "Server confirmed PAUSED"),
    ("FLUSHING",          "Flush() in progress; awaiting new segments"),
    ("FLUSHED",           "SEEK_DONE received; awaiting Rialto PLAYING/PAUSED"),
    ("ERROR",             "Server reported a fatal error"),
]

TRANSITIONS = [
    # ── Normal playback lifecycle ──────────────────────────────────────────
    ("IDLE",              "onPipelineLoaded",     "load() OK",             "PIPELINE_CREATED"),
    ("PIPELINE_CREATED",  "onSourceAttaching",    "attachSource()",        "SOURCES_ATTACHING"),
    ("SOURCES_ATTACHING", "onAllSourcesAttached", "allSourcesAttached()",  "SOURCES_ATTACHED"),
    ("SOURCES_ATTACHED",  "onPlaybackStarted",    "play() confirmed",      "PLAYING"),
    ("PLAYING",           "onPlaybackPaused",     "pause() confirmed",     "PAUSED"),
    ("PAUSED",            "onPlaybackStarted",    "play() confirmed",      "PLAYING"),

    # ── Pending position (not an FSM state) ────────────────────────────────
    # Discontinuity() no longer drives any state transition. It sets the
    # out-of-band m_positionPending flag (gating sources, deferring play())
    # so that whichever track's SendSample()/SendTransfer()/SendCopy() first
    # observes m_positionPending calls MaybeFlushForPendingPosition(), which
    # issues the real onFlush() below using that track's first post-event
    # PTS. Configure() also arms m_positionPending on every tune (including
    # first tune), so the same mechanism resolves the initial position.

    # ── Flush / seek ───────────────────────────────────────────────────────
    ("SOURCES_ATTACHED",  "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),
    ("PLAYING",           "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),
    ("PAUSED",            "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),

    # Normal flush exit: Rialto's SEEK_DONE notification completes the
    # pipeline-level flushing seek.  onFlushComplete() takes the machine
    # unconditionally to FLUSHED - it does NOT remember or restore the
    # pre-flush state.
    ("FLUSHING",          "onFlushComplete",      "SEEK_DONE",             "FLUSHED"),

    # FLUSHED — Rialto's own subsequent notification (guaranteed to follow
    # SEEK_DONE for the same flush cycle) drives the machine the rest of the
    # way.  If Flush() happened before the first play() (pre-flush state was
    # SOURCES_ATTACHED), the machine simply waits in FLUSHED until
    # Stream()/play() eventually triggers Rialto's PLAYING notification -
    # functionally equivalent to the old SOURCES_ATTACHED-restore path.
    ("FLUSHED",           "onPlaybackStarted",    "PLAYING notification",  "PLAYING"),
    ("FLUSHED",           "onPlaybackPaused",     "PAUSED notification",   "PAUSED"),
    ("FLUSHED",           "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),

    # After flush + re-configure, new init fragments re-drive attachment.
    # Note: there is NO direct FLUSHING→SOURCES_ATTACHING transition via
    # onSourceAttaching.  A pipeline rebuild always goes through onReconfigure
    # (FLUSHING→IDLE) before sources re-attach.  onSourceAttaching from
    # FLUSHING is unreachable and will produce a WARN log if it ever fires.

    # ── Stop — valid from all active states except FLUSHING (Stop() waits for
    # flush to complete before dispatching onStop, so FLUSHING is never the
    # current state) and IDLE (nothing to stop).
    ("PIPELINE_CREATED",  "onStop",               "stop()",                "IDLE"),
    ("SOURCES_ATTACHING", "onStop",               "stop()",                "IDLE"),
    ("SOURCES_ATTACHED",  "onStop",               "stop()",                "IDLE"),
    ("PLAYING",           "onStop",               "stop()",                "IDLE"),
    ("PAUSED",            "onStop",               "stop()",                "IDLE"),
    ("FLUSHED",           "onStop",               "stop()",                "IDLE"),
    ("ERROR",             "onStop",               "stop()",                "IDLE"),

    # ── Error — valid from any non-terminal state ──────────────────────────
    ("PIPELINE_CREATED",  "onError",              "FAILURE notification",  "ERROR"),
    ("SOURCES_ATTACHING", "onError",              "FAILURE notification",  "ERROR"),
    ("SOURCES_ATTACHED",  "onError",              "FAILURE notification",  "ERROR"),
    ("PLAYING",           "onError",              "FAILURE notification",  "ERROR"),
    ("PAUSED",            "onError",              "FAILURE notification",  "ERROR"),
    ("FLUSHING",          "onError",              "FAILURE notification",  "ERROR"),
    ("FLUSHED",           "onError",              "FAILURE notification",  "ERROR"),

    # ── Reconfigure (re-tune) — valid from IDLE (first tune) and all active
    # states except FLUSHING (Configure() calls WaitForFlushToComplete()
    # directly before dispatching onReconfigure, so FLUSHING is never current
    # when onReconfigure fires).
    ("IDLE",              "onReconfigure",        "re-tune",               "IDLE"),
    ("PIPELINE_CREATED",  "onReconfigure",        "re-tune",               "IDLE"),
    ("SOURCES_ATTACHING", "onReconfigure",        "re-tune",               "IDLE"),
    ("SOURCES_ATTACHED",  "onReconfigure",        "re-tune",               "IDLE"),
    ("PLAYING",           "onReconfigure",        "re-tune",               "IDLE"),
    ("PAUSED",            "onReconfigure",        "re-tune",               "IDLE"),
    ("FLUSHED",           "onReconfigure",        "re-tune",               "IDLE"),

    ("ERROR",             "onReconfigure",        "re-tune",               "IDLE"),
]

# ---------------------------------------------------------------------------
# Colour scheme
# ---------------------------------------------------------------------------
STATE_COLOURS = {
    "IDLE":              ("#E8F4FD", "#2196F3"),   # light-blue / blue
    "PIPELINE_CREATED":  ("#E3F2FD", "#1565C0"),
    "SOURCES_ATTACHING": ("#FFF8E1", "#FFA000"),   # amber
    "SOURCES_ATTACHED":  ("#E8F5E9", "#2E7D32"),   # green
    "PLAYING":           ("#F3E5F5", "#6A1B9A"),   # purple
    "PAUSED":            ("#FBE9E7", "#BF360C"),   # deep-orange
    "FLUSHING":          ("#EFEBE9", "#4E342E"),   # brown
    "FLUSHED":           ("#D7CCC8", "#4E342E"),   # darker brown
    "ERROR":             ("#FFEBEE", "#C62828"),   # red
}

EDGE_COLOURS = {
    "onStop":        "#757575",   # grey
    "onError":       "#C62828",   # red
    "onReconfigure": "#0288D1",   # light-blue
}
DEFAULT_EDGE_COLOUR = "#333333"

# ---------------------------------------------------------------------------
# PlantUML colour mappings
# ---------------------------------------------------------------------------
# Map our hex fill colours to PlantUML named/hex colours.
# PlantUML state background: #RRGGBB or named colour.
PUML_STATE_COLOURS = {
    "IDLE":              "#E8F4FD",
    "PIPELINE_CREATED":  "#E3F2FD",
    "SOURCES_ATTACHING": "#FFF8E1",
    "SOURCES_ATTACHED":  "#E8F5E9",
    "PLAYING":           "#F3E5F5",
    "PAUSED":            "#FBE9E7",
    "FLUSHING":          "#EFEBE9",
    "FLUSHED":           "#D7CCC8",
    "ERROR":             "#FFEBEE",
}

# Map event → PlantUML line colour for the arrow.
PUML_EDGE_COLOURS = {
    "onStop":        "#757575",
    "onError":       "#C62828",
    "onReconfigure": "#0288D1",
}
PUML_DEFAULT_EDGE_COLOUR = "#333333"

# States that are rendered as end-states (double circle) in PlantUML.
TERMINAL_STATES = {"ERROR"}


def build_plantuml(show_reconfigure: bool = True) -> str:
    """Return a PlantUML @startuml … @enduml string for the state machine."""
    lines = []
    lines.append("@startuml AampRialtoPlayer_StateMachine")
    lines.append("hide empty description")
    lines.append("")
    lines.append("title AampRialtoPlayer — GoF State Machine\\n"
                 "(direct-rialto/AampPlayerStateMachine.cpp)")
    lines.append("")
    lines.append("' ── Skinparam ──────────────────────────────────────────")
    lines.append("skinparam state {")
    lines.append("    FontName Helvetica")
    lines.append("    FontSize 11")
    lines.append("    BorderColor #333333")
    lines.append("}")
    lines.append("skinparam ArrowFontName Helvetica")
    lines.append("skinparam ArrowFontSize 9")
    lines.append("")
    lines.append("' ── State declarations ──────────────────────────────────")
    for state, tooltip in STATES:
        fill = PUML_STATE_COLOURS.get(state, "#FFFFFF")
        if state in TERMINAL_STATES:
            # Double-border via <<end>> stereotype won't keep custom colours;
            # use a note instead and mark with bold border.
            lines.append(f'state "{state}" as {state} {fill}##[bold]')
        else:
            lines.append(f'state "{state}" as {state} {fill}')
        lines.append(f'    {state} : {tooltip}')
    lines.append("")
    lines.append("' ── Initial state ───────────────────────────────────────")
    lines.append("[*] --> IDLE")
    lines.append("")
    lines.append("' ── Transitions ─────────────────────────────────────────")
    for (src, event, action, dst) in TRANSITIONS:
        if not show_reconfigure and event == "onReconfigure":
            continue
        colour = PUML_EDGE_COLOURS.get(event, PUML_DEFAULT_EDGE_COLOUR)
        label = f"{event}\\n[{action}]" if action else event
        is_dashed = event in ("onStop", "onError", "onReconfigure")
        arrow = f"-[{colour},dashed]->" if is_dashed else f"-[{colour}]->"
        lines.append(f"{src} {arrow} {dst} : {label}")
    lines.append("")
    lines.append("@enduml")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Build graph (Graphviz)
# ---------------------------------------------------------------------------

def build_graph(show_reconfigure: bool = True):
    graphviz = _require_graphviz()
    dot = graphviz.Digraph(
        name="AampRialtoPlayer State Machine",
        comment="GoF State pattern — PlayerStateMachine",
    )
    dot.attr(
        rankdir="TB",
        fontname="Helvetica",
        fontsize="12",
        label=(
            r"AampRialtoPlayer — GoF State Machine\n"
            r"(direct-rialto/AampPlayerStateMachine.cpp)"
        ),
        labelloc="t",
        pad="0.4",
        nodesep="0.6",
        ranksep="0.8",
        bgcolor="white",
    )

    # Nodes
    for state, tooltip in STATES:
        fill, border = STATE_COLOURS.get(state, ("#FFFFFF", "#333333"))
        shape = "doublecircle" if state in ("ERROR",) else "box"
        style = "filled,rounded"
        dot.node(
            state,
            label=state.replace("_", "\\n"),
            shape=shape,
            style=style,
            fillcolor=fill,
            color=border,
            fontname="Helvetica-Bold",
            fontsize="11",
            tooltip=tooltip,
        )

    # Initial state marker
    dot.node("__start__", label="", shape="point", width="0.2", color="#333333")
    dot.edge("__start__", "IDLE", color="#333333", penwidth="1.5")

    # Edges — optionally collapse all onReconfigure arcs into a note table
    for (src, event, action, dst) in TRANSITIONS:
        if not show_reconfigure and event == "onReconfigure":
            continue
        colour = EDGE_COLOURS.get(event, DEFAULT_EDGE_COLOUR)
        label  = f"{event}\\n[{action}]" if action else event
        is_self   = (src == dst)
        is_dashed = event in ("onStop", "onError", "onReconfigure")
        # Cross-cutting edges (onStop/onError/onReconfigure) must not affect
        # the rank computation or they pull ERROR/IDLE to extreme
        # positions, causing long edges to be routed through unrelated nodes
        # and misplacing their labels.
        dot.edge(
            src, dst,
            label=label,
            color=colour,
            fontcolor=colour,
            fontname="Helvetica",
            fontsize="9",
            penwidth="1.5" if not is_self else "1.0",
            style="dashed" if is_dashed else "solid",
            constraint="false" if (is_self or is_dashed) else "true",
        )

    return dot


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Draw the AampRialtoPlayer GoF state machine."
    )
    parser.add_argument(
        "--format", choices=["dot", "svg", "png", "pdf", "plantuml"],
        default="svg",
        help="Output format (default: svg). Use 'plantuml' to emit a .puml file."
    )
    parser.add_argument(
        "--output", default=None,
        help="Output file path (without extension). "
             "Default: docs/rialto-integration/player_state_machine"
    )
    parser.add_argument(
        "--view", action="store_true",
        help="Open the rendered image after generation (not supported for plantuml)."
    )
    parser.add_argument(
        "--no-reconfigure", action="store_true",
        help="Omit onReconfigure arcs to reduce visual clutter."
    )
    args = parser.parse_args()

    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_out = os.path.join(script_dir, "player_state_machine")
    output_path = args.output or default_out

    show_reconfigure = not args.no_reconfigure

    if args.format == "plantuml":
        puml_path = output_path + ".puml"
        content = build_plantuml(show_reconfigure=show_reconfigure)
        with open(puml_path, "w", encoding="utf-8") as fh:
            fh.write(content)
        print(f"PlantUML source written to: {puml_path}")
        return

    dot = build_graph(show_reconfigure=show_reconfigure)

    if args.format == "dot":
        # Write raw DOT source
        dot_path = output_path + ".dot"
        dot.save(dot_path)
        print(f"DOT source written to: {dot_path}")
    else:
        rendered = dot.render(
            filename=output_path,
            format=args.format,
            cleanup=True,
            view=args.view,
        )
        print(f"State machine diagram written to: {rendered}")


if __name__ == "__main__":
    main()
