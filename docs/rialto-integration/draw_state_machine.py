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
    pip install graphviz          # Python bindings
    apt install graphviz          # or: brew install graphviz (macOS)

Usage
~~~~~
    python3 docs/rialto-integration/draw_state_machine.py

Outputs
~~~~~~~
    docs/rialto-integration/player_state_machine.svg  (default)
    docs/rialto-integration/player_state_machine.png  (--format png)

Optional flags
~~~~~~~~~~~~~~
    --format  dot|svg|png|pdf   (default: svg)
    --output  <path>            override output file path
    --view                      open the rendered image after generation
"""

import argparse
import sys
try:
    import graphviz
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
    ("STOPPED",           "Stop() was called"),
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

    # ── Flush / seek ───────────────────────────────────────────────────────
    ("SOURCES_ATTACHED",  "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),
    ("PLAYING",           "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),
    ("PAUSED",            "onFlush",              "flush() + setSourcePosition()", "FLUSHING"),

    # After flush + re-configure, new init fragments re-drive attachment.
    ("FLUSHING",          "onSourceAttaching",    "attachSource()",        "SOURCES_ATTACHING"),

    # ── Stop — valid from any non-terminal state ───────────────────────────
    ("PIPELINE_CREATED",  "onStop",               "stop()",                "STOPPED"),
    ("SOURCES_ATTACHING", "onStop",               "stop()",                "STOPPED"),
    ("SOURCES_ATTACHED",  "onStop",               "stop()",                "STOPPED"),
    ("PLAYING",           "onStop",               "stop()",                "STOPPED"),
    ("PAUSED",            "onStop",               "stop()",                "STOPPED"),
    ("FLUSHING",          "onStop",               "stop()",                "STOPPED"),

    # ── Error — valid from any non-terminal state ──────────────────────────
    ("PIPELINE_CREATED",  "onError",              "FAILURE notification",  "ERROR"),
    ("SOURCES_ATTACHING", "onError",              "FAILURE notification",  "ERROR"),
    ("SOURCES_ATTACHED",  "onError",              "FAILURE notification",  "ERROR"),
    ("PLAYING",           "onError",              "FAILURE notification",  "ERROR"),
    ("PAUSED",            "onError",              "FAILURE notification",  "ERROR"),
    ("FLUSHING",          "onError",              "FAILURE notification",  "ERROR"),

    # ── Reconfigure (re-tune) — valid from every state ────────────────────
    ("IDLE",              "onReconfigure",        "re-tune",               "IDLE"),
    ("PIPELINE_CREATED",  "onReconfigure",        "re-tune",               "IDLE"),
    ("SOURCES_ATTACHING", "onReconfigure",        "re-tune",               "IDLE"),
    ("SOURCES_ATTACHED",  "onReconfigure",        "re-tune",               "IDLE"),
    ("PLAYING",           "onReconfigure",        "re-tune",               "IDLE"),
    ("PAUSED",            "onReconfigure",        "re-tune",               "IDLE"),
    ("FLUSHING",          "onReconfigure",        "re-tune",               "IDLE"),
    ("STOPPED",           "onReconfigure",        "re-tune",               "IDLE"),
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
    "STOPPED":           ("#FAFAFA", "#616161"),   # grey
    "ERROR":             ("#FFEBEE", "#C62828"),   # red
}

EDGE_COLOURS = {
    "onStop":        "#757575",   # grey
    "onError":       "#C62828",   # red
    "onReconfigure": "#0288D1",   # light-blue
}
DEFAULT_EDGE_COLOUR = "#333333"

# ---------------------------------------------------------------------------
# Build graph
# ---------------------------------------------------------------------------

def build_graph(show_reconfigure: bool = True) -> graphviz.Digraph:
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
        shape = "doublecircle" if state in ("STOPPED", "ERROR") else "box"
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
        is_self = (src == dst)
        dot.edge(
            src, dst,
            label=label,
            color=colour,
            fontcolor=colour,
            fontname="Helvetica",
            fontsize="9",
            penwidth="1.5" if not is_self else "1.0",
            style="dashed" if event in ("onStop", "onError", "onReconfigure") else "solid",
            constraint="false" if is_self else "true",
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
        "--format", choices=["dot", "svg", "png", "pdf"], default="svg",
        help="Output format (default: svg)."
    )
    parser.add_argument(
        "--output", default=None,
        help="Output file path (without extension). "
             "Default: docs/rialto-integration/player_state_machine"
    )
    parser.add_argument(
        "--view", action="store_true",
        help="Open the rendered image after generation."
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

    dot = build_graph(show_reconfigure=not args.no_reconfigure)

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
