/*
 * SparkMiner - OLED Display Driver Implementation
 * U8g2-based driver for small monochrome OLED displays
 *
 * GPL v3 License
 */

#include <Arduino.h>
#include <board_config.h>
#include "display/display_oled.h"

#if USE_OLED_DISPLAY

#include <U8g2lib.h>
#include <Wire.h>

// ============================================================
// Configuration
// ============================================================

// Default I2C pins (can be overridden in board_config.h)
#ifndef OLED_SDA_PIN
    #define OLED_SDA_PIN 21
#endif

#ifndef OLED_SCL_PIN
    #define OLED_SCL_PIN 22
#endif

#ifndef OLED_I2C_ADDR
    #define OLED_I2C_ADDR 0x3C
#endif

// Default SPI pins for ST7565/ST7567 (can be overridden in board_config.h)
#ifndef OLED_CS_PIN
    #define OLED_CS_PIN  SS
#endif

#ifndef OLED_DC_PIN
    #define OLED_DC_PIN  -1
#endif

#ifndef OLED_RST_PIN
    #define OLED_RST_PIN U8X8_PIN_NONE
#endif

// SW SPI requires explicit SCK and MOSI pins
#ifndef OLED_SCK_PIN
    #define OLED_SCK_PIN  SCK
#endif

#ifndef OLED_MOSI_PIN
    #define OLED_MOSI_PIN MOSI
#endif

// Display dimensions (set in board_config.h)
#ifndef OLED_WIDTH
    #define OLED_WIDTH 128
#endif

#ifndef OLED_HEIGHT
    #define OLED_HEIGHT 64
#endif

// Screen cycling (OLED has limited space, fewer screens)
#define OLED_SCREEN_MAIN    0
#define OLED_SCREEN_STATS   1
#define OLED_SCREEN_COUNT   2

// ============================================================
// ST7567 Custom Raw SPI Driver (replaces U8g2 for ST7567)
// Init sequence is identical to the working STM32 reference driver.
// ============================================================
#if defined(OLED_DRIVER_ST7567)

// 5×7 ASCII font — column-major, bit0=top, chars 0x20–0x7E.
// Copied verbatim from the STM32 reference driver (st7567.c).
static const uint8_t ST7_FONT[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20   */
    {0x00,0x00,0x5F,0x00,0x00}, /* 0x21 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 0x22 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 0x23 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 0x24 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 0x25 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 0x26 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 0x27 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 0x28 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 0x29 ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 0x2A * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 0x2B + */
    {0x00,0x50,0x30,0x00,0x00}, /* 0x2C , */
    {0x08,0x08,0x08,0x08,0x08}, /* 0x2D - */
    {0x00,0x60,0x60,0x00,0x00}, /* 0x2E . */
    {0x20,0x10,0x08,0x04,0x02}, /* 0x2F / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0x30 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 0x31 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 0x32 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 0x33 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 0x34 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 0x35 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 0x36 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 0x37 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 0x39 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 0x3A : */
    {0x00,0x56,0x36,0x00,0x00}, /* 0x3B ; */
    {0x00,0x08,0x14,0x22,0x41}, /* 0x3C < */
    {0x14,0x14,0x14,0x14,0x14}, /* 0x3D = */
    {0x41,0x22,0x14,0x08,0x00}, /* 0x3E > */
    {0x02,0x01,0x51,0x09,0x06}, /* 0x3F ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 0x40 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 0x41 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 0x42 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 0x43 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 0x44 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 0x45 E */
    {0x7F,0x09,0x09,0x09,0x01}, /* 0x46 F */
    {0x3E,0x41,0x49,0x49,0x3A}, /* 0x47 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 0x48 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 0x49 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 0x4A J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 0x4B K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 0x4C L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 0x4D M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 0x4E N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 0x4F O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 0x50 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 0x51 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 0x52 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 0x53 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 0x54 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 0x55 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 0x56 V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 0x57 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 0x58 X */
    {0x07,0x08,0x70,0x08,0x07}, /* 0x59 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 0x5A Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* 0x5B [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 0x5C \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* 0x5D ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 0x5E ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 0x5F _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 0x60 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 0x61 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 0x62 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 0x63 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 0x64 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 0x65 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 0x66 f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 0x67 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 0x68 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 0x69 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 0x6A j */
    {0x7F,0x10,0x28,0x44,0x00}, /* 0x6B k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 0x6C l */
    {0x7C,0x04,0x18,0x04,0x7C}, /* 0x6D m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 0x6E n */
    {0x38,0x44,0x44,0x44,0x38}, /* 0x6F o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 0x70 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 0x71 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 0x72 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 0x73 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 0x74 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 0x75 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 0x76 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 0x77 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 0x78 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 0x79 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 0x7A z */
    {0x00,0x08,0x36,0x41,0x00}, /* 0x7B { */
    {0x00,0x00,0x77,0x00,0x00}, /* 0x7C | */
    {0x00,0x41,0x36,0x08,0x00}, /* 0x7D } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* 0x7E ~ */
};

// Framebuffer: 8 pages × OLED_WIDTH columns (128×64 = 1024 bytes)
static uint8_t st7_fb[8][OLED_WIDTH];

// ---- Low-level SPI bit-bang ----
static void st7_spi_byte(uint8_t b) {
    shiftOut(OLED_MOSI_PIN, OLED_SCK_PIN, MSBFIRST, b);
}
static void st7_cmd(uint8_t c) {
    digitalWrite(OLED_DC_PIN, LOW);
    digitalWrite(OLED_CS_PIN, LOW);
    st7_spi_byte(c);
    digitalWrite(OLED_CS_PIN, HIGH);
}

// ---- Framebuffer drawing ----
static void st7_clear() { memset(st7_fb, 0, sizeof(st7_fb)); }

static void st7_set_pixel(int x, int y, uint8_t on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    if (on) st7_fb[y >> 3][x] |=  (uint8_t)(1u << (y & 7));
    else    st7_fb[y >> 3][x] &= ~(uint8_t)(1u << (y & 7));
}
static void st7_draw_hline(int x, int y, int w) {
    for (int i = 0; i < w; i++) st7_set_pixel(x + i, y, 1);
}
static void st7_draw_box(int x, int y, int w, int h) {
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++)
            st7_set_pixel(c, r, 1);
}

// Draw 5×7 char — y is TOP row of the character
static void st7_draw_char(int x, int y, char ch) {
    if ((uint8_t)ch < 0x20 || (uint8_t)ch > 0x7E) ch = ' ';
    const uint8_t *g = ST7_FONT[(uint8_t)ch - 0x20];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if ((g[col] >> row) & 1) st7_set_pixel(x + col, y + row, 1);
}
// Draw char at 2× scale (10×14 pixels, 11px stride per char)
static void st7_draw_char_2x(int x, int y, char ch) {
    if ((uint8_t)ch < 0x20 || (uint8_t)ch > 0x7E) ch = ' ';
    const uint8_t *g = ST7_FONT[(uint8_t)ch - 0x20];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if ((g[col] >> row) & 1) {
                st7_set_pixel(x + col*2,     y + row*2,     1);
                st7_set_pixel(x + col*2 + 1, y + row*2,     1);
                st7_set_pixel(x + col*2,     y + row*2 + 1, 1);
                st7_set_pixel(x + col*2 + 1, y + row*2 + 1, 1);
            }
}
// Draw string, y = top row; returns pixel width
static int st7_draw_str(int x, int y, const char *s) {
    int start = x;
    while (*s) { st7_draw_char(x, y, *s++); x += 6; }
    return x - start;
}
static int st7_draw_str_2x(int x, int y, const char *s) {
    int start = x;
    while (*s) { st7_draw_char_2x(x, y, *s++); x += 11; }
    return x - start;
}
static int st7_str_width(const char *s)    { int n = 0; while (*s++) n += 6;  return n; }
static int st7_str_width_2x(const char *s) { int n = 0; while (*s++) n += 11; return n; }

// Flush framebuffer to display (col offset = 0, same as STM32)
static void st7_send_buffer() {
    for (uint8_t page = 0; page < 8; page++) {
        st7_cmd(0xB0 | page);
        st7_cmd(0x10);   // column address high nibble = 0
        st7_cmd(0x00);   // column address low  nibble = 0
        digitalWrite(OLED_DC_PIN, HIGH);
        digitalWrite(OLED_CS_PIN, LOW);
        for (int i = 0; i < OLED_WIDTH; i++) st7_spi_byte(st7_fb[page][i]);
        digitalWrite(OLED_CS_PIN, HIGH);
    }
}
static void st7_set_contrast(uint8_t ev) {
    st7_cmd(0x81);
    st7_cmd(ev & 0x3F);
}

// Full hardware init — exact STM32 reference driver sequence
static void st7_hw_init(uint8_t rotation) {
    pinMode(OLED_SCK_PIN,  OUTPUT);
    pinMode(OLED_MOSI_PIN, OUTPUT);
    pinMode(OLED_CS_PIN,   OUTPUT);
    pinMode(OLED_DC_PIN,   OUTPUT);
    pinMode(OLED_RST_PIN,  OUTPUT);
    digitalWrite(OLED_CS_PIN,  HIGH);
    digitalWrite(OLED_SCK_PIN, LOW);

    // Hardware reset (STM32 timing: 10 ms pulse, 10 ms settle)
    digitalWrite(OLED_RST_PIN, HIGH); delay(1);
    digitalWrite(OLED_RST_PIN, LOW);  delay(10);
    digitalWrite(OLED_RST_PIN, HIGH); delay(10);

    // Init sequence — identical to STM32 reference driver
    st7_cmd(0xE2); delay(5);   // Software reset
    st7_cmd(0xAE);              // Display off
    if (rotation == 2) {
        st7_cmd(0xA1);          // ADC reverse  → horizontal flip (180°)
        st7_cmd(0xC0);          // COM normal   → vertical flip   (180°)
    } else {
        st7_cmd(0xA0);          // ADC normal
        st7_cmd(0xC8);          // COM reverse
    }
    st7_cmd(0xA2);              // 1/9 bias
    st7_cmd(0x2F);              // Power control: booster + regulator + follower ON
    delay(5);
    st7_cmd(0x26);              // Regulation ratio 4.5×
    st7_cmd(0x81);              // Electronic volume (contrast) command
    st7_cmd(0x0A);              // Contrast EV = 10 (same as STM32 reference)
    st7_cmd(0x40);              // Display start line = 0
    st7_cmd(0xA6);              // Normal display (not inverted)
    st7_cmd(0xA4);              // All pixels normal (not forced on)
    st7_cmd(0xAF);              // Display ON
}

#define OLED_IS_SPI 1

#else // !OLED_DRIVER_ST7567 — use U8g2 for all other display types

// ============================================================
// U8g2 Display Object
// ============================================================

// Select display constructor based on driver type and size
#if defined(OLED_DRIVER_ST7565)
    // 128x64 ST7565 (4-wire HW SPI)
    // Select variant via OLED_ST7565_VARIANT (default=1):
    //   1 = ERC12864       (most common)
    //   2 = ERC12864_ALT   (alternate init, try if 1 shows nothing)
    //   3 = 64128N         (generic/universal)
    //   4 = JLX12864       (JLX panel)
    #ifndef OLED_ST7565_VARIANT
        #define OLED_ST7565_VARIANT 1
    #endif
    #if (OLED_ST7565_VARIANT == 2)
        static U8G2_ST7565_ERC12864_ALT_F_4W_HW_SPI s_u8g2(U8G2_R0, OLED_CS_PIN, OLED_DC_PIN, OLED_RST_PIN);
    #elif (OLED_ST7565_VARIANT == 3)
        static U8G2_ST7565_64128N_F_4W_HW_SPI s_u8g2(U8G2_R0, OLED_CS_PIN, OLED_DC_PIN, OLED_RST_PIN);
    #elif (OLED_ST7565_VARIANT == 4)
        static U8G2_ST7565_JLX12864_F_4W_HW_SPI s_u8g2(U8G2_R0, OLED_CS_PIN, OLED_DC_PIN, OLED_RST_PIN);
    #else
        // OLED_ST7565_VARIANT == 1 (default)
        static U8G2_ST7565_ERC12864_F_4W_HW_SPI s_u8g2(U8G2_R0, OLED_CS_PIN, OLED_DC_PIN, OLED_RST_PIN);
    #endif
    #define OLED_IS_SPI 1
#elif (OLED_HEIGHT == 64)
    // 128x64 SSD1306 (I2C)
    static U8G2_SSD1306_128X64_NONAME_F_HW_I2C s_u8g2(U8G2_R0, U8X8_PIN_NONE);
    #define OLED_IS_SPI 0
#elif (OLED_HEIGHT == 32)
    // 128x32 SSD1306 (I2C)
    static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C s_u8g2(U8G2_R0, U8X8_PIN_NONE);
    #define OLED_IS_SPI 0
#else
    // Default to 128x64 SSD1306 (I2C)
    static U8G2_SSD1306_128X64_NONAME_F_HW_I2C s_u8g2(U8G2_R0, U8X8_PIN_NONE);
    #define OLED_IS_SPI 0
#endif

#endif // !OLED_DRIVER_ST7567

// ============================================================
// State
// ============================================================

static uint8_t s_currentScreen = OLED_SCREEN_MAIN;
static uint8_t s_brightness = 255;
static uint8_t s_rotation = 0;
static bool s_needsRedraw = true;
static bool s_inverted = false;

// ============================================================
// Helper Functions
// ============================================================

static String formatHashrateCompact(double hashrate) {
    if (hashrate >= 1e9) {
        return String(hashrate / 1e9, 1) + "G";
    } else if (hashrate >= 1e6) {
        return String(hashrate / 1e6, 1) + "M";
    } else if (hashrate >= 1e3) {
        return String(hashrate / 1e3, 1) + "K";
    } else {
        return String((int)hashrate);
    }
}

static String formatUptimeCompact(uint32_t seconds) {
    uint32_t days = seconds / 86400;
    uint32_t hours = (seconds % 86400) / 3600;
    uint32_t mins = (seconds % 3600) / 60;

    if (days > 0) {
        return String(days) + "d" + String(hours) + "h";
    } else if (hours > 0) {
        return String(hours) + "h" + String(mins) + "m";
    } else {
        return String(mins) + "m";
    }
}

static String formatDiffCompact(double diff) {
    if (diff >= 1e12) {
        return String(diff / 1e12, 1) + "T";
    } else if (diff >= 1e9) {
        return String(diff / 1e9, 1) + "G";
    } else if (diff >= 1e6) {
        return String(diff / 1e6, 1) + "M";
    } else if (diff >= 1e3) {
        return String(diff / 1e3, 1) + "K";
    } else {
        return String((int)diff);
    }
}

// ============================================================
// Screen Drawing Functions
// ============================================================

static void drawMainScreen(const display_data_t *data) {
#if defined(OLED_DRIVER_ST7567)
    char buf[32];
    st7_clear();

    // Row 0: status bar — W/P indicators + uptime
    st7_draw_str(0, 0, data->wifiConnected ? "W" : "w");
    st7_draw_str(7, 0, data->poolConnected ? "P" : "p");
    String uptime = formatUptimeCompact(data->uptimeSeconds);
    st7_draw_str(OLED_WIDTH - st7_str_width(uptime.c_str()), 0, uptime.c_str());

    st7_draw_hline(0, 8, OLED_WIDTH);  // separator

    // Row 10: large hashrate (2× scale, centred)
    String hashrate = formatHashrateCompact(data->hashRate);
    int hrW = st7_str_width_2x(hashrate.c_str());
    st7_draw_str_2x((OLED_WIDTH - hrW) / 2, 10, hashrate.c_str());

    // Row 26: "H/s" unit
    st7_draw_str((OLED_WIDTH - 18) / 2, 26, "H/s");

    st7_draw_hline(0, 34, OLED_WIDTH);  // separator

    // Row 36: shares (left) + best diff (right)
    snprintf(buf, sizeof(buf), "S:%d", data->sharesAccepted);
    st7_draw_str(0, 36, buf);
    String best = "B:" + formatDiffCompact(data->bestDifficulty);
    st7_draw_str(OLED_WIDTH - st7_str_width(best.c_str()), 36, best.c_str());

    // Row 45: pool status (left) + ping (right)
    st7_draw_str(0, 45, data->poolConnected ? "Pool:OK" : "Pool:--");
    snprintf(buf, sizeof(buf), "%dms", data->avgLatency);
    st7_draw_str(OLED_WIDTH - st7_str_width(buf), 45, buf);

    // Row 54: block height
    snprintf(buf, sizeof(buf), "Blk:%lu", (unsigned long)data->blockHeight);
    st7_draw_str(0, 54, buf);

    st7_send_buffer();
#else
    s_u8g2.clearBuffer();

    // Header line with status icons
    s_u8g2.setFont(u8g2_font_6x10_tf);

    // WiFi status
    if (data->wifiConnected) {
        s_u8g2.drawStr(0, 8, "W");
    } else {
        s_u8g2.drawStr(0, 8, "-");
    }

    // Pool status
    if (data->poolConnected) {
        s_u8g2.drawStr(10, 8, "P");
    } else {
        s_u8g2.drawStr(10, 8, "-");
    }

    // Uptime (right aligned)
    String uptime = formatUptimeCompact(data->uptimeSeconds);
    int uptimeWidth = s_u8g2.getStrWidth(uptime.c_str());
    s_u8g2.drawStr(OLED_WIDTH - uptimeWidth, 8, uptime.c_str());

    // Separator line
    s_u8g2.drawHLine(0, 10, OLED_WIDTH);

    // Large hashrate display
    s_u8g2.setFont(u8g2_font_logisoso16_tn);  // Large numeric font
    String hashrate = formatHashrateCompact(data->hashRate);
    int hrWidth = s_u8g2.getStrWidth(hashrate.c_str());
    s_u8g2.drawStr((OLED_WIDTH - hrWidth) / 2, 32, hashrate.c_str());

    // "H/s" label below
    s_u8g2.setFont(u8g2_font_6x10_tf);
    s_u8g2.drawStr((OLED_WIDTH - 18) / 2, 42, "H/s");

    // Bottom stats row
    #if (OLED_HEIGHT == 64)
        s_u8g2.drawHLine(0, 48, OLED_WIDTH);

        // Shares
        String shares = "S:" + String(data->sharesAccepted);
        s_u8g2.drawStr(0, 60, shares.c_str());

        // Best diff (right)
        String best = "B:" + formatDiffCompact(data->bestDifficulty);
        int bestWidth = s_u8g2.getStrWidth(best.c_str());
        s_u8g2.drawStr(OLED_WIDTH - bestWidth, 60, best.c_str());
    #endif

    s_u8g2.sendBuffer();
#endif // OLED_DRIVER_ST7567
}

static void drawStatsScreen(const display_data_t *data) {
#if defined(OLED_DRIVER_ST7567)
    char buf[32];
    st7_clear();

    st7_draw_str(0, 0, "STATS");
    st7_draw_hline(0, 8, OLED_WIDTH);

    String poolLine = String("Pool:") + (data->poolConnected ? "OK" : "--");
    st7_draw_str(0, 10, poolLine.c_str());

    snprintf(buf, sizeof(buf), "HR:%.0f H/s", data->hashRate);
    st7_draw_str(0, 19, buf);

    snprintf(buf, sizeof(buf), "S:%d", data->sharesAccepted);
    st7_draw_str(0, 28, buf);

    String bestLine = "Best:" + formatDiffCompact(data->bestDifficulty);
    st7_draw_str(0, 37, bestLine.c_str());

    String diffLine = "Diff:" + formatDiffCompact(data->poolDifficulty);
    st7_draw_str(0, 46, diffLine.c_str());

    if (data->wifiConnected) {
        snprintf(buf, sizeof(buf), "RSSI:%ddBm", data->wifiRssi);
    } else {
        snprintf(buf, sizeof(buf), "WiFi:--");
    }
    st7_draw_str(0, 55, buf);

    st7_send_buffer();
#else
    s_u8g2.clearBuffer();
    s_u8g2.setFont(u8g2_font_6x10_tf);

    // Title
    s_u8g2.drawStr(0, 8, "STATS");
    s_u8g2.drawHLine(0, 10, OLED_WIDTH);

    // Pool info
    String poolLine = "Pool: ";
    poolLine += data->poolConnected ? "OK" : "---";
    s_u8g2.drawStr(0, 22, poolLine.c_str());

    // Difficulty
    String diffLine = "Diff: " + formatDiffCompact(data->poolDifficulty);
    s_u8g2.drawStr(0, 34, diffLine.c_str());

    // Templates
    String templLine = "Tmpl: " + String(data->templates);
    s_u8g2.drawStr(0, 46, templLine.c_str());

    #if (OLED_HEIGHT == 64)
        // WiFi signal
        String rssiLine = "RSSI: ";
        if (data->wifiConnected) {
            rssiLine += String(data->wifiRssi) + "dBm";
        } else {
            rssiLine += "---";
        }
        s_u8g2.drawStr(0, 58, rssiLine.c_str());
    #endif

    s_u8g2.sendBuffer();
#endif // OLED_DRIVER_ST7567
}
// ============================================================

void oled_display_init(uint8_t rotation, uint8_t brightness) {
    Serial.printf("[OLED] Initializing %dx%d display\n", OLED_WIDTH, OLED_HEIGHT);

    // Print pin assignments for wiring verification
    #if defined(OLED_DRIVER_ST7567) || defined(OLED_DRIVER_ST7565)
        Serial.println("[OLED] === ST7567/ST7565 SPI Pin Map ===");
        Serial.printf("[OLED]   SCK  (clock)       -> GPIO%d\n", (int)OLED_SCK_PIN);
        Serial.printf("[OLED]   MOSI (data)         -> GPIO%d\n", (int)OLED_MOSI_PIN);
        Serial.printf("[OLED]   CS   (chip select)  -> GPIO%d\n", (int)OLED_CS_PIN);
        Serial.printf("[OLED]   DC   (data/cmd)     -> GPIO%d\n", (int)OLED_DC_PIN);
        Serial.printf("[OLED]   RST  (reset)        -> GPIO%d\n", (int)OLED_RST_PIN);
        Serial.println("[OLED] VDD=3.3V  VSS=GND  BL+=3.3V  BL-=GND");
        Serial.println("[OLED] ======================================");
    #else
        Serial.printf("[OLED]   SDA -> GPIO%d\n", (int)OLED_SDA_PIN);
        Serial.printf("[OLED]   SCL -> GPIO%d\n", (int)OLED_SCL_PIN);
    #endif

    // Initialize bus and display
    #if !OLED_IS_SPI
        Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    #endif

    s_rotation = rotation;

    #if defined(OLED_DRIVER_ST7567)
        // Raw SPI init — exact STM32 sequence (covers reset + contrast + display on)
        st7_hw_init(rotation);
        Serial.println("[OLED] ST7567 raw SPI init complete");

        // Diagnostic: full-white fill for 1 s, then checkerboard for 1 s
        st7_clear();
        st7_draw_box(0, 0, OLED_WIDTH, OLED_HEIGHT);
        st7_send_buffer();
        Serial.println("[OLED] DIAG: full-white fill sent — should be visible now");
        delay(1000);
        st7_clear();
        for (int dy = 0; dy < OLED_HEIGHT; dy += 8)
            for (int dx = 0; dx < OLED_WIDTH; dx += 8)
                if (((dx / 8) + (dy / 8)) % 2 == 0)
                    st7_draw_box(dx, dy, 8, 8);
        st7_send_buffer();
        Serial.println("[OLED] DIAG: checkerboard sent");
        delay(1000);
        // Contrast already set to EV=0x0A inside st7_hw_init (same as STM32)
        s_brightness = 0x0A;
    #else
        // U8g2 init
        bool initOk = s_u8g2.begin();
        Serial.printf("[OLED] begin() = %s\n", initOk ? "OK" : "FAIL");
        if (rotation == 2) {
            s_u8g2.setDisplayRotation(U8G2_R2);
        } else {
            s_u8g2.setDisplayRotation(U8G2_R0);
        }
        s_brightness = (uint8_t)((brightness * 255UL) / 100UL);
        s_u8g2.setContrast(s_brightness);
    #endif

    // Show boot screen
    oled_display_show_boot();

    Serial.println("[OLED] Display initialized");
}

void oled_display_update(const display_data_t *data) {
    switch (s_currentScreen) {
        case OLED_SCREEN_MAIN:
            drawMainScreen(data);
            break;
        case OLED_SCREEN_STATS:
            drawStatsScreen(data);
            break;
        default:
            drawMainScreen(data);
            break;
    }
    s_needsRedraw = false;
}

void oled_display_set_brightness(uint8_t brightness) {
    #if defined(OLED_DRIVER_ST7567)
        s_brightness = (uint8_t)((brightness * 63UL) / 100UL);
        if (s_brightness > 20) s_brightness = 20;
        st7_set_contrast(s_brightness);
    #else
        s_brightness = (uint8_t)((brightness * 255UL) / 100UL);
        s_u8g2.setContrast(s_brightness);
    #endif
}

void oled_display_next_screen() {
    s_currentScreen = (s_currentScreen + 1) % OLED_SCREEN_COUNT;
    s_needsRedraw = true;
    Serial.printf("[OLED] Screen: %d\n", s_currentScreen);
}

void oled_display_show_ap_config(const char *ssid, const char *password, const char *ip) {
#if defined(OLED_DRIVER_ST7567)
    st7_clear();
    st7_draw_str(0, 0,  "WiFi Setup");
    st7_draw_hline(0, 8, OLED_WIDTH);
    st7_draw_str(0, 11, "SSID:");
    st7_draw_str(0, 20, ssid);
    st7_draw_str(0, 30, "Pass:");
    st7_draw_str(0, 39, password);
    st7_draw_str(0, 49, ip);
    st7_send_buffer();
#else
    s_u8g2.clearBuffer();
    s_u8g2.setFont(u8g2_font_6x10_tf);
    s_u8g2.drawStr(0, 10, "WiFi Setup");
    s_u8g2.drawHLine(0, 12, OLED_WIDTH);
    s_u8g2.drawStr(0, 26, "SSID:");
    s_u8g2.drawStr(0, 38, ssid);
    s_u8g2.drawStr(0, 52, "Pass:");
    s_u8g2.drawStr(30, 52, password);
    #if (OLED_HEIGHT == 64)
        s_u8g2.drawStr(0, 64, ip);
    #endif
    s_u8g2.sendBuffer();
#endif
}

void oled_display_show_boot() {
#if defined(OLED_DRIVER_ST7567)
    st7_clear();
    // "SparkMiner" at 2× scale centred
    const char *title = "SparkMiner";
    int tw = st7_str_width_2x(title);
    st7_draw_str_2x((OLED_WIDTH - tw) / 2, 10, title);
    // Version below
    const char *version = AUTO_VERSION;
    int vw = st7_str_width(version);
    st7_draw_str((OLED_WIDTH - vw) / 2, 36, version);
    st7_draw_str((OLED_WIDTH - st7_str_width("Initializing...")) / 2, 50, "Initializing...");
    st7_send_buffer();
#else
    s_u8g2.clearBuffer();
    s_u8g2.setFont(u8g2_font_7x14B_tf);
    const char *title = "SparkMiner";
    int titleWidth = s_u8g2.getStrWidth(title);
    s_u8g2.drawStr((OLED_WIDTH - titleWidth) / 2, 28, title);
    s_u8g2.setFont(u8g2_font_6x10_tf);
    const char *version = AUTO_VERSION;
    int versionWidth = s_u8g2.getStrWidth(version);
    s_u8g2.drawStr((OLED_WIDTH - versionWidth) / 2, 42, version);
    #if (OLED_HEIGHT == 64)
        s_u8g2.drawStr((OLED_WIDTH - 60) / 2, 58, "Initializing...");
    #endif
    s_u8g2.sendBuffer();
#endif
}

void oled_display_show_reset_countdown(int seconds) {
#if defined(OLED_DRIVER_ST7567)
    st7_clear();
    st7_draw_str_2x(10, 2,  "FACTORY");
    st7_draw_str_2x(22, 20, "RESET");
    char buf[8]; snprintf(buf, sizeof(buf), "%d", seconds);
    st7_draw_str_2x((OLED_WIDTH - st7_str_width_2x(buf)) / 2, 44, buf);
    st7_send_buffer();
#else
    s_u8g2.clearBuffer();
    s_u8g2.setFont(u8g2_font_7x14B_tf);
    s_u8g2.drawStr(20, 20, "FACTORY");
    s_u8g2.drawStr(28, 36, "RESET");
    s_u8g2.setFont(u8g2_font_logisoso16_tn);
    String countdown = String(seconds);
    int w = s_u8g2.getStrWidth(countdown.c_str());
    s_u8g2.drawStr((OLED_WIDTH - w) / 2, 58, countdown.c_str());
    s_u8g2.sendBuffer();
#endif
}

void oled_display_show_reset_complete() {
#if defined(OLED_DRIVER_ST7567)
    st7_clear();
    st7_draw_str_2x(22, 16, "RESET");
    st7_draw_str_2x(2,  36, "COMPLETE");
    st7_send_buffer();
#else
    s_u8g2.clearBuffer();
    s_u8g2.setFont(u8g2_font_7x14B_tf);
    s_u8g2.drawStr(28, 28, "RESET");
    s_u8g2.drawStr(16, 46, "COMPLETE");
    s_u8g2.sendBuffer();
#endif
}

void oled_display_redraw() {
    s_needsRedraw = true;
}

uint8_t oled_display_flip_rotation() {
    s_rotation = (s_rotation == 0) ? 2 : 0;
#if defined(OLED_DRIVER_ST7567)
    st7_hw_init(s_rotation);  // re-init with new ADC/COM direction
#else
    if (s_rotation == 2) { s_u8g2.setDisplayRotation(U8G2_R2); }
    else                 { s_u8g2.setDisplayRotation(U8G2_R0); }
#endif
    s_needsRedraw = true;
    return s_rotation;
}

void oled_display_set_rotation(uint8_t rotation) {
    s_rotation = (rotation >= 2) ? 2 : 0;
#if defined(OLED_DRIVER_ST7567)
    st7_hw_init(s_rotation);
#else
    if (s_rotation == 2) { s_u8g2.setDisplayRotation(U8G2_R2); }
    else                 { s_u8g2.setDisplayRotation(U8G2_R0); }
#endif
    s_needsRedraw = true;
}

void oled_display_set_inverted(bool inverted) {
    s_inverted = inverted;
    // U8g2 doesn't have direct invert, we'd need to redraw with XOR
    // For now just track state
    s_needsRedraw = true;
}

uint16_t oled_display_get_width() {
    return OLED_WIDTH;
}

uint16_t oled_display_get_height() {
    return OLED_HEIGHT;
}

bool oled_display_is_portrait() {
    return false;  // OLEDs are typically landscape
}

uint8_t oled_display_get_screen() {
    return s_currentScreen;
}

void oled_display_set_screen(uint8_t screen) {
    if (screen < OLED_SCREEN_COUNT) {
        s_currentScreen = screen;
        s_needsRedraw = true;
    }
}

// ============================================================
// Display Driver Interface
// ============================================================

static DisplayDriver s_oledDriver = {
    .init = oled_display_init,
    .update = oled_display_update,
    .set_brightness = oled_display_set_brightness,
    .next_screen = oled_display_next_screen,
    .show_ap_config = oled_display_show_ap_config,
    .show_boot = oled_display_show_boot,
    .show_reset_countdown = oled_display_show_reset_countdown,
    .show_reset_complete = oled_display_show_reset_complete,
    .redraw = oled_display_redraw,
    .flip_rotation = oled_display_flip_rotation,
    .set_inverted = oled_display_set_inverted,
    .get_width = oled_display_get_width,
    .get_height = oled_display_get_height,
    .is_portrait = oled_display_is_portrait,
    .get_screen = oled_display_get_screen,
    .set_screen = oled_display_set_screen,
    .name = "U8g2 OLED"
};

DisplayDriver* oled_get_driver() {
    return &s_oledDriver;
}

// ============================================================
// Standard display API wrappers (for main.cpp compatibility)
// ============================================================

void display_init(uint8_t rotation, uint8_t brightness) {
    oled_display_init(rotation, brightness);
}

void display_update(const display_data_t *data) {
    oled_display_update(data);
}

void display_set_brightness(uint8_t brightness) {
    oled_display_set_brightness(brightness);
}

void display_set_screen(uint8_t screen) {
    oled_display_set_screen(screen);
}

uint8_t display_get_screen() {
    return oled_display_get_screen();
}

void display_next_screen() {
    oled_display_next_screen();
}

void display_redraw() {
    oled_display_redraw();
}

uint8_t display_flip_rotation() {
    return oled_display_flip_rotation();
}

void display_set_rotation(uint8_t rotation) {
    oled_display_set_rotation(rotation);
}

uint16_t display_get_width() {
    return oled_display_get_width();
}

uint16_t display_get_height() {
    return oled_display_get_height();
}

bool display_is_portrait() {
    return oled_display_is_portrait();
}

bool display_touched() {
    return false;  // OLED has no touch
}

void display_handle_touch() {
    // No-op for OLED
}

void display_show_ap_config(const char *ssid, const char *password, const char *ip) {
    oled_display_show_ap_config(ssid, password, ip);
}

void display_set_inverted(bool inverted) {
    oled_display_set_inverted(inverted);
}

void display_show_reset_countdown(int seconds) {
    oled_display_show_reset_countdown(seconds);
}

void display_show_reset_complete() {
    oled_display_show_reset_complete();
}

void display_set_backlight_off() {}
void display_set_backlight_on() {}
bool display_is_backlight_off() { return false; }

#endif // USE_OLED_DISPLAY
