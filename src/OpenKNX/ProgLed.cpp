#include "OpenKNX/Common.h"
#include "ProgLed.h"

namespace OpenKNX
{
    void ProgLed::loop()
    {
        if(_progLedMode == ProgLedMode::Debug)
        {
            int frequency;
            if (_ledState)
                frequency = DEBUGPROGLED_FREQ_ON;
            else
                frequency = 1000;
            
            if (delayCheck(_millis, DEBUGPROGLED_FREQ_OFF))
            {
                _millis = millis();
                _blinkState = !_blinkState;
                SwitchLED(_blinkState);
            }
        }
    }

    void ProgLed::SetLedState(bool state)
    {
        _ledState = state;
        if (_progLedMode == ProgLedMode::Normal)
        {
            SwitchLED(_ledState);
        }
    }

    void ProgLed::SwitchLED(bool state)
    {
        if (PROG_LED_PIN_ACTIVE_ON == LOW)
            state = !state;

        if (_brightness == 255)
        {
            digitalWrite(PROG_LED_PIN, state);
        }
        else if (_brightness == 0)
        {
            digitalWrite(PROG_LED_PIN, LOW);
        }
        else
        {
            analogWrite(PROG_LED_PIN, state?_brightness:0);
        }
    }

    void ProgLed::ProgLedOn()
    {
        progLed.SetLedState(true);
    }

    void ProgLed::ProgLedOff()
    {
        progLed.SetLedState(false);
    }

    OpenKNX::ProgLed progLed;
} // namespace OpenKNX