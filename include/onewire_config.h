/**
 * @file onewire_config.h
 * @brief Compile-time configuration settings for the `avr-onewire` library.
 *
 * @details Allows application-level tuning of CPU frequency validation, interrupt handling,
 *          and CRC table lookup optimizations.
 *
 * @author Arif Rachmat (ngaripar1203@gmail.com)
 * @date 2026-09-22
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 Muhammad Arif Rachmat
 *            Licensed under the MIT License (see LICENSE for details).
 *
 */

#ifndef AVR_ONEWIRE_CONFIG_H
#define AVR_ONEWIRE_CONFIG_H

/* Validate application F_CPU definition */
#ifndef F_CPU
#error "F_CPU must be defined before building avr-onewire"
#endif

/**
 * @brief Minimum supported CPU clock frequency (in Hz).
 *
 * Standard speed timing (1 us sampling windows) requires high timing fidelity.
 * Default minimum clock frequency is set to 4 MHz.
 */
#ifndef AVR_ONEWIRE_MIN_F_CPU
#define AVR_ONEWIRE_MIN_F_CPU 4000000UL
#endif

#if F_CPU < AVR_ONEWIRE_MIN_F_CPU
#error "avr-onewire requires F_CPU >= AVR_ONEWIRE_MIN_F_CPU (default 4 MHz) for portable GPIO drivers"
#endif

/**
 * @brief Global interrupt control policy during microsecond-critical slots.
 *
 * - `1`: Default. Disables interrupts (`cli()`) during critical timing windows.
 * - `0`: Leaves interrupts enabled (use with caution or custom critical section callbacks).
 */
#ifndef AVR_ONEWIRE_DISABLE_INTERRUPTS
#define AVR_ONEWIRE_DISABLE_INTERRUPTS 1
#endif

/**
 * @brief Enable lookup-table optimized CRC calculation.
 *
 * - `0`: Bitwise calculation (smaller flash usage). Default.
 * - `1`: 16-entry nibble lookup table for faster calculation (+16 bytes Flash).
 */
#ifndef AVR_ONEWIRE_CRC_TABLE
#define AVR_ONEWIRE_CRC_TABLE 0
#endif

#endif /* AVR_ONEWIRE_CONFIG_H */