# SeaGL 2025 Conference Badge (Unofficial)

An interactive digital conference badge for the LilyGO T-Display S3R8 (ESP32-S3) that displays your name, QR code, and live community posts from Bluesky using the #seagl2025 hashtag.

Additional project details and 3d printed case available at [Hackaday.io](https://hackaday.io/project/204270-seagl-2025-badge-unofficial).

## Features

- **Personal Welcome Screens**: Displays "Howdy" and your name with custom styling
- **QR Code with Label**: Shows "Find Me Here" with a scannable QR code to your profile
- **Live Community Feed**: Displays the 3 most recent #seagl2025 posts from Bluesky (excludes official @seagl.org posts to show community content)
- **Call-to-Action**: Encourages attendees to join the conversation on Bluesky
- **Auto-refresh**: Updates posts every 3 minutes
- **WiFi Connectivity**: Connects automatically to configured network
- **Adjustable Brightness**: Press either button to cycle through 4 brightness levels (10%, 40%, 70%, 100%)

## Hardware Requirements

- **LilyGO T-Display S3R8**
  - ESP32-S3R8 microcontroller with 8MB PSRAM
  - ST7789 LCD display (320x170 pixels)
  - 8-bit parallel I80 interface
  - Built-in WiFi

## Software Requirements

- [PlatformIO](https://platformio.org/) (recommended)
- [Visual Studio Code](https://code.visualstudio.com/) (if using PlatformIO)

## Installation

### 1. Clone the Repository

```bash
git clone <repository-url>
cd "seagl badge"
```

### 2. Install PlatformIO

- Install [VS Code](https://code.visualstudio.com/)
- Install the PlatformIO IDE extension from the VS Code marketplace

### 3. Configure Your Badge

**Copy the example config file:**

```bash
cp include/config.h.example include/config.h
```

**Edit `include/config.h`** with your information:

```cpp
// WiFi Configuration
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

// Personal Information
#define BADGE_NAME "Your Name"
#define BADGE_HANDLE "@yourhandle.bsky.social"
#define BADGE_PROFILE_URL "https://yourwebsite.com"

// Bluesky API Configuration
#define BLUESKY_IDENTIFIER "yourhandle.bsky.social"  // Without the @
#define BLUESKY_APP_PASSWORD "your-app-password"  // Create at https://bsky.app/settings/app-passwords
```

**To get a Bluesky App Password:**
1. Go to [https://bsky.app/settings/app-passwords](https://bsky.app/settings/app-passwords)
2. Click "Add App Password"
3. Give it a name (e.g., "SeaGL Badge")
4. Copy the generated password into your config.h

### 4. Build and Upload

```bash
pio run --target upload
```

Or use the PlatformIO buttons in VS Code:
- Click the checkmark ✓ to build
- Click the arrow → to upload

### 5. Monitor Serial Output (Optional)

```bash
pio device monitor
```

Press `Ctrl+C` to exit the monitor.

## Project Structure

```
seagl badge/
├── include/
│   ├── config.h.example     # Template for your configuration
│   ├── config.h            # Your personal config (DO NOT commit!)
│   └── lv_conf.h           # LVGL configuration
├── src/
│   └── main.cpp            # Main program
├── platformio.ini          # PlatformIO configuration
├── .gitignore             # Git ignore file
└── README.md              # This file
```

## Display Sequence

The badge cycles through 7 screens, each shown for 10 seconds:

1. **Welcome Screen**: "Howdy" with "My Name Is" below
2. **Name Screen**: Your first and last name
3. **QR Code Screen**: "Find Me Here" text with QR code linking to your profile
4. **Community Post 1**: Most recent #seagl2025 post from the community
5. **Community Post 2**: Second most recent community post
6. **Community Post 3**: Third most recent community post
7. **Call-to-Action**: "#SeaGL2025 on BlueSky" - encouraging participation

## Configuration Options

All settings are in `include/config.h`:

```cpp
// WiFi Settings
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"

// Personal Information
#define BADGE_NAME "Your Name"                    // Displayed on name screen
#define BADGE_HANDLE "@yourhandle.bsky.social"    // Your Bluesky handle
#define BADGE_PROFILE_URL "https://yoursite.com"  // URL for QR code

// Bluesky API Settings
#define BLUESKY_IDENTIFIER "yourhandle.bsky.social"  // Handle without @
#define BLUESKY_APP_PASSWORD "xxxx-xxxx-xxxx-xxxx"  // App password from Bluesky
#define BLUESKY_SEARCH_TAG "seagl2025"               // Hashtag to search (without #)

// Display Timing
#define SCREEN_DISPLAY_TIME 10000        // Milliseconds per screen (10 seconds)
#define API_REFRESH_INTERVAL 180000      // Post refresh interval (3 minutes)

// Display Settings
#define SCREEN_ROTATION 1                // 0-3 for different orientations
```

## How It Works

### Community Post Filtering

The badge fetches posts with #seagl2025 but **excludes @seagl.org posts** for two key reasons:

1. **Pinned Post Issue**: The Bluesky search API has an issue where pinned posts from accounts interfere with the chronological sorting of search results. The @seagl.org account has pinned posts that were appearing in search results even when newer community posts existed, disrupting the "most recent" post order.

2. **Community Focus**: By filtering out the official account, the badge showcases community conversations and attendee posts, creating a more interactive, peer-to-peer experience at the conference.

This workaround ensures you see the actual most recent community posts rather than having pinned posts from the official account dominate the feed.

### Bluesky API

The badge uses Bluesky's public AT Protocol API:
- Authenticates using your app password
- Searches for posts with the configured hashtag
- Fetches 15 posts per request (optimized for memory and reliability)
- Filters out official account posts
- Displays the 3 most recent community posts
- Refreshes every 3 minutes

#### Technical Notes: Post Limit

The badge is configured to fetch **15 posts** per API request, which provides a good balance between:

1. **Community Post Coverage**: With 15 posts, there's a higher chance of finding 3+ community posts after filtering out @seagl.org posts (which are often promotional/schedule posts)

2. **Memory Constraints**: The ESP32-S3 has limited RAM for processing JSON responses:
   - 15 posts ≈ 33KB response
   - 40KB JSON buffer (provides safe headroom)
   - Uses Arduino String class for HTTP response handling

3. **String Class Limitations**: The ESP32 Arduino String class has practical limitations with large string concatenation operations (>40KB), which prevented using higher limits like 20-25 posts despite having sufficient heap memory

While the ESP32-S3 has ~150KB of free heap memory, the String handling performance becomes unreliable with responses larger than ~35-40KB, causing JSON parsing errors. The 15-post limit with 40KB buffer provides reliable operation while maximizing community post discovery.

### Screen Layout

- **Welcome/Name Screens**: Blue and green backgrounds with large, easy-to-read text
- **QR Code Screen**: Blue background with "Find Me Here" on the left, QR code on the right
- **Post Screens**: Red background with community member posts
- **CTA Screen**: Red background encouraging participation

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password in `config.h`
- ESP32 only supports 2.4GHz WiFi (not 5GHz)
- Check serial monitor for connection status

### No Posts Displaying
- Verify your Bluesky app password is correct
- Check that there are community posts with #seagl2025 (excluding @seagl.org)
- Monitor serial output for API errors
- Ensure WiFi is connected

### Display Issues
- Check that the board is properly selected in `platformio.ini` (lilygo-t-display-s3)
- Verify all libraries are installed (should auto-install with PlatformIO)

### Compilation Errors
- Make sure you've created `include/config.h` from `config.h.example`
- Check that all required fields in config.h are filled in
- Try `pio run --target clean` then rebuild

## Serial Monitor Output

At 115200 baud, you'll see:
- WiFi connection status and IP address
- Bluesky authentication success
- Post fetch results with timestamps
- Current screen being displayed
- Any error messages

## Brightness Control

The badge features adjustable screen brightness to save battery or improve visibility:

- **Buttons**: Press either button (GPIO 0 or GPIO 14) to cycle through brightness levels
- **Brightness Levels**:
  - 10% - Maximum battery life
  - 40% - Balanced for indoor use
  - 70% - Bright for well-lit areas
  - 100% - Maximum visibility (default)
- **How It Works**: Uses PWM (Pulse Width Modulation) to control the backlight LED
- **Debouncing**: 200ms delay prevents accidental multiple presses

The brightness setting persists while the badge is powered on. After a reset, it defaults to 100%.

## Power Management

Tips for all-day conference use:
- The display auto-updates every 3 minutes
- WiFi stays connected for instant refreshes
- Consider using a portable USB battery pack
- **Lower brightness** to 40% or 10% for significantly longer battery life

For longer battery life:
- Press either button to reduce brightness to 40% or 10%
- Increase `API_REFRESH_INTERVAL` (e.g., 600000 for 10 minutes)
- Increase `SCREEN_DISPLAY_TIME` for slower screen cycling

## Customization

### Change the Hashtag
Edit `BLUESKY_SEARCH_TAG` in config.h to track a different hashtag

### Adjust Colors
In [src/main.cpp](src/main.cpp), search for `lv_color_hex()` to change screen colors:
- `0x1E3A8A` - Blue (Welcome, QR)
- `0x059669` - Green (Name)
- `0xDC2626` - Red (Posts, CTA)

### Modify Screen Timing
Change `SCREEN_DISPLAY_TIME` in config.h (in milliseconds)

### Add Your Own Screens
Extend the switch statement in `showScreen()` function in main.cpp

## Privacy & Security

- **Never commit your `config.h` file** - it contains WiFi passwords and API credentials
- The `.gitignore` file is configured to exclude `config.h`
- Use `config.h.example` as a template for others
- Create a new Bluesky app password specifically for this badge
- You can revoke app passwords anytime at https://bsky.app/settings/app-passwords

## Credits

- **Hardware**: LilyGO T-Display S3R8
- **Libraries**:
  - LVGL 8.4.0 (graphics)
  - ArduinoJson 6.21.5 (JSON parsing)
  - QRCode 0.0.1 (QR code generation)
- **API**: Bluesky AT Protocol
- **Conference**: SeaGL 2025

## License

This project is open source and provided as-is for conference use. Feel free to modify and adapt for your own events!

## Contributing

Found a bug or want to add a feature? Pull requests are welcome!

## Support

- **PlatformIO**: https://docs.platformio.org/
- **LVGL**: https://docs.lvgl.io/
- **LilyGO Hardware**: https://github.com/Xinyuan-LilyGO/T-Display-S3
- **Bluesky AT Protocol**: https://docs.bsky.app/

---

**Enjoy SeaGL 2025! Post with #seagl2025 on Bluesky!**
