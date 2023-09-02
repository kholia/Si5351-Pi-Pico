#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

#include "si5351.h"

int main(void)
{
  int code;
  uint64_t frequency = 14074000ULL * 100ULL;

  stdio_init_all();

  // Initialize the Si5351
  si5351_init(0x60, SI5351_CRYSTAL_LOAD_8PF, 26000000, 0); // I am using a 26 MHz TCXO
  si5351_drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
  si5351_drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
  si5351_drive_strength(SI5351_CLK3, SI5351_DRIVE_8MA);

  si5351_set_freq(frequency, SI5351_CLK0);
  si5351_output_enable(SI5351_CLK0, 0);
  si5351_set_freq(frequency, SI5351_CLK1);
  si5351_output_enable(SI5351_CLK1, 0);
  // Invert CLK1
  set_clock_invert(SI5351_CLK1, 1);
  // Enable output
  si5351_output_enable(SI5351_CLK0, 1);
  si5351_output_enable(SI5351_CLK1, 1);
  set_clock_invert(SI5351_CLK1, 1);
  pll_reset(SI5351_PLLA);

  // "CAT control"
  while (1) {
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
  }

  return 0;
}
