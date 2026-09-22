/**
 * @file onewire.c
 * @brief Implementation of 1-Wire master driver, bit-banging timing, and AN187 search algorithm.
 *
 * @details Implements Standard-Speed Dallas 1-Wire master timing parameters based on Maxim Application
 *          Note 187 (AN187) and Dallas 1-Wire specification guidelines.
 *
 *          Timing Slots (Standard Speed):
 *          - $t_A$ (Write-1 Low / Read-Initiate): 6 us
 *          - $t_B$ (Write-1 Recovery / Slot tail): 64 us
 *          - $t_C$ (Write-0 Low): 60 us
 *          - $t_D$ (Write-0 Recovery): 10 us
 *          - $t_E$ (Read Sample Window): 9 us
 *          - $t_F$ (Read Slot Recovery): 55 us
 *          - $t_H$ (Reset Low): 480 us
 *          - $t_I$ (Presence Sample Delay): 70 us
 *          - $t_J$ (Reset Recovery): 410 us
 *
 * @author Arif Rachmat (ngaripar1203@gmail.com)
 * @date 2026-09-22
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 Muhammad Arif Rachmat
 *            Licensed under the MIT License (see LICENSE for details).
 */

#include "onewire.h"

#include <avr/io.h>
#include <util/delay.h>

#if AVR_ONEWIRE_DISABLE_INTERRUPTS
#include <avr/interrupt.h>
#endif

/* Maxim/Analog Devices standard-speed master timings. */
#define OW_T_A_US 6u
#define OW_T_B_US 64u
#define OW_T_C_US 60u
#define OW_T_D_US 10u
#define OW_T_E_US 9u
#define OW_T_F_US 55u
#define OW_T_G_US 0u
#define OW_T_H_US 480u
#define OW_T_I_US 70u
#define OW_T_J_US 410u

/* ROM/function commands common to the 1-Wire bus family. */
#define OW_CMD_READ_ROM 0x33u
#define OW_CMD_MATCH_ROM 0x55u
#define OW_CMD_SKIP_ROM 0xCCu
#define OW_CMD_SEARCH_ROM 0xF0u
#define OW_CMD_ALARM_SEARCH 0xECu

static bool gpio_valid(const onewire_gpio_t *gpio) {
    return (gpio != NULL) && (gpio->ddr != NULL) && (gpio->out != NULL) &&
           (gpio->in != NULL) && (gpio->mask != 0u);
}

static bool bus_valid(const onewire_bus_t *bus) {
    return (bus != NULL) && gpio_valid(&bus->data);
}

static bool strong_pullup_valid(const onewire_bus_t *bus) {
    return (bus != NULL) && (bus->strong_pullup.ddr != NULL) &&
           (bus->strong_pullup.out != NULL) && (bus->strong_pullup.mask != 0u);
}

static inline void drive_low(const onewire_gpio_t *gpio) {
    /* Always pre-load the output latch low before enabling the driver. */
    *gpio->out &= (uint8_t)~gpio->mask;
    *gpio->ddr |= gpio->mask;
}

static inline void release_bus(const onewire_gpio_t *gpio) {
    /* Never write a logic-high push-pull level onto the 1-Wire bus. */
    *gpio->out &= (uint8_t)~gpio->mask;
    *gpio->ddr &= (uint8_t)~gpio->mask;
}

static inline uint8_t sample_bus(const onewire_gpio_t *gpio) {
    return ((*gpio->in & gpio->mask) != 0u) ? 1u : 0u;
}

static inline onewire_critical_state_t
critical_enter(const onewire_bus_t *bus) {
    if ((bus->critical != NULL) && (bus->critical->enter != NULL) &&
        (bus->critical->exit != NULL)) {
        return bus->critical->enter(bus->critical->context);
    }

#if AVR_ONEWIRE_DISABLE_INTERRUPTS
    const uint8_t saved_sreg = SREG;
    cli();
    return saved_sreg;
#else
    return 0u;
#endif
}

static inline void critical_exit(const onewire_bus_t *bus,
                                 onewire_critical_state_t state) {
    if ((bus->critical != NULL) && (bus->critical->enter != NULL) &&
        (bus->critical->exit != NULL)) {
        bus->critical->exit(bus->critical->context, state);
        return;
    }

#if AVR_ONEWIRE_DISABLE_INTERRUPTS
    SREG = state;
#else
    (void)state;
#endif
}

static inline void delay_us_const(double microseconds) {
    /*
     * Every caller passes a compile-time constant.  avr-libc explicitly
     * documents this requirement for predictable _delay_us() operation.
     */
    _delay_us(microseconds);
}

static inline uint8_t read_bit_timed(const onewire_bus_t *bus) {
    const onewire_critical_state_t critical_state = critical_enter(bus);
    uint8_t result;

    /* The complete slot is timing-critical; do not allow preemption here. */
    drive_low(&bus->data);
    _delay_us(OW_T_A_US);
    release_bus(&bus->data);
    _delay_us(OW_T_E_US);
    result = sample_bus(&bus->data);
    _delay_us(OW_T_F_US);

    critical_exit(bus, critical_state);
    return result;
}

static inline void write_bit_timed(const onewire_bus_t *bus,
                                   uint8_t bit_value) {
    const onewire_critical_state_t critical_state = critical_enter(bus);

    /* The complete slot is timing-critical; do not allow preemption here. */
    drive_low(&bus->data);

    if (bit_value != 0u) {
        _delay_us(OW_T_A_US);
        release_bus(&bus->data);
        _delay_us(OW_T_B_US);
    } else {
        _delay_us(OW_T_C_US);
        release_bus(&bus->data);
        _delay_us(OW_T_D_US);
    }

    critical_exit(bus, critical_state);
}

static inline void write_bit_timed_with_strong_pullup(const onewire_bus_t *bus,
                                                      uint8_t bit_value) {
    const onewire_critical_state_t critical_state = critical_enter(bus);

    drive_low(&bus->data);

    if (bit_value != 0u) {
        _delay_us(OW_T_A_US);
        release_bus(&bus->data);
        _delay_us(OW_T_B_US);
    } else {
        _delay_us(OW_T_C_US);
        release_bus(&bus->data);
        _delay_us(OW_T_D_US);
    }

    /* Still inside the critical section: assert strong pull-up immediately
     * after the command byte's final time slot. */
    (void)onewire_set_strong_pullup(bus, true);
    critical_exit(bus, critical_state);
}

onewire_status_t onewire_init(const onewire_bus_t *bus) {
    if (!bus_valid(bus)) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    release_bus(&bus->data);

    if (strong_pullup_valid(bus)) {
        const bool inactive_high = !bus->strong_pullup_active_high;
        *bus->strong_pullup.out =
            inactive_high
                ? (uint8_t)(*bus->strong_pullup.out | bus->strong_pullup.mask)
                : (uint8_t)(*bus->strong_pullup.out &
                            (uint8_t)~bus->strong_pullup.mask);
        *bus->strong_pullup.ddr |= bus->strong_pullup.mask;
    }

    return ONEWIRE_OK;
}

onewire_status_t onewire_reset(const onewire_bus_t *bus) {
    if (!bus_valid(bus)) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    /*
     * The reset-low interval has a minimum duration but no specified maximum
     * in the standard-speed reset cycle. Keep interrupts enabled while the
     * master holds the line low so a scheduler can run during this long phase.
     *
     * Only the release/presence-detect window is protected because the sample
     * point must remain within the slave's presence-pulse timing window.
     */
    _delay_us(OW_T_G_US);
    drive_low(&bus->data);
    _delay_us(OW_T_H_US);

    const onewire_critical_state_t critical_state = critical_enter(bus);

    release_bus(&bus->data);
    _delay_us(OW_T_I_US);

    /* Presence is an active-low pulse. */
    const uint8_t presence = (sample_bus(&bus->data) == 0u) ? 1u : 0u;

    critical_exit(bus, critical_state);

    /* Reset recovery is intentionally interruptible. */
    _delay_us(OW_T_J_US);

    return (presence != 0u) ? ONEWIRE_OK : ONEWIRE_ERR_NO_DEVICE;
}

void onewire_write_bit(const onewire_bus_t *bus, uint8_t bit_value) {
    if (bus_valid(bus)) {
        write_bit_timed(bus, (bit_value != 0u) ? 1u : 0u);
    }
}

uint8_t onewire_read_bit(const onewire_bus_t *bus) {
    if (!bus_valid(bus)) {
        return 1u;
    }
    return read_bit_timed(bus);
}

uint8_t onewire_touch_byte(const onewire_bus_t *bus, uint8_t value) {
    uint8_t received = 0u;

    if (!bus_valid(bus)) {
        return 0xFFu;
    }

    for (uint8_t bit = 0u; bit < 8u; ++bit) {
        if ((value & (uint8_t)(1u << bit)) != 0u) {
            if (read_bit_timed(bus) != 0u) {
                received |= (uint8_t)(1u << bit);
            }
        } else {
            write_bit_timed(bus, 0u);
        }
    }

    return received;
}

void onewire_write_byte(const onewire_bus_t *bus, uint8_t value) {
    (void)onewire_touch_byte(bus, value);
}

onewire_status_t
onewire_write_byte_and_enable_strong_pullup(const onewire_bus_t *bus,
                                            uint8_t value) {
    if (!bus_valid(bus)) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    if (!strong_pullup_valid(bus)) {
        return ONEWIRE_ERR_NO_STRONG_PULLUP;
    }

    for (uint8_t bit = 0u; bit < 7u; ++bit) {
        write_bit_timed(bus, (uint8_t)((value >> bit) & 1u));
    }

    write_bit_timed_with_strong_pullup(bus, (uint8_t)((value >> 7u) & 1u));

    return ONEWIRE_OK;
}

uint8_t onewire_read_byte(const onewire_bus_t *bus) {
    return onewire_touch_byte(bus, 0xFFu);
}

void onewire_write_block(const onewire_bus_t *bus, const uint8_t *data,
                         size_t length) {
    if (!bus_valid(bus) || (data == NULL && length != 0u)) {
        return;
    }

    while (length-- != 0u) {
        onewire_write_byte(bus, *data++);
    }
}

void onewire_read_block(const onewire_bus_t *bus, uint8_t *data,
                        size_t length) {
    if (!bus_valid(bus) || (data == NULL && length != 0u)) {
        return;
    }

    while (length-- != 0u) {
        *data++ = onewire_read_byte(bus);
    }
}

onewire_status_t onewire_skip_rom(const onewire_bus_t *bus) {
    if (!bus_valid(bus)) {
        return ONEWIRE_ERR_INVALID_ARG;
    }
    onewire_write_byte(bus, OW_CMD_SKIP_ROM);
    return ONEWIRE_OK;
}

onewire_status_t onewire_match_rom(const onewire_bus_t *bus,
                                   const uint8_t rom[8]) {
    if (!bus_valid(bus) || rom == NULL) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    onewire_write_byte(bus, OW_CMD_MATCH_ROM);
    onewire_write_block(bus, rom, 8u);
    return ONEWIRE_OK;
}

onewire_status_t onewire_read_rom(const onewire_bus_t *bus, uint8_t rom[8]) {
    if (!bus_valid(bus) || rom == NULL) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    onewire_status_t status = onewire_reset(bus);
    if (status != ONEWIRE_OK) {
        return status;
    }

    onewire_write_byte(bus, OW_CMD_READ_ROM);
    onewire_read_block(bus, rom, 8u);

    return onewire_crc8_valid(rom, 8u) ? ONEWIRE_OK : ONEWIRE_ERR_CRC;
}

static bool rom_is_zero(const uint8_t rom[8]) {
    uint8_t accumulator = 0u;

    for (uint8_t i = 0u; i < 8u; ++i) {
        accumulator |= rom[i];
    }
    return accumulator == 0u;
}

static onewire_status_t search_next_common(const onewire_bus_t *bus,
                                           onewire_search_t *search,
                                           uint8_t command) {
    uint8_t last_zero = 0u;

    if (!bus_valid(bus) || search == NULL) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    if (search->last_device_flag) {
        return ONEWIRE_ERR_SEARCH_END;
    }

    onewire_status_t status = onewire_reset(bus);
    if (status != ONEWIRE_OK) {
        onewire_search_init(search);
        return status;
    }

    onewire_write_byte(bus, command);

    for (uint8_t bit_number = 1u; bit_number <= 64u; ++bit_number) {
        const uint8_t id_bit = onewire_read_bit(bus);
        const uint8_t cmp_id_bit = onewire_read_bit(bus);
        uint8_t search_direction;

        if ((id_bit != 0u) && (cmp_id_bit != 0u)) {
            /* No device participated in this branch. */
            onewire_search_init(search);
            return ONEWIRE_ERR_SEARCH_END;
        }

        if (id_bit != cmp_id_bit) {
            /* Only one logic value exists in this bit position. */
            search_direction = id_bit;
        } else {
            /* Discrepancy: choose the branch prescribed by AN187. */
            if (bit_number < search->last_discrepancy) {
                search_direction =
                    (uint8_t)((search->rom[(bit_number - 1u) >> 3u] >>
                               ((bit_number - 1u) & 7u)) &
                              1u);
            } else {
                search_direction =
                    (bit_number == search->last_discrepancy) ? 1u : 0u;
            }

            if (search_direction == 0u) {
                last_zero = bit_number;
                if (bit_number <= 8u) {
                    search->last_family_discrepancy = bit_number;
                }
            }
        }

        if (search_direction != 0u) {
            search->rom[(bit_number - 1u) >> 3u] |=
                (uint8_t)(1u << ((bit_number - 1u) & 7u));
        } else {
            search->rom[(bit_number - 1u) >> 3u] &=
                (uint8_t)~(uint8_t)(1u << ((bit_number - 1u) & 7u));
        }

        onewire_write_bit(bus, search_direction);
    }

    search->last_discrepancy = last_zero;
    if (search->last_discrepancy == 0u) {
        search->last_device_flag = true;
    }

    if (rom_is_zero(search->rom) || !onewire_crc8_valid(search->rom, 8u)) {
        onewire_search_init(search);
        return ONEWIRE_ERR_CRC;
    }

    return ONEWIRE_OK;
}

void onewire_search_init(onewire_search_t *search) {
    if (search == NULL) {
        return;
    }

    for (uint8_t i = 0u; i < 8u; ++i) {
        search->rom[i] = 0u;
    }
    search->last_discrepancy = 0u;
    search->last_family_discrepancy = 0u;
    search->last_device_flag = false;
}

onewire_status_t onewire_search_next(const onewire_bus_t *bus,
                                     onewire_search_t *search) {
    return search_next_common(bus, search, OW_CMD_SEARCH_ROM);
}

onewire_status_t onewire_alarm_search_next(const onewire_bus_t *bus,
                                           onewire_search_t *search) {
    return search_next_common(bus, search, OW_CMD_ALARM_SEARCH);
}

onewire_status_t onewire_search_verify(const onewire_bus_t *bus,
                                       const uint8_t rom[8]) {
    if (!bus_valid(bus) || rom == NULL) {
        return ONEWIRE_ERR_INVALID_ARG;
    }

    onewire_search_t search;
    onewire_search_init(&search);

    /* AN187 VERIFY: set the target ROM and search from discrepancy 64. */
    for (uint8_t i = 0u; i < 8u; ++i) {
        search.rom[i] = rom[i];
    }
    search.last_discrepancy = 64u;

    onewire_status_t status =
        search_next_common(bus, &search, OW_CMD_SEARCH_ROM);
    if (status != ONEWIRE_OK) {
        return status;
    }

    for (uint8_t i = 0u; i < 8u; ++i) {
        if (search.rom[i] != rom[i]) {
            return ONEWIRE_ERR_NO_DEVICE;
        }
    }
    return ONEWIRE_OK;
}

#if AVR_ONEWIRE_CRC_TABLE
static const uint8_t crc4_table[16] = {0x00u, 0x9Du, 0x23u, 0xBEu, 0x46u, 0xDBu,
                                       0x65u, 0xF8u, 0x8Cu, 0x11u, 0xAFu, 0x32u,
                                       0xCAu, 0x57u, 0xE9u, 0x74u};
#endif

uint8_t onewire_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0u;

    if (data == NULL && length != 0u) {
        return 0u;
    }

    while (length-- != 0u) {
        crc ^= *data++;
#if AVR_ONEWIRE_CRC_TABLE
        crc = crc4_table[crc & 0x0Fu] ^ (uint8_t)(crc >> 4u);
        crc = crc4_table[crc & 0x0Fu] ^ (uint8_t)(crc >> 4u);
#else
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = (uint8_t)((crc >> 1u) ^ 0x8Cu);
            } else {
                crc >>= 1u;
            }
        }
#endif
    }

    return crc;
}

bool onewire_crc8_valid(const uint8_t *data, size_t length) {
    if (data == NULL || length < 2u) {
        return false;
    }

    return onewire_crc8(data, length - 1u) == data[length - 1u];
}
