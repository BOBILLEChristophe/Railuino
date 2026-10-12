
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 – 2026 christophe bobille
 * Copyright (C) 2026 Uwe Monreal
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

#ifndef RAILUINO_MARKLIN_PROTOCOL_H
#define RAILUINO_MARKLIN_PROTOCOL_H

#include <stdint.h>

/**
 * Constants of the Märklin CAN protocol (CS2 CAN protocol 2.0,
 * https://streaming.maerklin.de/public-media/cs2/cs2CAN-Protokoll-2_0.pdf).
 *
 * The macros of Config.h (ADDR_MFX, DIR_FORWARD, ACC_STRAIGHT...) are kept
 * for existing sketches; new code can use these typed constants.
 */
namespace Marklin
{
  /**
   * Commands (bits 17..24 of the CAN identifier).
   */
  namespace Cmd
  {
    constexpr uint8_t System = 0x00;      ///< system command, sub-command in data[4]
    constexpr uint8_t Discovery = 0x01;   ///< MFX locomotive discovery
    constexpr uint8_t Bind = 0x02;        ///< MFX bind (assign a SID to a UID)
    constexpr uint8_t Verify = 0x03;      ///< MFX verify
    constexpr uint8_t Speed = 0x04;       ///< locomotive speed
    constexpr uint8_t Direction = 0x05;   ///< locomotive direction
    constexpr uint8_t Function = 0x06;    ///< locomotive function
    constexpr uint8_t ReadConfig = 0x07;  ///< read a decoder CV
    constexpr uint8_t WriteConfig = 0x08; ///< write a decoder CV
    constexpr uint8_t Accessory = 0x0B;   ///< switch an accessory
    constexpr uint8_t S88Event = 0x11;    ///< feedback event (S88 Link...)
    constexpr uint8_t Ping = 0x18;        ///< ping: every device answers
    constexpr uint8_t Bootloader = 0x1B;  ///< bootloader (Gleisbox wake-up)
  }

  /**
   * Sub-commands of Cmd::System (data[4]).
   */
  namespace SysCmd
  {
    constexpr uint8_t Stop = 0x00;             ///< track power off
    constexpr uint8_t Go = 0x01;               ///< track power on
    constexpr uint8_t Halt = 0x02;             ///< all locomotives stop
    constexpr uint8_t LocoEmergency = 0x03;    ///< emergency stop of one locomotive
    constexpr uint8_t TrackProtocol = 0x08;    ///< enable track protocols
    constexpr uint8_t MfxRegCounter = 0x09;    ///< MFX re-registration counter
  }

  /**
   * Track protocols for SysCmd::TrackProtocol (bit field in data[5]).
   */
  namespace TrackProtocol
  {
    constexpr uint8_t MM2 = 0x01;
    constexpr uint8_t MFX = 0x02;
    constexpr uint8_t DCC = 0x04;
    constexpr uint8_t All = MM2 | MFX | DCC;
  }

  /**
   * Base of the locomotive and accessory identifiers (Loc-ID), to which
   * the address (or the MFX SID) is added: ADDR_MFX + 7 = 0x4007.
   */
  namespace Addr
  {
    constexpr uint16_t MM2 = 0x0000;
    constexpr uint16_t SX1 = 0x0800;
    constexpr uint16_t MFX = 0x4000;
    constexpr uint16_t SX2 = 0x8000;
    constexpr uint16_t DCC = 0xC000;
    constexpr uint16_t AccSX1 = 0x2000;
    constexpr uint16_t AccMM2 = 0x2FFF; ///< + address from 1
    constexpr uint16_t AccDCC = 0x3800;
  }

  /**
   * Locomotive direction (Cmd::Direction, data[4]).
   */
  enum class Direction : uint8_t
  {
    Keep = 0,
    Forward = 1,
    Reverse = 2,
    Toggle = 3
  };

  /**
   * Device types answered to Cmd::Ping (data[6..7]).
   */
  namespace Device
  {
    constexpr uint16_t Gleisbox = 0x0010;       ///< Gleisbox 60112 / 60113
    constexpr uint16_t GleisboxNew = 0x0011;    ///< seen with firmware 1.47
  }

  /**
   * Wake-up code sent to the Gleisbox with Cmd::Bootloader (data[4]).
   */
  constexpr uint8_t GleisboxWakeUp = 0x11;

  /**
   * Bit rate of the Märklin CAN bus.
   */
  constexpr uint32_t BitRate = 250000UL;
}

#endif // RAILUINO_MARKLIN_PROTOCOL_H
