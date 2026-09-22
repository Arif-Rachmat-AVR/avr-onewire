/**
 * @file onewire.h
 * @brief Portable 1-Wire (Dallas/Maxim) master protocol library for AVR microcontrollers.
 *
 * @details Provides core primitives and high-level utilities for 1-Wire bus communication
 *          on Microchip/Atmel AVR devices.
 *
 *          Key features:
 *          - Compatible with classic AVR registers (PORTx/DDRx/PINx) and modern 0/1/2-series / AVR-Dx
 *            register interfaces (PORTx.DIR/OUT/IN, VPORTx).
 *          - Safe open-drain bus driving: never drives high logic levels on the data line.
 *          - Parasite power support via dedicated hardware strong-pull-up enable routines.
 *          - Full Maxim AN187 ROM search algorithm (ROM search, alarm search, verify ROM).
 *          - Dallas/Maxim 8-bit CRC calculation with optional lookup table optimization.
 *          - Deterministic, timing-critical hooks for RTOS and preemptive environment integration.
 *
 * @author Arif Rachmat (ngaripar1203@gmail.com)
 * @date 2026-09-22
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 Muhammad Arif Rachmat
 *            Licensed under the MIT License (see LICENSE for details).
 */

#ifndef AVR_ONEWIRE_H
#define AVR_ONEWIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "onewire_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return codes for 1-Wire bus operations.
 */
typedef enum {
    ONEWIRE_OK = 0,                 /**< Operation completed successfully. */
    ONEWIRE_ERR_INVALID_ARG,        /**< Null pointer or invalid argument supplied. */
    ONEWIRE_ERR_NO_DEVICE,          /**< No device responded with a presence pulse. */
    ONEWIRE_ERR_CRC,                /**< CRC validation failed on received data. */
    ONEWIRE_ERR_SEARCH_END,         /**< Bus search reached the end of enumeration. */
    ONEWIRE_ERR_NO_STRONG_PULLUP    /**< Strong pull-up pin requested but not configured. */
} onewire_status_t;

/**
 * @brief Representation of an AVR GPIO pin using byte-register addresses.
 *
 * @details Operates using volatile pointers to DDR, OUT, and IN registers, allowing
 *          abstraction over standard AVR registers (`DDRB`, `PORTB`, `PINB`) as well as
 *          structured peripheral registers (`PORTA.DIR`, `PORTA.OUT`, `PORTA.IN`).
 *
 * @note The output register bit must be driven low prior to activating output mode.
 *       The library strictly operates in open-drain mode (pull-low or high-impedance).
 */
typedef struct {
    volatile uint8_t *ddr;  /**< Pointer to Data Direction Register (e.g., &DDRB or &PORTA.DIR). */
    volatile uint8_t *out;  /**< Pointer to Output Latch Register (e.g., &PORTB or &PORTA.OUT). */
    volatile uint8_t *in;   /**< Pointer to Input Pin Register (e.g., &PINB or &PORTA.IN). */
    uint8_t mask;           /**< Bitmask identifying the pin (e.g., 1 << PB0). */
} onewire_gpio_t;

/**
 * @brief Macro helper to construct a bidirectional GPIO descriptor.
 *
 * @param ddr_reg Data Direction Register lvalue (e.g., `DDRB` or `PORTA.DIR`).
 * @param out_reg Output Latch Register lvalue (e.g., `PORTB` or `PORTA.OUT`).
 * @param in_reg  Input Register lvalue (e.g., `PINB` or `PORTA.IN`).
 * @param bit_number Bit position (0..7).
 */
#define ONEWIRE_GPIO(ddr_reg, out_reg, in_reg, bit_number) \
    { &(ddr_reg), &(out_reg), &(in_reg), (uint8_t)(1u << (bit_number)) }

/**
 * @brief Macro helper to construct an output-only GPIO descriptor (e.g., strong pull-up transistor switch).
 *
 * @param ddr_reg Data Direction Register lvalue.
 * @param out_reg Output Latch Register lvalue.
 * @param bit_number Bit position (0..7).
 */
#define ONEWIRE_OUTPUT_GPIO(ddr_reg, out_reg, bit_number) \
    { &(ddr_reg), &(out_reg), NULL, (uint8_t)(1u << (bit_number)) }

/**
 * @brief Token type returned by timing-critical entry hooks.
 *
 * Stores processor interrupt status (`SREG`) in default AVR deployments or custom
 * state in RTOS environments.
 */
typedef uint8_t onewire_critical_state_t;

/** @brief Function signature for entering a timing-critical section. */
typedef onewire_critical_state_t (*onewire_critical_enter_fn)(void *context);

/** @brief Function signature for exiting a timing-critical section. */
typedef void (*onewire_critical_exit_fn)(void *context, onewire_critical_state_t state);

/**
 * @brief Optional platform- or RTOS-specific timing exclusion callbacks.
 */
typedef struct {
    onewire_critical_enter_fn enter; /**< Callback executed before microsecond timing slots. */
    onewire_critical_exit_fn exit;   /**< Callback executed after microsecond timing slots. */
    void *context;                   /**< User context pointer passed to callbacks. */
} onewire_critical_ops_t;

/**
 * @brief Logical 1-Wire bus instance descriptor.
 */
typedef struct {
    onewire_gpio_t data;                   /**< Primary 1-Wire data bus GPIO configuration. */
    onewire_gpio_t strong_pullup;          /**< Optional strong pull-up enable pin. */
    bool strong_pullup_active_high;        /**< `true` if active-high logic enables strong pull-up. */
    const onewire_critical_ops_t *critical;/**< Optional critical section overrides (NULL for default). */
} onewire_bus_t;

/**
 * @brief State descriptor for Maxim 1-Wire ROM search operations.
 *
 * Encapsulates state data required by the AN187 ROM search algorithm.
 */
typedef struct {
    uint8_t rom[8];                 /**< Discovered 64-bit ROM code (8 bytes including CRC). */
    uint8_t last_discrepancy;       /**< Bit index of last discrepancy encountered. */
    uint8_t last_family_discrepancy;/**< Bit index of last discrepancy within family code (bits 1..8). */
    bool last_device_flag;          /**< Set to `true` when search traversal completes. */
} onewire_search_t;

/* --- API Function Declarations --- */

/**
 * @brief Initialize a 1-Wire bus instance.
 * @param bus Pointer to configured `onewire_bus_t` descriptor.
 * @return `ONEWIRE_OK` on success, or error code.
 */
onewire_status_t onewire_init(const onewire_bus_t *bus);

/**
 * @brief Perform standard 1-Wire reset pulse and check for device presence.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @return `ONEWIRE_OK` if presence pulse detected, `ONEWIRE_ERR_NO_DEVICE` if absent.
 */
onewire_status_t onewire_reset(const onewire_bus_t *bus);

/**
 * @brief Write a single bit to the 1-Wire bus.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param bit_value Non-zero writes logic 1, zero writes logic 0.
 */
void onewire_write_bit(const onewire_bus_t *bus, uint8_t bit_value);

/**
 * @brief Read a single bit from the 1-Wire bus.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @return Read bit value (0 or 1).
 */
uint8_t onewire_read_bit(const onewire_bus_t *bus);

/**
 * @brief Write a byte to the bus (LSB first).
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param value Byte to transmit.
 */
void onewire_write_byte(const onewire_bus_t *bus, uint8_t value);

/**
 * @brief Transmit a byte and immediately activate strong pull-up mode.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param value Command byte to transmit (e.g., `0x44` for DS18B20 `Convert T`).
 * @return `ONEWIRE_OK` on success, `ONEWIRE_ERR_NO_STRONG_PULLUP` if unconfigured.
 */
onewire_status_t onewire_write_byte_and_enable_strong_pullup(
    const onewire_bus_t *bus,
    uint8_t value);

/**
 * @brief Read a single byte from the bus (LSB first).
 * @param bus Pointer to `onewire_bus_t` instance.
 * @return Byte read from the bus.
 */
uint8_t onewire_read_byte(const onewire_bus_t *bus);

/**
 * @brief Simultaneously write and read a byte ("touch byte").
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param value Bit pattern to transmit (bits set to 1 become read slots).
 * @return Received byte value.
 */
uint8_t onewire_touch_byte(const onewire_bus_t *bus, uint8_t value);

/**
 * @brief Write a buffer of bytes to the bus.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param data Data buffer pointer.
 * @param length Number of bytes to transmit.
 */
void onewire_write_block(const onewire_bus_t *bus, const uint8_t *data, size_t length);

/**
 * @brief Read a buffer of bytes from the bus.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param data Output buffer pointer.
 * @param length Number of bytes to receive.
 */
void onewire_read_block(const onewire_bus_t *bus, uint8_t *data, size_t length);

/**
 * @brief Enable or disable strong pull-up GPIO.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param enable `true` to enable strong pull-up, `false` to release.
 * @return `ONEWIRE_OK` on success, `ONEWIRE_ERR_NO_STRONG_PULLUP` if unconfigured.
 */
static inline onewire_status_t onewire_set_strong_pullup(const onewire_bus_t *bus, bool enable)
{
    if ((bus == NULL) ||
        (bus->strong_pullup.ddr == NULL) ||
        (bus->strong_pullup.out == NULL) ||
        (bus->strong_pullup.mask == 0u)) {
        return ONEWIRE_ERR_NO_STRONG_PULLUP;
    }

    if (bus->strong_pullup_active_high) {
        if (enable) {
            *bus->strong_pullup.out |= bus->strong_pullup.mask;
        } else {
            *bus->strong_pullup.out &= (uint8_t)~bus->strong_pullup.mask;
        }
    } else {
        if (enable) {
            *bus->strong_pullup.out &= (uint8_t)~bus->strong_pullup.mask;
        } else {
            *bus->strong_pullup.out |= bus->strong_pullup.mask;
        }
    }

    return ONEWIRE_OK;
}

/**
 * @brief Send 1-Wire Skip ROM command (0xCC).
 * @param bus Pointer to `onewire_bus_t` instance.
 * @return `ONEWIRE_OK` on success.
 */
onewire_status_t onewire_skip_rom(const onewire_bus_t *bus);

/**
 * @brief Send Match ROM command (0x55) followed by target 64-bit ROM.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param rom 8-byte array containing target 64-bit ROM code.
 * @return `ONEWIRE_OK` on success.
 */
onewire_status_t onewire_match_rom(const onewire_bus_t *bus, const uint8_t rom[8]);

/**
 * @brief Read ROM command (0x33) for single-device buses with CRC check.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param rom 8-byte output buffer for storing read ROM.
 * @return `ONEWIRE_OK` on success, `ONEWIRE_ERR_CRC` if CRC calculation fails.
 */
onewire_status_t onewire_read_rom(const onewire_bus_t *bus, uint8_t rom[8]);

/**
 * @brief Initialize a `onewire_search_t` context before starting new enumeration.
 * @param search Pointer to search context struct.
 */
void onewire_search_init(onewire_search_t *search);

/**
 * @brief Discover next device on bus using standard Search ROM (0xF0).
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param search Pointer to search state context.
 * @return `ONEWIRE_OK` on discovery, `ONEWIRE_ERR_SEARCH_END` when finished.
 */
onewire_status_t onewire_search_next(const onewire_bus_t *bus, onewire_search_t *search);

/**
 * @brief Perform alarm search sequence using Alarm Search command (0xEC).
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param search Pointer to search state context.
 * @return `ONEWIRE_OK` on discovery, `ONEWIRE_ERR_SEARCH_END` when finished.
 */
onewire_status_t onewire_alarm_search_next(const onewire_bus_t *bus, onewire_search_t *search);

/**
 * @brief Verify presence of specific 64-bit ROM on bus.
 * @param bus Pointer to `onewire_bus_t` instance.
 * @param rom 8-byte array containing target ROM.
 * @return `ONEWIRE_OK` if verified, `ONEWIRE_ERR_NO_DEVICE` if absent.
 */
onewire_status_t onewire_search_verify(const onewire_bus_t *bus, const uint8_t rom[8]);

/**
 * @brief Calculate Dallas/Maxim 8-bit CRC over array ($x^8 + x^5 + x^4 + 1$).
 * @param data Input data buffer pointer.
 * @param length Length of buffer in bytes.
 * @return Calculated 8-bit CRC value.
 */
uint8_t onewire_crc8(const uint8_t *data, size_t length);

/**
 * @brief Validate buffer where final byte is expected to be valid Dallas CRC8.
 * @param data Buffer pointer including trailing CRC byte.
 * @param length Total length of buffer (minimum 2 bytes).
 * @return `true` if calculated CRC matches final byte.
 */
bool onewire_crc8_valid(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* AVR_ONEWIRE_H */