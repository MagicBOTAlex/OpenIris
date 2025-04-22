#include <Arduino.h>

struct HapticInstace {
    int gpioPin;
    int strength = 0;
    String name = "none";
    
    HapticInstace(int pin, int str, const String& nm)
      : gpioPin(pin),
        strength(str),
        name(nm)
    {}
};