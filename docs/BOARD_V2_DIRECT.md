# Board v2 — Direct-to-Morpho Design (supersedes JST harness)

Simplified single-perfboard design. Nucleo sits directly on morpho headers,
all signal traces short. No isolators, no stacked perf, no JST harness.
See `docs/SHIELD_LAYOUT.md` and `docs/JST_CONNECTORS.md` for the deprecated
v1 design and lessons learned (ADuM1201 pinout mismatch, BKIN latch behavior
— still useful background even though the topology changed).

## Why the change

v1 (stacked perf + JST + Si8621 isolators) had long flying leads and
hand-soldered SOIC-8 breakouts — too many failure points, confirmed the hard
way debugging a dead isolator channel. v2 removes both: single board, short
direct traces, isolators dropped (their isolation rationale was already gone
once we went single-star-ground; see below for what replaces their level-shift
function).

---

## PWM Output Path (Nucleo → 74HCT14 → IXDN604 → GDT)

```mermaid
graph LR
    PA8["PA8 (TIM1_CH1)"] -->|"100Ω, short trace"| HCT1["74HCT14<br/>Gate 1 IN"]
    PB13["PB13 (TIM1_CH1N)"] -->|"100Ω, short trace"| HCT2["74HCT14<br/>Gate 2 IN"]
    HCT1 -->|"~5V out"| IXDN1["IXDN604 #1<br/>IN (pin 4)"]
    HCT2 -->|"~5V out"| IXDN2["IXDN604 #2<br/>IN (pin 4)"]
    IXDN1 -->|"OUT (pin 2)"| GDT1["GDT Primary A"]
    IXDN2 -->|"OUT (pin 2)"| GDT2["GDT Primary B"]
```

> **Use SN74HCT14N (TI), NOT the SN74HC14N, for this level-shift.** IXDN630MCI
> needs VIH=3.5V min. The genuine TI SN74HC14N at VCC=4.5-5V has VIH≈3.15-3.5V
> (0.7×VCC, CMOS-level) — a 3.3V input from the Nucleo is marginal/out-of-spec
> against that. The SN74HCT14N has TTL-compatible inputs, VIH=2.0V FIXED
> regardless of VCC, so 3.3V clears it with real margin. Pin-compatible
> drop-in, same DIP-14 footprint. **Confirmed in parts drawer — no order
> needed.** 10kΩ pulldown stays on each IXDN604 input (fail-safe if the
> 74HCT14 output is ever disconnected).

## Feedback / Fault Path (Power stage → Nucleo)

```mermaid
graph RL
    OCP["OCP Comparator<br/>(future)"] -->|"3.3V logic, pull-up to 3.3V for now"| PB12["PB12 (TIM1_BKIN)"]
    CT["CT Burden"] -->|"10k/15k divider"| HC14["74HC14<br/>(existing, Schmitt)"]
    HC14 -->|"3.3V VCC — see note"| PA0["PA0 (TIM2_CH1)"]
```

> **Run the existing 74HC14 (CT frequency conditioner) from 3.3V, not 5V.**
> Its output feeds directly into PA0 — a 5V swing would exceed the Nucleo's
> GPIO absolute max (~VDD+0.3V ≈ 3.6V). At 3.3V VCC, HC14 thresholds become
> VIH≈2.3V / VIL≈1.0V — recheck the CT divider still crosses these cleanly.
> **BKIN (PB12):** until the OCP comparator is built, add a pull-up to
> **3.3V** (not 5V) so it idles safely HIGH (break inactive). When the
> comparator is added, its output must also be 3.3V-logic (or clamped/divided)
> before reaching PB12 — never feed it 5V directly.

## User Interface (unchanged from v1)

```mermaid
graph LR
    subgraph Display
        PA5["PA5 SCK"] --> TFT["ST7735S TFT"]
        PA7["PA7 MOSI"] --> TFT
        PB6["PB6 CS"] --> TFT
        PC7["PC7 DC"] --> TFT
        PA9["PA9 RST"] --> TFT
        PB2["PB2 BLK"] --> TFT
    end
    subgraph Controls
        ENC["EC11 Encoder"] --> PB4["PB4 ENC_A"]
        ENC --> PB5["PB5 ENC_B"]
        ENC --> PC13["PC13 BTN"]
    end
```

## Power Distribution (shared ground, no isolation)

```mermaid
graph TD
    IN12["12V INPUT"] --> NUC["Nucleo VIN (7-12V)"]
    IN5["5V INPUT"] --> HCT["74HCT14 VCC"]
    IN5 --> HC["74HC14 VCC (3.3V — see note above, not 5V!)"]
    IN15["15V (existing separate rail)"] --> IXDN["IXDN604 VCC (both)"]

    IN12 --> STAR["★ SHARED GROUND ★"]
    IN5 --> STAR
    IN15 --> STAR
    NUC --> STAR
```

> Correction to the note above: if 74HC14 runs from 3.3V, source that 3.3V
> from the Nucleo's own regulator output (short trace), not the 5V input rail.
> So really: **12V → Nucleo VIN**, **5V → 74HCT14 + IXDN604 logic-adjacent
> needs**, **Nucleo 3.3V → 74HC14 VCC**, **15V (separate, existing) → IXDN604
> VCC**, all sharing one ground.

**Rail summary:**

| Rail | Source | Powers |
|------|--------|--------|
| 12V | Board input | Nucleo VIN |
| 5V | Board input | 74HCT14 VCC |
| 3.3V | Nucleo onboard regulator | 74HC14 VCC (CT conditioner), pull-ups |
| 15V | Existing separate rail | IXDN604 VCC (gate drive) |

---

## BOM Changes from v1

**Removed:**
- 2x Si8621BB-B-IS + ADuM1201 breakouts
- 2x 8-pin JST connectors + associated wiring
- TVS diodes / Schottky clamps that were part of the isolator input protection

**Added:**
- 1x **SN74HCT14N** (TI, DIP-14) — PWM level-shift, 2 gates used (4 spare).
  **In parts drawer, confirmed, no order needed.**
- 1x pull-up resistor (10kΩ to 3.3V) on BKIN, temporary until OCP comparator exists

**Unchanged:**
- Existing **SN74HC14N** (TI, genuine, CT frequency conditioner) — reroute its
  VCC from 5V to 3.3V (see feedback path note above)
- IXDN604 x2, GDT, gate resistors/diodes, TFT, encoder, LEDs, NTC

> Two different 74x14 parts in this design, same DIP-14 pinout, different
> logic families — **do not mix them up on the bench.** SN74HCT14N = PWM
> level-shift (near the IXDN604s). SN74HC14N = CT feedback conditioner (near
> the frequency sense input). Consider labeling them physically.

## Physical Layout Notes

- Nucleo morpho headers (CN7/CN10) soldered directly to the new perfboard —
  no cables between Nucleo and driver components
- Keep PA8/PB13 → 74HCT14 traces as short as physically possible (this was
  the exact class of problem that caused the v1 debug session)
- 74HCT14 and IXDN604s clustered close together, short traces between them too
- TFT + encoder can stay on longer leads (low-speed, non-critical signals)
- Single ground — no star-point complexity needed for a one-board design,
  just a solid ground plane/bus

## Still TODO on this design

- Confirm CT divider values still work with 74HC14 at 3.3V VCC (was sized
  assuming 5V thresholds)
- Design/wire the OCP comparator (currently just a 3.3V pull-up placeholder
  on BKIN)
- Verify 74HCT14 availability (you may need to order — check stock first)
