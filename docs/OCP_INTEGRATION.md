# OCP Board Integration — Bolt-On, Unmodified

**Decision:** reuse the SCITUBE HD Flip-Flop Current Control board exactly
as originally designed — populate the whole board, no skipped sections,
no trace cuts. It's a proven, self-contained protection unit (current AND
tank-voltage sensing, working together) that ran successfully on the
original heater. The goal here is only to get its trip signal into the
Nucleo for software visibility/control — not to redesign or simplify it.

This replaces the earlier populate/skip/cut build sheet, which was solving
a problem that no longer exists once the board is populated in full.

---

## 1. Populate the entire board

Every silkscreen reference gets populated, per the original design:

- Front end: CT input, 220Ω (sub for 200Ω), 330nF coupling, 1N5819 clamp
  pair (D2/D3), 1N4739A zener pair (D4/D5)
- Comparator: MPSA06 (Q1), R12 (47kΩ sub for 51k), R16 (15k), R13, R1
  (10k each), 10T 20kΩ "Current Limit" pot
- Latch: U13 (74HC14 Schmitt cleanup), U12 (MM74HCT74N)
- Soft-start/ramp: 555, "START FREQ" 200kΩ pot, R20 (100k), R17 (1k), LED1
- Tank-voltage sense: R2 (100k, 1W), D1, C8 (1kV/10nF) — feeds into the
  pot network alongside the current-threshold tap
- Power: U11 (regulator), C1 (0.1µF/50V)
- Driver output: UCC27425 — populate if you want the board to also drive
  its original output role; not required for the Nucleo tap below, but
  no reason to leave it off either since this is a full, unmodified build

Substitutions confirmed fine and already accounted for: **220Ω** (was
200Ω), **47kΩ** (was 51k, R12). No other substitutions needed.

**No trace cuts. No jumpers. No bypassed sections.**

---

## 2. What connects to the Nucleo (the only new wiring)

### 2a. CT/burden input (existing connection point on the OCP board)

Wire a **parallel tap** off the same two burden wires already feeding the
PLL-feedback 74HC14 (on the separate board) into this OCP board's
**"FB CURRENT XFMR"** pad. Don't disturb the existing PLL-feedback
connection — this is a second, parallel tap, not in series.

### 2b. Tank voltage sense input

**Confirmed: Tank V+ and Tank V- are a differential tap straight off the
work coil itself — one wire on each leg.** Not off the DC bus, not off
the H-bridge output directly — the coil's own terminals.

**⚠️ This is a genuinely high-voltage connection, treat with HV-level
care.** In a resonant tank circuit, the voltage across the coil at
resonance can swing well ABOVE the DC bus voltage (that's the nature of
resonant voltage rise — the whole reason the tank works) — consistent
with C8 being rated 1kV in this network. Wire this exactly as the
original heater did (same wire gauge/rating, same physical routing/
clearance practices), and treat it with the same respect as any other
HV connection on this build (mains, DC bus, H-bridge) — not as a casual
low-voltage sense wire.

Replicate the original heater's exact tap points and wiring on Board v2's
work coil — same two legs, same polarity (V+ / V-) if that mattered in
the original design (worth confirming — some peak-detector networks care
about polarity, others don't; check D1's orientation relative to which
leg is V+ vs V- if you're not 100% sure it's symmetric).

### 2c. Power

Supply the board's own power input (feeds U11 → regulated rail for the
logic ICs). Confirm the voltage/rating this expects — likely the same
supply used elsewhere on your original build (12V? Check what U11
regulates from before wiring in Board v2's rails.)

### 2d. Trip signal out → PB12

**74HCT74 pin 6 (1Q̄)** is the confirmed trip output — active per the
latch, feeds (in the original board) into the UCC27425's enable/shutdown
pins. Tap this same pin/pad for the Nucleo.

**Voltage note — this board runs on 5V logic (confirmed at 74HCT74 pin
14).** STM32 PB12 absolute max is ~3.6V. **Do not wire pin 6 directly to
PB12** — build a small resistor divider in line:
- Suggested starting point: 10kΩ / 10kΩ (5V → ~2.5V at the midpoint,
  safe margin under 3.6V).
- Confirm pin 6's actual measured HIGH/LOW swing once the board is
  powered, before finalizing the divider ratio (see bring-up doc).
- Wire the divider midpoint to **PB12 (TIM1_BKIN)**.

### 2e. Fault clear — decide based on whether you want the board's own auto-clear behavior

The board's original design auto-clears nuisance trips during its 555-driven
soft-start ramp, then latches for real once past that window. Since the
whole board (555 included) is staying intact and unmodified, **this
auto-clear behavior is still active** — you likely don't need to add a
separate STM32-driven clear signal, since the board handles its own reset
timing the same way it always did on the original heater.

If you still want the option to manually/programmatically force-clear a
trip from the Nucleo (e.g. to unify with the encoder-button fault-clear
flow already in `StateManager::handleButton()`), that would mean adding a
new wire from a spare GPIO to 74HCT74 pin 1 (CLR) IN PARALLEL with the
existing pot/R1/C8 network already on that pin — no cut needed, just an
added parallel drive. Optional — decide once you've seen how the board
behaves in practice on the bench.

---

## Build order

1. Populate the entire board per section 1.
2. Power it up standalone (5V + whatever Tank V+ needs) and confirm it
   behaves as it did on the original heater — LED blinking during
   soft-start, pot adjusts threshold, etc. This alone proves the
   rebuild/population was done correctly before touching the Nucleo at
   all.
3. Wire the CT/burden parallel tap (2a).
4. Wire the Tank V+ connection (2b) and power (2c).
5. Build and wire the PB12 level-shift divider (2d) — bench-verify pin 6's
   real voltage swing first (see `docs/OCP_BRINGUP.md` Stages 1-2, still
   applicable for this verification step even though the board itself is
   no longer being selectively populated).
6. Decide on 2e (optional STM32 clear wire) once you've seen real
   behavior on the bench.
7. Only after Stage 4 of the bring-up doc (hardware fault path confirmed
   end-to-end with PWM running, no HV) — move to real coil testing.

---

## Parts needed beyond what's already gathered

- [ ] Full original BOM for ALL sections (555 timer, ramp pot, R20, R17,
      LED1, UCC27425 support parts) — these were previously marked "skip,"
      now needed since the whole board is being populated.
- [ ] Two resistors for the PB12 level-shift divider (10kΩ/10kΩ suggested).
- [ ] Confirm you still have (or need) the Tank V+ tap point identified/
      wired — this wasn't needed in the earlier "current-only" plan.
