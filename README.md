# OverDrive — runtime CPU overclock for Symbian S60 3rd Edition FP2

**OverDrive** raises the CPU clock of the **Nokia E5-00** (RM-632) at runtime, with no firmware
modification. Pick a level, wait about a minute, and the phone runs at 744 MHz instead of the stock
597 MHz. A reboot always returns the phone to its factory clock.

> Tiếng Việt: xem [README.vi.md](README.vi.md).

Author: **Ngọc Sang** (Việt Nam). The idea and the early approach come from
[1234bpv/SymbianOverclock](https://github.com/1234bpv/SymbianOverclock) (MIT, N95 8GB / E90, OMAP2420).
The Nokia Rapido platform of the E5-00 is completely different from both the E72 and the OMAP phones,
so every address, signature and measurement in this project was found and tested independently on a
real E5-00 (RM-632, firmware 102.002).

## Status

| Item | State |
|---|---|
| Overclock engine (K=30, 744 MHz) | Tested on a real Nokia E5-00, RM-632, firmware 102.002 |
| New simple UI (v44: clock list in MHz, language + font size, topic guide with progress bar, about) | Written and syntax-checked; **to be confirmed on a device** |
| Other devices / firmware | Not supported (different addresses). Do not try. |

## Features

- **Choose clock:** a single safe level, K=30 (744 MHz), with the real MHz shown.
- **3 languages:** English (default), Vietnamese, Indonesian.
- **Both screen layouts:** 320×240 and 240×320.
- **Built-in guide** and a short about page.
- **Keep-max-clock patch** (`Always_Max_CPU_Speed.rmp`) for RomPatcher+, extracted automatically to
  `C:\patches` (and `E:\patches` if that folder exists).
- **Developer mode:** every experiment, benchmark and stability test used during research (see
  [docs/DEVELOPER_MODE.md](docs/DEVELOPER_MODE.md)).

## Which level should I pick?

On the E5-00 the PLL exhibits a "fake lock": it reports locked=1 and reads back the correct multiplier,
but the VCO actually runs much slower. This happens already at K=32 (734 MHz), so only K=30 is offered.

| K | Clock | Meaning |
|---|---|---|
| 30 | 744 MHz (+21 %) | Most stable. **Recommended and default.** |
| 32 | 734 MHz | Causes CPU collapse (fake lock): PLL reports 734 MHz but the CPU runs ~165 MHz. |

**Going beyond K=30 is not recommended on the E5-00.** The risk grows much faster than the gain.

## How to use

1. Install `OverDrive_E5.sisx` (self-signed, see [docs/BUILD.md](docs/BUILD.md)). The app needs
   **RomPatcher+** (its `patcherShadow.ldd` kernel driver) on the phone.
2. Open RomPatcher+ and enable **Always_Max_CPU_Speed** (the app copies the file for you on first start).
   This stops the OS from dropping the CPU to its low DVFS levels, so the overclock applies all the time.
3. Open OverDrive → **Choose clock** → pick a level → **Apply**. It takes about one minute.
4. You can exit the app. The overclock stays active until the phone restarts.
5. To change the level, open the app again and apply another one (one apply per app start).

After a restart everything is back to factory speed; just apply again.

## Safety

- Everything is RAM and register state. Nothing is flashed. A reboot (or pulling the battery) always
  restores stock behaviour.
- The code only ever **raises** the PLL multiplier and never lowers it while running (lowering while
  running caused hard hangs during development). See [docs/LESSONS.md](docs/LESSONS.md).
- Levels K≥32 make the PLL behave badly and freeze the phone. If the phone gets slow or noisy, open the
  app's developer menu and choose **Rescue**, or pull the battery.
- You use this at your own risk. No warranty (see [LICENSE](LICENSE)).

## How it works (short)

- PLL0 of the E5-00 runs at `38.4 MHz × M / (N+1)` with `N=8`, `M=140` at the top DVFS level = 597 MHz.
  The app makes the OS use `M = 140 + K` instead, so K=30 gives `M=170` → 744 MHz.
- Through the RomPatcher+ driver the app patches the OS function `SetPll` in `power_resources.dll`,
  so the OS itself performs the safe PLL relock sequence with the higher multiplier.
- The patch lives in RAM and survives closing the app, not a reboot.

Full details: [docs/TECHNICAL.md](docs/TECHNICAL.md), measurements: [docs/FINDINGS.md](docs/FINDINGS.md),
history: [docs/CHANGELOG.md](docs/CHANGELOG.md).

## Repository layout

```
group/      bld.inf, E72Oc.mmp (project and UID)
src/        C++ sources (engine, UI views, string and patch data)
inc/        headers
data/       .rss resources and icon include files
gfx/        icon source (SVG)
sis/        package template
patch/      Always_Max_CPU_Speed.rmp (RomPatcher+ patch)
tools/      string generator, log analyzer, ARM disassembly helpers, host syntax checks
docs/       documentation
archive/    older source versions (v7 … v42)
```

Internal names still use the prefix `E72Oc` (the project started as an E72 probe). The app, package and
icon are called **OverDrive**. The Nokia E5-00 port keeps the internal filenames and UID3 (`E72Oc.exe`,
`0xE0A1D4C5`) for compatibility.

## Build

Needs the S60 3rd FP2 SDK, a GCCE toolchain, Perl and Windows. See [docs/BUILD.md](docs/BUILD.md).

## Credits

- Ngọc Sang — author, reverse engineering, testing on the E72 and E5-00.
- [1234bpv/SymbianOverclock](https://github.com/1234bpv/SymbianOverclock) — original idea and the
  lessons about patching the DVFS driver on OMAP2420 phones (MIT).
- [IlSocio/Symbian-RomPatcherPlus](https://github.com/IlSocio/Symbian-RomPatcherPlus) — RomPatcher+
  and its kernel driver, which this project depends on.

## License

MIT. See [LICENSE](LICENSE).