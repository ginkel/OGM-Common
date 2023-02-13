#include "OpenKNX/Common.h"
#include "ProgLed.h"

namespace OpenKNX
{
    void ProgLed::loop()
    {
        // TODO: PROG_LED_PIN_ACTIVE_ON
        // Dimming

        int frequency;
        if(_ledState)
            frequency = DEBUGPROGLED_FREQ_ON;
        else
            frequency = 1000;
        
        if(delayCheck(_millis, DEBUGPROGLED_FREQ_OFF))
        {
            _millis = millis();
            _blinkState = !_blinkState;
            digitalWrite(PROG_LED_PIN, _blinkState);
        }
    }

    void ProgLed::SetLedState(bool state)
    {
        _ledState = state;
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