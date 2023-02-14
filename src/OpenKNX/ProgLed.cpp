#include "ProgLed.h"
#include "OpenKNX/Common.h"

namespace OpenKNX
{
    void ProgLed::loop()
    {
        if (_mode == ProgLedMode::Off && _ledState)
        {
            switchLED(_blinkState);
            return;
        }

        if (_mode != ProgLedMode::Debug)
            return;

        if (!delayCheck(_millis, OPENKNX_PROGLED_DEBUG_FREQ))
            return;

        _millis = millis();
        _blinkState = !_blinkState;
        switchLED(_blinkState);
    }

    void ProgLed::switchLED(bool state)
    {
        if (_mode == ProgLedMode::Off)
            return;

        if (PROG_LED_PIN_ACTIVE_ON == LOW)
            state = !state;

        if (_brightness == 255)
            digitalWrite(PROG_LED_PIN, state);
        else if (_brightness == 0)
            digitalWrite(PROG_LED_PIN, LOW);
        else
            analogWrite(PROG_LED_PIN, state ? _brightness : 0);
    }

    void ProgLed::brightness(uint8_t brightness)
    {
        _brightness = brightness;
    }

    void ProgLed::mode(ProgLedMode mode)
    {
        _mode = mode;
    }

    void ProgLed::state(bool state)
    {
        if (_mode == ProgLedMode::Off)
            return;

        _ledState = state;
        if (_mode == ProgLedMode::Normal)
            switchLED(_ledState);
    }

    void ProgLed::on()
    {
        state(true);
    }

    void ProgLed::off()
    {
        state(false);
    }
} // namespace OpenKNX