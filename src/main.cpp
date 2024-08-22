// Author: AURi
// PB0 = op pin to DIAC (generates impulse)
// PB2 = ip pin from OPTOCOUPLER (generates INT0 for 0 cross detector)
// PB3 = ip from IR_RECEIVER (generates PCINT0 to decode ir signal)

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

uint16_t wait_after_0 = 5120;
bool zero_cross = false;
uint32_t crossing_time = 0;

volatile bool rcv_started = false;
bool main_data_stream = false;
bool valid_data = false;
uint32_t prev_ir_rcv_time = 0;
uint32_t ir_rcv_time = 0;
uint16_t ir_rcv_duration = 0;
volatile uint32_t ir_data_revd = 0;
volatile uint8_t bit_index = 0;




ISR(INT0_vect)  // triggered when zero cross
{
  crossing_time = micros();
  zero_cross = true;
}

ISR(PCINT0_vect)  // triggered when incomming ir signal
{
  ir_rcv_time = micros();
  rcv_started = true; // ir bit stream comming
}




void generate_impulse()
{
  uint32_t time_elapsed = micros() - crossing_time;
  if (wait_after_0 < time_elapsed)
  {
    PORTB |= (1 << PB0);
    if ((wait_after_0 + 25) < time_elapsed)
    {
      PORTB &= ~(1 << PB0);
      zero_cross = false;
    }
  }
}

bool decode_ir()
{

  ir_rcv_duration = ir_rcv_time - prev_ir_rcv_time;
  prev_ir_rcv_time = ir_rcv_time;
  rcv_started = false;

  if (!main_data_stream) // first portion of signal detected
  {
    if (ir_rcv_duration >= 9000 && ir_rcv_duration <= 9500)
    {
      bit_index = 0;
      ir_data_revd = 0;
    }
    else if (ir_rcv_duration >= 4000 && ir_rcv_duration <= 5000)
    {
      // Detected the space after the start pulse (4.5ms)
      // Ready to receive bits
    }
    else if (ir_rcv_duration >= 500 && ir_rcv_duration <= 700)
    {
      main_data_stream = true; // first 562.5µs low pulse detected
    }
  }
  else // 1st half of signal is conferm, compairison of 2nd half
  {
    main_data_stream = false;
    if (ir_rcv_duration >= 400 && ir_rcv_duration <= 700) // logical 0
    {
      ir_data_revd <<= 1; // shift ny 1 bit
    }
    else if (ir_rcv_duration >= 1400 && ir_rcv_duration <= 1900) // logical '1'
    {
      ir_data_revd <<= 1; // shift ny 1 bit
      ir_data_revd |= 1;  // set the least significant bit
    }
    bit_index++;
  }

  if (bit_index >= 32) // data receive complete
  {
    bit_index = 0;
    return true;
  }
  return false;
}

void take_action()
{
  switch (ir_data_revd)
  {
  case 0x1FE50AF:
    wait_after_0 += 900;
    break;

  case 0xFF2857:
    wait_after_0 += 900;
    break;

  case 0x1FED827:
    wait_after_0 -= 900;
    break;

  case 0xFF6C13:
    wait_after_0 -= 900;
    break;

  default:
    break;
  }

  // constrain wait_after_0
  if (wait_after_0 < 96)
  {
    wait_after_0 = 96;
  }
  else if (wait_after_0 > 9980)
  {
    wait_after_0 = 9980;
  }

  ir_data_revd = 0;
}




void setup()
{
  // declare i/o:
  DDRB &= ~(1 << DDB2); // make pb2 ip
  DDRB &= ~(1 << DDB3); // make pb3 ip
  DDRB |= (1 << DDB0);  // make pb0 op

  // zero corss detector INT0 setup:
  GIMSK |= (1 << INT0);  // enable hardware interrupt
  MCUCR |= (1 << ISC01); // rising edge intrrupt
  MCUCR |= (1 << ISC00); // rising edge interrupt

  // ir signal decoder PCINT3 setup
  GIMSK |= (1 << PCIE);   // enable pin change interrupt
  PCMSK |= (1 << PCINT3); // enable PCINT3 or PB3

  // enable global interrupt:
  SREG |= (1 << 7);
}

void loop()
{
  // zero cross detector
  if (zero_cross)
  {
    generate_impulse();
  }

  // decoding ir signal
  if (rcv_started)
  {
    valid_data = decode_ir();
  }
  // if valid signal is received
  if (valid_data)
  {
    valid_data = false;
    take_action();
  }

}