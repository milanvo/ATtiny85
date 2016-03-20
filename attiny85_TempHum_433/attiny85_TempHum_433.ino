/*
 * connectingStuff, Oregon Scientific v2.1 Emitter
 * http://connectingstuff.net/blog/encodage-protocoles-oregon-scientific-sur-arduino/
 *
 * Copyright (C) 2013 olivier.lebrun@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * V2 par vil1driver
 * sketch unique pour sonde ds18b20 ou DHT11/22
 * choix de la fréquence de transmission
 * fonction niveau de batterie
 * vérification valeur erronées
 * 
*/

/************************************************************

		emplacement des PIN de la puce ATtiny85

                         +-/\-+
Ain0       (D  5)  PB5  1|    |8   VCC
Ain3       (D  3)  PB3  2|    |7   PB2  (D  2)  INT0  Ain1
Ain2       (D  4)  PB4  3|    |6   PB1  (D  1)        pwm1
				   GND  4|    |5   PB0  (D  0)        pwm0
						 +----+	

						 
****************       Confuguration       *****************/


#define UnitCode 0xCC	// Identifiant unique de votre sonde (hexadecimal)
#define MinVcc 3000		// Voltage minumum (mV) avant d'indiquer batterie faible
int counter = 15; 		// Nombre de cycles entre chaque transmission (1 cycles = 8 secondes, 15x8 = 120s soit 2 minutes)
// commentez la ligne suivante si vous utilisez une sonde DHT11 ou DHT22
#define THN132N		// Sonde de température simple (ds18b20)

#define SERIAL_RX PB3 // pin 2 // INPUT
#define SERIAL_TX PB4 // pin 3 // OUTPUT

#define SONDE PB1 // pin 6 // data sonde


/****************   Fin de configuration    *****************/


// Chargement des librairies
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SoftwareSerial.h>

#ifdef THN132N
	#include <OneWire.h>
	#define DS18B20 0x28     // Adresse 1-Wire du DS18B20
	OneWire ds(SONDE); // Création de l'objet OneWire ds
#else
	#include <DHT.h>
	DHT dht;
#endif

#include <EEPROM.h>

#ifndef cbi
	#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
	#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

volatile boolean f_wdt = 1;


SoftwareSerial TinySerial(SERIAL_RX, SERIAL_TX); // RX, TX

int cnt = 0;	// Initialisation du compte de cycle
 
const byte TX_PIN = 0;
 
const unsigned long TIME = 512;
const unsigned long TWOTIME = TIME*2;
 
#define SEND_HIGH() digitalWrite(TX_PIN, HIGH)
#define SEND_LOW() digitalWrite(TX_PIN, LOW)
 
// Buffer for Oregon message
#ifdef THN132N
  byte OregonMessageBuffer[8];
#else
  byte OregonMessageBuffer[9];
#endif
 
/**
 * \brief    Send logical "0" over RF
 * \details  azero bit be represented by an off-to-on transition
 * \         of the RF signal at the middle of a clock period.
 * \         Remenber, the Oregon v2.1 protocol add an inverted bit first 
 */
inline void sendZero(void) 
{
  SEND_HIGH();
  delayMicroseconds(TIME);
  SEND_LOW();
  delayMicroseconds(TWOTIME);
  SEND_HIGH();
  delayMicroseconds(TIME);
}
 
/**
 * \brief    Send logical "1" over RF
 * \details  a one bit be represented by an on-to-off transition
 * \         of the RF signal at the middle of a clock period.
 * \         Remenber, the Oregon v2.1 protocol add an inverted bit first 
 */
inline void sendOne(void) 
{
   SEND_LOW();
   delayMicroseconds(TIME);
   SEND_HIGH();
   delayMicroseconds(TWOTIME);
   SEND_LOW();
   delayMicroseconds(TIME);
}
 
/**
* Send a bits quarter (4 bits = MSB from 8 bits value) over RF
*
* @param data Source data to process and sent
*/
 
/**
 * \brief    Send a bits quarter (4 bits = MSB from 8 bits value) over RF
 * \param    data   Data to send
 */
inline void sendQuarterMSB(const byte data) 
{
  (bitRead(data, 4)) ? sendOne() : sendZero();
  (bitRead(data, 5)) ? sendOne() : sendZero();
  (bitRead(data, 6)) ? sendOne() : sendZero();
  (bitRead(data, 7)) ? sendOne() : sendZero();
}
 
/**
 * \brief    Send a bits quarter (4 bits = LSB from 8 bits value) over RF
 * \param    data   Data to send
 */
inline void sendQuarterLSB(const byte data) 
{
  (bitRead(data, 0)) ? sendOne() : sendZero();
  (bitRead(data, 1)) ? sendOne() : sendZero();
  (bitRead(data, 2)) ? sendOne() : sendZero();
  (bitRead(data, 3)) ? sendOne() : sendZero();
}
 
/******************************************************************/
/******************************************************************/
/******************************************************************/
 
/**
 * \brief    Send a buffer over RF
 * \param    data   Data to send
 * \param    size   size of data to send
 */
void sendData(byte *data, byte size)
{
  for(byte i = 0; i < size; ++i)
  {
    sendQuarterLSB(data[i]);
    sendQuarterMSB(data[i]);
  }
}
 
/**
 * \brief    Send an Oregon message
 * \param    data   The Oregon message
 */
void sendOregon(byte *data, byte size)
{
    sendPreamble();
    //sendSync();
    sendData(data, size);
    sendPostamble();
}
 
/**
 * \brief    Send preamble
 * \details  The preamble consists of 16 "1" bits
 */
inline void sendPreamble(void)
{
  byte PREAMBLE[]={0xFF,0xFF};
  sendData(PREAMBLE, 2);
}
 
/**
 * \brief    Send postamble
 * \details  The postamble consists of 8 "0" bits
 */
inline void sendPostamble(void)
{
#ifdef THN132N
  sendQuarterLSB(0x00);
#else
  byte POSTAMBLE[]={0x00};
  sendData(POSTAMBLE, 1);  
#endif
}
 
/**
 * \brief    Send sync nibble
 * \details  The sync is 0xA. It is not use in this version since the sync nibble
 * \         is include in the Oregon message to send.
 */
inline void sendSync(void)
{
  sendQuarterLSB(0xA);
}
 
/******************************************************************/
/******************************************************************/
/******************************************************************/
 
/**
 * \brief    Set the sensor type
 * \param    data       Oregon message
 * \param    type       Sensor type
 */
inline void setType(byte *data, byte* type) 
{
  data[0] = type[0];
  data[1] = type[1];
}
 
/**
 * \brief    Set the sensor channel
 * \param    data       Oregon message
 * \param    channel    Sensor channel (0x10, 0x20, 0x30)
 */
inline void setChannel(byte *data, byte channel) 
{
    data[2] = channel;
}
 
/**
 * \brief    Set the sensor ID
 * \param    data       Oregon message
 * \param    ID         Sensor unique ID
 */
inline void setId(byte *data, byte ID) 
{
  data[3] = ID;
}
 
/**
 * \brief    Set the sensor battery level
 * \param    data       Oregon message
 * \param    level      Battery level (0 = low, 1 = high)
 */
void setBatteryLevel(byte *data, byte level)
{
  if(!level) data[4] = 0x0C;
  else data[4] = 0x00;
}
 
/**
 * \brief    Set the sensor temperature
 * \param    data       Oregon message
 * \param    temp       the temperature
 */
void setTemperature(byte *data, float temp) 
{
  // Set temperature sign
  if(temp < 0)
  {
    data[6] = 0x08;
    temp *= -1;  
  }
  else
  {
    data[6] = 0x00;
  }
 
  // Determine decimal and float part
  int tempInt = (int)temp;
  int td = (int)(tempInt / 10);
  int tf = (int)round((float)((float)tempInt/10 - (float)td) * 10);
 
  int tempFloat =  (int)round((float)(temp - (float)tempInt) * 10);
 
  // Set temperature decimal part
  data[5] = (td << 4);
  data[5] |= tf;
 
  // Set temperature float part
  data[4] |= (tempFloat << 4);
}
 
/**
 * \brief    Set the sensor humidity
 * \param    data       Oregon message
 * \param    hum        the humidity
 */
void setHumidity(byte* data, byte hum)
{
    data[7] = (hum/10);
    data[6] |= (hum - data[7]*10) << 4;
}
 
/**
 * \brief    Sum data for checksum
 * \param    count      number of bit to sum
 * \param    data       Oregon message
 */
int Sum(byte count, const byte* data)
{
  int s = 0;
 
  for(byte i = 0; i<count;i++)
  {
    s += (data[i]&0xF0) >> 4;
    s += (data[i]&0xF);
  }
 
  if(int(count) != count)
    s += (data[count]&0xF0) >> 4;
 
  return s;
}
 
/**
 * \brief    Calculate checksum
 * \param    data       Oregon message
 */
void calculateAndSetChecksum(byte* data)
{
#ifdef THN132N
    int s = ((Sum(6, data) + (data[6]&0xF) - 0xa) & 0xff);
 
    data[6] |=  (s&0x0F) << 4;     data[7] =  (s&0xF0) >> 4;
#else
    data[8] = ((Sum(8, data) - 0xa) & 0xFF);
#endif
}
 
/******************************************************************/
/******************************************************************/


// Fonction récupérant la température depuis le DS18B20
// Retourne true si tout va bien, ou false en cas d'erreur
boolean getTemperature(float *temp){

#ifdef THN132N 	
	byte data[9], addr[8];
	// data : Données lues depuis le scratchpad
	// addr : adresse du module 1-Wire détecté


	if (!ds.search(addr)) { // Recherche un module 1-Wire
	ds.reset_search();    // Réinitialise la recherche de module
	return false;         // Retourne une erreur
	}

	if (OneWire::crc8(addr, 7) != addr[7]) // Vérifie que l'adresse a été correctement reçue
	return false;                        // Si le message est corrompu on retourne une erreur

	if (addr[0] != DS18B20) // Vérifie qu'il s'agit bien d'un DS18B20
	return false;         // Si ce n'est pas le cas on retourne une erreur

	ds.reset();             // On reset le bus 1-Wire
	ds.select(addr);        // On sélectionne le DS18B20

	ds.write(0x44, 1);      // On lance une prise de mesure de température
	delay(1000);             // Et on attend la fin de la mesure

	ds.reset();             // On reset le bus 1-Wire
	ds.select(addr);        // On sélectionne le DS18B20
	ds.write(0xBE);         // On envoie une demande de lecture du scratchpad

	for (byte i = 0; i < 9; i++) // On lit le scratchpad
	data[i] = ds.read();       // Et on stock les octets reçus

	// Calcul de la température en degré Celsius
	*temp = ((data[1] << 8) | data[0]) * 0.0625; 

#else
	delay(dht.getMinimumSamplingPeriod());
	*temp = dht.getTemperature();
#endif
	// Pas d'erreur
	return true;
}

 
/******************************************************************/
 
void setup()
{
  
  setup_watchdog(9);
  //pinMode(PB3, OUTPUT);
  //digitalWrite(PB3,HIGH);delay(500);
  //digitalWrite(PB3,LOW);delay(500);
  //digitalWrite(PB3,HIGH);
  pinMode(TX_PIN, OUTPUT);
  
 
  pinMode(PB4, OUTPUT); //tx
  TinySerial.begin(9600);
  TinySerial.println("\n[Oregon V2.1 encoder]");
 
  SEND_LOW();  
 
#ifdef THN132N  
  // Create the Oregon message for a temperature only sensor (TNHN132N)
  byte ID[] = {0xEA,0x4C};
#else
  // Create the Oregon message for a temperature/humidity sensor (THGR2228N)
  byte ID[] = {0x1A,0x2D};
  dht.setup(SONDE);
#endif  
 
  setType(OregonMessageBuffer, ID);
  setChannel(OregonMessageBuffer, 0x20);
  setId(OregonMessageBuffer, UnitCode);
}


// set system into the sleep state 
// system wakes up when wtchdog is timed out
void system_sleep() {
  cbi(ADCSRA,ADEN);                    // switch Analog to Digitalconverter OFF

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();

  sleep_mode();                        // System sleeps here

  sleep_disable();                     // System continues execution here when watchdog timed out 
  sbi(ADCSRA,ADEN);                    // switch Analog to Digitalconverter ON
}

// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
void setup_watchdog(int ii) {

  byte bb;
  int ww;
  if (ii > 9 ) ii=9;
  bb=ii & 7;
  if (ii > 7) bb|= (1<<5);
  bb|= (1<<WDCE);
  ww=bb;

  MCUSR &= ~(1<<WDRF);
  // start timed sequence
  WDTCR |= (1<<WDCE) | (1<<WDE);
  // set new watchdog timeout value
  WDTCR = bb;
  WDTCR |= _BV(WDIE);
}
  
// Watchdog Interrupt Service / is executed when watchdog timed out
ISR(WDT_vect) {
  f_wdt=1;  // set global flag
}

// Helper function to read battery voltage, return value in millivolts
uint16_t readVcc()
{  
  // Read bandgap reference voltage (1.1V) with reference at Vcc (?V)
  ADMUX = _BV(MUX3) | _BV(MUX2);
  
  // Wait for voltage reference reference to settle, 3ms is minimum
  _delay_ms(5);
  
  // Convert analog to digital twice (first value is unreliable)
  ADCSRA |= _BV(ADSC);
  while(bit_is_set(ADCSRA,ADSC));
  ADCSRA |= _BV(ADSC);
  while(bit_is_set(ADCSRA,ADSC));

  // Convert the value to millivolts
  return 1126400L / ADC;
}
 
void loop()
{
  
	if (f_wdt==1)	// wait for timed out watchdog / flag is set when a watchdog timeout occurs
	{  
		f_wdt=0;	// reset flag
   
		if(cnt == counter)	// Nombre de cycle de réveil atteint
		{
			// Get the battery voltage
			int bat;
			uint16_t voltage = readVcc();
			TinySerial.print("Battery : ");TinySerial.print(voltage);TinySerial.write('mV'); TinySerial.println();
			
			if (voltage < MinVcc) {
				bat = 0; 	// Low
			}
			else {
				bat = 1;	// High
			}
			
			// Get Temperature, humidity and battery level from sensors
			float temp;
			
			// Lecture de la dernière valeur de température
			//float last = EEPROM.read(0);
			
			
			if (getTemperature(&temp)) {
			
				TinySerial.print("Temperature : ");TinySerial.print(temp);TinySerial.write(176); // caractère °
				TinySerial.write('C'); TinySerial.println();
				
				// Mémorisation de la tepérature relevée
				//EEPROM.write(0,temp);
				
				setBatteryLevel(OregonMessageBuffer, bat);
				setTemperature(OregonMessageBuffer, temp);
			 
				#ifndef THN132N
					// Set Humidity
					float humidity = dht.getHumidity();
					TinySerial.print("Humidity : ");TinySerial.print(humidity);TinySerial.write('%'); TinySerial.println();
					setHumidity(OregonMessageBuffer, humidity);
				#endif  
			 
				// Calculate the checksum
				calculateAndSetChecksum(OregonMessageBuffer);
			 			 
				// Send the Message over RF
				sendOregon(OregonMessageBuffer, sizeof(OregonMessageBuffer));
				// Send a "pause"
				SEND_LOW();
				delayMicroseconds(TWOTIME*8);
				// Send a copie of the first message. The v2.1 protocol send the message two time 
				sendOregon(OregonMessageBuffer, sizeof(OregonMessageBuffer));
				SEND_LOW();
			}
			else {
				TinySerial.println("getTemperature failed");
			}
			  
			cnt = 0;
		}
		else {
			cnt++;
		}
		
		system_sleep();
	  
	}
  
}
