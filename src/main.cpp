/*
 * Author: Varad Chaskar
 * Description: This code interfaces with a TFT display and a fingerprint sensor to create a user interface 
 *               for scanning and enrolling fingerprints. It uses the LVGL library for graphical interface and 
 *               handles touch input and display updates. The fingerprint sensor is used for detecting and identifying 
 *               fingerprints. The system includes buttons for scanning and enrolling fingerprints, and provides 
 *               feedback on the TFT display.
 */

#include <Arduino.h>                // Arduino core library for basic functions
#include <FS.h>                     // File system library for SPIFFS operations
#include <SPI.h>                    // SPI library for interfacing with SPI devices
#include <lvgl.h>                   // LVGL library for graphical user interface
#include <TFT_eSPI.h>               // TFT_eSPI library for handling TFT display operations
#include <Adafruit_Fingerprint.h>   // Adafruit Fingerprint library for fingerprint sensor operations

// Pins for Fingerprint Sensor and LVGL Display
#define RX_PIN     25               // RX pin for fingerprint sensor
#define TX_PIN     33               // TX pin for fingerprint sensor
#define TOUCH_CS   21               // Chip select pin for touch controller

// TFT and Fingerprint configurations
TFT_eSPI tft = TFT_eSPI();          // Create instance of TFT_eSPI for TFT operations
HardwareSerial mySerial(2);        // Create instance of HardwareSerial for fingerprint sensor communication on Serial2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial); // Create instance of Adafruit_Fingerprint with Serial2

// Screen resolution
static const uint32_t screenWidth  = 320;  // Define screen width for the TFT display
static const uint32_t screenHeight = 240;  // Define screen height for the TFT display

static lv_disp_draw_buf_t draw_buf;       // LVGL display buffer structure
static lv_color_t buf[screenWidth * 10];  // Buffer to hold pixel data

// Global labels and buttons
lv_obj_t *fingerLabel;    // Label for displaying fingerprint scan results
lv_obj_t *scanButton;     // Button for initiating fingerprint scan
lv_obj_t *enrollButton;   // Button for enrolling new fingerprints

// Flag for scanning mode
bool scanningMode = false; // Boolean flag to indicate whether the system is in scanning mode

/* Touch calibration function */
void touch_calibrate() {
    uint16_t calData[5];     // Array to store touch calibration data
    uint8_t calDataOK = 0;   // Flag to indicate if calibration data is valid

    // Initialize the SPIFFS file system
    if (!SPIFFS.begin()) {
        Serial.println("Formatting file system"); // Notify user that file system is being formatted
        SPIFFS.format(); // Format the file system
        SPIFFS.begin(); // Re-initialize the file system
    }

    // Check if touch calibration data file exists
    if (SPIFFS.exists("/TouchCalData3")) {
        File f = SPIFFS.open("/TouchCalData3", "r"); // Open the calibration data file for reading
        if (f) {
            // Read 14 bytes of data from the file
            if (f.readBytes((char *)calData, 14) == 14) {
                calDataOK = 1; // Check if 14 bytes were read successfully
            }
            f.close(); // Close the file
        }
    }

    // If valid calibration data exists, set the touch calibration data
    if (calDataOK) {
        tft.setTouch(calData); // Apply calibration data to the TFT display
    } 
    else {
        // No valid calibration data, perform calibration
        tft.fillScreen(TFT_BLACK); // Fill the screen with black color
        tft.setCursor(20, 0); // Set cursor position
        tft.setTextFont(2); // Set text font
        tft.setTextSize(1); // Set text size
        tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color (white on black background)
        tft.println("Touch corners as indicated"); // Prompt user to touch screen corners for calibration

        // Perform calibration and store data
        tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15); // Calibrate touch with magenta and black colors
        File f = SPIFFS.open("/TouchCalData3", "w"); // Open the calibration data file for writing
        if (f) {
            f.write((const unsigned char *)calData, 14); // Write calibration data to the file
            f.close(); // Close the file
        }
    }
}

/* Display flushing function for LVGL */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1); // Calculate width of the area to be updated
    uint32_t h = (area->y2 - area->y1 + 1); // Calculate height of the area to be updated

    tft.startWrite(); // Begin writing to the TFT display
    tft.setAddrWindow(area->x1, area->y1, w, h); // Set the address window for updating
    tft.pushColors((uint16_t *)&color_p->full, w * h, true); // Push color data to the display
    tft.endWrite(); // End writing to the TFT display

    lv_disp_flush_ready(disp); // Notify LVGL that the display flushing is complete
}

/* Touchpad input handler for LVGL */
void lvgl_port_tp_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    uint16_t touchX, touchY; // Variables to store touch coordinates
    bool touched = tft.getTouch(&touchX, &touchY); // Get touch input from the TFT display

    // Set the touch input state and coordinates
    if (!touched) {
        data->state = LV_INDEV_STATE_REL; // No touch detected
    } 
    else {
        data->state = LV_INDEV_STATE_PR; // Touch detected
        data->point.x = touchX; // Set X coordinate of touch
        data->point.y = touchY; // Set Y coordinate of touch
    }
}

// Function to handle fingerprint detection and matching
uint8_t getFingerprintID() {
    uint8_t p = finger.getImage(); // Capture fingerprint image

    // No finger detected
    if (p == FINGERPRINT_NOFINGER) {
        return FINGERPRINT_NOFINGER;
    }

    // Check if the image can be converted to features
    if (p != FINGERPRINT_OK) {
        return p;
    }

    p = finger.image2Tz(); // Convert image to template

    // Check if template conversion was successful
    if (p != FINGERPRINT_OK) {
        return p;
    }

    p = finger.fingerSearch(); // Search for fingerprint in database

    // Check if fingerprint was found
    if (p != FINGERPRINT_OK) {
        return p;
    }

    return finger.fingerID; // Return matched fingerprint ID
}

// Fingerprint scanning logic
void scanFingerprint() {
    uint8_t fingerprintID = getFingerprintID(); // Get fingerprint ID
    switch (fingerprintID) {
        case FINGERPRINT_NOFINGER:
            lv_label_set_text(fingerLabel, "No Finger Detected"); // Update label text
            Serial.println("No Finger Detected"); // Print message to serial monitor
            break;

        case FINGERPRINT_NOTFOUND:
            lv_label_set_text(fingerLabel, "No Match Found"); // Update label text
            Serial.println("No Match Found"); // Print message to serial monitor
            break;

        default:
            if (fingerprintID >= 0) {
                String msg = "Fingerprint ID: " + String(fingerprintID); // Create message with fingerprint ID
                lv_label_set_text(fingerLabel, msg.c_str()); // Update label text
                Serial.println(msg); // Print message to serial monitor
            }
            break;
    }
}

// Event handler for the Scan/Return button
void scan_button_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e); // Get event code

    if (code == LV_EVENT_CLICKED) {
        if (!scanningMode) {
            scanningMode = true; // Set scanning mode to true
            lv_label_set_text(fingerLabel, "Scanning..."); // Update label text
            lv_label_set_text(lv_obj_get_child(scanButton, NULL), "Return"); // Update button label

            // Center the Return button
            lv_obj_align(scanButton, LV_ALIGN_CENTER, 0, 40); // Align button in the center
            
            // Hide the Enroll button
            lv_obj_add_flag(enrollButton, LV_OBJ_FLAG_HIDDEN); // Hide Enroll button
        } 
        else {
            scanningMode = false; // Set scanning mode to false
            lv_label_set_text(fingerLabel, "Returning to main menu..."); // Update label text
            lv_label_set_text(lv_obj_get_child(scanButton, NULL), "Scan"); // Update button label

            // Restore the Scan button to its original position
            lv_obj_align(scanButton, LV_ALIGN_CENTER, -80, 40); // Align button to original position
            
            // Show the Enroll button again
            lv_obj_clear_flag(enrollButton, LV_OBJ_FLAG_HIDDEN); // Show Enroll button
        }
    }
}

// Event handler for the Enroll button
void enroll_button_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e); // Get event code

    if (code == LV_EVENT_CLICKED) {
        // TODO: Add enroll logic here
        Serial.println("Enroll button clicked"); // Print message to serial monitor
    }
}

/* Setup function: Initializes the display, touch, and fingerprint sensor */
void setup() {
    Serial.begin(115200);  // Start serial communication
    mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN); // Initialize serial communication with fingerprint sensor

    tft.begin(); // Initialize TFT display
    tft.setRotation(3); // Set display rotation
    tft.fillScreen(TFT_BLACK); // Fill screen with black color

    // Initialize LVGL
    lv_init(); // Initialize LVGL library
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10); // Initialize LVGL draw buffer
    lv_disp_drv_t disp_drv; // LVGL display driver
    lv_indev_drv_t indev_drv; // LVGL input device driver

    // Configure display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth; // Set horizontal resolution
    disp_drv.ver_res = screenHeight; // Set vertical resolution
    disp_drv.flush_cb = my_disp_flush; // Set flush callback function
    disp_drv.draw_buf = &draw_buf; // Set draw buffer
    lv_disp_drv_register(&disp_drv); // Register display driver

    // Configure touch driver
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER; // Set input device type to pointer
    indev_drv.read_cb = lvgl_port_tp_read; // Set touch input read callback function
    lv_indev_drv_register(&indev_drv); // Register input device driver

    // Initialize fingerprint sensor
    if (finger.verifyPassword()) {
        Serial.println("Found fingerprint sensor"); // Notify if fingerprint sensor is found
    } 
    else {
        Serial.println("Did not find fingerprint sensor :("); // Notify if fingerprint sensor is not found
        while (1); // Stop execution
    }

    // Initialize touch calibration
    touch_calibrate(); // Call touch calibration function

    // Create LVGL objects
    fingerLabel = lv_label_create(lv_scr_act()); // Create label for fingerprint results
    lv_obj_set_size(fingerLabel, LV_SIZE_CONTENT, LV_SIZE_CONTENT); // Set size of the label
    lv_obj_align(fingerLabel, LV_ALIGN_CENTER, 0, -40); // Align label in the center

    scanButton = lv_btn_create(lv_scr_act()); // Create button for scanning fingerprints
    lv_obj_set_size(scanButton, 120, 50); // Set button size
    lv_obj_align(scanButton, LV_ALIGN_CENTER, -80, 40); // Align button
    lv_obj_add_event_cb(scanButton, scan_button_event_handler, LV_EVENT_ALL, NULL); // Add event handler for button
    lv_obj_t *scanLabel = lv_label_create(scanButton); // Create label for Scan button
    lv_label_set_text(scanLabel, "Scan"); // Set button label text

    enrollButton = lv_btn_create(lv_scr_act()); // Create button for enrolling fingerprints
    lv_obj_set_size(enrollButton, 120, 50); // Set button size
    lv_obj_align(enrollButton, LV_ALIGN_CENTER, 80, 40); // Align button
    lv_obj_add_event_cb(enrollButton, enroll_button_event_handler, LV_EVENT_ALL, NULL); // Add event handler for button
    lv_obj_t *enrollLabel = lv_label_create(enrollButton); // Create label for Enroll button
    lv_label_set_text(enrollLabel, "Enroll"); // Set button label text
}

/* Loop function: Continuously updates the LVGL task handler */
void loop() {
    lv_task_handler(); // Handle LVGL tasks
    delay(5); // Add delay for stability
}
