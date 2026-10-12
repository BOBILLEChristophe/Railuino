/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
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

/**
 * Single header to include in a sketch:
 *
 *   #include <Railuino.h>
 *
 * Supported boards: ESP32 (built-in TWAI controller, ACAN_ESP32 library,
 * or an MCP2515) and AVR boards such as Uno and Mega with an MCP2515
 * (ACAN2515 library). See CanDriver.h.
 */

#ifndef RAILUINO_H
#define RAILUINO_H

#include <Arduino.h>

#include "Config.h"
#include "MarklinProtocol.h"
#include "CanDriver.h"
#include "Esp32TwaiDriver.h"
#include "Mcp2515Driver.h"
#include "TrackMessage.h"
#include "TrackController.h"
#include "TrackReporterS88.h"
#include "CommandParser.h"

/**
 * Library version, kept in sync with library.properties and library.json.
 */
#define RAILUINO_VERSION_MAJOR 0
#define RAILUINO_VERSION_MINOR 12
#define RAILUINO_VERSION_PATCH 0
#define RAILUINO_VERSION_STRING "0.12.0"

#endif // RAILUINO_H
