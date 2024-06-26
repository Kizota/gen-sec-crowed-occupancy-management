#pragma once

#include "..\include\entryDetector.hpp"
#include "..\include\my_gate_sensor\headers\IMySensorCalculator.hpp"
#include "..\include\my_gate_sensor\headers\IMySensorOperator.hpp"


class MyUltrasonicSensor : public IMySensorCalculator
{
private:
    uint8_t echo_pin;
    uint8_t trigger_pin;

public:
    MyUltrasonicSensor(uint8_t, uint8_t);
    ~MyUltrasonicSensor();

    void setupSensor();

    float findDistance() override;
};