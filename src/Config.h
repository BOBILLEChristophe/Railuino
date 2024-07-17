
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
 * 
 * This example is free software; you can redistribute it and/or
 * modify it under the terms of the Creative Commons Zero License,
 * version 1.0, as published by the Creative Commons Organisation.
 * This effectively puts the file into the public domain.
 *
 * This example is distributed in the hope that it will be useful,
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
//#define RAILUINO_VERSION 0x005B // 0.91 -> upgrade 0.90 to 0.91 christophe bobille 2024
#define RAILUINO_VERSION 0x005C  // 0.92 -> upgrade 0.90 to 0.91 christophe bobille 2024
#define TRACKBOX_VERSION 0x0127 // 1.39

/**
 * Constants for protocol base addresses.
 */
#define ADDR_MM2     0x0000 // MM2 locomotive
#define ADDR_SX1     0x0800 // Selectrix (old) locomotive
#define ADDR_MFX     0x4000 // MFX locomotive
#define ADDR_SX2     0x8000 // Selectrix (new) locomotive
#define ADDR_DCC     0xC000 // DCC locomotive
#define ADDR_ACC_SX1 0x2000 // Selectrix (old) magnetic accessory
#define ADDR_ACC_MM2 0x2FFF // MM2 magnetic accessory
#define ADDR_ACC_DCC 0x3800 // DCC magnetic accessory

/**
 * Constants for classic MM2 Delta addresses.
 */
#define DELTA1       78
#define DELTA2       72
#define DELTA3       60
#define DELTA4       24

/**
 * Constants for locomotive directions.
 */
#define DIR_CURRENT  0
#define DIR_FORWARD  1
#define DIR_REVERSE  2
#define DIR_CHANGE   3

/**
 * Constants for accessory states including some aliases.
 */
#define ACC_OFF      0
#define ACC_ROUND    0
#define ACC_RED      0
#define ACC_RIGHT    0
#define ACC_HP0      0

#define ACC_ON       1
#define ACC_GREEN    1
#define ACC_STRAIGHT 1
#define ACC_HP1      1

#define ACC_YELLOW   2
#define ACC_LEFT     2
#define ACC_HP2      2

#define ACC_WHITE    3
#define ACC_SH0      3


#endif // CONFIG_H