
//  Author: avishorp@gmail.com
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

extern "C" {
  #include <stdlib.h>
  #include <string.h>
  #include <inttypes.h>
}

#include "TM1637DisplayAdvanced.h"
#include <Arduino.h>

#define TM1637_I2C_COMM1    0x40
#define TM1637_I2C_COMM2    0xC0
#define TM1637_I2C_COMM3    0x80

//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D
const uint8_t digitToSegment[] = {
 // XGFEDCBA
  0b00111111,    // 0
  0b00000110,    // 1
  0b01011011,    // 2
  0b01001111,    // 3
  0b01100110,    // 4
  0b01101101,    // 5
  0b01111101,    // 6
  0b00000111,    // 7
  0b01111111,    // 8
  0b01101111,    // 9
  0b01110111,    // A
  0b01111100,    // b
  0b00111001,    // C
  0b01011110,    // d
  0b01111001,    // E
  0b01110001     // F
  };


TM1637Display::TM1637Display(uint8_t pinClk, uint8_t pinDISPLAY_DIO_PIN)
{
	// Copy the pin numbers
	m_pinClk = pinClk;
	m_pinDISPLAY_DIO_PIN = pinDISPLAY_DIO_PIN;

	// Set the pin direction and default value.
	// Both pins are set as inputs, allowing the pull-up resistors to pull them up
    pinMode(m_pinClk, INPUT);
    pinMode(m_pinDISPLAY_DIO_PIN,INPUT);
	digitalWrite(m_pinClk, LOW);
	digitalWrite(m_pinDISPLAY_DIO_PIN, LOW);
}




void TM1637Display::setSegmentState(uint8_t digit, uint8_t high, uint8_t low){
  if (high != 255) m_segmentsHigh[digit] = high;
  if (low != 255) m_segmentsLow[digit] = 127 - low;
  refresh(digit);
}

void TM1637Display::clearSegmentState(){
  for (byte i = 0; i < 4; i++){
    if (m_segmentsHigh[i] || m_segmentsLow[i] != 127){ m_segmentsHigh[i] = 0; m_segmentsLow[i] = 127; refresh(i);}
  }
}

void TM1637Display::setInverted(uint8_t inv){
  if (inv > 0) inv = 1;
  m_inverted = inv;
  refresh();
}

void TM1637Display::setBrightness(uint8_t brightness, bool on)
{
	m_brightness = (brightness & 0x7) | (on? 0x08 : 0x00);

    // Write COMM3 + brightness
  start();
  writeByte(TM1637_I2C_COMM3 + (m_brightness & 0x0f));
  stop();
}



void TM1637Display::refresh(uint8_t pos)
{
  if (pos == 4) setSegments(m_lastBytes);
  else {
        // Write COMM1
    start();
    writeByte(TM1637_I2C_COMM1);
    stop();
  
    if (m_inverted) writeAddress(3 - pos);
    else writeAddress(pos);
    
    if (!m_inverted) writeByte((m_lastBytes[pos] & m_segmentsLow[pos]) | m_segmentsHigh[pos]);
    else writeByte((m_lastBytes[pos] & m_segmentsLow[pos]) | m_segmentsHigh[pos], 1);

  }
  stop();
}



void TM1637Display::setSegments(const uint8_t segments[], uint8_t length, uint8_t pos)
{
  length--;
  
    // Write COMM1
	start();
	writeByte(TM1637_I2C_COMM1);
	stop();

	// Write COMM2 + first digit address
	if (!m_inverted) writeAddress(pos);
  else writeAddress(3 - pos - length);  //NEW 1
  
	// Write the data bytes
	for (uint8_t k=0; k <= length; k++)
  {
    m_lastBytes[k + pos] = segments[k];     //Save byte in case refresh is called
	  if (!m_inverted) {
	    //if ((m_digitMask >> k) & 1) 
	    writeByte((segments[k] & m_segmentsLow[k]) | m_segmentsHigh[k]);
      //else writeByte(0);
	  }
    else {
      //if ((m_digitMask >> (3 - k)) & 1) 
      writeByte((segments[length - k] & m_segmentsLow[length - k]) | m_segmentsHigh[length - k], 1);    //NEW 1
      //else writeByte(0);
    }
  }

	stop();
}



void TM1637Display::showNumberDec(int num, bool leading_zero, uint8_t length, uint8_t pos)
{
  showNumberDecEx(num, 0, leading_zero, length, pos);
}

void TM1637Display::showNumberDecEx(int num, uint8_t dots, bool leading_zero,
                                    uint8_t length, uint8_t pos)
{
  uint8_t digits[4];
	const static int divisors[] = { 1, 10, 100, 1000 };
	bool leading = true;

	for(int8_t k = 0; k < 4; k++) {
	    int divisor = divisors[4 - 1 - k];
		int d = num / divisor;
    uint8_t digit = 0;

		if (d == 0) {
		  if (leading_zero || !leading || (k == 3))
		      digit = encodeDigit(d);
	      else
		      digit = 0;
		}
		else {
			digit = encodeDigit(d);
			num -= d * divisor;
			leading = false;
		}
    
    // Add the decimal point/colon to the digit
    digit |= (dots & 0x80); 
    dots <<= 1;
    
    digits[k] = digit;
	}

	setSegments(digits + (4 - length), length, pos);
}


void TM1637Display::bitDelay()
{
	delayMicroseconds(50);
}

void TM1637Display::start()
{
  pinMode(m_pinDISPLAY_DIO_PIN, OUTPUT);
  bitDelay();
}

void TM1637Display::stop()
{
	pinMode(m_pinDISPLAY_DIO_PIN, OUTPUT);
	bitDelay();
	pinMode(m_pinClk, INPUT);
	bitDelay();
	pinMode(m_pinDISPLAY_DIO_PIN, INPUT);
	bitDelay();
}

bool TM1637Display::writeByte(uint8_t b, bool inverted)
{
  uint8_t data;
  if (inverted) {data = (b & 0b01000000); data += (b & 0b00000111) << 3; data += (b & 0b00111000) >> 3;}
  else data = b;

  // 8 Data Bits
  for(uint8_t i = 0; i < 8; i++) {
    // DISPLAY_CLK_PIN low
    pinMode(m_pinClk, OUTPUT);
    bitDelay();

	// Set data bit
    if (data & 0x01)
      pinMode(m_pinDISPLAY_DIO_PIN, INPUT);
    else
      pinMode(m_pinDISPLAY_DIO_PIN, OUTPUT);

    bitDelay();

	// DISPLAY_CLK_PIN high
    pinMode(m_pinClk, INPUT);
    bitDelay();
    data = data >> 1;
  }

  // Wait for acknowledge
  // DISPLAY_CLK_PIN to zero
  pinMode(m_pinClk, OUTPUT);
  pinMode(m_pinDISPLAY_DIO_PIN, INPUT);
  bitDelay();

  // DISPLAY_CLK_PIN to high
  pinMode(m_pinClk, INPUT);
  bitDelay();
  uint8_t ack = digitalRead(m_pinDISPLAY_DIO_PIN);
  if (ack == 0)
    pinMode(m_pinDISPLAY_DIO_PIN, OUTPUT);


  bitDelay();
  pinMode(m_pinClk, OUTPUT);
  bitDelay();

  return ack;
}

uint8_t TM1637Display::encodeDigit(uint8_t digit)
{
	return digitToSegment[digit & 0x0f];
}

void TM1637Display::writeAddress(uint8_t addr){
  start();
  writeByte(TM1637_I2C_COMM2 + (addr & 0x03));
}