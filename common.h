#include <stdint.h>

#define SSB_SUPPORT

// CHANGEME
#define DEFAULT_BAUDRATE 115200
#define OVERSAMPLING_CLOCKS 4
#define PROCESSOR_CLOCK_FREQ 176000000ULL
#define TIMER1_INTERRUPT_FREQ (2000*OVERSAMPLING_CLOCKS)
#define TIMER1_COUNT_MAX (PROCESSOR_CLOCK_FREQ / TIMER1_INTERRUPT_FREQ)

void set_frequency(uint32_t freq, uint8_t clockno);
void set_frequency_receive();
void set_clock_onoff(uint8_t onoff, uint8_t clockno);
void set_clock_onoff_mask(uint8_t on_mask);
void transmit_set(uint8_t set);
void muteaudio_set(uint8_t set);
void idle_task(void);
void delayidle(uint32_t ms);
void tone_on(uint8_t freq, uint8_t vol);
void tone_off(void);
void test_tone(uint16_t freq);

void set_frequency_offset(int16_t offset);
void set_transmit_pwm(uint16_t pwm);
void set_tuning_pwm(uint16_t pwm);

void transmit_pwm_mode(uint8_t set);
