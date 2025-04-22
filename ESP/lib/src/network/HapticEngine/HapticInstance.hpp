#pragma once
#ifndef HAPTIC_INSTANCE_HPP
#define HAPTIC_INSTANCE_HPP
#include <Arduino.h>

struct HapticInstance {
  int gpioPin;
  int strength = 0; // Precentage
  String name = "none";

  HapticInstance()
    : gpioPin(-1),
      strength(0),
      name("undefined")
  {}
  
  HapticInstance(int pin, const String& nm)
    : gpioPin(pin),
      strength(0),
      name(nm)
  {
    pinMode(pin, OUTPUT);
    analogWrite(pin, 0);
  }

public: 
  void updateInstance(){
    int mappedValue = map(strength, 0, 100, 0, 255);
    Serial.printf("Haptic \"%s\" set to: %d\n", name);
    analogWrite(gpioPin, mappedValue);
  }
};

#endif // HAPTIC_INSTANCE_HPP