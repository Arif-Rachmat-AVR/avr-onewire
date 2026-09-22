# **AVR 1-Wire — A Software Based 1-Wire Implementation**

Portable, allocation-free, standard-speed 1-Wire master library for 8-bit AVR microcontrollers. Designed for standard C and C++ bare-metal development workflows and intended for seamless integration with the [AVR-CMake-Template](https://github.com/Arif-Rachmat-AVR/AVR-Cmake-Template).

The library implements the **generic Maxim/Dallas 1-Wire master protocol**. It provides reset and presence detection, bit/byte/block transfers, ROM commands, multidrop ROM search, alarm search, CRC-8, optional external strong pull-up control, and scheduler-aware timing-critical sections.

---

## **Table of Contents 📋**

- [**AVR 1-Wire — A Software Based 1-Wire Implementation**](#avr-1-wire--a-software-based-1-wire-implementation)
  - [**Table of Contents 📋**](#table-of-contents-)
  - [**Features ✨**](#features-)
  - [**Prerequisites ❗**](#prerequisites-)
  - [**Installation 🛠️**](#installation-️)
    - [**Recommended: Git Submodule**](#recommended-git-submodule)
    - [Why use a submodule?](#why-use-a-submodule)
    - [**Alternative: Git Clone**](#alternative-git-clone)
  - [**Quick Start 🚀**](#quick-start-)
  - [**Usage Example 💡**](#usage-example-)
    - [Single-Device ROM Read](#single-device-rom-read)
    - [Multidrop ROM Search](#multidrop-rom-search)
    - [Preemptive Scheduler Integration](#preemptive-scheduler-integration)
  - [**API Reference 📖**](#api-reference-)
    - [`onewire_status_t onewire_init(const onewire_bus_t *bus)`](#onewire_status_t-onewire_initconst-onewire_bus_t-bus)
    - [`onewire_status_t onewire_reset(const onewire_bus_t *bus)`](#onewire_status_t-onewire_resetconst-onewire_bus_t-bus)
    - [`onewire_write_bit()` / `onewire_read_bit()`](#onewire_write_bit--onewire_read_bit)
    - [`onewire_write_byte()` / `onewire_read_byte()`](#onewire_write_byte--onewire_read_byte)
    - [`onewire_touch_byte()`](#onewire_touch_byte)
    - [`onewire_write_block()` / `onewire_read_block()`](#onewire_write_block--onewire_read_block)
    - [`onewire_write_byte_and_enable_strong_pullup()`](#onewire_write_byte_and_enable_strong_pullup)
    - [`onewire_set_strong_pullup()`](#onewire_set_strong_pullup)
    - [ROM Commands](#rom-commands)
    - [ROM Search](#rom-search)
    - [CRC-8](#crc-8)
  - [**Hardware Configuration ⚙️**](#hardware-configuration-️)
    - [1-Wire Data Pin](#1-wire-data-pin)
    - [Strong Pull-Up](#strong-pull-up)
    - [GPIO Binding](#gpio-binding)
    - [Clock and Optimization](#clock-and-optimization)
  - [**Important Notes ⚠️**](#important-notes-️)
    - [Interrupt and Timing Behavior](#interrupt-and-timing-behavior)
    - [Preemptive Scheduler Compatibility](#preemptive-scheduler-compatibility)
    - [Error Handling](#error-handling)
  - [**Roadmap 📌**](#roadmap-)
  - [**License 📜**](#license-)

---

## **Features ✨**

- Standard-speed Maxim/Dallas 1-Wire master implementation.
- Portable GPIO binding through register pointers rather than AVR-family-specific port structures.
- Supports classic AVR `DDRx/PORTx/PINx` registers.
- Supports newer AVR `PORTx.DIR/OUT/IN` register layouts.
- Supports `VPORTx` GPIO registers where provided by the target device.
- No dynamic memory allocation.
- No hidden global bus state.
- Caller-owned ROM-search state, allowing multiple independent buses/searches.
- Reset and presence-pulse detection.
- Bit, byte, touch-byte, and block transfers.
- `Skip ROM`, `Match ROM`, `Read ROM`, `Search ROM`, and `Alarm Search` support.
- Dallas/Maxim CRC-8 with optional lookup-table implementation.
- Optional external strong-pull-up control for devices that require it.
- Final-command/strong-pull-up primitive for power-sensitive device operations.
- Scheduler/RTOS critical-section hooks for timing-sensitive systems.
- Interrupt blocking is scoped to timing-critical slots rather than entire byte or block transactions.
- Designed for both C and C++ AVR projects.
- Suitable for integration with the AVR-CMake-Template library structure.
- CMake static-library build with no Arduino framework dependency.

---

## **Prerequisites ❗**

This library is specifically built on top of and designed to integrate with the **[AVR-CMake-Template](https://github.com/Arif-Rachmat/AVR-CMake-Template)** repository. For smooth development and compilation, ensure your project is built using that template as its base.

Your host environment must meet the base project toolchain requirements:

* **Base Project Template**: **[AVR-CMake-Template](https://github.com/Arif-Rachmat/AVR-CMake-Template)** (verify your main application setup follows this structure).
* **AVR Toolchain**: [Microchip AVR Toolchain](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio/gcc-compilers) (`avr-gcc`, `binutils-avr`, and `avr-libc`).
* **Build System**: [CMake](https://cmake.org/download/) (v3.16+) and a build generator like [Ninja](https://ninja-build.org/) or GNU [Make].
* A target clock of at least **4 MHz** for the current portable GPIO backend.

The application/toolchain must define `F_CPU` to the **actual CPU clock** used while the driver is executing.

> Note: The library is intentionally device-independent at the protocol layer, but exact timing still depends on the target AVR clock, compiler optimization, GPIO access latency, interrupt load, and bus electrical characteristics. Hardware validation is required for production use.

> **Note**: For platform-specific toolchain installation steps (Windows/MSYS2 UCRT64, Linux/Debian/Ubuntu, or macOS/Homebrew), please refer directly to the **[AVR-CMake-Template Prerequisites](https://github.com/Arif-Rachmat/AVR-CMake-Template#prerequisites)** section.

---

## **Installation 🛠️**

### **Recommended: Git Submodule**

When this library is used as part of a project based on the AVR-CMake-Template, using a Git submodule is recommended.

A submodule keeps the library as an independent Git repository while allowing the application project to track the exact library revision it depends on.

From the root of your AVR application project:

```bash
git submodule add https://github.com/Arif-Rachmat-AVR/avr-onewire.git lib/onewire
```

Your project can then have a structure similar to:

```text
Your-AVR-Project/
├── CMakeLists.txt
├── Lib/
│   └── onewire/
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── onewire.h
│       │   └── onewire_config.h
│       └── src/
│           └── onewire.c
├── src/
│   └── main.cpp
└── ...
```

When cloning an existing project that contains this library as a submodule, use:

```bash
git clone --recurse-submodules <your-project-url>
```

If the project has already been cloned without its submodules:

```bash
git submodule update --init --recursive
```

### Why use a submodule?

Using a submodule is particularly useful for the AVR-CMake-Template workflow because:

- The library remains an independent Git repository.
- The application stores the exact library commit it uses.
- Updating the library can be done deliberately rather than implicitly.
- The library can be shared between multiple AVR projects.
- The complete application project can be cloned together with all of its dependencies.

For long-term projects, this is generally preferable to manually cloning libraries into the project directory.

---

### **Alternative: Git Clone**

For standalone projects or quick experiments, the library can also be cloned directly into the `lib` directory:

```bash
git clone https://github.com/Arif-Rachmat-AVR/avr-onewire.git lib/onewire
```

This method is simpler, but the application repository does not automatically track which version of the 1-Wire library it is using.

---

## **Quick Start 🚀**

The following example assumes a single generic 1-Wire device connected to `PB0` with an external pull-up resistor.

```c
#include <avr/io.h>
#include <stdint.h>

#include "onewire.h"

static const onewire_bus_t bus = {
    .data = ONEWIRE_GPIO(DDRB, PORTB, PINB, PB0),
    .strong_pullup = { 0 },
    .strong_pullup_active_high = true,
    .critical = NULL,
};

int main(void)
{
    uint8_t rom[8];

    if (onewire_init(&bus) != ONEWIRE_OK) {
        for (;;) {
            /* Invalid bus configuration. */
        }
    }

    /* Read the unique 64-bit ROM from a single-device bus. */
    if (onewire_read_rom(&bus, rom) != ONEWIRE_OK) {
        for (;;) {
            /* No device, invalid ROM, or CRC failure. */
        }
    }

    /* Use rom[0..7] to select the device-specific protocol/driver. */
    for (;;) {
        /* Application-specific 1-Wire device operations. */
    }
}
```

The generic library stops at the transport/protocol boundary. After discovering a device ROM, the application or a device-specific driver can use the ROM address to issue commands defined by that device's datasheet.

---

## **Usage Example 💡**

### Single-Device ROM Read

`onewire_read_rom()` is intended for a bus containing one 1-Wire slave. It sends `Read ROM (33h)`, reads the complete 64-bit ROM, and validates the ROM CRC.

```c
uint8_t rom[8];

if (onewire_read_rom(&bus, rom) == ONEWIRE_OK) {
    /* rom[0] = family code
     * rom[1..6] = serial number
     * rom[7] = CRC */
}
```

For a bus with several devices, use ROM search instead of `Read ROM`.

### Multidrop ROM Search

Use `onewire_search_init()` followed by repeated `onewire_search_next()` calls:

```c
onewire_search_t search;
onewire_search_init(&search);

while (onewire_search_next(&bus, &search) == ONEWIRE_OK) {
    /* search.rom[0..7] contains one valid device ROM. */
}
```

The search state is owned by the caller. This means multiple independent 1-Wire buses can maintain independent searches without library-global state.

The library also provides `onewire_alarm_search_next()` for devices participating in the 1-Wire alarm-search mechanism.

### Preemptive Scheduler Integration

The library is designed so that the **timing-critical unit is an individual 1-Wire time slot**, not an entire byte or transaction.

By default:

```text
Reset low (~480 µs)       interruptible
Presence detect (~70 µs) protected
Reset recovery (~410 µs)  interruptible

Read/write slot (~60–70 µs) protected
Between slots                interruptible
```

This is useful with a preemptive scheduler because long byte/block transfers no longer require global interrupts to stay disabled for the complete transaction.

If your scheduler already provides its own timing-exclusion primitive, assign an `onewire_critical_ops_t` to the bus:

```c
static uint8_t scheduler_enter(void *context)
{
    (void)context;

    /* Enter the scheduler/interrupt critical section here. */
    return 0u;
}

static void scheduler_exit(void *context, uint8_t state)
{
    (void)context;
    (void)state;

    /* Restore the previous scheduler/interrupt state here. */
}

static const onewire_critical_ops_t scheduler_critical = {
    .enter = scheduler_enter,
    .exit = scheduler_exit,
    .context = NULL,
};

static const onewire_bus_t bus = {
    .data = ONEWIRE_GPIO(DDRB, PORTB, PINB, PB0),
    .strong_pullup = { 0 },
    .strong_pullup_active_high = true,
    .critical = &scheduler_critical,
};
```

The hook must guarantee that the protected 1-Wire slot cannot be stretched beyond the protocol timing window. Merely preventing a task switch is not enough if a long-running ISR can still delay the slot.

For systems with tight real-time requirements, a bounded critical section or a hardware/peripheral-assisted 1-Wire master is preferable.

---

## **API Reference 📖**

### `onewire_status_t onewire_init(const onewire_bus_t *bus)`

Initializes the bus descriptor and releases the 1-Wire data line.

```c
onewire_init(&bus);
```

Return values:

| Value | Meaning |
| --- | --- |
| `ONEWIRE_OK` | Bus descriptor accepted and initialized |
| `ONEWIRE_ERR_INVALID_ARG` | Invalid bus/GPIO descriptor |

If a strong-pull-up GPIO is configured, the output is initialized to its inactive level.

### `onewire_status_t onewire_reset(const onewire_bus_t *bus)`

Generates a standard-speed reset sequence and checks for a slave presence pulse.

```c
if (onewire_reset(&bus) == ONEWIRE_OK) {
    /* At least one slave responded. */
}
```

Return values:

| Value | Meaning |
| --- | --- |
| `ONEWIRE_OK` | Presence pulse detected |
| `ONEWIRE_ERR_NO_DEVICE` | No presence pulse detected |
| `ONEWIRE_ERR_INVALID_ARG` | Invalid bus descriptor |

### `onewire_write_bit()` / `onewire_read_bit()`

Write or read one standard-speed 1-Wire time slot.

```c
onewire_write_bit(&bus, 1u);

uint8_t bit = onewire_read_bit(&bus);
```

The implementation uses the timing-critical section around the complete slot so scheduler/ISR activity cannot stretch the waveform unexpectedly.

### `onewire_write_byte()` / `onewire_read_byte()`

Transfer a byte least-significant bit first.

```c
onewire_write_byte(&bus, 0xCCu);
uint8_t value = onewire_read_byte(&bus);
```

Byte transfers are built from individual timing-safe bit slots. The API does not keep interrupts disabled across the entire byte.

### `onewire_touch_byte()`

Simultaneously performs the read/write behavior used by a classic 1-Wire touch-byte primitive.

```c
uint8_t received = onewire_touch_byte(&bus, transmitted);
```

### `onewire_write_block()` / `onewire_read_block()`

Transfer arbitrary byte buffers.

```c
onewire_write_block(&bus, buffer, length);
onewire_read_block(&bus, buffer, length);
```

The bus timing protection is still scoped to individual slots rather than the whole block.

### `onewire_write_byte_and_enable_strong_pullup()`

Writes one command byte and asserts the configured external strong-pull-up stage immediately after the final time slot.

```c
onewire_status_t status =
    onewire_write_byte_and_enable_strong_pullup(&bus, command);
```

Use this for a device-specific command whose datasheet requires an immediate strong-pull-up assertion. The generic library does not interpret the command byte; the device driver is responsible for knowing whether the operation needs this mechanism.

The function returns `ONEWIRE_ERR_NO_STRONG_PULLUP` when no valid strong-pull-up GPIO is configured.

### `onewire_set_strong_pullup()`

Enables or disables the external strong-pull-up control output.

```c
onewire_set_strong_pullup(&bus, true);

/* Keep the strong pull-up active while the device performs its operation. */

onewire_set_strong_pullup(&bus, false);
```

Do not perform unrelated 1-Wire bus transactions while the strong pull-up is active.

### ROM Commands

The generic layer provides:

| Function | Purpose |
| --- | --- |
| `onewire_skip_rom()` | Address all devices on the bus |
| `onewire_match_rom()` | Address one specific device by 64-bit ROM |
| `onewire_read_rom()` | Read one device's unique ROM |
| `onewire_search_next()` | Enumerate devices using Search ROM |
| `onewire_alarm_search_next()` | Enumerate devices using Alarm Search |
| `onewire_search_verify()` | Verify that a previously discovered ROM is still present |

### ROM Search

`onewire_search_t` contains all traversal state required by the Search ROM algorithm:

```c
typedef struct {
    uint8_t rom[8];
    uint8_t last_discrepancy;
    uint8_t last_family_discrepancy;
    bool last_device_flag;
} onewire_search_t;
```

The state remains caller-owned and therefore does not consume hidden global library storage.

### CRC-8

The library implements the Dallas/Maxim 1-Wire CRC-8 polynomial ($x^8 + x^5 + x^4 + 1$):

```c
uint8_t crc = onewire_crc8(data, length);
bool valid = onewire_crc8_valid(data, length);
```

The lookup-table implementation can be enabled at compile time with:

```text
-DAVR_ONEWIRE_CRC_TABLE=1
```

The default bitwise implementation uses less flash memory.

---

## **Hardware Configuration ⚙️**

### 1-Wire Data Pin

The 1-Wire DQ line is an open-drain-style bus. The AVR should only actively pull the bus **low** and otherwise release it so the external pull-up can produce the logic-high level.

A typical single-device connection is:

![Conventional 1 Wire device connection](/img/ExternalSupply.png)

> Do **not** configure the AVR data pin as a push-pull output and drive it high as part of normal 1-Wire operation.

### Strong Pull-Up

Some 1-Wire operations require significantly more current than the passive bus pull-up resistor can provide.

Use an external transistor, MOSFET, or suitable power-switching stage controlled by a separate AVR GPIO:

![Strong Pull Up Configuration for Parasite-Powered devices](/img/ParasitePower.png)

Then configure the second GPIO as `strong_pullup`.

The generic library only provides the control mechanism. A device-specific driver determines when to enable the stage and how long to keep it active.

### GPIO Binding

Classic AVR:

```c
static const onewire_bus_t bus = {
    .data = ONEWIRE_GPIO(DDRB, PORTB, PINB, PB0),
    .strong_pullup = ONEWIRE_OUTPUT_GPIO(DDRB, PORTB, PB1),
    .strong_pullup_active_high = true,
    .critical = NULL,
};
```

Newer AVR register layouts:

```c
static const onewire_bus_t bus = {
    .data = ONEWIRE_GPIO(PORTA.DIR, PORTA.OUT, PORTA.IN, 3),
    .strong_pullup = { 0 },
    .strong_pullup_active_high = true,
    .critical = NULL,
};
```

Virtual ports where available:

```c
.data = ONEWIRE_GPIO(VPORTA.DIR, VPORTA.OUT, VPORTA.IN, 3),
```

This abstraction is the primary mechanism that avoids separate protocol implementations for different AVR GPIO register layouts.

### Clock and Optimization

`F_CPU` must describe the real CPU clock at runtime.

The current portable GPIO backend requires:

```text
F_CPU >= 4 MHz
```

The timing implementation uses avr-libc delay primitives with compile-time constant arguments. Build optimized firmware, typically with `-Os` or `-O2`, and do not change the CPU clock without rebuilding/revalidating the timing-sensitive code.

For a new board, long cable, noisy environment, or unusual compiler/toolchain configuration, verify the actual DQ waveform with a logic analyzer.

---

## **Important Notes ⚠️**

### Interrupt and Timing Behavior

The library does **not** need global interrupts disabled for an entire transaction.

The current timing model is:

```text
Reset low (~480 µs)        interruptible
Presence detect (~70 µs)  protected
Reset recovery (~410 µs)  interruptible

Read/write time slot       protected
Between slots              interruptible
```

This approach reduces interrupt latency compared with wrapping an entire reset or byte/block transfer in `cli()`.

### Preemptive Scheduler Compatibility

The optional `onewire_critical_ops_t` hook allows a preemptive scheduler to control the timing-exclusion mechanism.

Use the default behavior when ordinary AVR interrupt masking is sufficient:

```c
.critical = NULL,
```

Use a custom hook when your scheduler has a dedicated critical-section abstraction:

```c
.critical = &scheduler_critical,
```

The hook must be **bounded**. A long ISR can still stretch a 1-Wire slot even if task preemption is disabled. For hard real-time applications, use a critical section with known maximum latency or move timing generation to a hardware peripheral.

The library is intentionally non-reentrant. Do not start a second transaction on the same bus from an ISR while another transaction is active.

### Error Handling

Most protocol-level APIs return `onewire_status_t`:

| Status | Meaning |
| --- | --- |
| `ONEWIRE_OK` | Operation completed successfully |
| `ONEWIRE_ERR_INVALID_ARG` | Invalid pointer or configuration |
| `ONEWIRE_ERR_NO_DEVICE` | No slave presence response |
| `ONEWIRE_ERR_CRC` | CRC validation failed |
| `ONEWIRE_ERR_SEARCH_END` | ROM search has no more devices |
| `ONEWIRE_ERR_NO_STRONG_PULLUP` | Required strong-pull-up output is not configured |

Low-level `onewire_write_bit()` and `onewire_write_byte()` are intentionally `void` because they have no protocol response of their own. Use `onewire_reset()` and higher-level device drivers to detect missing devices and data-integrity failures.

---

## **Roadmap 📌**

- [ ] Add a timer/peripheral-assisted timing backend for applications with tighter real-time requirements.
- [ ] Add optional overdrive-speed support as a separate timing mode.
- [ ] Expand target validation across additional classic AVR, tinyAVR, megaAVR, and AVR Dx devices.
- [ ] Add configurable timing backends without changing the protocol API.

---

## **License 📜**

This project is licensed under the [MIT License](LICENSE) — free for both personal and commercial use.
