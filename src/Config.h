
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
 * Copyright (C) 2024 Uwe Monreal
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===================================================================
// === Common definitions ============================================
// ===================================================================

/**
 * Version of Railuino library and required connection box software.
 */
// #define RAILUINO_VERSION 0x005B // 0.91 -> upgrade 0.90 to 0.91 christophe bobille 2024
// #define RAILUINO_VERSION 0x005C // 0.92 -> upgrade 0.91 to 0.92 christophe bobille 17/07/2024
// #define RAILUINO_VERSION 0x005D // 0.93 -> upgrade 0.92 to 0.93 christophe bobille 2024
// #define RAILUINO_VERSION 0x3AD  // 0.941 -> upgrade 0.93 to 0.94 for Arduino christophe bobille 03/2025
// #define RAILUINO_VERSION 0x005F // 0.95 -> upgrade 0.941 to 0.95 for DesktopStation Uwe Monreal 05/2026
#define RAILUINO_VERSION 0x03B7 // 0.951 -> upgrade 0.95 to 0.951 christophe bobille 05/2026
#define TRACKBOX_VERSION 0x0127 // 1.39 (tested on a layout with firmware 1.47 = 0x012F)

// Pins for CAN bus on ESP32
constexpr uint8_t CAN_RX_PIN = 22;
constexpr uint8_t CAN_TX_PIN = 23;

/**
 * Constants for protocol base addresses.
 */
#define ADDR_MM2 0x0000     // MM2 locomotive
#define ADDR_SX1 0x0800     // Selectrix (old) locomotive
#define ADDR_MFX 0x4000     // MFX locomotive
#define ADDR_SX2 0x8000     // Selectrix (new) locomotive
#define ADDR_DCC 0xC000     // DCC locomotive
#define ADDR_ACC_SX1 0x2000 // Selectrix (old) magnetic accessory
#define ADDR_ACC_MM2 0x2FFF // MM2 magnetic accessory
#define ADDR_ACC_DCC 0x3800 // DCC magnetic accessory

/**
 * Constants for classic MM2 Delta addresses.
 */
#define DELTA1 78
#define DELTA2 72
#define DELTA3 60
#define DELTA4 24

/**
 * Constants for locomotive directions.
 */
#define DIR_CURRENT 0
#define DIR_FORWARD 1
#define DIR_REVERSE 2
#define DIR_CHANGE 3

/**
 * Constants for accessory states including some aliases.
 */
#define ACC_OFF 0
#define ACC_ROUND 0
#define ACC_RED 0
#define ACC_RIGHT 0
#define ACC_HP0 0

#define ACC_ON 1
#define ACC_GREEN 1
#define ACC_STRAIGHT 1
#define ACC_HP1 1

#define ACC_YELLOW 2
#define ACC_LEFT 2
#define ACC_HP2 2

#define ACC_WHITE 3
#define ACC_SH0 3

#endif // CONFIG_H