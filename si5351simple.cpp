// Borrowed from https://github.com/profdc9/RFBitBanger/blob/main/Code/RFBitBanger/si5351simple.cpp

/*
   Copyright (c) 2021 Daniel Marks

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

// This is cobbled together from many sources

#include "si5351simple.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static uint8_t i2c_bus_addr;

uint8_t si5351_write_bulk(uint8_t regAddr, uint8_t length, uint8_t *data) {
	int num_bytes_read = 0;
	uint8_t msg[length + 1];

	// Append register address to front of data packet
	msg[0] = regAddr;
	for (int i = 0; i < length; i++) {
		msg[i + 1] = data[i];
	}

	// Write data to register(s) over I2C
	i2c_write_blocking(i2c0, i2c_bus_addr, msg, (length + 1), false);

	return num_bytes_read;
}

uint8_t si5351_write(uint8_t regAddr, uint8_t data) {
	si5351_write_bulk(regAddr, 1, &data);

	return 0;
}

si5351simple::si5351simple(uint8_t p_cap, uint32_t p_xo_freq)
{
	i2c_bus_addr = 0x60;
	cap = p_cap;
	xo_freq = p_xo_freq;
}

void si5351simple::set_xo_freq(uint32_t p_xo_freq)
{
	xo_freq = p_xo_freq;
}

uint32_t si5351simple::get_xo_freq(void)
{
	return xo_freq;
}


static void print_hex(unsigned char *str, int len)
{
        int i;
        for (i = 0; i < len; ++i)
                printf("%02x", str[i]);
        printf("\n");
}


void si5351simple::set_offset_fast(int16_t offset)
{
	uint16_t a;
	int32_t b;
	uint32_t P1, P2;

	printf("O -> %d\n", c_regs.b_offset_pos);

	a = c_regs.a;
	if (offset > 0)
		b = c_regs.b + offset * c_regs.b_offset_pos /  SI5351_FREQ_OFFSET;
	else
		b = c_regs.b - offset * c_regs.b_offset_neg /  SI5351_FREQ_OFFSET;

	if (b < 0)
	{
		a--;
		b += FEEDBACK_MULTIPLIER_C;
	} else if (b > FEEDBACK_MULTIPLIER_C)
	{
		a++;
		b -= FEEDBACK_MULTIPLIER_C;
	}

	P2 = b >> (FEEDBACK_MULTIPLIER_SHIFT-7);
	P1 = (a << 7) + P2 - 512;
	P2 = (b << 7) - (P2 << FEEDBACK_MULTIPLIER_SHIFT);

	/* twi_start();
	   twi_write(SI5351_ADDRESS << 1);
	   twi_write(SI5351_MULTISYNTH_0 + 3);
	   twi_write((P1 >> 8) & 0xFF);
	   twi_write(P1 & 0xFF);
	   twi_write(((uint8_t)(FEEDBACK_MULTIPLIER_C >> 12) & 0xF0) | ((uint8_t)(P2 >> 16) & 0x0F));
	   twi_write((P2 >> 8) & 0xFF);
	   twi_write(P2 & 0xFF);
	   twi_stop(); */

	uint8_t buffer[5] = { (uint8_t)((P1 >> 8) & 0xFF), (uint8_t)(P1 & 0xFF), (uint8_t)(((uint8_t)(FEEDBACK_MULTIPLIER_C >> 12) & 0xF0) | ((uint8_t)(P2 >> 16) & 0x0F)), (uint8_t)((P2 >> 8) & 0xFF), (uint8_t)(P2 & 0xFF) };

	print_hex(buffer, 5);

	si5351_write_bulk(SI5351_MULTISYNTH_0 + 3, 5, buffer);
}

void si5351simple::set_registers(uint8_t synth_no, si5351_synth_regs *s_regs,
		uint8_t multisynth_no, si5351_multisynth_regs *m_regs)
{
	uint8_t synth_base = 0xFF, multisynth_base = 0xFF, i;
	switch (multisynth_no)
	{
		case 0: multisynth_base = SI5351_MULTISYNTH_0; break;
		case 1: multisynth_base = SI5351_MULTISYNTH_1; break;
		case 2: multisynth_base = SI5351_MULTISYNTH_2; break;
	}
	if (multisynth_base == 0xFF) return;
	switch (synth_no)
	{
		case 0: synth_base = SI5351_SYNTH_PLL_A; break;
		case 1: synth_base = SI5351_SYNTH_PLL_B; break;
	}

	setSourceAndPower(multisynth_no, 0, 0, 0, 0, 0);
	if (synth_base != 0xFF)
	{
		for (i=0;i<8;i++) si5351_write(synth_base+i, s_regs->regs[i]);
	}
	for (i=0;i<8;i++) si5351_write(multisynth_base+i, m_regs->regs[i]);
	si5351_write(multisynth_no+165, m_regs->offset);

	setSourceAndPower(multisynth_no, m_regs->offset != 0, 1, synth_no, 3, m_regs->inv);
	if (synth_base != 0xFF)
		si5351_write(177, synth_no ? 0x80 : 0x20);
}

static void calc_multisynth_registers(uint8_t regs[], uint8_t R, uint32_t a, uint32_t b)
{
	uint32_t P1, P2;

	P2 = b >> (FEEDBACK_MULTIPLIER_SHIFT-7);
	P1 = (a << 7) + P2 - 512;
	P2 = (b << 7) - (P2 << FEEDBACK_MULTIPLIER_SHIFT);

	regs[0] = (FEEDBACK_MULTIPLIER_C >> 8) & 0xFF;
	regs[1] = FEEDBACK_MULTIPLIER_C & 0xFF;
	regs[2] = (((uint8_t)(P1 >> 16)) & 0x03) | (R << 4);
	regs[3] = (P1 >> 8) & 0xFF;
	regs[4] = P1 & 0xFF;
	regs[5] = ((uint8_t)(FEEDBACK_MULTIPLIER_C >> 12) & 0xF0) | ((uint8_t)(P2 >> 16) & 0x0F);
	regs[6] = (P2 >> 8) & 0xFF;
	regs[7] = P2 & 0xFF;
}

int32_t si5351simple::calculate_b(uint32_t frequency)
{
	int32_t b;
	b = c_regs.fvco - c_regs.a * frequency;
	b = (((int64_t)b) << FEEDBACK_MULTIPLIER_SHIFT) / frequency;
	return b;
}

void si5351simple::calc_registers(uint32_t frequency, uint8_t phase, uint8_t calc_offset, si5351_synth_regs *s_regs, si5351_multisynth_regs *m_regs)
{
	c_regs.R = 0;

	if (phase >= 0x80)
	{
		phase -= 0x80;
		m_regs->inv = 1;
	} else m_regs->inv = 0;

	if (frequency < 5000000)
		c_regs.mult_ratio = 16;
	else if (frequency < 7000000)
		c_regs.mult_ratio = 22;
	else c_regs.mult_ratio = 34;

	c_regs.fvco = xo_freq * c_regs.mult_ratio;

	int16_t offset = SI5351_FREQ_OFFSET;

	for (;;)
	{
		c_regs.a = (c_regs.fvco / frequency);
		if (c_regs.a <= 900) break;
		frequency <<= 1;
		offset <<= 1;
		c_regs.R += 1;
	}

	c_regs.b = calculate_b(frequency);
	if (calc_offset)
	{
		c_regs.b_offset_pos = calculate_b(frequency + offset) - c_regs.b;
		c_regs.b_offset_neg = calculate_b(frequency - offset) - c_regs.b;
	}

	if (phase)
		m_regs->offset = (uint8_t)((((uint32_t)c_regs.a)*phase) >> 6) & 0x7F;
	else
		m_regs->offset = 0;

	if (s_regs != NULL)
		calc_multisynth_registers(s_regs->regs, 0, c_regs.mult_ratio, 0);

	calc_multisynth_registers(m_regs->regs, c_regs.R, c_regs.a, c_regs.b);
}

// off_on = 0 is off, off_on = 1 is on
// pll_source = 1 is PLLB, pll_source = 0 is PLLA
// power=0 2mA, power=1 4 mA, power=2 6 mA, power=3 8 mA
void si5351simple::setSourceAndPower(uint8_t clock_no, uint8_t frac, uint8_t off_on, uint8_t pll_source, uint8_t power, uint8_t inv)
{
	if (power > 3) return;
	si5351_write(clock_no+16, (off_on ? 0 : 0x80) + (frac ? 0 : 0x40) + (pll_source ? 0x20 : 0x00) + (inv ? 0x10 : 0x00) + power + 0x0C);
}

void si5351simple::setOutputOnOffMask(uint8_t new_on_mask)
{
	on_mask = new_on_mask ^ 0xFF;
	si5351_write(3, on_mask);
}

void si5351simple::setOutputOnOff(uint8_t clock_no, uint8_t off_on)
{
	if (off_on) on_mask &= ~(1 << clock_no);
	else on_mask |= (1 << clock_no);
	si5351_write(3, on_mask);
}

/* I2C0 pins */
#define I2C0_SDA 12
#define I2C0_SCL 13

void si5351simple::start(void)
{
	i2c_init(i2c0, 800*1000);
	gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
	gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(I2C0_SDA);
	gpio_pull_up(I2C0_SCL);

	si5351_write(24, 0xAA);  // make all outputs high impedance when disabled.
	si5351_write(149, 0);    // disable spread spectrum
	si5351_write(9, 0xFF);   // OEB does not control on
	si5351_write(3, 0xFF);   // all outputs off initially
	on_mask = 0xFF;

	switch (cap)
	{
		case 6: si5351_write(183, 0x40); break;
		case 8: si5351_write(183, 0x80); break;
		case 10: si5351_write(183, 0xC0); break;
	}
}
