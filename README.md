This is a native port of https://github.com/etherkit/Si5351Arduino to Raspberry Pi Pico.

Also, a native port of https://github.com/profdc9/RFBitBanger/blob/main/Code/RFBitBanger/si5351simple.cpp to Pi Pico.

Note: The `etherkit/Si5351Arduino` library can't be mixed with the `RFBitBanger/si5351simple.cpp` currently.

Default pinout (changeable):

```
SDA is on GP12 (16)

SCL is on GP13 (17)
```

Note: SSB quality is directly dependant on I2C speed. Use `1k` pull-up
resistors and avoid logic-level translators - use https://github.com/kholia/Si5351-Module-Clone-TCXO/
for best results!
