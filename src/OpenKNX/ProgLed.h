#pragma once
#include "knx.h"

#ifndef OPENKNX_PROGLED_DEBUG_FREQ
#define OPENKNX_PROGLED_DEBUG_FREQ 1000
#endif

namespace OpenKNX
{
    enum class ProgLedMode
    {
        Normal,
        Debug,
        Off
    };

    class ProgLed
    {
      private:
        uint32_t _millis = 0;
        bool _ledState = false;
        bool _blinkState = false;
        void switchLED(bool state);
        uint8_t _brightness = 255;
        ProgLedMode _mode = ProgLedMode::Normal;

      public:
        void loop();
        // Dimm the ProgLED. only useable if PROG_LED_PIN supports PWM (analogWrite)
        void brightness(uint8_t brightness);
        void mode(ProgLedMode mode);
        void state(bool state);
        void on();
        void off();
    };
} // namespace OpenKNX