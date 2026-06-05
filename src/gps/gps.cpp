/*
 * SparkMiner - GPS Module Driver Implementation
 * GY-NEO6MV2 (u-blox NEO-6M) via UART1
 *
 * GPL v3 License
 */

#include "gps.h"

#ifdef GPS_ENABLED

#include <TinyGPSPlus.h>

// ============================================================
// Configuration (from build flags or defaults)
// ============================================================
#ifndef GPS_RX_PIN
    #define GPS_RX_PIN  4
#endif
#ifndef GPS_TX_PIN
    #define GPS_TX_PIN  5
#endif
#ifndef GPS_BAUD
    #define GPS_BAUD    9600
#endif

#define GPS_TASK_STACK   3072
#define GPS_TASK_PRIO    1
#define GPS_LOG_INTERVAL 10000   // Print fix to Serial every 10 s

// ============================================================
// Module-private state
// ============================================================
static HardwareSerial   s_gpsSerial(1);   // UART1
static TinyGPSPlus      s_gps;
static gps_data_t       s_data;
static portMUX_TYPE     s_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t     s_task = NULL;

// ============================================================
// GPS background task
// ============================================================
static void gps_task(void *param) {
    uint32_t lastLog = 0;

    Serial.printf("[GPS] Task started (RX=GPIO%d, TX=GPIO%d, %u baud)\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);

    for (;;) {
        // Feed all available bytes into TinyGPSPlus
        while (s_gpsSerial.available()) {
            s_gps.encode(s_gpsSerial.read());
        }

        // Update shared struct when location is updated
        if (s_gps.location.isUpdated()) {
            gps_data_t tmp;
            tmp.valid      = s_gps.location.isValid();
            tmp.latitude   = s_gps.location.lat();
            tmp.longitude  = s_gps.location.lng();
            tmp.altitude   = s_gps.altitude.isValid()   ? s_gps.altitude.meters()   : 0.0;
            tmp.speed      = s_gps.speed.isValid()      ? s_gps.speed.kmph()        : 0.0;
            tmp.course     = s_gps.course.isValid()     ? s_gps.course.deg()        : 0.0;
            tmp.satellites = s_gps.satellites.isValid() ? s_gps.satellites.value()  : 0;
            tmp.hdop       = s_gps.hdop.isValid()       ? s_gps.hdop.value()        : 9999;
            if (s_gps.date.isValid()) {
                tmp.year  = s_gps.date.year();
                tmp.month = s_gps.date.month();
                tmp.day   = s_gps.date.day();
            }
            if (s_gps.time.isValid()) {
                tmp.hour   = s_gps.time.hour();
                tmp.minute = s_gps.time.minute();
                tmp.second = s_gps.time.second();
            }
            tmp.updatedMs = millis();

            portENTER_CRITICAL(&s_mux);
            s_data = tmp;
            portEXIT_CRITICAL(&s_mux);
        }

        // Periodic Serial log
        uint32_t now = millis();
        if (now - lastLog >= GPS_LOG_INTERVAL) {
            lastLog = now;
            if (s_gps.location.isValid()) {
                Serial.printf("[GPS] Fix: %.6f, %.6f  Alt:%.1fm  Spd:%.1fkm/h  Sat:%u  HDOP:%.2f\n",
                              s_gps.location.lat(),
                              s_gps.location.lng(),
                              s_gps.altitude.meters(),
                              s_gps.speed.kmph(),
                              s_gps.satellites.value(),
                              s_gps.hdop.hdop());
                if (s_gps.date.isValid() && s_gps.time.isValid()) {
                    Serial.printf("[GPS] UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                                  s_gps.date.year(), s_gps.date.month(),  s_gps.date.day(),
                                  s_gps.time.hour(), s_gps.time.minute(), s_gps.time.second());
                }
            } else {
                Serial.printf("[GPS] No fix yet — chars:%lu sentences:%lu failed:%lu\n",
                              s_gps.charsProcessed(),
                              s_gps.sentencesWithFix(),
                              s_gps.failedChecksum());
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));   // 20 ms — plenty for 9600 baud
    }
}

// ============================================================
// Public API
// ============================================================

void gps_init() {
    memset(&s_data, 0, sizeof(s_data));

    // Open UART1 on the configured pins
    s_gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // Create the background task (unpinned — works on single-core C3)
    xTaskCreate(
        gps_task,
        "GPS",
        GPS_TASK_STACK,
        NULL,
        GPS_TASK_PRIO,
        &s_task
    );

    Serial.println("[GPS] Initialized — waiting for satellite fix...");
}

void gps_get_data(gps_data_t *out) {
    portENTER_CRITICAL(&s_mux);
    *out = s_data;
    portEXIT_CRITICAL(&s_mux);
}

bool gps_has_fix() {
    portENTER_CRITICAL(&s_mux);
    bool fix = s_data.valid;
    portEXIT_CRITICAL(&s_mux);
    return fix;
}

#endif // GPS_ENABLED
