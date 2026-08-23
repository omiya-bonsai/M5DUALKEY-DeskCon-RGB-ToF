#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDMouse.h"

#include "M5Unified.h"
#include "M5Chain.h"

#include <Adafruit_NeoPixel.h>

// ============================================================
// Hardware layout
//
// Front:
//   DualKey -> Encoder -> Angle -> RGB -> ToF
//
// DualKey itself is the ESP32-S3 host.
// M5Chain therefore enumerates FOUR devices:
//   Encoder / Angle / RGB / ToF
// ============================================================

// ============================================================
// DualKey buttons
// ============================================================

#define PIN_KEY_LEFT  17
#define PIN_KEY_RIGHT 0

m5::Button_Class KeyLeft;
m5::Button_Class KeyRight;

// ============================================================
// USB HID
// ============================================================

USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
USBHIDMouse Mouse;

// ============================================================
// Chain UART
//
// Confirmed working by minimal diagnostics.
// ============================================================

#define CHAIN_RXD_PIN GPIO_NUM_5
#define CHAIN_TXD_PIN GPIO_NUM_6

Chain M5Chain;

device_list_t *device_list = nullptr;

uint16_t device_count = 0;

uint8_t encoder_id = 0;
uint8_t angle_id   = 0;
uint8_t rgb_id     = 0;
uint8_t tof_id     = 0;

// ============================================================
// Shared Chain status
// ============================================================

uint8_t operation_status = 0;

// ============================================================
// DualKey LEDs
// ============================================================

constexpr uint8_t DUALKEY_LED_POWER_PIN  = 40;
constexpr uint8_t DUALKEY_LED_SIGNAL_PIN = 21;
constexpr uint8_t DUALKEY_LED_COUNT      = 2;

constexpr uint8_t DUALKEY_LEFT_LED_INDEX  = 0;
constexpr uint8_t DUALKEY_RIGHT_LED_INDEX = 1;

Adafruit_NeoPixel DualKeyLeds(
    DUALKEY_LED_COUNT,
    DUALKEY_LED_SIGNAL_PIN,
    NEO_GRB + NEO_KHZ800);

// ============================================================
// Colors
// ============================================================

// ORA4 = red
constexpr uint8_t ORA4_R = 255;
constexpr uint8_t ORA4_G = 40;
constexpr uint8_t ORA4_B = 40;

// Studio Display = yellow
constexpr uint8_t STUDIO_R = 255;
constexpr uint8_t STUDIO_G = 220;
constexpr uint8_t STUDIO_B = 0;

// Encoder / Mute = purple
constexpr uint8_t MUTE_R = 170;
constexpr uint8_t MUTE_G = 40;
constexpr uint8_t MUTE_B = 255;

// Angle = blue
constexpr uint8_t ANGLE_R = 40;
constexpr uint8_t ANGLE_G = 140;
constexpr uint8_t ANGLE_B = 255;

// RGB Matrix feedback colors
constexpr uint8_t MATRIX_R = 30;
constexpr uint8_t MATRIX_G = 220;
constexpr uint8_t MATRIX_B = 180;

constexpr uint8_t MATRIX_WHITE_R = 220;
constexpr uint8_t MATRIX_WHITE_G = 235;
constexpr uint8_t MATRIX_WHITE_B = 255;

// ============================================================
// LED brightness
// ============================================================

// IMPORTANT:
// M5Chain LED brightness uses 0-100.
constexpr uint8_t CHAIN_LED_BRIGHTNESS = 100;

// Chain RGB safety limit. The Matrix hardware/master brightness
// must never exceed 50%. Per-frame RGB values apply a second,
// lower brightness limit for idle and action rendering.
constexpr uint8_t RGB_MATRIX_MAX_BRIGHTNESS = 50;
constexpr uint8_t MATRIX_MASTER_BRIGHTNESS = 50;

static_assert(
    MATRIX_MASTER_BRIGHTNESS <=
        RGB_MATRIX_MAX_BRIGHTNESS,
    "Chain RGB brightness must not exceed 50%");

constexpr float STANDBY_LEVEL = 0.20f;

constexpr float AUDIO_MIN_LEVEL = 0.35f;
constexpr float AUDIO_MAX_LEVEL = 0.80f;

constexpr float MUTE_MIN_LEVEL = 0.45f;
constexpr float MUTE_MAX_LEVEL = 0.85f;

// ============================================================
// Audio state
// ============================================================

enum class AudioOutput
{
  UNKNOWN,
  ORA4,
  STUDIO_DISPLAY
};

AudioOutput currentAudioOutput =
    AudioOutput::UNKNOWN;

bool muted = false;

// ============================================================
// DualKey chord handling
// ============================================================

constexpr uint32_t CHORD_WINDOW_MS = 80;

enum class PendingKey
{
  NONE,
  LEFT,
  RIGHT
};

PendingKey pendingKey =
    PendingKey::NONE;

uint32_t pendingSince = 0;

bool chordConsumed  = false;
bool singleConsumed = false;

// ============================================================
// DualKey action flash
// ============================================================

constexpr uint32_t KEY_FLASH_MS = 80;

bool leftFlashActive  = false;
bool rightFlashActive = false;

uint32_t leftFlashStarted  = 0;
uint32_t rightFlashStarted = 0;

// ============================================================
// Encoder
// ============================================================

constexpr uint32_t ENCODER_VALUE_INTERVAL_MS  = 15;
constexpr uint32_t ENCODER_BUTTON_INTERVAL_MS = 25;

uint32_t lastEncoderValuePoll  = 0;
uint32_t lastEncoderButtonPoll = 0;

int16_t lastEncoderValue = 0;
bool encoderInitialized = false;

uint8_t lastEncoderButton = 0;
bool encoderButtonInitialized = false;

constexpr uint32_t ENCODER_LED_FLASH_MS = 350;

bool encoderLedFlash = false;
uint32_t encoderLedFlashStarted = 0;

// ============================================================
// Angle
// ============================================================

constexpr uint32_t ANGLE_READ_INTERVAL_MS = 20;

uint32_t lastAngleRead = 0;

constexpr uint16_t ANGLE_MIN = 0;
constexpr uint16_t ANGLE_MAX = 4095;

uint16_t angleCenter = 2048;
uint16_t angleValue  = 2048;

constexpr uint16_t ANGLE_STOP_OFFSET  = 95;
constexpr uint16_t ANGLE_START_OFFSET = 135;

constexpr uint32_t SCROLL_SLOWEST_INTERVAL_MS = 220;
constexpr uint32_t SCROLL_FASTEST_INTERVAL_MS = 25;

constexpr float SCROLL_BASE_SPEED = 0.35f;

constexpr int8_t SCROLL_UP_STEP   = -1;
constexpr int8_t SCROLL_DOWN_STEP = 1;

enum class ScrollState
{
  STOPPED,
  UP,
  DOWN
};

ScrollState scrollState =
    ScrollState::STOPPED;

uint32_t lastScrollMs = 0;

float angleActivity = 0.0f;

// ============================================================
// ToF SINGLE
// ============================================================

constexpr uint8_t TOF_MEASUREMENT_TIME_MS = 33;

// Do not hammer the shared UART.
constexpr uint32_t TOF_POLL_INTERVAL_MS = 40;

uint32_t lastTofPoll = 0;

uint16_t tofRawDistance = 0;
uint16_t tofDistance    = 0;

bool tofDistanceValid = false;

uint32_t lastTofSuccess = 0;

constexpr uint32_t TOF_INVALID_TIMEOUT_MS = 500;

// ============================================================
// RGB Matrix
// ============================================================

constexpr uint8_t MATRIX_WIDTH = 8;
constexpr uint8_t MATRIX_PIXELS = 64;

// Limit large RGB buffer transfers.
constexpr uint32_t MATRIX_UPDATE_INTERVAL_MS = 80;

constexpr float MATRIX_AMBIENT_MIN_LEVEL = 0.05f;
constexpr float MATRIX_AMBIENT_MAX_LEVEL = 0.12f;
constexpr float MATRIX_ACTION_LEVEL = 0.30f;
constexpr float MATRIX_ACTION_PEAK_LEVEL = 0.50f;

// Valid, smoothed ToF distance gently shapes the ambient
// animation. It never starts an action or emits HID input.
constexpr uint16_t TOF_AMBIENT_NEAR_MM = 100;
constexpr uint16_t TOF_AMBIENT_FAR_MM = 500;
constexpr uint8_t TOF_AMBIENT_PROXIMITY_STEPS = 16;
constexpr float TOF_AMBIENT_PROXIMITY_SMOOTHING = 0.16f;
constexpr float TOF_AMBIENT_SPEED_BOOST = 0.60f;
constexpr float TOF_AMBIENT_BRIGHTNESS_BOOST = 0.08f;
constexpr float TOF_AMBIENT_SPREAD_BOOST = 0.35f;
constexpr float TOF_AMBIENT_SHIMMER_MAX = 0.12f;

static_assert(
    MATRIX_ACTION_PEAK_LEVEL <= 0.50f,
    "Chain RGB pixel level must not exceed 50%");

static_assert(
    TOF_AMBIENT_FAR_MM >
        TOF_AMBIENT_NEAR_MM,
    "ToF ambient distance range must be positive");

static_assert(
    (MATRIX_AMBIENT_MAX_LEVEL +
     TOF_AMBIENT_BRIGHTNESS_BOOST) *
            (1.0f +
             TOF_AMBIENT_SHIMMER_MAX) <=
        0.50f,
    "ToF ambient pixel level must not exceed 50%");

enum class MatrixAnimation
{
  IDLE,
  ORA4_RIPPLE,
  STUDIO_RIPPLE,
  CHORD_FLASH,
  VOLUME_EXPAND,
  VOLUME_CONTRACT,
  MUTE_PULSE,
  SCROLL_UP,
  SCROLL_DOWN
};

MatrixAnimation matrixAnimation =
    MatrixAnimation::IDLE;

uint32_t matrixAnimationStarted = 0;
uint32_t matrixAnimationDuration = 0;

uint32_t lastMatrixUpdate = 0;

uint16_t previousMatrix[MATRIX_PIXELS] = {0};

bool matrixInitialized = false;

float matrixProximity = 0.0f;
float matrixAmbientTime = 0.0f;

uint32_t lastMatrixAmbientUpdate = 0;

void startMatrixAnimation(
    MatrixAnimation animation,
    uint32_t now);

// ============================================================
// LED update
// ============================================================

constexpr uint32_t LED_UPDATE_INTERVAL_MS = 40;

uint32_t lastLedUpdate = 0;

uint8_t previousEncoderRgb[3] = {0, 0, 0};
uint8_t previousAngleRgb[3]   = {0, 0, 0};

bool encoderRgbInitialized = false;
bool angleRgbInitialized   = false;

// ============================================================
// Utility
// ============================================================

float clamp01(float value)
{
  if (value < 0.0f)
    return 0.0f;

  if (value > 1.0f)
    return 1.0f;

  return value;
}

float gammaCorrect(float value)
{
  return powf(
      clamp01(value),
      2.2f);
}

void scaleRgb(
    uint8_t baseR,
    uint8_t baseG,
    uint8_t baseB,
    float level,
    uint8_t &r,
    uint8_t &g,
    uint8_t &b)
{
  const float corrected =
      gammaCorrect(level);

  r = (uint8_t)(baseR * corrected);
  g = (uint8_t)(baseG * corrected);
  b = (uint8_t)(baseB * corrected);
}

uint16_t rgb565(
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
  return
      ((uint16_t)(r & 0xF8) << 8) |
      ((uint16_t)(g & 0xFC) << 3) |
      ((uint16_t)b >> 3);
}

// ============================================================
// Breathing
// ============================================================

float breathingWave(uint32_t now)
{
  const float t =
      now / 1000.0f;

  const float mainWave =
      0.5f +
      0.5f *
          sinf(
              2.0f * PI * t / 3.2f);

  const float slow1 =
      sinf(
          2.0f * PI * t / 7.1f +
          0.8f);

  const float slow2 =
      sinf(
          2.0f * PI * t / 13.7f +
          2.1f);

  float result =
      mainWave +
      0.18f *
          (0.65f * slow1 +
           0.35f * slow2);

  return clamp01(result);
}

// ============================================================
// Chain RGB single LED
// ============================================================

bool setChainLed(
    uint8_t id,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t previous[3],
    bool &initialized)
{
  if (id == 0)
    return false;

  if (initialized &&
      previous[0] == r &&
      previous[1] == g &&
      previous[2] == b)
  {
    return true;
  }

  uint8_t rgb[3] = {
      r,
      g,
      b};

  uint8_t op = 0;

  const chain_status_t status =
      M5Chain.setRGBValue(
          id,
          0,
          1,
          rgb,
          3,
          &op);

  if (status != CHAIN_OK ||
      !op)
  {
    return false;
  }

  previous[0] = r;
  previous[1] = g;
  previous[2] = b;

  initialized = true;

  return true;
}

// ============================================================
// HID
// ============================================================

void sendOra4()
{
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('1');

  delay(15);

  Keyboard.releaseAll();
}

void sendStudioDisplay()
{
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('2');

  delay(15);

  Keyboard.releaseAll();
}

void sendDualKeyChord()
{
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('e');

  delay(15);

  Keyboard.releaseAll();
}

void volumeUp()
{
  ConsumerControl.press(
      CONSUMER_CONTROL_VOLUME_INCREMENT);

  delay(4);

  ConsumerControl.release();
}

void volumeDown()
{
  ConsumerControl.press(
      CONSUMER_CONTROL_VOLUME_DECREMENT);

  delay(4);

  ConsumerControl.release();
}

void sendMute()
{
  ConsumerControl.press(
      CONSUMER_CONTROL_MUTE);

  delay(4);

  ConsumerControl.release();
}

// ============================================================
// Chain enumeration
// ============================================================

bool enumerateChain()
{
  Serial.println();
  Serial.println(
      "========== CHAIN ENUMERATION ==========");

  M5Chain.begin(
      &Serial2,
      115200,
      CHAIN_RXD_PIN,
      CHAIN_TXD_PIN);

  Serial.printf(
      "[Chain] UART RX=%d TX=%d\n",
      CHAIN_RXD_PIN,
      CHAIN_TXD_PIN);

  if (!M5Chain.isDeviceConnected())
  {
    Serial.println(
        "[Chain] ERROR: no device");

    return false;
  }

  const chain_status_t countStatus =
      M5Chain.getDeviceNum(
          &device_count);

  Serial.printf(
      "[Chain] count status=0x%02X count=%u\n",
      (unsigned int)countStatus,
      device_count);

  if (countStatus != CHAIN_OK ||
      device_count == 0)
  {
    return false;
  }

  device_list =
      (device_list_t *)malloc(
          sizeof(device_list_t));

  if (!device_list)
    return false;

  device_list->count =
      device_count;

  device_list->devices =
      (device_info_t *)malloc(
          sizeof(device_info_t) *
          device_count);

  if (!device_list->devices)
    return false;

  if (!M5Chain.getDeviceList(
          device_list))
  {
    Serial.println(
        "[Chain] ERROR: getDeviceList");

    return false;
  }

  for (uint8_t i = 0;
       i < device_list->count;
       ++i)
  {
    const uint8_t id =
        device_list->devices[i].id;

    const uint16_t type =
        device_list->devices[i].device_type;

    Serial.printf(
        "[Chain] index=%u id=%u type=0x%04X",
        i,
        id,
        type);

    if (type ==
        CHAIN_ENCODER_TYPE_CODE)
    {
      encoder_id = id;
      Serial.print(" ENCODER");
    }
    else if (type ==
             CHAIN_ANGLE_TYPE_CODE)
    {
      angle_id = id;
      Serial.print(" ANGLE");
    }
    else if (type ==
             CHAIN_RGB_TYPE_CODE)
    {
      rgb_id = id;
      Serial.print(" RGB");
    }
    else if (type ==
             CHAIN_TOF_TYPE_CODE)
    {
      tof_id = id;
      Serial.print(" TOF");
    }

    Serial.println();
  }

  Serial.printf(
      "[Chain] encoder=%u angle=%u rgb=%u tof=%u\n",
      encoder_id,
      angle_id,
      rgb_id,
      tof_id);

  return
      encoder_id != 0 &&
      angle_id != 0 &&
      rgb_id != 0 &&
      tof_id != 0;
}

// ============================================================
// Encoder initialization
// ============================================================

void initEncoder()
{
  if (!encoder_id)
    return;

  uint8_t op = 0;

  chain_status_t status =
      M5Chain.setEncoderABDirect(
          encoder_id,
          ENCODER_AB,
          &op);

  Serial.printf(
      "[Encoder] direction status=0x%02X op=%u\n",
      (unsigned int)status,
      op);

  op = 0;

  status =
      M5Chain.setEncoderButtonTriggerInterval(
          encoder_id,
          BUTTON_DOUBLE_CLICK_TIME_500MS,
          BUTTON_LONG_PRESS_TIME_5S,
          &op);

  Serial.printf(
      "[Encoder] button status=0x%02X op=%u\n",
      (unsigned int)status,
      op);
}

// ============================================================
// Chain LED initialization
// ============================================================

void initChainLeds()
{
  uint8_t op = 0;

  if (encoder_id)
  {
    op = 0;

    const chain_status_t status =
        M5Chain.setRGBLight(
            encoder_id,
            CHAIN_LED_BRIGHTNESS,
            &op);

    Serial.printf(
        "[Encoder LED] brightness status=0x%02X op=%u\n",
        (unsigned int)status,
        op);
  }

  if (angle_id)
  {
    op = 0;

    const chain_status_t status =
        M5Chain.setRGBLight(
            angle_id,
            CHAIN_LED_BRIGHTNESS,
            &op);

    Serial.printf(
        "[Angle LED] brightness status=0x%02X op=%u\n",
        (unsigned int)status,
        op);
  }

  if (rgb_id)
  {
    op = 0;

    chain_status_t status =
        M5Chain.setRGBMode(
            rgb_id,
            RGB_PIXEL_MODE,
            &op);

    Serial.printf(
        "[Matrix] mode status=0x%02X op=%u\n",
        (unsigned int)status,
        op);

    op = 0;

    status =
        M5Chain.setRGBBrightness(
            rgb_id,
            MATRIX_MASTER_BRIGHTNESS,
            &op);

    Serial.printf(
        "[Matrix] brightness status=0x%02X op=%u\n",
        (unsigned int)status,
        op);

    op = 0;

    status =
        M5Chain.setRGBClear(
            rgb_id,
            &op);

    Serial.printf(
        "[Matrix] clear status=0x%02X op=%u\n",
        (unsigned int)status,
        op);
  }
}

// ============================================================
// ToF initialization
//
// EXACT strategy proven in the diagnostic:
// SINGLE -> 33 ms -> START
// ============================================================

bool initTof()
{
  if (!tof_id)
    return false;

  Serial.println();
  Serial.println(
      "========== TOF SINGLE INIT ==========");

  uint8_t op = 0;

  chain_status_t status =
      M5Chain.setToFMeasureMode(
          tof_id,
          CHAIN_TOF_MODE_SINGLE,
          &op);

  Serial.printf(
      "[ToF] SINGLE status=0x%02X op=%u\n",
      (unsigned int)status,
      op);

  if (status != CHAIN_OK ||
      !op)
  {
    return false;
  }

  op = 0;

  status =
      M5Chain.setToFMeasureTime(
          tof_id,
          TOF_MEASUREMENT_TIME_MS,
          &op);

  Serial.printf(
      "[ToF] time=%u status=0x%02X op=%u\n",
      TOF_MEASUREMENT_TIME_MS,
      (unsigned int)status,
      op);

  if (status != CHAIN_OK ||
      !op)
  {
    return false;
  }

  op = 0;

  status =
      M5Chain.setToFMeasureStatus(
          tof_id,
          CHAIN_TOF_STATUS_START,
          &op);

  Serial.printf(
      "[ToF] START status=0x%02X op=%u\n",
      (unsigned int)status,
      op);

  return
      status == CHAIN_OK &&
      op;
}

// ============================================================
// Angle calibration
// ============================================================

void calibrateAngle()
{
  if (!angle_id)
    return;

  Serial.println();
  Serial.println(
      "========== ANGLE CALIBRATION ==========");

  uint32_t sum = 0;
  uint16_t validSamples = 0;

  // Need 40 valid readings.
  // Allow up to 80 attempts.
  for (uint16_t attempt = 0;
       attempt < 80 &&
       validSamples < 40;
       ++attempt)
  {
    uint16_t value = 0;

    const chain_status_t status =
        M5Chain.getAngle12BitAdc(
            angle_id,
            &value);

    if (status == CHAIN_OK)
    {
      sum += value;
      ++validSamples;
    }

    delay(10);
  }

  if (validSamples == 40)
  {
    angleCenter =
        (uint16_t)(
            sum / validSamples);

    angleValue =
        angleCenter;

    Serial.printf(
        "[Angle] center=%u\n",
        angleCenter);
  }
  else
  {
    Serial.printf(
        "[Angle] calibration FAILED samples=%u\n",
        validSamples);
  }
}

// ============================================================
// DualKey LED initialization
// ============================================================

void initDualKeyLeds()
{
  pinMode(
      DUALKEY_LED_POWER_PIN,
      OUTPUT);

  digitalWrite(
      DUALKEY_LED_POWER_PIN,
      HIGH);

  DualKeyLeds.begin();
  DualKeyLeds.setBrightness(255);
  DualKeyLeds.clear();
  DualKeyLeds.show();
}

// ============================================================
// Boot test
// ============================================================

void bootLedTest()
{
  Serial.println();
  Serial.println(
      "========== LED BOOT TEST ==========");

  // DualKey left
  DualKeyLeds.setPixelColor(
      DUALKEY_LEFT_LED_INDEX,
      DualKeyLeds.Color(
          100,
          10,
          10));

  DualKeyLeds.show();

  delay(150);

  // DualKey right
  DualKeyLeds.setPixelColor(
      DUALKEY_RIGHT_LED_INDEX,
      DualKeyLeds.Color(
          100,
          80,
          0));

  DualKeyLeds.show();

  delay(150);

  // Encoder
  setChainLed(
      encoder_id,
      100,
      20,
      120,
      previousEncoderRgb,
      encoderRgbInitialized);

  delay(150);

  // Angle
  setChainLed(
      angle_id,
      20,
      80,
      150,
      previousAngleRgb,
      angleRgbInitialized);

  delay(150);

  // Matrix test
  uint16_t buffer[MATRIX_PIXELS];

  const uint16_t color =
      rgb565(
          0,
          80,
          120);

  for (uint8_t i = 0;
       i < MATRIX_PIXELS;
       ++i)
  {
    buffer[i] = color;
  }

  uint8_t op = 0;

  const chain_status_t status =
      M5Chain.setRGBBufferRefresh(
          rgb_id,
          buffer,
          &op);

  Serial.printf(
      "[Boot Matrix] status=0x%02X op=%u\n",
      (unsigned int)status,
      op);

  delay(350);

  // Matrix off
  memset(
      buffer,
      0,
      sizeof(buffer));

  op = 0;

  const chain_status_t clearStatus =
      M5Chain.setRGBBufferRefresh(
          rgb_id,
          buffer,
          &op);

  if (clearStatus == CHAIN_OK &&
      op)
  {
    memcpy(
        previousMatrix,
        buffer,
        sizeof(buffer));

    matrixInitialized = true;
  }
}

// ============================================================
// DualKey
// ============================================================

void updateDualKey()
{
  const uint32_t now =
      millis();

  KeyLeft.setRawState(
      now,
      !digitalRead(
          PIN_KEY_LEFT));

  KeyRight.setRawState(
      now,
      !digitalRead(
          PIN_KEY_RIGHT));

  const bool left =
      KeyLeft.isPressed();

  const bool right =
      KeyRight.isPressed();

  if (KeyLeft.wasPressed())
  {
    leftFlashActive = true;
    leftFlashStarted = now;
  }

  if (KeyRight.wasPressed())
  {
    rightFlashActive = true;
    rightFlashStarted = now;
  }

  // Chord has priority.
  if (left && right)
  {
    if (!chordConsumed)
    {
      pendingKey =
          PendingKey::NONE;

      sendDualKeyChord();

      startMatrixAnimation(
          MatrixAnimation::CHORD_FLASH,
          now);

      chordConsumed = true;
      singleConsumed = false;
    }

    return;
  }

  if (chordConsumed)
  {
    if (!left &&
        !right)
    {
      chordConsumed = false;
    }

    return;
  }

  if (singleConsumed)
  {
    if (!left &&
        !right)
    {
      singleConsumed = false;
    }

    return;
  }

  if (pendingKey ==
      PendingKey::NONE)
  {
    if (left && !right)
    {
      pendingKey =
          PendingKey::LEFT;

      pendingSince =
          now;
    }
    else if (
        right && !left)
    {
      pendingKey =
          PendingKey::RIGHT;

      pendingSince =
          now;
    }
  }

  if (pendingKey !=
          PendingKey::NONE &&
      now - pendingSince >=
          CHORD_WINDOW_MS)
  {
    if (pendingKey ==
            PendingKey::LEFT &&
        left)
    {
      sendOra4();

      currentAudioOutput =
          AudioOutput::ORA4;

      startMatrixAnimation(
          MatrixAnimation::ORA4_RIPPLE,
          now);

      singleConsumed =
          true;
    }
    else if (
        pendingKey ==
            PendingKey::RIGHT &&
        right)
    {
      sendStudioDisplay();

      currentAudioOutput =
          AudioOutput::STUDIO_DISPLAY;

      startMatrixAnimation(
          MatrixAnimation::STUDIO_RIPPLE,
          now);

      singleConsumed =
          true;
    }

    pendingKey =
        PendingKey::NONE;
  }

  // Short tap released before chord window.
  if (pendingKey ==
          PendingKey::LEFT &&
      KeyLeft.wasReleased())
  {
    sendOra4();

    currentAudioOutput =
        AudioOutput::ORA4;

    startMatrixAnimation(
        MatrixAnimation::ORA4_RIPPLE,
        now);

    pendingKey =
        PendingKey::NONE;

    singleConsumed =
        true;
  }

  if (pendingKey ==
          PendingKey::RIGHT &&
      KeyRight.wasReleased())
  {
    sendStudioDisplay();

    currentAudioOutput =
        AudioOutput::STUDIO_DISPLAY;

    startMatrixAnimation(
        MatrixAnimation::STUDIO_RIPPLE,
        now);

    pendingKey =
        PendingKey::NONE;

    singleConsumed =
        true;
  }
}

// ============================================================
// Encoder
// ============================================================

void updateEncoder()
{
  if (!encoder_id)
    return;

  const uint32_t now =
      millis();

  // ----------------------------------------------------------
  // Rotation
  // ----------------------------------------------------------

  if (now - lastEncoderValuePoll >=
      ENCODER_VALUE_INTERVAL_MS)
  {
    lastEncoderValuePoll =
        now;

    int16_t value = 0;

    const chain_status_t status =
        M5Chain.getEncoderValue(
            encoder_id,
            &value);

    if (status == CHAIN_OK)
    {
      if (!encoderInitialized)
      {
        lastEncoderValue =
            value;

        encoderInitialized =
            true;
      }
      else
      {
        const int16_t delta =
            value -
            lastEncoderValue;

        if (delta != 0)
        {
          // Send one HID event per encoder count.
          // Cap burst size so a corrupted / stale reading
          // cannot generate a huge volume jump.

          int steps =
              abs((int)delta);

          if (steps > 4)
            steps = 4;

          for (int i = 0;
               i < steps;
               ++i)
          {
            if (delta > 0)
              volumeUp();
            else
              volumeDown();
          }

          lastEncoderValue =
              value;

          encoderLedFlash =
              true;

          encoderLedFlashStarted =
              now;

          startMatrixAnimation(
              delta > 0
                  ? MatrixAnimation::VOLUME_EXPAND
                  : MatrixAnimation::VOLUME_CONTRACT,
              now);
        }
      }
    }
  }

  // ----------------------------------------------------------
  // Button
  // ----------------------------------------------------------

  if (now - lastEncoderButtonPoll >=
      ENCODER_BUTTON_INTERVAL_MS)
  {
    lastEncoderButtonPoll =
        now;

    uint8_t button = 0;

    const chain_status_t status =
        M5Chain.getEncoderButtonStatus(
            encoder_id,
            &button);

    if (status == CHAIN_OK)
    {
      if (!encoderButtonInitialized)
      {
        lastEncoderButton =
            button;

        encoderButtonInitialized =
            true;
      }
      else
      {
        const bool previousPressed =
            lastEncoderButton != 0;

        const bool currentPressed =
            button != 0;

        if (!previousPressed &&
            currentPressed)
        {
          sendMute();

          muted =
              !muted;

          startMatrixAnimation(
              MatrixAnimation::MUTE_PULSE,
              now);
        }

        lastEncoderButton =
            button;
      }
    }
  }
}

// ============================================================
// Angle
// ============================================================

void updateAngle()
{
  if (!angle_id)
    return;

  const uint32_t now =
      millis();

  if (now - lastAngleRead >=
      ANGLE_READ_INTERVAL_MS)
  {
    lastAngleRead =
        now;

    uint16_t value = 0;

    const chain_status_t status =
        M5Chain.getAngle12BitAdc(
            angle_id,
            &value);

    if (status == CHAIN_OK)
    {
      angleValue =
          value;
    }
  }

  const int32_t center =
      angleCenter;

  const int32_t stopLow =
      center -
      ANGLE_STOP_OFFSET;

  const int32_t stopHigh =
      center +
      ANGLE_STOP_OFFSET;

  const int32_t startLow =
      center -
      ANGLE_START_OFFSET;

  const int32_t startHigh =
      center +
      ANGLE_START_OFFSET;

  const ScrollState previousScrollState =
      scrollState;

  switch (scrollState)
  {
    case ScrollState::STOPPED:
    {
      if ((int32_t)angleValue <=
          startLow)
      {
        scrollState =
            ScrollState::UP;
      }
      else if (
          (int32_t)angleValue >=
          startHigh)
      {
        scrollState =
            ScrollState::DOWN;
      }
      else
      {
        angleActivity =
            0.0f;

        return;
      }

      break;
    }

    case ScrollState::UP:
    {
      if ((int32_t)angleValue >=
          stopLow)
      {
        scrollState =
            ScrollState::STOPPED;

        angleActivity =
            0.0f;

        lastScrollMs =
            now;

        return;
      }

      break;
    }

    case ScrollState::DOWN:
    {
      if ((int32_t)angleValue <=
          stopHigh)
      {
        scrollState =
            ScrollState::STOPPED;

        angleActivity =
            0.0f;

        lastScrollMs =
            now;

        return;
      }

      break;
    }
  }

  if (scrollState !=
      previousScrollState)
  {
    if (scrollState ==
        ScrollState::UP)
    {
      startMatrixAnimation(
          MatrixAnimation::SCROLL_UP,
          now);
    }
    else if (
        scrollState ==
        ScrollState::DOWN)
    {
      startMatrixAnimation(
          MatrixAnimation::SCROLL_DOWN,
          now);
    }
  }

  float normalized = 0.0f;

  if (scrollState ==
      ScrollState::UP)
  {
    const float distance =
        (float)(
            startLow -
            (int32_t)angleValue);

    const float range =
        (float)(
            startLow -
            ANGLE_MIN);

    if (range > 0.0f)
      normalized =
          distance / range;
  }
  else if (
      scrollState ==
      ScrollState::DOWN)
  {
    const float distance =
        (float)(
            (int32_t)angleValue -
            startHigh);

    const float range =
        (float)(
            ANGLE_MAX -
            startHigh);

    if (range > 0.0f)
      normalized =
          distance / range;
  }

  normalized =
      clamp01(normalized);

  angleActivity =
      normalized;

  const float curve =
      powf(
          normalized,
          1.5f);

  const float speed =
      SCROLL_BASE_SPEED +
      (1.0f -
       SCROLL_BASE_SPEED) *
          curve;

  const uint32_t interval =
      SCROLL_SLOWEST_INTERVAL_MS -
      (uint32_t)(
          speed *
          (SCROLL_SLOWEST_INTERVAL_MS -
           SCROLL_FASTEST_INTERVAL_MS));

  if (now - lastScrollMs >=
      interval)
  {
    lastScrollMs =
        now;

    if (scrollState ==
        ScrollState::UP)
    {
      Mouse.move(
          0,
          0,
          SCROLL_UP_STEP);
    }
    else if (
        scrollState ==
        ScrollState::DOWN)
    {
      Mouse.move(
          0,
          0,
          SCROLL_DOWN_STEP);
    }
  }
}

// ============================================================
// ToF SINGLE
// ============================================================

void updateTof()
{
  if (!tof_id)
    return;

  const uint32_t now =
      millis();

  if (tofDistanceValid &&
      now - lastTofSuccess >=
          TOF_INVALID_TIMEOUT_MS)
  {
    tofDistanceValid =
        false;
  }

  if (now - lastTofPoll <
      TOF_POLL_INTERVAL_MS)
  {
    return;
  }

  lastTofPoll =
      now;

  chain_tof_measure_status_t measureStatus =
      CHAIN_TOF_STATUS_START;

  uint8_t completeFlag = 0;

  chain_status_t status =
      M5Chain.getToFMeasureStatus(
          tof_id,
          &measureStatus);

  if (status != CHAIN_OK)
    return;

  status =
      M5Chain.getToFMeasureCompleteFlag(
          tof_id,
          &completeFlag);

  if (status != CHAIN_OK)
    return;

  // Official SINGLE-mode completion condition.
  if (measureStatus !=
          CHAIN_TOF_STATUS_STOP ||
      completeFlag != 1)
  {
    if (tofDistanceValid &&
        now - lastTofSuccess >=
            TOF_INVALID_TIMEOUT_MS)
    {
      tofDistanceValid =
          false;
    }

    return;
  }

  uint16_t distance = 0;

  status =
      M5Chain.getToFDistance(
          tof_id,
          &distance);

  if (status == CHAIN_OK)
  {
    tofRawDistance =
        distance;

    if (!tofDistanceValid)
    {
      tofDistance =
          distance;
    }
    else
    {
      // 75% previous + 25% new.
      tofDistance =
          (uint16_t)(
              (tofDistance * 3UL +
               distance) /
              4UL);
    }

    tofDistanceValid =
        true;

    lastTofSuccess =
        now;
  }

  // Always attempt to start the next SINGLE measurement
  // after a completed measurement.

  uint8_t op = 0;

  M5Chain.setToFMeasureStatus(
      tof_id,
      CHAIN_TOF_STATUS_START,
      &op);
}

// ============================================================
// Matrix ambient and action animations
// ============================================================

uint32_t matrixAnimationDurationFor(
    MatrixAnimation animation)
{
  switch (animation)
  {
    case MatrixAnimation::ORA4_RIPPLE:
    case MatrixAnimation::STUDIO_RIPPLE:
      return 560;

    case MatrixAnimation::CHORD_FLASH:
      return 500;

    case MatrixAnimation::VOLUME_EXPAND:
    case MatrixAnimation::VOLUME_CONTRACT:
      return 320;

    case MatrixAnimation::MUTE_PULSE:
      return 480;

    case MatrixAnimation::SCROLL_UP:
    case MatrixAnimation::SCROLL_DOWN:
      return 420;

    case MatrixAnimation::IDLE:
    default:
      return 0;
  }
}

void startMatrixAnimation(
    MatrixAnimation animation,
    uint32_t now)
{
  if (matrixAnimation ==
          animation &&
      matrixAnimationDuration != 0 &&
      now - matrixAnimationStarted <
          matrixAnimationDuration)
  {
    return;
  }

  matrixAnimation =
      animation;

  matrixAnimationStarted =
      now;

  matrixAnimationDuration =
      matrixAnimationDurationFor(
          animation);

  // Allow the newest action to replace the previous frame
  // at the next update without exceeding the 80 ms limit.
  if (now - lastMatrixUpdate >=
      MATRIX_UPDATE_INTERVAL_MS)
  {
    lastMatrixUpdate =
        now -
        MATRIX_UPDATE_INTERVAL_MS;
  }
}

uint16_t matrixColor(
    uint8_t baseR,
    uint8_t baseG,
    uint8_t baseB,
    float level)
{
  const float limitedLevel =
      clamp01(level);

  return rgb565(
      (uint8_t)(baseR * limitedLevel),
      (uint8_t)(baseG * limitedLevel),
      (uint8_t)(baseB * limitedLevel));
}

float matrixRadius(
    uint8_t x,
    uint8_t y,
    float centerX,
    float centerY)
{
  const float dx =
      (float)x -
      centerX;

  const float dy =
      (float)y -
      centerY;

  return sqrtf(
      dx * dx +
      dy * dy);
}

void updateMatrixAmbientState(
    uint32_t now)
{
  float targetProximity = 0.0f;

  if (tofDistanceValid)
  {
    if (tofDistance <=
        TOF_AMBIENT_NEAR_MM)
    {
      targetProximity = 1.0f;
    }
    else if (tofDistance <
             TOF_AMBIENT_FAR_MM)
    {
      targetProximity =
          (float)(TOF_AMBIENT_FAR_MM -
                  tofDistance) /
          (float)(TOF_AMBIENT_FAR_MM -
                  TOF_AMBIENT_NEAR_MM);
    }
  }

  targetProximity =
      roundf(
          targetProximity *
          TOF_AMBIENT_PROXIMITY_STEPS) /
      TOF_AMBIENT_PROXIMITY_STEPS;

  matrixProximity +=
      (targetProximity -
       matrixProximity) *
      TOF_AMBIENT_PROXIMITY_SMOOTHING;

  if (fabsf(
          targetProximity -
          matrixProximity) < 0.002f)
  {
    matrixProximity =
        targetProximity;
  }

  uint32_t elapsed =
      MATRIX_UPDATE_INTERVAL_MS;

  if (lastMatrixAmbientUpdate != 0)
  {
    elapsed =
        now -
        lastMatrixAmbientUpdate;

    if (elapsed > 250)
    {
      elapsed = 250;
    }
  }

  lastMatrixAmbientUpdate = now;

  const float speed =
      1.0f +
      matrixProximity *
          TOF_AMBIENT_SPEED_BOOST;

  matrixAmbientTime +=
      elapsed / 1000.0f *
      speed;
}

void renderMatrixAmbient(
    uint16_t buffer[MATRIX_PIXELS],
    uint32_t now)
{
  (void)now;

  const float t =
      matrixAmbientTime;

  const float drift =
      0.22f +
      matrixProximity *
          0.12f;

  const float centerX =
      3.5f +
      drift *
          sinf(
              2.0f * PI * t / 8.7f);

  const float centerY =
      3.5f +
      drift * 0.80f *
          sinf(
              2.0f * PI * t / 11.3f +
              1.4f);

  const float mainWave =
      0.5f +
      0.5f *
          sinf(
              2.0f * PI * t / 3.2f);

  const float slowWave =
      0.18f *
      sinf(
          2.0f * PI * t / 7.1f +
          0.8f);

  const float breathing =
      clamp01(
          mainWave +
          slowWave);

  const float ambientLevel =
      MATRIX_AMBIENT_MIN_LEVEL +
      (MATRIX_AMBIENT_MAX_LEVEL -
       MATRIX_AMBIENT_MIN_LEVEL) *
          breathing +
      matrixProximity *
          TOF_AMBIENT_BRIGHTNESS_BOOST;

  const float falloff =
      0.55f -
      matrixProximity *
          TOF_AMBIENT_SPREAD_BOOST;

  for (uint8_t y = 0;
       y < MATRIX_WIDTH;
       ++y)
  {
    for (uint8_t x = 0;
         x < MATRIX_WIDTH;
         ++x)
    {
      const float radius =
          matrixRadius(
              x,
              y,
              centerX,
              centerY);

      const float glow =
          1.0f /
          (1.0f +
           radius * radius *
               falloff);

      const float shimmer =
          1.0f +
          matrixProximity *
              TOF_AMBIENT_SHIMMER_MAX *
              sinf(
                  radius * 2.4f -
                  2.0f * PI * t /
                      2.8f);

      buffer[
          y * MATRIX_WIDTH +
          x] =
          matrixColor(
              MATRIX_R,
              MATRIX_G,
              MATRIX_B,
              ambientLevel *
                  glow *
                  shimmer);
    }
  }
}

void renderMatrixRing(
    uint16_t buffer[MATRIX_PIXELS],
    float progress,
    bool expanding,
    uint8_t baseR,
    uint8_t baseG,
    uint8_t baseB,
    float peakLevel)
{
  const float outerRadius =
      5.0f;

  const float targetRadius =
      expanding
          ? progress * outerRadius
          : (1.0f - progress) *
                outerRadius;

  const float fade =
      1.0f -
      progress * 0.35f;

  for (uint8_t y = 0;
       y < MATRIX_WIDTH;
       ++y)
  {
    for (uint8_t x = 0;
         x < MATRIX_WIDTH;
         ++x)
    {
      const float radius =
          matrixRadius(
              x,
              y,
              3.5f,
              3.5f);

      const float distanceFromRing =
          fabsf(
              radius -
              targetRadius);

      float intensity =
          1.0f -
          distanceFromRing /
              1.15f;

      intensity =
          clamp01(intensity) *
          peakLevel *
          fade;

      buffer[
          y * MATRIX_WIDTH +
          x] =
          matrixColor(
              baseR,
              baseG,
              baseB,
              intensity);
    }
  }
}

void renderMatrixPulse(
    uint16_t buffer[MATRIX_PIXELS],
    float progress)
{
  const float pulse =
      0.20f +
      0.80f *
          sinf(
              PI * progress);

  for (uint8_t y = 0;
       y < MATRIX_WIDTH;
       ++y)
  {
    for (uint8_t x = 0;
         x < MATRIX_WIDTH;
         ++x)
    {
      const float radius =
          matrixRadius(
              x,
              y,
              3.5f,
              3.5f);

      const float glow =
          1.0f /
          (1.0f +
           radius * radius * 0.28f);

      buffer[
          y * MATRIX_WIDTH +
          x] =
          matrixColor(
              MUTE_R,
              MUTE_G,
              MUTE_B,
              MATRIX_ACTION_PEAK_LEVEL *
                  pulse *
                  glow);
    }
  }
}

void renderMatrixScroll(
    uint16_t buffer[MATRIX_PIXELS],
    float progress,
    bool upward)
{
  const float bandY =
      upward
          ? 7.5f -
                progress * 8.0f
          : -0.5f +
                progress * 8.0f;

  for (uint8_t y = 0;
       y < MATRIX_WIDTH;
       ++y)
  {
    const float verticalDistance =
        fabsf(
            (float)y -
            bandY);

    const float verticalLevel =
        clamp01(
            1.0f -
            verticalDistance /
                1.8f);

    for (uint8_t x = 0;
         x < MATRIX_WIDTH;
         ++x)
    {
      const float horizontalDistance =
          fabsf(
              (float)x -
              3.5f) /
          3.5f;

      const float horizontalLevel =
          1.0f -
          0.35f *
              horizontalDistance;

      buffer[
          y * MATRIX_WIDTH +
          x] =
          matrixColor(
              ANGLE_R,
              ANGLE_G,
              ANGLE_B,
              MATRIX_ACTION_LEVEL *
                  verticalLevel *
                  horizontalLevel);
    }
  }
}

void renderMatrixChord(
    uint16_t buffer[MATRIX_PIXELS],
    float progress)
{
  if (progress < 0.20f)
  {
    const float flashProgress =
        progress /
        0.20f;

    const float level =
        MATRIX_ACTION_PEAK_LEVEL *
        (1.0f -
         0.35f *
             flashProgress);

    const uint16_t color =
        matrixColor(
            MATRIX_WHITE_R,
            MATRIX_WHITE_G,
            MATRIX_WHITE_B,
            level);

    for (uint8_t i = 0;
         i < MATRIX_PIXELS;
         ++i)
    {
      buffer[i] =
          color;
    }

    return;
  }

  renderMatrixRing(
      buffer,
      (progress - 0.20f) /
          0.80f,
      false,
      MATRIX_WHITE_R,
      MATRIX_WHITE_G,
      MATRIX_WHITE_B,
      MATRIX_ACTION_LEVEL);
}

void renderMatrixAnimation(
    uint16_t buffer[MATRIX_PIXELS],
    uint32_t now)
{
  if (matrixAnimation ==
          MatrixAnimation::IDLE ||
      matrixAnimationDuration == 0)
  {
    renderMatrixAmbient(
        buffer,
        now);

    return;
  }

  const uint32_t elapsed =
      now -
      matrixAnimationStarted;

  if (elapsed >=
      matrixAnimationDuration)
  {
    matrixAnimation =
        MatrixAnimation::IDLE;

    matrixAnimationDuration = 0;

    renderMatrixAmbient(
        buffer,
        now);

    return;
  }

  const float progress =
      (float)elapsed /
      (float)matrixAnimationDuration;

  switch (matrixAnimation)
  {
    case MatrixAnimation::ORA4_RIPPLE:
      renderMatrixRing(
          buffer,
          progress,
          true,
          ORA4_R,
          ORA4_G,
          ORA4_B,
          MATRIX_ACTION_LEVEL);
      break;

    case MatrixAnimation::STUDIO_RIPPLE:
      renderMatrixRing(
          buffer,
          progress,
          true,
          STUDIO_R,
          STUDIO_G,
          STUDIO_B,
          MATRIX_ACTION_LEVEL);
      break;

    case MatrixAnimation::CHORD_FLASH:
      renderMatrixChord(
          buffer,
          progress);
      break;

    case MatrixAnimation::VOLUME_EXPAND:
      renderMatrixRing(
          buffer,
          progress,
          true,
          MUTE_R,
          MUTE_G,
          MUTE_B,
          MATRIX_ACTION_LEVEL);
      break;

    case MatrixAnimation::VOLUME_CONTRACT:
      renderMatrixRing(
          buffer,
          progress,
          false,
          MUTE_R,
          MUTE_G,
          MUTE_B,
          MATRIX_ACTION_LEVEL);
      break;

    case MatrixAnimation::MUTE_PULSE:
      renderMatrixPulse(
          buffer,
          progress);
      break;

    case MatrixAnimation::SCROLL_UP:
      renderMatrixScroll(
          buffer,
          progress,
          true);
      break;

    case MatrixAnimation::SCROLL_DOWN:
      renderMatrixScroll(
          buffer,
          progress,
          false);
      break;

    case MatrixAnimation::IDLE:
    default:
      renderMatrixAmbient(
          buffer,
          now);
      break;
  }
}

void updateMatrix()
{
  if (!rgb_id)
    return;

  const uint32_t now =
      millis();

  if (now - lastMatrixUpdate <
      MATRIX_UPDATE_INTERVAL_MS)
  {
    return;
  }

  lastMatrixUpdate =
      now;

  updateMatrixAmbientState(
      now);

  uint16_t buffer[MATRIX_PIXELS] = {0};

  renderMatrixAnimation(
      buffer,
      now);

  if (matrixInitialized &&
      memcmp(
          buffer,
          previousMatrix,
          sizeof(buffer)) == 0)
  {
    return;
  }

  uint8_t op = 0;

  const chain_status_t status =
      M5Chain.setRGBBufferRefresh(
          rgb_id,
          buffer,
          &op);

  if (status == CHAIN_OK &&
      op)
  {
    memcpy(
        previousMatrix,
        buffer,
        sizeof(buffer));

    matrixInitialized =
        true;
  }
  else
  {
    static uint32_t lastFailureLog = 0;

    if (now - lastFailureLog >=
        1000)
    {
      lastFailureLog = now;

      Serial.printf(
          "[Matrix] frame failed status=0x%02X op=%u\n",
          (unsigned int)status,
          op);
    }
  }
}

// ============================================================
// Status LEDs
// ============================================================

void updateStatusLeds()
{
  const uint32_t now =
      millis();

  if (now - lastLedUpdate <
      LED_UPDATE_INTERVAL_MS)
  {
    return;
  }

  lastLedUpdate =
      now;

  // ----------------------------------------------------------
  // DualKey
  // ----------------------------------------------------------

  DualKeyLeds.clear();

  uint8_t r;
  uint8_t g;
  uint8_t b;

  // Standby left
  scaleRgb(
      ORA4_R,
      ORA4_G,
      ORA4_B,
      STANDBY_LEVEL,
      r,
      g,
      b);

  DualKeyLeds.setPixelColor(
      DUALKEY_LEFT_LED_INDEX,
      DualKeyLeds.Color(
          r,
          g,
          b));

  // Standby right
  scaleRgb(
      STUDIO_R,
      STUDIO_G,
      STUDIO_B,
      STANDBY_LEVEL,
      r,
      g,
      b);

  DualKeyLeds.setPixelColor(
      DUALKEY_RIGHT_LED_INDEX,
      DualKeyLeds.Color(
          r,
          g,
          b));

  const float wave =
      breathingWave(now);

  const float audioLevel =
      AUDIO_MIN_LEVEL +
      (AUDIO_MAX_LEVEL -
       AUDIO_MIN_LEVEL) *
          wave;

  if (currentAudioOutput ==
      AudioOutput::ORA4)
  {
    scaleRgb(
        ORA4_R,
        ORA4_G,
        ORA4_B,
        audioLevel,
        r,
        g,
        b);

    DualKeyLeds.setPixelColor(
        DUALKEY_LEFT_LED_INDEX,
        DualKeyLeds.Color(
            r,
            g,
            b));
  }
  else if (
      currentAudioOutput ==
      AudioOutput::STUDIO_DISPLAY)
  {
    scaleRgb(
        STUDIO_R,
        STUDIO_G,
        STUDIO_B,
        audioLevel,
        r,
        g,
        b);

    DualKeyLeds.setPixelColor(
        DUALKEY_RIGHT_LED_INDEX,
        DualKeyLeds.Color(
            r,
            g,
            b));
  }

  if (leftFlashActive)
  {
    if (now -
            leftFlashStarted <
        KEY_FLASH_MS)
    {
      DualKeyLeds.setPixelColor(
          DUALKEY_LEFT_LED_INDEX,
          DualKeyLeds.Color(
              ORA4_R,
              ORA4_G,
              ORA4_B));
    }
    else
    {
      leftFlashActive =
          false;
    }
  }

  if (rightFlashActive)
  {
    if (now -
            rightFlashStarted <
        KEY_FLASH_MS)
    {
      DualKeyLeds.setPixelColor(
          DUALKEY_RIGHT_LED_INDEX,
          DualKeyLeds.Color(
              STUDIO_R,
              STUDIO_G,
              STUDIO_B));
    }
    else
    {
      rightFlashActive =
          false;
    }
  }

  DualKeyLeds.show();

  // ----------------------------------------------------------
  // Encoder
  // ----------------------------------------------------------

  float encoderLevel =
      STANDBY_LEVEL;

  if (muted)
  {
    encoderLevel =
        MUTE_MIN_LEVEL +
        (MUTE_MAX_LEVEL -
         MUTE_MIN_LEVEL) *
            wave;
  }

  if (encoderLedFlash)
  {
    const uint32_t elapsed =
        now -
        encoderLedFlashStarted;

    if (elapsed <
        ENCODER_LED_FLASH_MS)
    {
      const float p =
          1.0f -
          (float)elapsed /
              ENCODER_LED_FLASH_MS;

      const float flash =
          p * p;

      encoderLevel =
          encoderLevel +
          (1.0f -
           encoderLevel) *
              flash;
    }
    else
    {
      encoderLedFlash =
          false;
    }
  }

  scaleRgb(
      MUTE_R,
      MUTE_G,
      MUTE_B,
      encoderLevel,
      r,
      g,
      b);

  setChainLed(
      encoder_id,
      r,
      g,
      b,
      previousEncoderRgb,
      encoderRgbInitialized);

  // ----------------------------------------------------------
  // Angle
  // ----------------------------------------------------------

  float angleLevel =
      STANDBY_LEVEL;

  if (scrollState !=
      ScrollState::STOPPED)
  {
    angleLevel =
        0.30f +
        0.70f *
            powf(
                angleActivity,
                0.7f);
  }

  scaleRgb(
      ANGLE_R,
      ANGLE_G,
      ANGLE_B,
      angleLevel,
      r,
      g,
      b);

  setChainLed(
      angle_id,
      r,
      g,
      b,
      previousAngleRgb,
      angleRgbInitialized);
}

// ============================================================
// setup
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(400);

  Serial.println();
  Serial.println(
      "==========================================");
  Serial.println(
      " M5 DualKey Full 5-Unit Test");
  Serial.println(
      " DualKey -> Encoder -> Angle -> RGB -> ToF");
  Serial.println(
      "==========================================");

  // ----------------------------------------------------------
  // Local buttons
  // ----------------------------------------------------------

  pinMode(
      PIN_KEY_LEFT,
      INPUT);

  pinMode(
      PIN_KEY_RIGHT,
      INPUT);

  // ----------------------------------------------------------
  // Local LEDs
  // ----------------------------------------------------------

  initDualKeyLeds();

  // ----------------------------------------------------------
  // IMPORTANT:
  // Establish Chain first, using the exact basic structure
  // already proven by the diagnostic.
  // ----------------------------------------------------------

  const bool chainOk =
      enumerateChain();

  if (!chainOk)
  {
    Serial.println(
        "[FATAL] Required Chain devices missing.");

    // DualKey HID can still work,
    // but do not pretend Chain is healthy.
  }
  else
  {
    initEncoder();
    initChainLeds();
    initTof();

    // Boot test only after Chain setup.
    bootLedTest();

    // Keep Angle centered during startup.
    calibrateAngle();
  }

  // ----------------------------------------------------------
  // USB HID LAST
  //
  // This is deliberate.
  // Chain is fully initialized before USB is started.
  // ----------------------------------------------------------

  Keyboard.begin();
  ConsumerControl.begin();
  Mouse.begin();

  USB.begin();

  Serial.println();
  Serial.println(
      "========== USB HID STARTED ==========");

  Serial.println(
      "Full system running.");
}

// ============================================================
// loop
// ============================================================

void loop()
{
  // Local GPIO / USB HID
  updateDualKey();

  // Shared Chain bus
  updateEncoder();
  updateAngle();
  updateTof();

  // Large Matrix transfer is rate-limited
  updateMatrix();

  // LED updates are also rate-limited
  updateStatusLeds();

  delay(2);
}
