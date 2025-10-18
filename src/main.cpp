#include <Arduino.h>
#include <lvgl.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "qrcode.h"
#include "config.h"

// Pin definitions
#define PIN_POWER_ON 15
#define PIN_LCD_BL 38
#define PIN_BUTTON_1 0   // Boot button
#define PIN_BUTTON_2 14  // Second button
#define PIN_LCD_D0 39
#define PIN_LCD_D1 40
#define PIN_LCD_D2 41
#define PIN_LCD_D3 42
#define PIN_LCD_D4 45
#define PIN_LCD_D5 46
#define PIN_LCD_D6 47
#define PIN_LCD_D7 48
#define PIN_LCD_RES 5
#define PIN_LCD_CS 6
#define PIN_LCD_DC 7
#define PIN_LCD_WR 8
#define PIN_LCD_RD 9

// Display settings
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (10 * 1000 * 1000)
#define LVGL_LCD_BUF_SIZE (SCREEN_WIDTH * 40)

// LVGL objects
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_color_t *lv_disp_buf;
static lv_color_t *lv_disp_buf2;
static esp_lcd_panel_handle_t panel_handle = NULL;

// Screen objects
lv_obj_t *screen_obj = NULL;
lv_obj_t *main_label = NULL;
lv_obj_t *sub_label = NULL;

// State
int currentScreen = 0;
unsigned long lastScreenChange = 0;
unsigned long lastAPIFetch = 0;
String blueskyPosts[3];
String blueskyAuthors[3];
int postCount = 0;
String accessToken = "";  // Bluesky access token

// Brightness control
const uint8_t brightnessLevels[] = {26, 102, 179, 255};  // 10%, 40%, 70%, 100%
const char* brightnessLabels[] = {"10%", "40%", "70%", "100%"};
int currentBrightnessIndex = 3;  // Start at 100%
unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 200;  // 200ms debounce

// Screen timing (milliseconds)
const unsigned long screenTimes[] = {
    3000,  // Screen 0: Welcome - 3 seconds
    6000,  // Screen 1: Name - 6 seconds
    6000,  // Screen 2: Handle/QR - 6 seconds
    6000,  // Screen 3: Bluesky 1 - 6 seconds
    6000,  // Screen 4: Bluesky 2 - 6 seconds
    6000,  // Screen 5: Bluesky 3 - 6 seconds
    6000   // Screen 6: CTA - 6 seconds
};

// Forward declarations
bool authenticateBluesky();
void fetchBlueskyPosts();
void showScreen(int screen);
void screenTimerCallback(lv_timer_t * timer);
void setBrightness(int index);
void checkButtons();

// LVGL flush callback
static bool lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

// LVGL flush function
static void lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

void initDisplay() {
    pinMode(PIN_LCD_RD, OUTPUT);
    digitalWrite(PIN_LCD_RD, HIGH);

    // Initialize I80 bus
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .wr_gpio_num = PIN_LCD_WR,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_gpio_nums = {
            PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
            PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
        .psram_trans_align = 0,
        .sram_trans_align = 0
    };
    esp_lcd_new_i80_bus(&bus_config, &i80_bus);

    // Initialize panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 20,
        .on_color_trans_done = lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
    };
    esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle);

    // Initialize LCD panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RES,
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);

    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 35);

    // Initialize LVGL
    lv_init();

    lv_disp_buf = (lv_color_t *)heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_disp_buf2 = (lv_color_t *)heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, lv_disp_buf2, LVGL_LCD_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=================================");
    Serial.println("SeaGL 2025 Conference Badge");
    Serial.println("=================================\n");

    // Enable display power
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    delay(100);

    // Initialize backlight with PWM
    ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit resolution
    ledcAttachPin(PIN_LCD_BL, 0);
    setBrightness(currentBrightnessIndex);  // Set to initial brightness (100%)
    Serial.print("Backlight initialized at ");
    Serial.print(brightnessLabels[currentBrightnessIndex]);
    Serial.println(" brightness");

    // Initialize buttons
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);
    Serial.println("Buttons initialized");

    Serial.println("Initializing display...");
    initDisplay();
    Serial.println("Display initialized!");

    // Connect to WiFi
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {  // Increased to 60 attempts (30 seconds)
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        fetchBlueskyPosts();
    } else {
        Serial.println("\nWiFi connection failed after 30 seconds!");
    }

    // Show first screen
    showScreen(0);

    // Create LVGL timer to handle screen changes
    lv_timer_t * screenTimer = lv_timer_create(screenTimerCallback, screenTimes[0], NULL);
    lv_timer_set_repeat_count(screenTimer, -1); // Repeat forever

    Serial.println("Timer started!");
}

bool authenticateBluesky() {
    Serial.println("\n=== Authenticating with Bluesky ===");

    // Check if we have credentials
    if (String(BLUESKY_APP_PASSWORD).length() == 0) {
        Serial.println("ERROR: No app password configured!");
        Serial.println("Please create an app password at: https://bsky.app/settings/app-passwords");
        return false;
    }

    HTTPClient http;
    http.begin(BLUESKY_AUTH_URL);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload
    DynamicJsonDocument doc(256);
    doc["identifier"] = BLUESKY_IDENTIFIER;
    doc["password"] = BLUESKY_APP_PASSWORD;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.print("Authenticating as: ");
    Serial.println(BLUESKY_IDENTIFIER);

    int httpCode = http.POST(requestBody);
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();

        DynamicJsonDocument responseDoc(2048);
        DeserializationError error = deserializeJson(responseDoc, response);

        if (!error) {
            accessToken = responseDoc["accessJwt"].as<String>();
            Serial.print("SUCCESS: Got access token (length: ");
            Serial.print(accessToken.length());
            Serial.println(")");
            http.end();
            return true;
        } else {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("Authentication failed with code: ");
        Serial.println(httpCode);
        if (httpCode > 0) {
            Serial.println("Response: " + http.getString());
        }
    }

    http.end();
    return false;
}

void fetchBlueskyPosts() {
    Serial.println("\n=== Fetching Bluesky posts ===");

    // Authenticate if we don't have a token
    if (accessToken.length() == 0) {
        if (!authenticateBluesky()) {
            Serial.println("ERROR: Authentication failed, cannot fetch posts");
            return;
        }
    }

    HTTPClient http;
    String url = String(BLUESKY_API_URL) + "?q=%23" + String(BLUESKY_SEARCH_TAG) + "&limit=10&sort=latest";

    Serial.print("URL: ");
    Serial.println(url);

    http.begin(url);
    http.setTimeout(10000);
    http.addHeader("User-Agent", "SeaGLBadge/1.0");
    http.addHeader("Authorization", "Bearer " + accessToken);
    Serial.println("Sending authenticated HTTP GET request...");
    int httpCode = http.GET();

    Serial.print("HTTP Code: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("Response length: ");
        Serial.println(payload.length());

        DynamicJsonDocument doc(20480);  // 20KB for search results
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            JsonArray postsArray = doc["posts"].as<JsonArray>();
            Serial.print("Posts array size: ");
            Serial.println(postsArray.size());

            // Create temporary arrays to store all posts with timestamps
            struct PostData {
                String author;
                String text;
                String timestamp;
            };
            PostData tempPosts[10];
            int tempCount = 0;

            // Collect posts, EXCLUDING @seagl.org
            for (JsonObject post : postsArray) {
                if (tempCount >= 10) break;

                String handle = post["author"]["handle"].as<String>();
                String text = post["record"]["text"].as<String>();
                String createdAt = post["record"]["createdAt"].as<String>();

                // Skip @seagl.org posts (official account)
                if (handle == "seagl.org") {
                    continue;
                }

                if (handle.length() > 0 && text.length() > 0 && createdAt.length() > 0) {
                    tempPosts[tempCount].author = "@" + handle;
                    tempPosts[tempCount].text = text;
                    tempPosts[tempCount].timestamp = createdAt;
                    tempCount++;
                }
            }

            // Sort posts by timestamp (descending - newest first)
            for (int i = 0; i < tempCount - 1; i++) {
                for (int j = i + 1; j < tempCount; j++) {
                    if (tempPosts[j].timestamp > tempPosts[i].timestamp) {
                        PostData temp = tempPosts[i];
                        tempPosts[i] = tempPosts[j];
                        tempPosts[j] = temp;
                    }
                }
            }

            // Take the 3 most recent posts
            postCount = 0;
            for (int i = 0; i < tempCount && i < 3; i++) {
                blueskyAuthors[postCount] = tempPosts[i].author;
                blueskyPosts[postCount] = tempPosts[i].text;
                postCount++;
            }
            Serial.print("SUCCESS: Fetched ");
            Serial.print(postCount);
            Serial.println(" community posts");
        } else {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("HTTP request failed with code: ");
        Serial.println(httpCode);

        // If we got 401 (Unauthorized), token might be expired
        if (httpCode == 401) {
            Serial.println("Token expired, clearing for re-authentication...");
            accessToken = "";
        }
    }
    http.end();
    Serial.println("=== Fetch complete ===\n");
}

void showScreen(int screen) {
    Serial.print("Showing screen: ");
    Serial.println(screen);

    // Clear screen
    if (screen_obj != NULL) {
        lv_obj_del(screen_obj);
        screen_obj = NULL;
    }

    screen_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen_obj, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(screen_obj);
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);

    switch (screen) {
        case 0: // Welcome
            lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x1E3A8A), 0);

            // "Howdy" - large (same size as Trevor)
            main_label = lv_label_create(screen_obj);
            lv_label_set_text(main_label, "Howdy");
            lv_obj_set_style_text_color(main_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(main_label, &lv_font_montserrat_48, 0);
            lv_obj_align(main_label, LV_ALIGN_CENTER, 0, -20);

            // "My Name Is" - smaller below
            sub_label = lv_label_create(screen_obj);
            lv_label_set_text(sub_label, "My Name Is");
            lv_obj_set_style_text_color(sub_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_28, 0);
            lv_obj_align(sub_label, LV_ALIGN_CENTER, 0, 30);
            break;

        case 1: // Name
            lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x059669), 0);

            // "Trevor" - large
            main_label = lv_label_create(screen_obj);
            lv_label_set_text(main_label, "Trevor");
            lv_obj_set_style_text_color(main_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(main_label, &lv_font_montserrat_48, 0);
            lv_obj_align(main_label, LV_ALIGN_CENTER, 0, -15);

            // "Johnson" - smaller below, letter-spaced to match width
            sub_label = lv_label_create(screen_obj);
            lv_label_set_text(sub_label, "Johnson");
            lv_obj_set_style_text_color(sub_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_letter_space(sub_label, 8, 0);  // Spread out to match Trevor width
            lv_obj_align(sub_label, LV_ALIGN_CENTER, 0, 30);
            break;

        case 2: // QR Code with "Find Me Here" text
            {
                lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x1E3A8A), 0);  // Same blue as Howdy screen

                // "Find Me Here" text on left side
                main_label = lv_label_create(screen_obj);
                lv_label_set_text(main_label, "Find\nMe\nHere");
                lv_obj_set_style_text_color(main_label, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_text_font(main_label, &lv_font_montserrat_32, 0);
                lv_obj_set_style_text_align(main_label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_align(main_label, LV_ALIGN_LEFT_MID, 20, 0);

                // Create QR code using canvas
                QRCode qrcode;
                uint8_t qrcodeData[qrcode_getBufferSize(3)];
                qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, BADGE_PROFILE_URL);

                // Create canvas for QR code
                static lv_color_t cbuf[120 * 120];
                lv_obj_t * canvas = lv_canvas_create(screen_obj);
                lv_canvas_set_buffer(canvas, cbuf, 120, 120, LV_IMG_CF_TRUE_COLOR);
                lv_canvas_fill_bg(canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);

                // Draw QR code on canvas
                int scale = 120 / qrcode.size;
                if (scale < 3) scale = 3;
                if (scale > 5) scale = 5;

                for (uint8_t y = 0; y < qrcode.size; y++) {
                    for (uint8_t x = 0; x < qrcode.size; x++) {
                        if (qrcode_getModule(&qrcode, x, y)) {
                            lv_canvas_set_px(canvas, x * scale, y * scale, lv_color_hex(0x000000));
                            for (int dy = 0; dy < scale; dy++) {
                                for (int dx = 0; dx < scale; dx++) {
                                    lv_canvas_set_px(canvas, x * scale + dx, y * scale + dy, lv_color_hex(0x000000));
                                }
                            }
                        }
                    }
                }

                lv_obj_align(canvas, LV_ALIGN_RIGHT_MID, -20, 0);  // Position on right side
            }
            break;

        case 3: // Bluesky Post 1
        case 4: // Bluesky Post 2
        case 5: // Bluesky Post 3
            {
                int postIndex = screen - 3;
                lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0xDC2626), 0);  // Same red as CTA screen

                if (postIndex < postCount) {
                    // Author handle - Montserrat 22
                    main_label = lv_label_create(screen_obj);
                    lv_label_set_text(main_label, blueskyAuthors[postIndex].c_str());
                    lv_obj_set_style_text_color(main_label, lv_color_hex(0x60A5FA), 0);
                    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_22, 0);
                    lv_obj_align(main_label, LV_ALIGN_TOP_LEFT, 0, 0);  // Move all the way to top-left corner

                    // Post text - Montserrat 22
                    sub_label = lv_label_create(screen_obj);
                    lv_label_set_text(sub_label, blueskyPosts[postIndex].c_str());
                    lv_obj_set_width(sub_label, SCREEN_WIDTH - 2);  // Use almost full width
                    lv_label_set_long_mode(sub_label, LV_LABEL_LONG_WRAP);
                    lv_obj_set_style_text_color(sub_label, lv_color_hex(0xFFFFFF), 0);
                    lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_22, 0);
                    lv_obj_set_style_text_line_space(sub_label, 3, 0);
                    lv_obj_align(sub_label, LV_ALIGN_TOP_LEFT, 0, 22);  // Tighter spacing - closer to username
                } else {
                    main_label = lv_label_create(screen_obj);
                    lv_label_set_text(main_label, "No posts yet!\nBe the first to post\n#seagl2025\non Bluesky");
                    lv_obj_set_style_text_color(main_label, lv_color_hex(0xFFFFFF), 0);
                    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_20, 0);
                    lv_obj_set_style_text_align(main_label, LV_TEXT_ALIGN_CENTER, 0);
                    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 0);
                }
            }
            break;

        case 6: // CTA
            lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0xDC2626), 0);
            main_label = lv_label_create(screen_obj);
            lv_label_set_text(main_label, "#SeaGL2025\non BlueSky");
            lv_obj_set_style_text_color(main_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(main_label, &lv_font_montserrat_32, 0);
            lv_obj_set_style_text_align(main_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(main_label, LV_ALIGN_CENTER, 0, -20);

            sub_label = lv_label_create(screen_obj);
            lv_label_set_text(sub_label, "Join the conversation!");
            lv_obj_set_style_text_color(sub_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_align(sub_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(sub_label, LV_ALIGN_CENTER, 0, 40);
            break;
    }
}

void screenTimerCallback(lv_timer_t * timer) {
    // Advance to next screen
    currentScreen = (currentScreen + 1) % 7;
    Serial.print(">>> TIMER: Switching to screen ");
    Serial.println(currentScreen);

    // Show the next screen
    showScreen(currentScreen);

    // Update timer period for next screen
    lv_timer_set_period(timer, screenTimes[currentScreen]);
}

void loop() {
    // Update LVGL tick - CRITICAL for timers to work!
    static unsigned long lastTick = 0;
    unsigned long now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;

    // Check for button presses to adjust brightness
    checkButtons();

    // Let LVGL handle everything including timers
    lv_timer_handler();

    // Refresh Bluesky posts periodically
    static unsigned long lastAPICheck = 0;
    if (WiFi.status() == WL_CONNECTED && (now - lastAPICheck > API_REFRESH_INTERVAL)) {
        fetchBlueskyPosts();
        lastAPICheck = now;
    }

    delay(5);
}

// Set backlight brightness using PWM
void setBrightness(int index) {
    if (index < 0 || index >= 4) return;
    ledcWrite(0, brightnessLevels[index]);
    currentBrightnessIndex = index;
}

// Check for button presses and cycle brightness
void checkButtons() {
    unsigned long now = millis();

    // Debounce check
    if (now - lastButtonPress < buttonDebounce) {
        return;
    }

    // Check if either button is pressed (active LOW with pullup)
    bool button1Pressed = digitalRead(PIN_BUTTON_1) == LOW;
    bool button2Pressed = digitalRead(PIN_BUTTON_2) == LOW;

    if (button1Pressed || button2Pressed) {
        lastButtonPress = now;

        // Cycle to next brightness level
        currentBrightnessIndex = (currentBrightnessIndex + 1) % 4;
        setBrightness(currentBrightnessIndex);

        Serial.print("Brightness changed to: ");
        Serial.println(brightnessLabels[currentBrightnessIndex]);
    }
}
