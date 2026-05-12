# Adaptive Bitrate & Latency Control
## Human‑Readable Conceptual Overview

This document explains **how and why** the ABR and latency‑control system behaves the way it does.  
It is intended for engineers, reviewers, and stakeholders who want to understand the design principles without parsing a rule‑heavy normative spec.

---

## Big Picture

The player is continuously balancing three competing forces:

1. **Avoid rebuffering** (most important)
2. **Deliver the best possible picture quality**
3. **Stay close to the live edge**

**Guiding philosophy:**

> *Freezes are always worse than pixelation,*  
> *and large ABR decisions matter far more than tiny playback‑speed tweaks.*

---

## Core Mental Model

### Buffer is the truth

The **buffer** represents how many seconds of fully decodable video are ready to play.

- Measured in media time at normal speed (1×)
- Only changes when:
  - new video is injected, or
  - playback consumes it
- No guessing, smoothing, or reinterpretation occurs

If the buffer ever reaches zero, playback freezes and the user notices immediately.

---

### Underflow vs. danger

**Underflow** means:
- there is literally no more video to show
- the user sees frozen video

**Dangerously low buffer** (defined by `cfgDangerBuffer`) is an early warning:
- a “dodged bullet” moment
- the system should act *before* the user feels pain

This separation allows proactive behavior without redefining failure.

---

## How ABR Decisions Work

### Predict first, act conservatively

Before choosing a video quality, the player:
- estimates how long the next segment will take to download
- based on recent network behavior

This is a best guess, not a guarantee — but it’s good enough to make smart choices.

---

### The single most important rule

> **If a choice is predicted to cause underflow, and another choice exists that avoids it, the bad choice is forbidden.**

This is a hard safety rule.  
Quality, latency, and smoothness never override it.

---

### Normal selection

Among all safe options, the player chooses the **highest‑quality profile** that:
- is predicted to arrive before the buffer runs dry

