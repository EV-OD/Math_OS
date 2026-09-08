# MathOS — a tiny multitasking OS with tmux UI and MATLAB-like math

Bare-metal 32-bit x86 OS booted via GRUB multiboot. Preemptive multitasking,
a tmux-style tiled shell/canvas UI, and a MATLAB-like engine for plotting,
FFT/spectrum analysis, and digital filter design — all on one screen.

## Build & run

Needs `gcc -m32`, `nasm`, `ld`, `grub-mkrescue`, `qemu-system-i386`.

```sh
make          # builds myos.iso
make run      # QEMU, shell on stdio + graphics window
```

## Authorship: human + AI

The initial OS foundation was written fully by me:

- kernel space setup, multiboot specification handling
- GDT (incl. user-space segments + TSS), IDT, interrupts, ISR/IRQ stubs
- PIC (remap/mask/EOI), PIT timer
- PS/2 keyboard driver and Framebuffer/VBE display driver

Everything else in this repo was built with AI assistance,
including: stdio/`scanf`, serial logging, scheduler/processes, the tmux
multiplexer + widget/canvas system, GPU/GL software rendering, the math/DSP
stack (`calc`, `fft`, `mat`, filters), RAM filesystem, the `code` editor,
and the command registry below.

## Features

- Preemptive scheduler (PIT 100 Hz), per-process FPU state, user mode (ring 3)
- tmux-like UI: splits, windows, focus, resize, canvases, status bar
- Canvas plots: function plot, FFT/DFT spectra, ADC/DAC, filter response,
  pole-zero maps, software-GL demo (`run glcube`)
- Expression engine: variables, user functions (`h(x)=sin(x)`), `ans`
- Digital filters: FIR (moving-average, windowed-sinc) + IIR (LP/HP),
  `freqz`, `pz`, `filter` apply
- RAM filesystem (gone on reboot) + fullscreen `code` editor + `.ez` scripts
- Serial logging to `logs/os.log` (`make run-headless`)

## Commands

### System / processes

| cmd | what |
| --- | ---- |
| `help` | list commands |
| `keys` | list shortcuts |
| `ps` | list processes |
| `run <prog\|file.ez>` | start builtin (`a b spin uhello gfx glcube`) or script |
| `kill <pid>` | stop process (Ctrl+C kills latest) |
| `echo <t>` / `uptime` / `clear [canvas]` | basics |

### Files (`code` opens fullscreen editor)

`ls [dir]` `lt [dir]` `cat <file>` `mkdir <dir>` `cd [dir]` `code <file>`

### Math (prefix with `N>` to target canvas N)

| cmd | what |
| --- | ---- |
| `calc <expr>` | evaluate; `h(x)=...` defines, `h(2)` calls |
| `plot <expr> [a] [b]` | line graph on canvas |
| `fft [expr] [a] [b] [n]` | spectrum, n in 16..256 pow2 |
| `freq` | redraw last spectrum |
| `dft <expr> [n]` | direct DFT, n in 8..256 |
| `adc <expr> <bits> [n]` / `dac` | quantize / reconstruct (`fft adc` = spectrum of samples) |
| `afft [expr] [Fs] [n]` | analog spectrum in Hz (use `h(t)`) |

### Filters

`fir avg <N>` `fir sinc <fc> [taps]` `iir lp|hp <fc>` then
`freqz` (response) `pz` (poles/zeros) `filter <expr>` (apply + overlay plot)

### Canvas & view

`canvas` (turn pane into canvas) `panes` `windows`
`N#` stick shell to canvas N, `#` unstick
`grid on|off` `autofit` `z+` `z-` `z<< [N]` `z>> [N]`

## Hotkeys

Prefix is `Ctrl+b`, then:

| key | action |
| --- | ------ |
| `"` / `%` (or `s`/`v`) | split horizontal / vertical |
| `h j k l` or arrows | focus pane (also `Ctrl+arrows`, no prefix) |
| `H J K L` or `Ctrl+Alt+arrows` | resize pane |
| `x` | kill pane |
| `c` / `n` / `p` / `0-9` | new / next / prev / select window |
| `PgUp`/`PgDn`, `Left/Right`, `Del`, `Backspace` | scrollback + line editing |

Typing in a focused canvas jumps focus to a shell automatically.

## Try this

```
canvas
h(x)=sin(x)+0.5*sin(3*x)+0.3*sin(7*x)+0.2*sin(15*x)
1> plot h(x) 0 6.28
1> fft h(x) 0 6.28 128
1> afft h(t) 200 128
```
