#pragma once
#include "knx.h"

#ifndef DEBUGPROGLED_FREQ_ON
#define DEBUGPROGLED_FREQ_ON 200
#endif

#ifndef DEBUGPROGLED_FREQ_OFF
#define DEBUGPROGLED_FREQ_OFF 1000
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
        void SwitchLED(bool state);

      public:
        // Dimm the ProgLED. only useable if PROG_LED_PIN supports PWM (analogWrite)
        uint8_t _brightness = 255;
        ProgLedMode _progLedMode = ProgLedMode::Normal;
        void loop();
        void SetLedState(bool state);
        static void ProgLedOn();
        static void ProgLedOff();
    };
    extern OpenKNX::ProgLed progLed;
} // namespace OpenKNX