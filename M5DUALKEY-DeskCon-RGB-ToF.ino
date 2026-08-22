#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "M5Unified.h"
#include "M5Chain.h"
#include "USBHIDMouse.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ============================================================
// DualKey
// ============================================================

#define PIN_KEY1 0
#define PIN_KEY2 17

m5::Button_Class Key1;
m5::Button_Class Key2;

// Front orientation:
// USB-C is on the rear side.
//
// Physical DualKey mapping confirmed on hardware:
// LEFT  = GPIO17 = ORA4
// RIGHT = GPIO0  = Studio Display

#define PIN_KEY_LEFT 17
#define PIN_KEY_RIGHT 0

USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
USBHIDMouse Mouse;

constexpr uint32_t CHORD_WINDOW_MS = 80;

enum class PendingKey {
  NONE,
  LEFT,
  RIGHT
};

PendingKey pending = PendingKey::NONE;
uint32_t pendingSince = 0;

bool chordConsumed = false;
bool singleConsumed = false;

// ============================================================
// Chain
// Physical layout:
// DualKey -> Encoder -> Angle -> Chain RGB -> Chain ToF
// ============================================================

#define CHAIN_RXD_PIN GPIO_NUM_47
#define CHAIN_TXD_PIN GPIO_NUM_48

Chain M5Chain;

device_list_t *device_list = nullptr;
uint16_t device_count = 0;
uint8_t opr_status = 0;

uint8_t encoder_id = 0;
uint8_t angle_id = 0;
uint8_t rgb_id = 0;
uint8_t tof_id = 0;

// ============================================================
// LED state
// ============================================================

constexpr uint8_t DUALKEY_LED_POWER_PIN = 40;
constexpr uint8_t DUALKEY_LED_SIGNAL_PIN = 21;
constexpr uint8_t DUALKEY_LED_COUNT = 2;

// Front orientation: USB-C is on the rear side.
//
// Physical layout:
// DualKey -> Encoder -> Angle -> Chain RGB -> Chain ToF
//
// DualKey mapping:
// LEFT  = ORA4           = Red
// RIGHT = Studio Display = Yellow

constexpr uint8_t DUALKEY_LEFT_LED_INDEX = 0;
constexpr uint8_t DUALKEY_RIGHT_LED_INDEX = 1;

// LED animation update interval
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 30;

// Base colors

constexpr uint8_t ORA4_R = 255;
constexpr uint8_t ORA4_G = 40;
constexpr uint8_t ORA4_B = 40;

constexpr uint8_t STUDIO_R = 255;
constexpr uint8_t STUDIO_G = 220;
constexpr uint8_t STUDIO_B = 0;

constexpr uint8_t MUTE_R = 170;
constexpr uint8_t MUTE_G = 40;
constexpr uint8_t MUTE_B = 255;

constexpr uint8_t ANGLE_R = 40;
constexpr uint8_t ANGLE_G = 140;
constexpr uint8_t ANGLE_B = 255;

// Brightness range.
//
// Audio output LEDs:
//   35% -> 80%
//
// Mute LED:
//   45% -> 85%

constexpr float AUDIO_LED_MIN_LEVEL = 0.35f;
constexpr float AUDIO_LED_MAX_LEVEL = 0.80f;

constexpr float MUTE_LED_MIN_LEVEL = 0.45f;
constexpr float MUTE_LED_MAX_LEVEL = 0.85f;

// Perceptual standby level before gamma correction.
// gammaCorrect(0.20) produces approximately 2.9% RGB output.

constexpr float LED_STANDBY_LEVEL = 0.20f;

// Action feedback temporarily overrides the underlying state.

constexpr float DUALKEY_FLASH_LEVEL = 0.95f;
constexpr uint32_t DUALKEY_FLASH_DURATION_MS = 80;

constexpr float ENCODER_IMPULSE_PEAK_LEVEL = 1.0f;
constexpr uint32_t ENCODER_IMPULSE_DECAY_MS = 500;

constexpr float ANGLE_LED_MIN_LEVEL = 0.30f;
constexpr float ANGLE_LED_ACTIVITY_EXPONENT = 0.7f;
constexpr uint32_t ANGLE_LED_FADE_MS = 150;

// How strongly the irregular fluctuation affects breathing.
// 0.0 = pure regular breathing
// 1.0 = much more irregular

constexpr float LED_FLUCTUATION_STRENGTH = 0.18f;

// Chain LED master brightness.

constexpr uint8_t CHAIN_LED_BRIGHTNESS = 255;

// ============================================================
// Chain RGB / ToF
// ============================================================

constexpr uint8_t RGB_MATRIX_WIDTH = 8;
constexpr uint8_t RGB_MATRIX_PIXEL_COUNT = 64;
constexpr uint8_t RGB_MATRIX_BRIGHTNESS = 100;

constexpr float RGB_MATRIX_STANDBY_LEVEL = 0.05f;
constexpr float RGB_MATRIX_ACTIVE_MAX_LEVEL = 0.50f;

constexpr uint16_t TOF_FAR_DISTANCE_MM = 400;
constexpr uint16_t TOF_NEAR_DISTANCE_MM = 50;

// Official SINGLE measurement:
// 33 ms measurement time.
// Poll completion every 40 ms.

constexpr uint32_t TOF_READ_INTERVAL_MS = 40;
constexpr uint8_t TOF_MEASUREMENT_TIME_MS = 33;

constexpr uint32_t TOF_INVALIDATION_TIMEOUT_MS = 500;
constexpr uint32_t DIAGNOSTIC_LOG_INTERVAL_MS = 200;

constexpr uint8_t RGB_MATRIX_R = 30;
constexpr uint8_t RGB_MATRIX_G = 220;
constexpr uint8_t RGB_MATRIX_B = 180;

Adafruit_NeoPixel DualKeyLeds(
  DUALKEY_LED_COUNT,
  DUALKEY_LED_SIGNAL_PIN,
  NEO_GRB + NEO_KHZ800);

enum class AudioOutput {
  UNKNOWN,
  STUDIO_DISPLAY,
  ORA4
};

AudioOutput currentAudioOutput = AudioOutput::UNKNOWN;

bool muted = false;
bool ledsDirty = true;

uint32_t lastLedUpdateMs = 0;

uint32_t dualKeyLeftFlashStartMs = 0;
uint32_t dualKeyRightFlashStartMs = 0;

bool dualKeyLeftFlashActive = false;
bool dualKeyRightFlashActive = false;

uint32_t encoderImpulseStartMs = 0;
bool encoderImpulseActive = false;

uint8_t lastEncoderLedRgb[3] = { 0, 0, 0 };
uint8_t lastAngleLedRgb[3] = { 0, 0, 0 };

bool encoderLedRgbInitialized = false;
bool angleLedRgbInitialized = false;

// ============================================================
// ToF state
// ============================================================

uint16_t tofDistanceMm = 0;
uint16_t tofRawDistanceMm = 0;

bool tofDistanceValid = false;

uint32_t lastTofReadMs = 0;
uint32_t lastTofSuccessMs = 0;
uint32_t lastTofSerialMs = 0;

chain_status_t tofLastStatus = CHAIN_TIMEOUT;
chain_status_t tofCompleteStatus = CHAIN_TIMEOUT;

uint8_t tofLastCompleteFlag = 0;

// ============================================================
// RGB Matrix state
// ============================================================

uint16_t lastRgbMatrix[RGB_MATRIX_PIXEL_COUNT] = { 0 };

bool rgbMatrixInitialized = false;

chain_status_t rgbLastStatus = CHAIN_TIMEOUT;
uint8_t rgbLastOperationStatus = 0;

float matrixProximity = 0.0f;
float matrixLevel = 0.0f;

uint8_t matrixSquareSize = 0;

uint32_t lastMatrixDiagnosticMs = 0;

// ============================================================
// Boot LED animation
// ============================================================

constexpr float BOOT_SEQUENCE_LEVEL = 0.55f;
constexpr uint32_t BOOT_STEP_MS = 140;
constexpr uint32_t BOOT_READY_HOLD_MS = 250;

constexpr uint8_t BOOT_FADE_STEPS = 18;
constexpr uint32_t BOOT_FADE_STEP_MS = 22;

// ============================================================
// Encoder state
// ============================================================

int16_t lastEncoderValue = 0;
bool encoderValueInitialized = false;

bool lastEncoderButton = false;
uint32_t lastEncoderButtonChangeMs = 0;

constexpr uint32_t ENCODER_BUTTON_DEBOUNCE_MS = 40;

// ============================================================
// Angle auto scroll
// ============================================================

constexpr uint16_t ANGLE_MIN = 0;
constexpr uint16_t ANGLE_MAX = 4095;

// 起動時キャリブレーションで実測値に更新する
uint16_t angleCenter = 2048;

constexpr uint16_t ANGLE_CALIBRATION_SAMPLES = 40;
constexpr uint32_t ANGLE_CALIBRATION_INTERVAL_MS = 10;

// ヒステリシス
//
// スクロール中は ±95 に戻るとSTOP
// STOP中は ±135 を超えると再開

constexpr uint16_t ANGLE_STOP_OFFSET = 95;
constexpr uint16_t ANGLE_START_OFFSET = 135;

// Angle読み取り周期

constexpr uint32_t ANGLE_READ_INTERVAL_MS = 20;

// スクロール速度

constexpr uint32_t SCROLL_SLOWEST_INTERVAL_MS = 220;
constexpr uint32_t SCROLL_FASTEST_INTERVAL_MS = 25;

// デッドゾーンを抜けた直後から実用速度を確保

constexpr float SCROLL_BASE_SPEED = 0.35f;

// macOS wheel direction

constexpr int8_t SCROLL_UP_STEP = -1;
constexpr int8_t SCROLL_DOWN_STEP = 1;

uint16_t angleValue = 2048;

uint32_t lastAngleReadMs = 0;
uint32_t lastScrollMs = 0;

enum class ScrollState {
  STOPPED,
  UP,
  DOWN
};

ScrollState scrollState = ScrollState::STOPPED;

float angleActivityLevel = 0.0f;
float angleLedLevel = 0.0f;
float angleFadeStartLevel = 0.0f;

uint32_t angleFadeStartMs = 0;

bool angleLedFading = false;

// ============================================================
// ANGLE calibration
// ============================================================

void calibrateAngleCenter() {
  if (angle_id == 0) {
    return;
  }

  uint32_t sum = 0;

  for (uint16_t i = 0;
       i < ANGLE_CALIBRATION_SAMPLES;
       ++i) {
    uint16_t value = 0;

    M5Chain.getAngle12BitAdc(
      angle_id,
      &value);

    sum += value;

    delay(
      ANGLE_CALIBRATION_INTERVAL_MS);
  }

  angleCenter =
    (uint16_t)(sum / ANGLE_CALIBRATION_SAMPLES);

  angleValue = angleCenter;

  scrollState =
    ScrollState::STOPPED;

  angleActivityLevel = 0.0f;
  angleLedLevel = 0.0f;
  angleLedFading = false;

  lastScrollMs = millis();
}

// ============================================================
// HID: Speaker switching
// ============================================================

void sendOra4() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('1');

  delay(20);

  Keyboard.releaseAll();
}

void sendStudioDisplay() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('2');

  delay(20);

  Keyboard.releaseAll();
}

void sendAudioToggle() {
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('s');

  delay(20);

  Keyboard.releaseAll();
}

// ============================================================
// HID: Audio volume
// ============================================================

void volumeUp() {
  ConsumerControl.press(
    CONSUMER_CONTROL_VOLUME_INCREMENT);

  delay(5);

  ConsumerControl.release();
}

void volumeDown() {
  ConsumerControl.press(
    CONSUMER_CONTROL_VOLUME_DECREMENT);

  delay(5);

  ConsumerControl.release();
}

void toggleMute() {
  ConsumerControl.press(
    CONSUMER_CONTROL_MUTE);

  delay(5);

  ConsumerControl.release();
}

// ============================================================
// LED control
// ============================================================

void setAudioOutputState(
  AudioOutput output) {
  currentAudioOutput = output;
  ledsDirty = true;
}

void toggleAudioOutputState() {
  if (currentAudioOutput == AudioOutput::STUDIO_DISPLAY) {
    setAudioOutputState(
      AudioOutput::ORA4);
  } else if (currentAudioOutput == AudioOutput::ORA4) {
    setAudioOutputState(
      AudioOutput::STUDIO_DISPLAY);
  }
}

void toggleMuteState() {
  muted = !muted;
  ledsDirty = true;
}

void triggerDualKeyFlash(
  PendingKey key) {
  const uint32_t now =
    millis();

  if (key == PendingKey::LEFT) {
    dualKeyLeftFlashStartMs = now;
    dualKeyLeftFlashActive = true;
  } else if (key == PendingKey::RIGHT) {
    dualKeyRightFlashStartMs = now;
    dualKeyRightFlashActive = true;
  }

  ledsDirty = true;
}

void triggerEncoderImpulse() {
  encoderImpulseStartMs = millis();
  encoderImpulseActive = true;

  ledsDirty = true;
}

bool rgbNeedsUpdate(
  uint8_t r,
  uint8_t g,
  uint8_t b,
  const uint8_t lastRgb[3],
  bool initialized) {
  if (!initialized) {
    return true;
  }

  if (r == 0 && g == 0 && b == 0 && (lastRgb[0] != 0 || lastRgb[1] != 0 || lastRgb[2] != 0)) {
    return true;
  }

  const int rDifference =
    abs((int)r - (int)lastRgb[0]);

  const int gDifference =
    abs((int)g - (int)lastRgb[1]);

  const int bDifference =
    abs((int)b - (int)lastRgb[2]);

  return rDifference > 1 || gDifference > 1 || bDifference > 1;
}

void setChainRgb(
  uint8_t deviceId,
  uint8_t r,
  uint8_t g,
  uint8_t b,
  uint8_t lastRgb[3],
  bool &initialized) {
  if (deviceId == 0 || !rgbNeedsUpdate(r, g, b, lastRgb, initialized)) {
    return;
  }

  uint8_t rgb[3] = {
    r,
    g,
    b
  };

  M5Chain.setRGBValue(
    deviceId,
    0,
    1,
    rgb,
    sizeof(rgb),
    &opr_status);

  lastRgb[0] = r;
  lastRgb[1] = g;
  lastRgb[2] = b;

  initialized = true;
}

void setEncoderRgb(
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  setChainRgb(
    encoder_id,
    r,
    g,
    b,
    lastEncoderLedRgb,
    encoderLedRgbInitialized);
}

void setAngleRgb(
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  setChainRgb(
    angle_id,
    r,
    g,
    b,
    lastAngleLedRgb,
    angleLedRgbInitialized);
}

uint8_t scaleBootChannel(
  uint8_t channel,
  float level) {
  return (uint8_t)(channel * level);
}

float gammaCorrect(float x);
void setRgbMatrixSolid(
  float level,
  uint8_t squareSize);
void updateRgbMatrix();

// ============================================================
// Boot animation
// ============================================================

void playBootLedAnimation() {
  DualKeyLeds.clear();
  DualKeyLeds.show();

  setEncoderRgb(
    0,
    0,
    0);

  setAngleRgb(
    0,
    0,
    0);

  setRgbMatrixSolid(
    0.0f,
    0);

  delay(80);

  // 1. DualKey LEFT = ORA4

  DualKeyLeds.setPixelColor(
    DUALKEY_LEFT_LED_INDEX,
    DualKeyLeds.Color(
      scaleBootChannel(
        ORA4_R,
        BOOT_SEQUENCE_LEVEL),

      scaleBootChannel(
        ORA4_G,
        BOOT_SEQUENCE_LEVEL),

      scaleBootChannel(
        ORA4_B,
        BOOT_SEQUENCE_LEVEL)));

  DualKeyLeds.show();

  delay(BOOT_STEP_MS);

  // 2. DualKey RIGHT = Studio Display

  DualKeyLeds.setPixelColor(
    DUALKEY_RIGHT_LED_INDEX,
    DualKeyLeds.Color(
      scaleBootChannel(
        STUDIO_R,
        BOOT_SEQUENCE_LEVEL),

      scaleBootChannel(
        STUDIO_G,
        BOOT_SEQUENCE_LEVEL),

      scaleBootChannel(
        STUDIO_B,
        BOOT_SEQUENCE_LEVEL)));

  DualKeyLeds.show();

  delay(BOOT_STEP_MS);

  // 3. Encoder

  setEncoderRgb(
    scaleBootChannel(
      MUTE_R,
      BOOT_SEQUENCE_LEVEL),

    scaleBootChannel(
      MUTE_G,
      BOOT_SEQUENCE_LEVEL),

    scaleBootChannel(
      MUTE_B,
      BOOT_SEQUENCE_LEVEL));

  delay(BOOT_STEP_MS);

  // 4. Angle

  setAngleRgb(
    scaleBootChannel(
      ANGLE_R,
      BOOT_SEQUENCE_LEVEL),

    scaleBootChannel(
      ANGLE_G,
      BOOT_SEQUENCE_LEVEL),

    scaleBootChannel(
      ANGLE_B,
      BOOT_SEQUENCE_LEVEL));

  delay(BOOT_STEP_MS);

  // 5. Chain RGB

  setRgbMatrixSolid(
    RGB_MATRIX_ACTIVE_MAX_LEVEL,
    RGB_MATRIX_WIDTH);

  if (rgb_id != 0 && rgbLastStatus == CHAIN_OK) {
    Serial.printf(
      "[RGB] boot test OK id=%u status=0x%02X operation=0x%02X\n",
      rgb_id,
      (unsigned int)rgbLastStatus,
      rgbLastOperationStatus);
  } else if (rgb_id != 0) {
    Serial.printf(
      "[RGB] boot test FAILED id=%u status=0x%02X operation=0x%02X\n",
      rgb_id,
      (unsigned int)rgbLastStatus,
      rgbLastOperationStatus);
  }

  delay(BOOT_STEP_MS);

  // READY

  DualKeyLeds.setPixelColor(
    DUALKEY_LEFT_LED_INDEX,
    DualKeyLeds.Color(
      ORA4_R,
      ORA4_G,
      ORA4_B));

  DualKeyLeds.setPixelColor(
    DUALKEY_RIGHT_LED_INDEX,
    DualKeyLeds.Color(
      STUDIO_R,
      STUDIO_G,
      STUDIO_B));

  DualKeyLeds.show();

  setEncoderRgb(
    MUTE_R,
    MUTE_G,
    MUTE_B);

  setAngleRgb(
    ANGLE_R,
    ANGLE_G,
    ANGLE_B);

  setRgbMatrixSolid(
    RGB_MATRIX_ACTIVE_MAX_LEVEL,
    RGB_MATRIX_WIDTH);

  delay(
    BOOT_READY_HOLD_MS);

  // Smooth fade-out

  const float standbyOutputLevel =
    gammaCorrect(
      LED_STANDBY_LEVEL);

  for (int step = BOOT_FADE_STEPS;
       step >= 0;
       --step) {
    const float level =
      (float)step / (float)BOOT_FADE_STEPS;

    const float corrected =
      powf(
        level,
        1.8f);

    const float outputLevel =
      standbyOutputLevel + (1.0f - standbyOutputLevel) * corrected;

    DualKeyLeds.setPixelColor(
      DUALKEY_LEFT_LED_INDEX,
      DualKeyLeds.Color(
        scaleBootChannel(
          ORA4_R,
          outputLevel),

        scaleBootChannel(
          ORA4_G,
          outputLevel),

        scaleBootChannel(
          ORA4_B,
          outputLevel)));

    DualKeyLeds.setPixelColor(
      DUALKEY_RIGHT_LED_INDEX,
      DualKeyLeds.Color(
        scaleBootChannel(
          STUDIO_R,
          outputLevel),

        scaleBootChannel(
          STUDIO_G,
          outputLevel),

        scaleBootChannel(
          STUDIO_B,
          outputLevel)));

    DualKeyLeds.show();

    setEncoderRgb(
      scaleBootChannel(
        MUTE_R,
        outputLevel),

      scaleBootChannel(
        MUTE_G,
        outputLevel),

      scaleBootChannel(
        MUTE_B,
        outputLevel));

    setAngleRgb(
      scaleBootChannel(
        ANGLE_R,
        outputLevel),

      scaleBootChannel(
        ANGLE_G,
        outputLevel),

      scaleBootChannel(
        ANGLE_B,
        outputLevel));

    setRgbMatrixSolid(
      outputLevel,
      RGB_MATRIX_WIDTH);

    delay(
      BOOT_FADE_STEP_MS);
  }

  angleLedLevel =
    LED_STANDBY_LEVEL;

  angleLedFading = false;

  setRgbMatrixSolid(
    0.0f,
    0);

  ledsDirty = true;
}

// ============================================================
// Gamma correction
// ============================================================

float gammaCorrect(float x) {
  if (x < 0.0f) {
    x = 0.0f;
  }

  if (x > 1.0f) {
    x = 1.0f;
  }

  return powf(
    x,
    2.2f);
}

// ============================================================
// 1/f-like breathing waveform
// ============================================================

float getLedBreathingLevel(
  uint32_t nowMs) {
  const float t =
    nowMs / 1000.0f;

  const float mainWave =
    0.5f + 0.5f * sinf(2.0f * PI * t / 3.2f);

  const float slowWave1 =
    sinf(
      2.0f * PI * t / 7.1f + 0.8f);

  const float slowWave2 =
    sinf(
      2.0f * PI * t / 13.7f + 2.1f);

  const float fluctuation =
    LED_FLUCTUATION_STRENGTH * (0.65f * slowWave1 + 0.35f * slowWave2);

  float level =
    mainWave + fluctuation;

  if (level < 0.0f) {
    level = 0.0f;
  }

  if (level > 1.0f) {
    level = 1.0f;
  }

  return level;
}

// ============================================================
// Breathing level helpers
// ============================================================

float getAudioLedLevel(
  uint32_t nowMs) {
  const float wave =
    getLedBreathingLevel(
      nowMs);

  return AUDIO_LED_MIN_LEVEL + (AUDIO_LED_MAX_LEVEL - AUDIO_LED_MIN_LEVEL) * wave;
}

float getMuteLedLevel(
  uint32_t nowMs) {
  const float wave =
    getLedBreathingLevel(
      nowMs);

  return MUTE_LED_MIN_LEVEL + (MUTE_LED_MAX_LEVEL - MUTE_LED_MIN_LEVEL) * wave;
}

// ============================================================
// RGB scale helper
// ============================================================

void scaleRgb(
  uint8_t baseR,
  uint8_t baseG,
  uint8_t baseB,
  float level,
  uint8_t &outR,
  uint8_t &outG,
  uint8_t &outB) {
  const float corrected =
    gammaCorrect(
      level);

  outR =
    (uint8_t)(baseR * corrected);

  outG =
    (uint8_t)(baseG * corrected);

  outB =
    (uint8_t)(baseB * corrected);
}

// ============================================================
// RGB565
// ============================================================

uint16_t makeRgb565(
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ============================================================
// Chain RGB Matrix
// ============================================================

void setRgbMatrixSolid(
  float level,
  uint8_t squareSize) {
  if (rgb_id == 0) {
    return;
  }

  if (squareSize > RGB_MATRIX_WIDTH) {
    squareSize =
      RGB_MATRIX_WIDTH;
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  scaleRgb(
    RGB_MATRIX_R,
    RGB_MATRIX_G,
    RGB_MATRIX_B,
    level,
    r,
    g,
    b);

  const uint16_t color =
    makeRgb565(
      r,
      g,
      b);

  uint16_t buffer[RGB_MATRIX_PIXEL_COUNT] = { 0 };

  const uint8_t start =
    (RGB_MATRIX_WIDTH - squareSize) / 2;

  for (uint8_t y = start;
       y < start + squareSize;
       ++y) {
    for (uint8_t x = start;
         x < start + squareSize;
         ++x) {
      buffer[y * RGB_MATRIX_WIDTH + x] = color;
    }
  }

  if (rgbMatrixInitialized && memcmp(buffer, lastRgbMatrix, sizeof(buffer)) == 0) {
    return;
  }

  rgbLastStatus =
    M5Chain.setRGBBufferRefresh(
      rgb_id,
      buffer,
      &rgbLastOperationStatus);

  if (rgbLastStatus == CHAIN_OK) {
    memcpy(
      lastRgbMatrix,
      buffer,
      sizeof(buffer));

    rgbMatrixInitialized = true;
  }
}

void updateRgbMatrix() {
  if (!tofDistanceValid || tofDistanceMm >= TOF_FAR_DISTANCE_MM) {
    matrixProximity = 0.0f;
    matrixLevel = 0.0f;
    matrixSquareSize = 0;
  } else {
    const uint16_t clampedDistance =
      max(
        TOF_NEAR_DISTANCE_MM,
        min(
          TOF_FAR_DISTANCE_MM,
          tofDistanceMm));

    matrixProximity =
      (float)(TOF_FAR_DISTANCE_MM - clampedDistance) / (float)(TOF_FAR_DISTANCE_MM - TOF_NEAR_DISTANCE_MM);

    matrixSquareSize =
      (uint8_t)(2.0f + matrixProximity * 6.0f + 0.5f);

    matrixLevel =
      RGB_MATRIX_STANDBY_LEVEL + (RGB_MATRIX_ACTIVE_MAX_LEVEL - RGB_MATRIX_STANDBY_LEVEL) * matrixProximity;
  }

  if (rgb_id == 0) {
    if (millis() - lastMatrixDiagnosticMs >= DIAGNOSTIC_LOG_INTERVAL_MS) {
      lastMatrixDiagnosticMs =
        millis();

      Serial.printf(
        "[Matrix] rgb_id=0 distance=%u level=%.2f stage=%u\n",
        tofDistanceMm,
        matrixLevel,
        matrixSquareSize);
    }

    return;
  }

  setRgbMatrixSolid(
    matrixLevel,
    matrixSquareSize);

  const uint32_t now =
    millis();

  if (now - lastMatrixDiagnosticMs >= DIAGNOSTIC_LOG_INTERVAL_MS) {
    lastMatrixDiagnosticMs = now;

    Serial.printf(
      "[Matrix] distance=%u mm level=%.2f stage=%u proximity=%.2f rgb_id=%u draw_status=0x%02X operation=0x%02X\n",
      tofDistanceMm,
      matrixLevel,
      matrixSquareSize,
      matrixProximity,
      rgb_id,
      (unsigned int)rgbLastStatus,
      rgbLastOperationStatus);
  }
}

// ============================================================
// DualKey LEDs
// ============================================================

void updateDualKeyLeds(
  uint32_t nowMs) {
  DualKeyLeds.clear();

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  // LEFT standby

  scaleRgb(
    ORA4_R,
    ORA4_G,
    ORA4_B,
    LED_STANDBY_LEVEL,
    r,
    g,
    b);

  DualKeyLeds.setPixelColor(
    DUALKEY_LEFT_LED_INDEX,
    DualKeyLeds.Color(
      r,
      g,
      b));

  // RIGHT standby

  scaleRgb(
    STUDIO_R,
    STUDIO_G,
    STUDIO_B,
    LED_STANDBY_LEVEL,
    r,
    g,
    b);

  DualKeyLeds.setPixelColor(
    DUALKEY_RIGHT_LED_INDEX,
    DualKeyLeds.Color(
      r,
      g,
      b));

  const float activeLevel =
    getAudioLedLevel(
      nowMs);

  if (currentAudioOutput == AudioOutput::ORA4) {
    scaleRgb(
      ORA4_R,
      ORA4_G,
      ORA4_B,
      activeLevel,
      r,
      g,
      b);

    DualKeyLeds.setPixelColor(
      DUALKEY_LEFT_LED_INDEX,
      DualKeyLeds.Color(
        r,
        g,
        b));
  } else if (
    currentAudioOutput == AudioOutput::STUDIO_DISPLAY) {
    scaleRgb(
      STUDIO_R,
      STUDIO_G,
      STUDIO_B,
      activeLevel,
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

  // Action > State

  if (dualKeyLeftFlashActive) {
    if (nowMs - dualKeyLeftFlashStartMs < DUALKEY_FLASH_DURATION_MS) {
      scaleRgb(
        ORA4_R,
        ORA4_G,
        ORA4_B,
        DUALKEY_FLASH_LEVEL,
        r,
        g,
        b);

      DualKeyLeds.setPixelColor(
        DUALKEY_LEFT_LED_INDEX,
        DualKeyLeds.Color(
          r,
          g,
          b));
    } else {
      dualKeyLeftFlashActive =
        false;
    }
  }

  if (dualKeyRightFlashActive) {
    if (nowMs - dualKeyRightFlashStartMs < DUALKEY_FLASH_DURATION_MS) {
      scaleRgb(
        STUDIO_R,
        STUDIO_G,
        STUDIO_B,
        DUALKEY_FLASH_LEVEL,
        r,
        g,
        b);

      DualKeyLeds.setPixelColor(
        DUALKEY_RIGHT_LED_INDEX,
        DualKeyLeds.Color(
          r,
          g,
          b));
    } else {
      dualKeyRightFlashActive =
        false;
    }
  }

  DualKeyLeds.show();
}

// ============================================================
// Encoder LED
// ============================================================

void updateEncoderLed(
  uint32_t nowMs) {
  if (encoder_id == 0) {
    return;
  }

  float stateLevel =
    LED_STANDBY_LEVEL;

  if (muted) {
    stateLevel =
      getMuteLedLevel(
        nowMs);
  }

  float outputLevel =
    stateLevel;

  if (encoderImpulseActive) {
    const uint32_t elapsed =
      nowMs - encoderImpulseStartMs;

    if (elapsed < ENCODER_IMPULSE_DECAY_MS) {
      const float progress =
        (float)elapsed / (float)ENCODER_IMPULSE_DECAY_MS;

      const float remaining =
        1.0f - progress;

      const float decay =
        remaining * remaining;

      outputLevel =
        stateLevel + (ENCODER_IMPULSE_PEAK_LEVEL - stateLevel) * decay;
    } else {
      encoderImpulseActive =
        false;
    }
  }

  uint8_t encoderRgb[3] = {
    0,
    0,
    0
  };

  scaleRgb(
    MUTE_R,
    MUTE_G,
    MUTE_B,
    outputLevel,
    encoderRgb[0],
    encoderRgb[1],
    encoderRgb[2]);

  setEncoderRgb(
    encoderRgb[0],
    encoderRgb[1],
    encoderRgb[2]);
}

// ============================================================
// Angle LED
// ============================================================

void updateAngleLed(
  uint32_t nowMs) {
  if (angle_id == 0) {
    return;
  }

  float targetLevel =
    LED_STANDBY_LEVEL;

  if (scrollState != ScrollState::STOPPED) {
    angleLedFading = false;

    const float activityCurve =
      powf(
        angleActivityLevel,
        ANGLE_LED_ACTIVITY_EXPONENT);

    targetLevel =
      ANGLE_LED_MIN_LEVEL + (1.0f - ANGLE_LED_MIN_LEVEL) * activityCurve;
  } else if (
    angleLedLevel > LED_STANDBY_LEVEL) {
    if (!angleLedFading) {
      angleLedFading = true;

      angleFadeStartMs =
        nowMs;

      angleFadeStartLevel =
        angleLedLevel;
    }

    const uint32_t fadeElapsed =
      nowMs - angleFadeStartMs;

    if (fadeElapsed < ANGLE_LED_FADE_MS) {
      const float fadeProgress =
        (float)fadeElapsed / (float)ANGLE_LED_FADE_MS;

      targetLevel =
        LED_STANDBY_LEVEL + (angleFadeStartLevel - LED_STANDBY_LEVEL) * (1.0f - fadeProgress);
    } else {
      angleLedFading =
        false;
    }
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  scaleRgb(
    ANGLE_R,
    ANGLE_G,
    ANGLE_B,
    targetLevel,
    r,
    g,
    b);

  setAngleRgb(
    r,
    g,
    b);

  angleLedLevel =
    targetLevel;
}

// ============================================================
// Initialize local DualKey LEDs
// ============================================================

void initLeds() {
  pinMode(
    DUALKEY_LED_POWER_PIN,
    OUTPUT);

  digitalWrite(
    DUALKEY_LED_POWER_PIN,
    HIGH);

  DualKeyLeds.begin();

  DualKeyLeds.setBrightness(
    255);

  DualKeyLeds.clear();
  DualKeyLeds.show();
}

// ============================================================
// Initialize Chain LEDs
// ============================================================

void initChainLeds() {
  uint8_t off[3] = {
    0,
    0,
    0
  };

  if (encoder_id != 0) {
    M5Chain.setRGBLight(
      encoder_id,
      CHAIN_LED_BRIGHTNESS,
      &opr_status);

    M5Chain.setRGBValue(
      encoder_id,
      0,
      1,
      off,
      sizeof(off),
      &opr_status);
  }

  if (angle_id != 0) {
    M5Chain.setRGBLight(
      angle_id,
      CHAIN_LED_BRIGHTNESS,
      &opr_status);

    M5Chain.setRGBValue(
      angle_id,
      0,
      1,
      off,
      sizeof(off),
      &opr_status);
  }

  if (rgb_id != 0) {
    rgbLastStatus =
      M5Chain.setRGBMode(
        rgb_id,
        RGB_PIXEL_MODE,
        &rgbLastOperationStatus);

    rgbLastStatus =
      M5Chain.setRGBBrightness(
        rgb_id,
        RGB_MATRIX_BRIGHTNESS,
        &rgbLastOperationStatus);

    rgbLastStatus =
      M5Chain.setRGBClear(
        rgb_id,
        &rgbLastOperationStatus);

    Serial.printf(
      "[RGB] init id=%u status=0x%02X operation=0x%02X\n",
      rgb_id,
      (unsigned int)rgbLastStatus,
      rgbLastOperationStatus);
  }
}

// ============================================================
// Main LED updater
// ============================================================

void updateLeds() {
  const uint32_t now =
    millis();

  if (!ledsDirty && now - lastLedUpdateMs < LED_UPDATE_INTERVAL_MS) {
    return;
  }

  lastLedUpdateMs = now;

  updateDualKeyLeds(
    now);

  updateEncoderLed(
    now);

  updateAngleLed(
    now);

  updateRgbMatrix();

  ledsDirty = false;
}

// ============================================================
// Chain initialization
// ============================================================

bool initChainDevices() {
  M5Chain.begin(
    &Serial2,
    115200,
    CHAIN_RXD_PIN,
    CHAIN_TXD_PIN);

  const uint32_t timeout =
    millis() + 3000;

  while (!M5Chain.isDeviceConnected()) {
    if ((int32_t)(millis() - timeout) >= 0) {
      Serial.println(
        "[WARN] Chain not detected");

      return false;
    }

    delay(50);
  }

  M5Chain.getDeviceNum(
    &device_count);

  if (device_count == 0) {
    Serial.println(
      "[WARN] Chain device list is empty");

    return false;
  }

  device_list =
    (device_list_t *)malloc(
      sizeof(device_list_t));

  if (!device_list) {
    Serial.println(
      "[WARN] Chain device list allocation failed");

    return false;
  }

  device_list->count =
    device_count;

  device_list->devices =
    (device_info_t *)malloc(
      sizeof(device_info_t) * device_count);

  if (!device_list->devices) {
    free(
      device_list);

    device_list =
      nullptr;

    Serial.println(
      "[WARN] Chain device entry allocation failed");

    return false;
  }

  M5Chain.getDeviceList(
    device_list);

  Serial.printf(
    "[Chain] device_count = %u\n",
    device_count);

  // ----------------------------------------------------------
  // Discover devices by type
  // ----------------------------------------------------------

  for (uint16_t i = 0;
       i < device_count;
       ++i) {
    const uint8_t id =
      (uint8_t)
        device_list
          ->devices[i]
          .id;

    const uint16_t type =
      device_list
        ->devices[i]
        .device_type;

    Serial.printf(
      "[Chain] index=%u id=%u type=0x%04X\n",
      i,
      id,
      type);

    if (type == CHAIN_ENCODER_TYPE_CODE) {
      encoder_id = id;
    }

    if (type == CHAIN_ANGLE_TYPE_CODE) {
      angle_id = id;
    }

    if (type == CHAIN_RGB_TYPE_CODE) {
      rgb_id = id;
    }

    if (type == CHAIN_TOF_TYPE_CODE) {
      tof_id = id;
    }
  }

  // ----------------------------------------------------------
  // Encoder setup
  // ----------------------------------------------------------

  if (encoder_id != 0) {
    M5Chain.setEncoderABDirect(
      encoder_id,
      ENCODER_AB,
      &opr_status);

    M5Chain.setEncoderButtonTriggerInterval(
      encoder_id,
      BUTTON_DOUBLE_CLICK_TIME_500MS,
      BUTTON_LONG_PRESS_TIME_5S,
      &opr_status);
  }

  // ==========================================================
  // ToF setup
  //
  // Official SINGLE mode sequence:
  //
  // SINGLE
  // -> measurement time 33 ms
  // -> START
  //
  // updateTof() will:
  //
  // status + complete flag
  // -> distance
  // -> START again
  // ==========================================================

  if (tof_id != 0) {
    opr_status = 0;

    chain_status_t tofSetupStatus =
      M5Chain.setToFMeasureMode(
        tof_id,
        CHAIN_TOF_MODE_SINGLE,
        &opr_status);

    Serial.printf(
      "[ToF] mode=SINGLE status=0x%02X operation=0x%02X\n",
      (unsigned int)tofSetupStatus,
      opr_status);

    opr_status = 0;

    tofSetupStatus =
      M5Chain.setToFMeasureTime(
        tof_id,
        TOF_MEASUREMENT_TIME_MS,
        &opr_status);

    Serial.printf(
      "[ToF] measurement_time=%u ms status=0x%02X operation=0x%02X\n",
      TOF_MEASUREMENT_TIME_MS,
      (unsigned int)tofSetupStatus,
      opr_status);

    // SINGLE mode requires explicit START.

    opr_status = 0;

    tofSetupStatus =
      M5Chain.setToFMeasureStatus(
        tof_id,
        CHAIN_TOF_STATUS_START,
        &opr_status);

    Serial.printf(
      "[ToF] initial START status=0x%02X operation=0x%02X\n",
      (unsigned int)tofSetupStatus,
      opr_status);

    lastTofReadMs =
      millis();

    tofDistanceValid =
      false;

    tofLastCompleteFlag =
      0;
  }

  Serial.printf(
    "[Chain] encoder_id = %u\n",
    encoder_id);

  Serial.printf(
    "[Chain] angle_id   = %u\n",
    angle_id);

  Serial.printf(
    "[Chain] rgb_id     = %u\n",
    rgb_id);

  Serial.printf(
    "[Chain] tof_id     = %u\n",
    tof_id);

  if (rgb_id == 0) {
    Serial.println(
      "[WARN] Chain RGB not detected");
  }

  if (tof_id == 0) {
    Serial.println(
      "[WARN] Chain ToF not detected");
  }

  return true;
}

// ============================================================
// Angle processing
// ============================================================

void updateAngle() {
  if (angle_id == 0) {
    return;
  }

  const uint32_t now =
    millis();

  if (now - lastAngleReadMs >= ANGLE_READ_INTERVAL_MS) {
    lastAngleReadMs = now;

    uint16_t value =
      angleCenter;

    M5Chain.getAngle12BitAdc(
      angle_id,
      &value);

    angleValue =
      value;
  }

  const int32_t center =
    (int32_t)angleCenter;

  const int32_t stopLow =
    center - ANGLE_STOP_OFFSET;

  const int32_t stopHigh =
    center + ANGLE_STOP_OFFSET;

  const int32_t startLow =
    center - ANGLE_START_OFFSET;

  const int32_t startHigh =
    center + ANGLE_START_OFFSET;

  switch (scrollState) {
    case ScrollState::STOPPED:

      if ((int32_t)angleValue <= startLow) {
        scrollState =
          ScrollState::UP;
      } else if ((int32_t)angleValue >= startHigh) {
        scrollState =
          ScrollState::DOWN;
      } else {
        angleActivityLevel = 0.0f;

        return;
      }

      break;

    case ScrollState::UP:

      if ((int32_t)angleValue >= stopLow) {
        scrollState =
          ScrollState::STOPPED;

        angleActivityLevel = 0.0f;

        lastScrollMs = now;

        return;
      }

      break;

    case ScrollState::DOWN:

      if ((int32_t)angleValue <= stopHigh) {
        scrollState =
          ScrollState::STOPPED;

        angleActivityLevel = 0.0f;

        lastScrollMs = now;

        return;
      }

      break;
  }

  float normalizedDistance =
    0.0f;

  if (scrollState == ScrollState::UP) {
    if ((int32_t)angleValue < startLow) {
      const float distance =
        (float)(startLow - (int32_t)angleValue);

      const float usableRange =
        (float)(startLow - (int32_t)ANGLE_MIN);

      if (usableRange > 0.0f) {
        normalizedDistance =
          distance / usableRange;
      }
    }
  } else if (
    scrollState == ScrollState::DOWN) {
    if ((int32_t)angleValue > startHigh) {
      const float distance =
        (float)((int32_t)angleValue - startHigh);

      const float usableRange =
        (float)((int32_t)ANGLE_MAX - startHigh);

      if (usableRange > 0.0f) {
        normalizedDistance =
          distance / usableRange;
      }
    }
  }

  if (normalizedDistance < 0.0f) {
    normalizedDistance =
      0.0f;
  }

  if (normalizedDistance > 1.0f) {
    normalizedDistance =
      1.0f;
  }

  angleActivityLevel =
    normalizedDistance;

  const float curve =
    powf(
      normalizedDistance,
      1.5f);

  const float speed =
    SCROLL_BASE_SPEED + (1.0f - SCROLL_BASE_SPEED) * curve;

  const uint32_t interval =
    SCROLL_SLOWEST_INTERVAL_MS - (uint32_t)(speed * (SCROLL_SLOWEST_INTERVAL_MS - SCROLL_FASTEST_INTERVAL_MS));

  if (now - lastScrollMs >= interval) {
    lastScrollMs = now;

    if (scrollState == ScrollState::UP) {
      Mouse.move(
        0,
        0,
        SCROLL_UP_STEP);
    } else if (
      scrollState == ScrollState::DOWN) {
      Mouse.move(
        0,
        0,
        SCROLL_DOWN_STEP);
    }
  }
}

// ============================================================
// ToF processing
//
// Official SINGLE sequence:
//
// START
//   ↓
// wait until:
// measure_status == STOP
// complete_flag == 1
//   ↓
// get distance
//   ↓
// START next measurement
// ============================================================

void updateTof() {
  if (tof_id == 0) {
    return;
  }

  const uint32_t now =
    millis();

  if (now - lastTofReadMs < TOF_READ_INTERVAL_MS) {
    return;
  }

  lastTofReadMs = now;

  // ----------------------------------------------------------
  // Measurement status
  // ----------------------------------------------------------

  chain_tof_measure_status_t
    measureStatus =
      CHAIN_TOF_STATUS_STOP;

  chain_status_t
    measureStatusResult =
      M5Chain.getToFMeasureStatus(
        tof_id,
        &measureStatus);

  // ----------------------------------------------------------
  // Measurement completion flag
  // ----------------------------------------------------------

  uint8_t completeFlag = 0;

  tofCompleteStatus =
    M5Chain.getToFMeasureCompleteFlag(
      tof_id,
      &completeFlag);

  tofLastCompleteFlag =
    completeFlag;

  // ----------------------------------------------------------
  // Official SINGLE completion condition
  // ----------------------------------------------------------

  if (measureStatusResult == CHAIN_OK && tofCompleteStatus == CHAIN_OK && measureStatus == CHAIN_TOF_STATUS_STOP && completeFlag == 1) {
    uint16_t distance = 0;

    tofLastStatus =
      M5Chain.getToFDistance(
        tof_id,
        &distance);

    if (tofLastStatus == CHAIN_OK) {
      tofRawDistanceMm =
        distance;

      // Existing low-pass filter:
      // previous 75%, new 25%

      if (!tofDistanceValid) {
        tofDistanceMm =
          distance;
      } else {
        tofDistanceMm =
          (uint16_t)((tofDistanceMm * 3UL + distance) / 4UL);
      }

      tofDistanceValid =
        true;

      lastTofSuccessMs =
        now;

      // Let RGB Matrix react immediately.

      ledsDirty =
        true;
    }

    // --------------------------------------------------------
    // Restart next SINGLE measurement
    // --------------------------------------------------------

    opr_status = 0;

    chain_status_t restartStatus =
      M5Chain.setToFMeasureStatus(
        tof_id,
        CHAIN_TOF_STATUS_START,
        &opr_status);

    if (restartStatus != CHAIN_OK) {
      Serial.printf(
        "[ToF] restart failed status=0x%02X operation=0x%02X\n",
        (unsigned int)restartStatus,
        opr_status);
    }
  }

  // ----------------------------------------------------------
  // Invalidate stale measurement
  // ----------------------------------------------------------

  if (tofDistanceValid && now - lastTofSuccessMs >= TOF_INVALIDATION_TIMEOUT_MS) {
    tofDistanceValid =
      false;

    ledsDirty =
      true;
  }

  // ----------------------------------------------------------
  // Diagnostics
  // ----------------------------------------------------------

  if (now - lastTofSerialMs >= DIAGNOSTIC_LOG_INTERVAL_MS) {
    lastTofSerialMs = now;

    Serial.printf(
      "[ToF] id=%u measure=%u complete=%u raw=%u mm filtered=%u mm valid=%u read_status=0x%02X complete_status=0x%02X\n",
      tof_id,
      (unsigned int)measureStatus,
      completeFlag,
      tofRawDistanceMm,
      tofDistanceMm,
      tofDistanceValid ? 1 : 0,
      (unsigned int)tofLastStatus,
      (unsigned int)tofCompleteStatus);
  }
}

// ============================================================
// setup
// ============================================================

void setup() {
  Serial.begin(
    115200);

  delay(300);

  pinMode(
    PIN_KEY1,
    INPUT);

  pinMode(
    PIN_KEY2,
    INPUT);

  Keyboard.begin();
  ConsumerControl.begin();
  Mouse.begin();
  USB.begin();

  initLeds();

  initChainDevices();

  initChainLeds();

  // USB-Cを背面とした正面から
  //
  // DualKey左
  // -> DualKey右
  // -> Encoder
  // -> Angle
  // -> Chain RGB

  playBootLedAnimation();

  // 起動時はAngleを物理的な中央位置に置いておく

  calibrateAngleCenter();

  // 起動後の通常LED状態を反映

  updateLeds();
}

// ============================================================
// DualKey processing
// ============================================================

void updateDualKey() {
  const uint32_t now =
    millis();

  Key1.setRawState(
    now,
    !digitalRead(
      PIN_KEY_LEFT));

  Key2.setRawState(
    now,
    !digitalRead(
      PIN_KEY_RIGHT));

  const bool leftPressed =
    Key1.isPressed();

  const bool rightPressed =
    Key2.isPressed();

  if (Key1.wasPressed()) {
    triggerDualKeyFlash(
      PendingKey::LEFT);
  }

  if (Key2.wasPressed()) {
    triggerDualKeyFlash(
      PendingKey::RIGHT);
  }

  // 両押し最優先

  if (leftPressed && rightPressed) {
    if (!chordConsumed) {
      pending =
        PendingKey::NONE;

      sendAudioToggle();

      toggleAudioOutputState();

      chordConsumed = true;
      singleConsumed = false;
    }

    return;
  }

  if (chordConsumed) {
    if (!leftPressed && !rightPressed) {
      chordConsumed =
        false;
    }

    return;
  }

  if (singleConsumed) {
    if (!leftPressed && !rightPressed) {
      singleConsumed =
        false;
    }

    return;
  }

  if (pending == PendingKey::NONE) {
    if (leftPressed && !rightPressed) {
      pending =
        PendingKey::LEFT;

      pendingSince =
        now;
    } else if (
      rightPressed && !leftPressed) {
      pending =
        PendingKey::RIGHT;

      pendingSince =
        now;
    }
  }

  if (pending != PendingKey::NONE && now - pendingSince >= CHORD_WINDOW_MS) {
    if (pending == PendingKey::LEFT && leftPressed) {
      sendOra4();

      setAudioOutputState(
        AudioOutput::ORA4);

      singleConsumed =
        true;
    } else if (
      pending == PendingKey::RIGHT && rightPressed) {
      sendStudioDisplay();

      setAudioOutputState(
        AudioOutput::STUDIO_DISPLAY);

      singleConsumed =
        true;
    }

    pending =
      PendingKey::NONE;
  }

  if (pending == PendingKey::LEFT && Key1.wasReleased()) {
    sendOra4();

    setAudioOutputState(
      AudioOutput::ORA4);

    pending =
      PendingKey::NONE;

    singleConsumed =
      true;
  }

  if (pending == PendingKey::RIGHT && Key2.wasReleased()) {
    sendStudioDisplay();

    setAudioOutputState(
      AudioOutput::STUDIO_DISPLAY);

    pending =
      PendingKey::NONE;

    singleConsumed =
      true;
  }
}

// ============================================================
// Encoder processing
// ============================================================

void updateEncoder() {
  if (encoder_id == 0) {
    return;
  }

  // ----------------------------------------------------------
  // Rotation
  // ----------------------------------------------------------

  int16_t currentValue = 0;

  M5Chain.getEncoderValue(
    encoder_id,
    &currentValue);

  if (!encoderValueInitialized) {
    lastEncoderValue =
      currentValue;

    encoderValueInitialized =
      true;
  } else {
    const int16_t delta =
      currentValue - lastEncoderValue;

    if (delta > 0) {
      volumeUp();

      triggerEncoderImpulse();

      lastEncoderValue =
        currentValue;
    } else if (delta < 0) {
      volumeDown();

      triggerEncoderImpulse();

      lastEncoderValue =
        currentValue;
    }
  }

  // ----------------------------------------------------------
  // Push button
  // ----------------------------------------------------------

  uint8_t buttonStatus = 0;

  M5Chain.getEncoderButtonStatus(
    encoder_id,
    &buttonStatus);

  const bool pressed =
    (buttonStatus != 0);

  const uint32_t now =
    millis();

  if (pressed != lastEncoderButton && (now - lastEncoderButtonChangeMs) >= ENCODER_BUTTON_DEBOUNCE_MS) {
    lastEncoderButtonChangeMs =
      now;

    if (pressed) {
      toggleMute();

      toggleMuteState();
    }

    lastEncoderButton =
      pressed;
  }
}

// ============================================================
// loop
// ============================================================

void loop() {
  updateDualKey();

  updateEncoder();

  updateAngle();

  updateTof();

  updateLeds();

  delay(2);
}