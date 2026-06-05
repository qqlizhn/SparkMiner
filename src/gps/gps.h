/*
 * SparkMiner - GPS Module Driver
 * GY-NEO6MV2 (u-blox NEO-6M) via UART1
 *
 * Wiring:
 *   GPS TX  ->  ESP32-C3 GPIO4 (UART1 RX)
 *   GPS RX  ->  ESP32-C3 GPIO5 (UART1 TX)
 *   GPS VCC ->  3.3V
 *   GPS GND ->  GND
 *
 * GPL v3 License
 */

#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

#ifdef GPS_ENABLED

// GPS data snapshot (thread-safe copy)
typedef struct {
    bool     valid;           // Fix acquired
    double   latitude;        // Degrees (+ = North, - = South)
    double   longitude;       // Degrees (+ = East, - = West)
    double   altitude;        // Metres above sea level
    double   speed;           // km/h
    double   course;          // Degrees (0-360)
    uint8_t  satellites;      // Satellites in use
    uint32_t hdop;            // Horizontal dilution * 100 (lower = better)
    // Date/Time (UTC)
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint32_t updatedMs;       // millis() when last updated
} gps_data_t;

/**
 * Initialize GPS UART and start background task.
 * Call once from setup() after Serial is ready.
 */
void gps_init();

/**
 * Get a snapshot of the latest GPS data.
 * Thread-safe: can be called from any task.
 *
 * @param out  Destination struct filled by this function.
 */
void gps_get_data(gps_data_t *out);

/**
 * Returns true if a valid fix has been received at least once.
 */
bool gps_has_fix();

#endif // GPS_ENABLED
#endif // GPS_H
