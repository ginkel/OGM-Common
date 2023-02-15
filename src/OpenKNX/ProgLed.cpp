#include "ProgLed.h"
#include "OpenKNX/Common.h"

namespace OpenKNX
{
    void ProgLed::loop()
    {
        loop(micros());
    }
    
    void ProgLed::loop(uint32_t currentMicros)
    {
        if (_mode == ProgLedMode::Off && _ledState)
        {
            switchLed(_blinkState);
            return;
        }

        if (_mode != ProgLedMode::Debug)
            return;

        if (!(currentMicros - _lastMicros >= OPENKNX_PROGLED_DEBUG_FREQ * 1000))
            return;

        _lastMicros = currentMicros;
        _blinkState = !_blinkState;
        switchLed(_blinkState);
    }

    void ProgLed::switchLed(bool state)
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
            switchLed(_ledState);
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