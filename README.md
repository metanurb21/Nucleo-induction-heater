# Nucleo Induction Heater — Digital PLL Controller

> **Work in progress.** This is an active build. Hardware is being assembled
> and the firmware is early scaffolding. Nothing here is final yet.

## What this is

This project is the next chapter of my
[esp32_igbt_driver](https://github.com/metanurb21/esp32_igbt_driver) — a
homemade high-power induction heater capable of melting copper, iron, and
aluminum. That first build got things working: an ESP32 generating the gate
signals, manual frequency tuning, and enough power to liquify a pound of metal.
The old-timers in the high-voltage community lovingly call that approach
"tune for maximum smoke." It worked, but I always knew there was a better way.

This repo moves the whole thing onto an **STM32 Nucleo F446RE** and swaps manual
tuning for a **digital phase-locked loop (PLL)** that finds and follows the
tank's resonant frequency automatically.

## Why the change

The ESP32 version balanced a lot of jobs in software, and I eventually leaned on
extra analog hardware (a self-oscillating feedback board) to keep the switching
stable. It ran cleaner but I gave up some of the raw power I had when I was
hand-tuning right up to the edge.

The Nucleo F446RE fixes this at the hardware level. Its advanced timer generates
the complementary gate signals *and* the dead-time between them directly in
silicon — no drift, no extra chips, adjustable on the fly. That frees me to chase
two goals at once:

- **Better control** — the PLL tracks resonance as the workpiece heats and its
  properties change, instead of me chasing the dial.
- **More power** — with precise, rock-solid switching I can push harder and
  safely, including the ability to intentionally nudge off-resonance for maximum
  coupling into the metal (the good part of "maximum smoke," minus the smoke).

## Why the Nucleo over the ESP32

This is also a learning project — my first real dive into STM32 after years of
ESP32 work. The benefits that made it worth the learning curve:

- **Hardware complementary PWM with dead-time** — the exact feature this
  application needs, built into the timer peripheral.
- **Hardware break input** — a fault line that kills the gate drive in
  nanoseconds, no software involved. Real protection for expensive IGBTs.
- **Precise, jitter-free timing** — dedicated timers for PWM, frequency
  capture, and the encoder, all running independently of the main loop.
- **One chip does it all** — the ESP32 build used two microcontrollers for
  redundancy. The Nucleo's hardware safety layers replace that with a single,
  more capable board.

## The plan (high level)

1. Build the controller board — Nucleo on a stacked perfboard with signal
   buffering, gate drivers, feedback conditioning, display, and safety
   interlocks.
2. Bring the firmware up in stages, verifying every signal on the scope before
   any power is applied.
3. Close the PLL loop on the real tank circuit.
4. Tune for stable resonance tracking, then optimize for power.
5. Run it up gently on a variac (always) and melt some metal.

## Status

Control board coming together, firmware partly verified:

- ✅ Board boots, TFT display, rotary encoder, temperature sensing all working
- ✅ Gate-drive waveforms verified on the scope — complementary output with
  adjustable hardware dead-time, frequency accurate to ~0.1%
- 🔨 Wiring the signal buffers and gate drivers now
- ⏳ Next: close the PLL loop on real tank feedback, then low-voltage gate drive
- ⏳ Then: gradual full-power runs on the variac

Everything here will change as the build progresses.

## Safety note

This machine runs off mains power, switches hundreds of amps, and gets things
hot enough to melt metal. It's built and operated with a lot of respect,
layered protection, EMI filtering, and a variac on the input. This repo
documents a personal project — it is not a build guide, and high-voltage work
is dangerous.

## Related

- [esp32_igbt_driver](https://github.com/metanurb21/esp32_igbt_driver) — the
  original ESP32-based build this continues from.

---

*Built by Dave (metanurb) — one of several tinkering projects in the workshop.*
