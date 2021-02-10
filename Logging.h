/**
 *  Provide logging macros and functions.
 *  Copyright (C) 2021 Martin Christian
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LOGGING_H
#define LOGGING_H

/**
 * Global serial device.
 */
void log_buffer(byte * buffer, byte bufferSize);

/**
 * Logging macros.
 */
#if defined(DEBUG_VERBOSE)
#  define DEBUG_VERBOSE_PRINTLN(x) Serial.println(x)
#  define DEBUG_VERBOSE_PRINT(x)   Serial.print(x)
#  define DEBUG_PRINTLN(x)         Serial.println(x)
#  define DEBUG_PRINT(x)           Serial.print(x)
#  define DEBUG_PRINTI(x, b)       Serial.print(x, b)
#  define DEBUG_PRINTB(b, l)       log_buffer(b, l)
#elif defined(DEBUG)
#  define DEBUG_VERBOSE_PRINTLN(x)
#  define DEBUG_VERBOSE_PRINT(x)
#  define DEBUG_PRINTLN(x)      Serial.println(x)
#  define DEBUG_PRINT(x)        Serial.print(x)
#  define DEBUG_PRINTI(x, b)    Serial.print(x, b)
#  define DEBUG_PRINTB(b, l)    log_buffer(b, l)
#else
#  define DEBUG_VERBOSE_PRINTLN(x)
#  define DEBUG_VERBOSE_PRINT(x)
#  define DEBUG_PRINTLN(x)
#  define DEBUG_PRINT(x)
#  define DEBUG_PRINTI(x, b)
#  define DEBUG_PRINTB(b, l)
#endif

#endif // LOGGING_H
