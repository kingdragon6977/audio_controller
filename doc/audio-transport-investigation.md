# Audio transport investigation — 2026-10-12

The user confirms nothing is connected to the TLV320 analog inputs. Sample
statistics here are an unterminated idle-input observation, not microphone,
sensor, or calibrated ADC noise performance.

## Delivery comparison

Added ESP `STREAM_STATUS` counters and `STREAM_SUBSCRIBE`, selecting the
requesting PC as a unicast destination on the configured audio port. The
receiver now supports `--esp`, startup subscription retries, five-second
refresh, counter polling, and finite `--duration --discard` checks. Raw PCM
remains on stdout and all diagnostics on stderr.

Measured before/after switching only delivery mode:

| Measure | Broadcast (8 s) | Unicast (12 s) |
| --- | --- | --- |
| PC PCM packets | 65 | 407 |
| ESP successful PCM UDP submissions | 271 | 407 |
| ESP parsed PCM frames | 1,083 | 1,626 |
| Missing UART sequences | 0 | 0 |
| Malformed UART headers | 0 | 0 |
| Reported UDP submission failures | 0 | 0 |
| PC heartbeat messages | 0 | 12 |

Small boundary differences are possible between counter snapshots and receive
windows. Unicast delivered the observed submitted packet count; broadcast did
not. This isolates delivery mode as one cause of the low PC rate, without
establishing whether the AP, LAN, or host discards broadcast traffic. Packet
capture was unavailable without sudo authentication; the user was given a
narrow packet-header capture command to run locally, with no password shared.

Evidence: [comparison JSON](audio-transport-2026-10-12.json),
[receiver check](audio-receiver-2026-10-12.log),
[final receiver check](audio-receiver-final-2026-10-12.log).

## Source timing mismatch

The ESP receives approximately 135.26 frames/second, with 64 samples per frame:
about 8.66 ksample/s rather than the advertised nominal 24 ksample/s. A new
non-driving UART2 command `i2s rate` pauses PCM and counts PB12/WCLK rising
edges over 250 ms using the STM32 millisecond tick. Three estimates:
17,220, 17,240, and 17,196 Hz, rather than the expected 48,000 Hz.
GPIO polling is an estimate; an appropriate scope/frequency counter remains
authoritative. The approximate factor-of-two relationship to decimated PCM
agrees with the independent ESP timing observation.

Clock register readback matches the programmed profile: clock mux `0x03`,
PLL `P=1 R=1 J=6 D=1440`, NADC=8, MADC=2, AOSR=128. That profile assumes
16 MHz MCLK and nominally produces 48 kHz WCLK. The user subsequently confirmed a 16 MHz crystal on the AV6301 board and
recognized that isolating/unpowering the AV6301 likely removed the TLV clock
feed. This strongly suggests a missing reference; actual MCLK connectivity
and waveform remain unmeasured.

TI identifies MCLK as pin 24 (pin 21 is IOVDD) and states that fractional PLL operation requires
PLL input/P in the 10–20 MHz range and PLL output in the 80–110 MHz range.
Do not infer a valid MCLK from WCLK through the PLL formula when lock/input
validity is unknown, and do not retune PLL divisors against that inference.
Source: [TI TLV320ADC3101 datasheet, clocking and pin descriptions](https://www.ti.com/lit/ds/symlink/tlv320adc3101.pdf).

Evidence: [WCLK measurements](i2s-rate-2026-10-12.log),
[codec register readback](codec-clock-2026-10-12.log).
The nominal 24 kHz frame label is not a validated physical sample rate;
frequency analysis/playback timing must account for this unresolved mismatch.

## Idle-input observation and deployed changes

Unicast idle samples: mean about 250.46 counts, AC RMS about 6.71 counts,
range 227–274, no signed-16-bit endpoint clips. No input termination, test
signal, known sensor gain, or calibrated noise measurement was supplied.

Deployed a diagnostic STM32 image using automatic authorization, page/full-image
verification, and application recovery: 54,560 bytes, SHA-256
`fd91cc7236e967d0da478a11d9d16c61d290c661a53bb757b3a8bd976379128d`.
Final ESP image with counters/subscription SHA-256:
`318bb5882f945823143b3b6ec8ec88e6f06de33f1047f8a0a77e7b30d4ff0320`.
ESP uploads returned OK; both remain at the established IP/UART locations.
Source changes remain uncommitted. Native OTA tests and all 11 Python tests
passed; receiver compilation and live unicast behavior were checked.

Next required information: physical MCLK source/frequency (or a suitable
measurement at pin 24), then correct/validate codec clocking before trusting
sample timing. Sustained transport integrity and calibrated analog tests follow.

## Clock-source clarification

Master mode derives BCLK and WCLK from an external reference. MCLK is an input
on pin 24. A passive crystal needs oscillator circuitry; it is not a standalone
digital clock source. User reports the AV6301 was isolated and left unpowered,
so its crystal-associated clock circuitry may no longer supply the TLV320.
The ~17.2 kHz output must not be used to infer a valid locked PLL reference.

Clock replacement selection is pending: dedicated 16 MHz oscillator, STM32 PA8
clock output with a matching PLL profile, or restoring the original clock feed
while retaining isolation of shared data/control lines. Verify IOVDD-compatible
clock amplitude and source isolation before driving MCLK. The project previously
misidentified MCLK as pin 21 in this document; it was corrected to pin 24 after
checking TI's pin table. No connection to pin 21 was requested or performed.
