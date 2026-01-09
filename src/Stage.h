#ifndef STAGE_H
#define STAGE_H

#include <Arduino.h>
#include <Preferences.h>

class Stage {
private:
    enum ErrorType { 
        NUM_ERRORS 
    };
    const String errorMessages[NUM_ERRORS] = {};
    uint8_t no_steps = 0;
    uint8_t current_step = 0;
    std::function<void ()> callback_init = NULL;
    std::function<void ()> callback_destroy = NULL;

    void ERROR(ErrorType error);
    void DEBUG_M(const char *message);
public:
    Stage(uint8_t no_stages, std::function<void ()> callback_init = NULL, std::function<void ()> callback_destroy = NULL);

    void init();
    void destroy();
    void nextStep();
    bool isInitialized();
    uint8_t getNoStep();
    uint8_t getCurrentStep();
    void setStep(uint8_t step);
};

#endif // STAGE_H