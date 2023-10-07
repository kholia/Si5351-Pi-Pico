#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

#include "si5351.h"
#include "si5351simple.h"

#include "pico/stdlib.h"   // stdlib
#include "hardware/irq.h"  // interrupts
#include "hardware/pwm.h"  // pwm
#include "hardware/sync.h" // wait for interrupt

#include "sample.h"
#include "common.h"
#include "ssb.h"

// Audio PIN is to match some of the design guide shields.
#define AUDIO_PIN 28  // you can change this to whatever you like

int wav_position = 0;

extern uint8_t ssb_active;

extern ssb_state ssbs;

si5351simple si5351(8, 26000000u); // ATTENTION: Please change this XTAL frequency as needed!
uint slice_num = pwm_gpio_to_slice_num(22);
uint chan = pwm_gpio_to_channel(22);

// https://www.i-programmer.info/programming/hardware/14849-the-pico-in-c-basic-pwm.html?start=2
uint32_t pwm_set_freq_duty(uint slice_num,
                           uint chan, uint32_t f, int d)
{
  uint32_t clock = 125000000;
  uint32_t divider16 = clock / f / 4096 +
                       (clock % (f * 4096) != 0);
  if (divider16 / 16 == 0)
    divider16 = 16;
  uint32_t wrap = clock * 16 / divider16 / f - 1;
  pwm_set_clkdiv_int_frac(slice_num, divider16 / 16,
                          divider16 & 0xF);
  pwm_set_wrap(slice_num, wrap);
  pwm_set_chan_level(slice_num, chan, wrap * d / 100);
  return wrap;
}

void set_transmit_pwm(uint16_t pwm)
{
  // OCR1A = pwm; // TODO
  pwm_set_freq_duty(slice_num, chan, 50, pwm);
  pwm_set_enabled(slice_num, true);
};

void set_frequency_offset(int16_t offset)
{
  si5351.set_offset_fast(offset);
}

void set_frequency(uint32_t freq, uint8_t clockno)
{
  si5351_synth_regs s_regs;
  si5351_multisynth_regs m_regs;

  if (ssb_active) ssb_state_change(0);
  si5351.calc_registers(freq, 0, 1, &s_regs, &m_regs);
  si5351.set_registers(0, &s_regs, clockno, &m_regs);
  //si5351.print_c_regs();
}

void set_clock_onoff_mask(uint8_t on_mask)
{
  si5351.setOutputOnOffMask(on_mask);
}

void set_clock_onoff(uint8_t onoff, uint8_t clockno)
{
  si5351.setOutputOnOff(clockno, onoff != 0);
}

/*
   PWM Interrupt Handler which outputs PWM level and advances the
   current sample.

   We repeat the same value for 8 cycles this means sample rate etc
   adjust by factor of 8 (this is what bitshifting <<3 is doing)

*/
void pwm_interrupt_handler() {
  uint8_t wdata = 0;
  int16_t sample = 0;
  pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
  if (wav_position < WAV_DATA_LENGTH - 1) {
    wdata = WAV_DATA[wav_position];
    sample = wdata;
    wav_position++;
    ssb_interrupt(sample);
  } else {
    // reset to start
    wav_position = 0;
  }

  /* if (wav_position < (WAV_DATA_LENGTH<<3) - 1) {
  	// set pwm level
  	// allow the pwm value to repeat for 8 cycles this is >>3
  	wdata = WAV_DATA[wav_position>>3];
  	sample = wdata;
  	pwm_set_gpio_level(AUDIO_PIN, wdata);
  	wav_position++;
  	ssb_interrupt(sample);
    } else {
  	// reset to start
  	wav_position = 0;
    } */
}

int main(void)
{
  int code;

  /* Overclocking for fun but then also so the system clock is a
     multiple of typical audio sampling rates.
  */
  stdio_init_all();
  set_sys_clock_khz(176000, true);
  gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
  int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

  // sleep_ms(3000);

  gpio_set_function(22, GPIO_FUNC_PWM);
  pwm_set_freq_duty(slice_num, chan, 50, 75);
  // pwm_set_enabled(slice_num, true); // Dhiru

  // Initialize the Si5351 (Ported Etherkit library code)
  /* si5351_init(0x60, SI5351_CRYSTAL_LOAD_8PF, 26000000, 0); // I am using a 26 MHz TCXO
     si5351_set_freq(30000000ULL * 100ULL, SI5351_CLK0);

     uint64_t begin = 0;
     uint64_t end = 0;

     while (1) {
     begin = time_us_64();
    // si5351_set_freq(30500000ULL * 100ULL, SI5351_CLK0);
    si5351_output_enable(SI5351_CLK0, 1); // takes < 120uS @ 800 kHz
    end = time_us_64();
    printf("[MIN] %.8f us\n", ((float)end-begin));
    begin = time_us_64();
    si5351_set_freq(30200000ULL * 100ULL, SI5351_CLK0);
    end = time_us_64();
    printf("[SF] %.8f us\n", ((float)end-begin));
    } */


  si5351.start();

  set_frequency(10000000ULL, 0); // ATTENTION: 10 MHz now!
  set_clock_onoff_mask(0x01);
  // Setup PWM interrupt to fire when PWM cycle is complete
  pwm_clear_irq(audio_pin_slice);
  pwm_set_irq_enabled(audio_pin_slice, true);
  // set the handle function above
  irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler);
  irq_set_enabled(PWM_IRQ_WRAP, true);

  // Setup PWM for audio output
  pwm_config config = pwm_get_default_config();
  /* Base clock 176,000,000 Hz divide by wrap 250 then the clock divider further divides
     to set the interrupt rate.

     11 KHz is fine for speech. Phone lines generally sample at 8 KHz

     So clkdiv should be as follows for given sample rate
      8.0f for 11 KHz
      4.0f for 22 KHz
      2.0f for 44 KHz etc
  */
  // pwm_config_set_clkdiv(&config, 8.0f);
  // In [1]: (176000000 / (250)) / 64
  // Out[1]: 11000.0
  pwm_config_set_clkdiv(&config, 64.0f);
  pwm_config_set_wrap(&config, 250);
  pwm_init(audio_pin_slice, &config, true);

  pwm_set_gpio_level(AUDIO_PIN, 0);

  ssb_active = 1;

  while (1) {
    __wfi(); // Wait for Interrupt

    // uint64_t begin = 0;
    // uint64_t end = 0;

    // printf("OK!\n");
    // begin = time_us_64();
    // set_frequency(10500000ULL * 100ULL, 0);
    // si5351.set_offset_fast(1);
    // end = time_us_64();
    // printf("[MIN] %.8f us\n", ((float)end-begin));
    // si5351.set_offset_fast(128);
  }

  // "CAT control"
  /* while (1) {
     code = getchar_timeout_us(100);

     if (code != PICO_ERROR_TIMEOUT) {
     switch (code) {
     case '0':
     si5351_set_freq(7074000ULL * 100ULL, SI5351_CLK1);
     printf("40m\n");
     break;

     case '1':
     si5351_set_freq(10136000ULL * 100ULL, SI5351_CLK1);
     printf("30m\n");
     break;

     case '2':
     si5351_set_freq(14074000ULL * 100ULL, SI5351_CLK1);
     printf("20m\n");
     break;

     case '3':
     si5351_set_freq(18100000ULL * 100ULL, SI5351_CLK1);
     printf("17m\n");
     break;

     case '4':
     si5351_set_freq(21074000ULL * 100ULL, SI5351_CLK1);
     printf("15m\n");
     break;

     case '5':
     si5351_set_freq(24915000ULL * 100ULL, SI5351_CLK1);
     printf("12m\n");
     break;

     case '6':
     si5351_set_freq(28074000ULL * 100ULL, SI5351_CLK1);
     printf("10m\n");
     break;

     default:
     printf("UNKNOWN!");
     break;
     }
     }
     sleep_ms(10);
     } */

  return 0;
}
