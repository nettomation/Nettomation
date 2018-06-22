/* Copyright (c) 2018 Jiri Ocenasek, http://nettomation.com
 *
 * This file is part of Nettomation.
 *
 * Nettomation is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * For tutorials, donations and proprietary licensing options
 * please visit http://nettomation.com
 */

/* The following code mimics the beer-brewing and beer-fermenting on Raspberry PI, 
 * but there is no real interaction with any hardware, so it can compile and run on any linux platform.
 * Note: Adding real hardware control functionality is at your own risk!
 */

#include "userpages.h"
#include "engine/autoregister.h"
#include "engine/smartlock.h"
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <fstream>
#include <sstream>

#include "dummyio.h"

/*
#include <wiringPi.h>
#include "ADCPi/ABE_ADCPi.h"
using namespace ABElectronics_CPP_Libraries;
*/

/* Every piece of web content needs to be registered.
 * parameter 1 : class name
 * parameter 2 : unique instance name
 * parameter 3 : type of rendering: TOP_VISIBLE, TOP_INVISIBLE, INNER
 */
AUTO_REGISTER_CONTENT(BrewingPage,"BREWINGPAGE",TOP_VISIBLE);
AUTO_REGISTER_CONTENT(BrewingStatus,"BREWINGSTATUS",INNER);
AUTO_REGISTER_CONTENT(FermentingPage,"FERMENTINGPAGE",TOP_INVISIBLE);
AUTO_REGISTER_CONTENT(HLTController,"HLTCONTROLLER",INNER);
AUTO_REGISTER_CONTENT(ValveController,"VALVECONTROLLER",INNER);
AUTO_REGISTER_CONTENT(PowerMeter,"POWERMETER",INNER);
AUTO_REGISTER_CONTENT(TiltReader,"TILTREADER",INNER);
AUTO_REGISTER_CONTENT(VolumeReader,"VOLUMEREADER",INNER);
AUTO_REGISTER_CONTENT(TemperatureReader,"TEMPERATUREREADER",INNER);

#define HYSTERESIS 0.5
#define BOILINGTEMP 97.0 // the sensor has offset, it reads 96.x during a good boil
#define MAX_HISTORY_SIZE (24*60*10) // 14 days

#define FERMENTER_PIN    0 // power socket, active in 0
#define PUMP_PIN         3 // power socket, active in 0
#define HEATER_3200W_PIN 2 // power socket, active in 0
#define HEATER_1600W_PIN 12 // power socket, active in 0

#define MASHING_PIN      1 // DC relay in the induction plate, active in 0

// all relays are active in 0, passive in 1 or as inputs
#define RELAY01_PIN 13 // bottom left, valve after pump to mash tun, close
#define RELAY02_PIN 6  // bottom second from left, valve after pump to mash tun, open
#define RELAY03_PIN 14 // bottom third from left, solenoid from barrel to pump, pin1
#define RELAY04_PIN 10 // bottom fourth from left, solenoid from barrel to pump, pin2
#define RELAY05_PIN 30 // bottom 5th from left, valve from boiler to pump, close
#define RELAY06_PIN 11 // bottom 6th from left, valve from boiler to pump, close
#define RELAY07_PIN 21 // bottom 7th from left, valve from mash tun to pump, open
#define RELAY08_PIN 31 // bottom right, valve from mash tun to pump, open
#define RELAY11_PIN 23 // top right, nc
#define RELAY10_PIN 26 // top 7th from left, nc
#define RELAY09_PIN 22 // top 6th from left, nc
#define RELAY12_PIN 27 // top 5th from left, solenoid water to CFC
#define RELAY13_PIN 24 // top 4th from left, solenoid water in to pump
#define RELAY14_PIN 28 // top 3rd from left, stirring motor
#define RELAY15_PIN 25 // top 2nd from left, valve after pump to boiler, open
#define RELAY16_PIN 29 // top left, valve after pump to boiler, close

#define STIRRING_PIN                      RELAY14_PIN

#define SOLENOID_WATER_TO_CFC_PIN         RELAY12_PIN
#define SOLENOID_WATER_TO_PUMP_PIN        RELAY13_PIN
#define SOLENOID2_BARREL_TO_PUMP_PIN      RELAY04_PIN
#define SOLENOID1_BARREL_TO_PUMP_PIN      RELAY03_PIN
#define VALVE_PUMP_TO_BOILER_OPEN_PIN     RELAY15_PIN
#define VALVE_PUMP_TO_BOILER_CLOSE_PIN    RELAY16_PIN
#define VALVE_PUMP_TO_MASH_TUN_OPEN_PIN   RELAY02_PIN
#define VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN  RELAY01_PIN
#define VALVE_MASH_TUN_TO_PUMP_OPEN_PIN   RELAY08_PIN
#define VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN  RELAY07_PIN
#define VALVE_BOILER_TO_PUMP_OPEN_PIN     RELAY05_PIN
#define VALVE_BOILER_TO_PUMP_CLOSE_PIN    RELAY06_PIN

#define POWER_IN_PIN 5

// the following paths need to be changed
#define PATH_TO_MASHING_SENSOR "/sys/bus/w1/devices/28-0fakedir1/w1_slave"
#define PATH_TO_BOILER_SENSOR "/sys/bus/w1/devices/28-0fakedir2/w1_slave"
#define PATH_TO_FERMENTING_SENSOR "/sys/bus/w1/devices/28-0fakedir3/w1_slave"
#define PATH_TO_AUX_SENSOR "/sys/bus/w1/devices/28-0fakedir4/w1_slave"

void WebContent::renderHeader( ostream& stream ) // this is a static function
{
  stream << "<title>Nettomation brewing demo</title>"
	 << "<script src=\"/dygraph.min.js\"></script>"
	 << "<link rel=\"stylesheet\" src=\"/dygraph.css\" />";
}


double getTemperature( const string& path )
{
  /*
    The following code reads DS18B20 sensors.
    Since we don't want to depend on third party hardware in this release, we comment this out
    and assume some pseudo-random values for the graph.
  */
  return 90 + (rand() % 100 ) / 100.0;

  /*
  //  system("sudo modprobe w1-gpio");
  //  system("sudo modprobe w1-therm");

  char buff[100];

  FILE *fp = fopen(path.c_str(),"r");
  if ( fp == NULL )
    return 0.0; // unable to open file

  int nrRead = fread(buff,1,sizeof(buff)-1,fp);
  fclose(fp);
  if ( nrRead < 70 )
    return 0.0; // unable to read the temperature part

  buff[nrRead] = 0; // put zero at the end
  char* ptr = buff;
  ptr += 69; // start reading at the temperature part

  return atof(ptr) / 1000.0;
  */
}

void TemperatureReader::controlLoop()
{
  while (1)
    {
      sleep(1);

      _mashingTemperature = getTemperature(PATH_TO_MASHING_SENSOR);
      _boilerTemperature = getTemperature(PATH_TO_BOILER_SENSOR);
      _fermentingTemperature = getTemperature(PATH_TO_FERMENTING_SENSOR);
      _auxTemperature = getTemperature(PATH_TO_AUX_SENSOR);

      invalidate();
    }
}

void TemperatureReader::render( ostream& stream )
{
  stream << "Mashing temperature=" << to_string(_mashingTemperature) << "&#8451;";
  stream << "<br>Boiler temperature=" << to_string(_boilerTemperature) << "&#8451;";
  stream << "<br>Fermenting temperature=" << to_string(_fermentingTemperature) << "&#8451;";
  stream << "<br>Aux temperature=" << to_string(_auxTemperature) << "&#8451;";
}

unsigned char fromHexa(unsigned char c1)
{
  if ( (c1 >= '0') && (c1 <= '9') )
    return c1 - '0';
  else if ( (c1 >= 'a') && (c1 <= 'f') )
    return c1 - 'a' + 10;
  else if ( (c1 >= 'A') && (c1 <= 'F') )
    return c1 - 'A' + 10;
  else
    return 0;
}
		       
		       
void heatMash()
{
  pinMode(MASHING_PIN,OUTPUT);
  digitalWrite(MASHING_PIN,0);
}

void noHeatMash()
{
  pinMode(MASHING_PIN,OUTPUT);
  digitalWrite(MASHING_PIN,1);
}

void heatBoiler()
{
  pinMode(HEATER_3200W_PIN,OUTPUT);
  digitalWrite(HEATER_3200W_PIN,0);
  pinMode(HEATER_1600W_PIN,OUTPUT);
  digitalWrite(HEATER_1600W_PIN,0);
}

void noHeatBoiler()
{
  pinMode(HEATER_3200W_PIN,OUTPUT);
  digitalWrite(HEATER_3200W_PIN,1);
  pinMode(HEATER_1600W_PIN,OUTPUT);
  digitalWrite(HEATER_1600W_PIN,1);
}

void chillFermenter()
{
  pinMode(FERMENTER_PIN,OUTPUT);
  digitalWrite(FERMENTER_PIN,0);
}

void noChillFermenter()
{
  pinMode(FERMENTER_PIN,OUTPUT);
  digitalWrite(FERMENTER_PIN,1);
}

void stirMash()
{
  pinMode(STIRRING_PIN,OUTPUT);
  digitalWrite(STIRRING_PIN,0);
}

void noStirMash()
{
  pinMode(STIRRING_PIN,OUTPUT);
  digitalWrite(STIRRING_PIN,1);
}

void pumpOn()
{
  pinMode(PUMP_PIN,OUTPUT);
  digitalWrite(PUMP_PIN,0);
}

void pumpOff()
{
  pinMode(PUMP_PIN,OUTPUT);
  digitalWrite(PUMP_PIN,1);
}

void ValveController::feedWaterToMashTun()
{
  // it sets the valves such that the water flows from the faucet to the mash tun
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::feedWaterToBoiler()
{
  // it sets the valves such that the water flows from the faucet to the boiler
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::feedSpargeToMashTun()
{
  // it sets the valves such that the hot sparge water can be pumped from the HLT to the mash tun
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,1);
}

void ValveController::feedWortToBoiler()
{
  // it sets the valves such that the wort flows from the barrel to the boiler - this is used after sparging before boiling
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::preChilling()
{
  // it sets the valves such that the water flows from the boiler to the barrel, and the cooling water from the faucet to the CFC
  // except the bottom barrel valve which will be partially open later
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,0);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::startChilling()
{
  // it sets the valves such that the water flows from the boiler to the barrel, and the cooling water from the faucet to the CFC
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,0);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,1);
}

void ValveController::preDraining()
{
  // it sets the valves such that the wort flows from the mash tun the barrel, except the bottom barrel valve which will be later partially open
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::startDraining()
{
  // it sets the valves such that the wort flows from the mash tun the barrel
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::startWhirlpooling()
{
  // it sets the valves such that the water flows from the bottom of the boiler to the top of the boiler, driven by a pump
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,1);
}

void ValveController::startWhirlpoolingWithCooling()
{
  // it sets the valves such that the water flows from the bottom of the boiler to the top of the boiler, driven by a pump
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,0);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,1);
}

void ValveController::startReturningWortToMashTun() // return first dirty sparge batch
{
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,0);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

void ValveController::closeValves()
{
  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID1_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(SOLENOID2_BARREL_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID1_BARREL_TO_PUMP_PIN,1);
  digitalWrite(SOLENOID2_BARREL_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,0);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,0);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,0);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,0);
}

bool ValveController::areValvesStabilized()
{
  return _valveActions.empty() && !_operationPending;
}

bool ValveController::isWaterLevelStabilized()
{
  return areValvesStabilized() && ((millis() - _lastMs) >= WATERTIME);
}

void ValveController::unplugBistableValves()
{
  // this is only for bistable valves, after 5 seconds they finish the change of state and can be powered off
  //  pinMode(SOLENOID_WATER_TO_CFC_PIN,OUTPUT);
  //  pinMode(SOLENOID_WATER_TO_PUMP_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_BOILER_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,OUTPUT);
  pinMode(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_OPEN_PIN,OUTPUT);
  pinMode(VALVE_BOILER_TO_PUMP_CLOSE_PIN,OUTPUT);
  
  //  digitalWrite(SOLENOID_WATER_TO_CFC_PIN,1);
  //  digitalWrite(SOLENOID_WATER_TO_PUMP_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_BOILER_CLOSE_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_OPEN_PIN,1);
  digitalWrite(VALVE_PUMP_TO_MASH_TUN_CLOSE_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_MASH_TUN_TO_PUMP_CLOSE_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_OPEN_PIN,1);
  digitalWrite(VALVE_BOILER_TO_PUMP_CLOSE_PIN,1);
}

void ValveController::addAction(ValveOperation op, long durationMs)
{
  SmartLock lockActions(_valveActions);
  if ( op == closeValves )
    _valveActions.clear(); // it does not make sense to pile up the actions if the last one closes all
  _valveActions.push_back(std::make_pair(op,durationMs));
}

void ValveController::controlLoop()
{
  _operationPending = false;
  addAction(ValveController::closeValves); // default state of valves
  
  while (1)
    {
      while (1)
	{
	  long ms;
	  {
	    SmartLock lockActions(_valveActions);
	    if ( _valveActions.empty() )
	      break;
	    _operationPending = true;
	    _valveActions.front().first(); // call the routine
	    ms = _valveActions.front().second; // read ms, but do not wait
	    _valveActions.pop_front();
	  }
	  usleep(ms * 1000); // wait in non-locked state
	  unplugBistableValves();
	  _lastMs = millis();
	  _operationPending = false;
	}
      sleep(1);
    }
}

void BrewingPage::controlLoop()
{
  _activeStep = -1;

  while ( 1 )
    {
      usleep(100000); // 100 ms

      {
	SmartLock lockSteps(_steps);
	{
	  SmartLock lockActions(_actions);
	  while ( !_actions.empty() )
	    {
	      CallbackRecord cr = _actions.front();
	      _actions.pop_front();

	      if ( cr.callbackName == "save_recipe" )
		{
		  fstream fs;
		  fs.open ("recipe.txt",fstream::out);
		  for ( size_t i = 0; i < _steps.size(); i++ )
		    _steps[i]->exportStep(fs);
		  fs.close();
		  _recipeSaved = true;
		  invalidate();
		  continue;
		}

	      _recipeSaved = false;
	      
	      if ( cr.callbackName == "load_recipe" )
		{
		  // finish running step
		  if ( _activeStep != -1 )
		    _steps[_activeStep]->finish();
		  _activeStep = -1;

		  // deallocate everything
		  for ( size_t i = 0; i < _steps.size(); i++ )
		    delete _steps[i];
		  _steps.clear();

		  // forget pending actions - a new recipe switches the context
		  _actions.clear();

		  string line;
		  istringstream stream(cr.value);
		  while ( getline(stream,line,'\n'))
		    {
		      istringstream lineStream(line);

		      string loader;
		      lineStream >> loader;
		      CallbackRecord tmp;
		      tmp.callbackName = loader;
		      _actions.push_back(tmp);

		      int index = 1;
		      while (!lineStream.eof())
			{
			  string value;
			  lineStream >> value;
			  if ( value != "*" ) // skip defaults
			    {
			      tmp.callbackName = "initialize_parameter";
			      tmp.index = index;
			      tmp.value = value;
			      _actions.push_back(tmp);
			    }
			  index++;
			}
		    }

		  dispatcher()->get<BrewingStatus>("BREWINGSTATUS").setStatus("New recipe loaded");
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_mashing" )
		{
		  _steps.push_back(new MashingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_boiling" )
		{
		  _steps.push_back(new BoilingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_waiting" )
		{
		  _steps.push_back(new WaitingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_returning_wort" )
		{
		  _steps.push_back(new ReturnTheWortStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_fill_mash_tun" )
		{
		  _steps.push_back(new FillMashTunStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_fill_hlt" )
		{
		  _steps.push_back(new FillHLTStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_chilling" )
		{
		  _steps.push_back(new ChillingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_resting" )
		{
		  _steps.push_back(new RestingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_whirlpooling" )
		{
		  _steps.push_back(new WhirlpoolingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_draining" )
		{
		  _steps.push_back(new DrainingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_pump_wort" )
		{
		  _steps.push_back(new PumpTheWortStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "add_sparging" )
		{
		  _steps.push_back(new SpargingStep(this,_name + "_STEP_" + to_string(dispatcher()->getNewTimeStamp()),cr.callbackName));
		  invalidate();
		  continue;
		}

	      if ( cr.callbackName == "play_step" )
		if ( cr.index < _steps.size() ) // outdated command arrived
		  {
		    //		  printf("Play step %d\n",cr.index);
		    if ( _activeStep != -1 )
		      _steps[_activeStep]->finish();
		    _activeStep = cr.index;
		    if ( _activeStep != -1 )
		      _steps[_activeStep]->start();
		    invalidate();
		    continue;
		  }

	      if ( cr.callbackName == "stop_step" )
		{
		  if ( _activeStep != -1 )
		    {
		      _steps[_activeStep]->finish();
		      dispatcher()->get<BrewingStatus>("BREWINGSTATUS").setStatus("Stopped step #" + to_string(_activeStep+1));
		      _activeStep = -1;
		      invalidate();
		    }
		  continue;
		}

	      if ( cr.callbackName == "remove_step" )
		if ( cr.index < _steps.size() ) // outdated command arrived
		  {
		    if ( _activeStep == cr.index )
		      {
			_steps[_activeStep]->finish();
			dispatcher()->get<BrewingStatus>("BREWINGSTATUS").setStatus("Stopped and removed step.");
			_activeStep = -1;
		      }
		    if ( _activeStep > cr.index )
		      {
			_activeStep--;
		      }
		    delete _steps[cr.index];
		    _steps.erase(_steps.begin() + cr.index);
		    invalidate();
		    continue;
		  }

	      if ( cr.callbackName == "move_down_step" )
		if ( (cr.index+1) < _steps.size() )
		  {
		    if ( _activeStep == cr.index )
		      {
			_activeStep = cr.index+1;
		      }
		    else if ( _activeStep == (cr.index+1) )
		      {
			_activeStep = cr.index;
		      }
		    std::swap(_steps[cr.index],_steps[cr.index+1]);
		    invalidate();
		    continue;
		  }

	      if ( cr.callbackName == "move_up_step" )
		if ( cr.index < _steps.size() ) // outdated command arrived
		  if ( cr.index > 0 )
		    {
		      if ( _activeStep == cr.index )
			{
			  _activeStep = cr.index-1;
			}
		      else if ( _activeStep == (cr.index-1) )
			{
			  _activeStep = cr.index;
			}
		      std::swap(_steps[cr.index],_steps[cr.index-1]);
		      invalidate();
		      continue;
		  }

	      if ( cr.callbackName == "swap_steps" )
		if ( cr.index < _steps.size() )
		  if ( cr.destinationIndex < _steps.size() )
		    if ( cr.index != cr.destinationIndex )
		      {
			if ( _activeStep == cr.index )
			  _activeStep = cr.destinationIndex;
			else
			  {
			    if ( _activeStep > cr.index )
			      _activeStep--;
			    if ( _activeStep >= cr.destinationIndex )
			      _activeStep++;
			  }

			BrewingStep* tmp = _steps[cr.index];
			_steps.erase(_steps.begin() + cr.index);
			_steps.insert(_steps.begin() + cr.destinationIndex, tmp);


			invalidate();
			continue;
		      }

	      for ( size_t i = 0; i < _steps.size(); i++ )
		if ( cr.callbackName == _steps[i]->name() )
		  {
		    _steps[i]->callbackParameter(cr.index,cr.value);
		    invalidate();
		    continue;
		  }

	      if ( cr.callbackName == "initialize_parameter" )
		if ( ! _steps.empty() )
		  {
		    _steps.back()->callbackParameter(cr.index,cr.value);
		    invalidate();
		    continue;
		  }
	    }

	  string status;

	  // actions processed, now perform the actual step
	  if ( _activeStep != -1 ) // if running
	    {
	      if ( _steps[_activeStep]->step(status) && !_manualMode ) // do one step, return immediately if finished
		{
		  _steps[_activeStep]->finish(); // finish/finalize finished step
		  _activeStep++;

		  if ( _activeStep < _steps.size() )
		    {
		      dispatcher()->get<BrewingStatus>("BREWINGSTATUS").setStatus("Starting step #" + to_string(_activeStep+1));
		      _steps[_activeStep]->start(); // continue with the next step
		    }
		  else
		    {
		      dispatcher()->get<BrewingStatus>("BREWINGSTATUS").setStatus("Finished all steps");
		      _activeStep = -1; // last step finished
		    }

		  invalidate(); // update the table - the activity has changed
		}
	      else
		{
		  dispatcher()->get<BrewingStatus>("BREWINGSTATUS").setStatus("Step #" + to_string(_activeStep+1) + ": " + status);
		}
	    }
	}
      }
    }
}

void BrewingPage::render( ostream& stream )
{
  SmartLock lockSteps(_steps);

  stream << "<hr>";
  stream << "Menu: <button onclick=" << generateCallback("fermenting_page",1,1) << ">Show fermenting</button> ";
  stream << "<button onclick=" << generateCallback("logout",1,1) << ">Logout</button>";
  stream << "<hr>";

  stream << "<h2>Brewing</h2>";

  stream << "<h3>Recipe</h3>";

  stream << "<table>";
  stream << "<td><button onclick=" << generateCallback("add_mashing",1,1) << ">Add mashing</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_boiling",1,1) << ">Add boiling</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_waiting",1,1) << ">Add waiting</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_fill_mash_tun",1,1) << ">Fill mash tun</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_fill_hlt",1,1) << ">Fill & set HLT</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_draining",1,1) << ">Add draining</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_returning_wort",1,1) << ">Return the wort</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_sparging",1,1) << ">Add sparging</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_pump_wort",1,1) << ">Pump the wort</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_resting",1,1) << ">Add resting</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_whirlpooling",1,1) << ">Add whirlpooling</button></td>";
  stream << "<td><button onclick=" << generateCallback("add_chilling",1,1) << ">Add chilling</button></td>";
  stream << "</tr>";
  stream << "</table>";
  stream << "<br>";

  for ( size_t i = 0; i < _steps.size(); i++ )
    {
      stream << "<div draggable='true' ondragstart=" << generateDragSource("swap_steps",i,1) << ">";
      stream << "<div ondrop=" << generateDragDestination("swap_steps",i,1) << " ondragover='allowDrop(event)'>";
      stream << "<table rules=rows style='width: 100%; border: 1px solid black;'>";

      if ( i == _activeStep )
	stream << "<tr bgcolor='#99CCFF'>";
      else
	stream << "<tr>";

      stream << "<td>";
      if ( i == _activeStep )
	stream << " <button onclick=" << generateCallback("stop_step",i,1) << ">&#9726;</button>";
      else
	stream << " <button onclick=" << generateCallback("play_step",i,1) << ">&#x25B6;</button>";
      stream << " " << (i+1) << ". ";
      _steps[i]->renderDescription(stream);
      stream << "</td>";

      stream <<	"<td align='right'>";
      if ( i > 0 )
	stream << " <button onclick=" << generateCallback("move_up_step",i,1) << ">&uarr;</button>";
      if ( (i+1) < _steps.size() )
	stream << " <button onclick=" << generateCallback("move_down_step",i,1) << ">&darr;</button>";
      stream <<	" <button onclick=" << generateCallback("remove_step",i,1) << ">&times;</button>";
      stream << "</td>";

      stream << "</tr>";
      stream << "</table>";
      stream << "</div>";
      stream << "</div>";
    }

  stream << "</table><br>";

  if ( _manualMode )
    stream << "Current mode: <button onclick=" << generateCallback("toggle_mode",1,1) << ">Manual</button>";
  else
    stream << "Current mode: <button onclick=" << generateCallback("toggle_mode",1,1) << ">Automatic</button>";

  stream << "<br><br>Import: <input type='file' onchange=" << generateFileCallback("load_recipe",1,1) << "/><br>";
  
  if ( _recipeSaved )
    stream << "Export: <a href='/recipe.txt' target='_blank'>Download recipe</a>";
  else
    stream << "Export: <button onclick=" << generateCallback("save_recipe",1,1) << ">Capture recipe</button>";

  //  stream << "</div>";

  stream << "<h3>Status</h3><b>";
  stream << renderContent("BREWINGSTATUS");
  stream << "</b><br>";
  stream << renderContent("HLTCONTROLLER");  
  stream << "<br>";
  stream << renderContent("POWERMETER");  
  stream << "<br>";
  stream << renderContent("TILTREADER");
  stream << "<br>";
  stream << renderContent("VOLUMEREADER");
  stream << "<br>";
  stream << renderContent("TEMPERATUREREADER");
}

void BrewingPage::callback(const string& callbackName, int index1, int index2, const string& value, int session)
{
  //  printf("Callback brewing page: name=%s, index1=%d, index2=%d, value=%s\n", callbackName.c_str(), index1, index2, value.c_str());

  if ( callbackName == "toggle_mode" )
    {
      _manualMode = !_manualMode;
      invalidate();
    }
  
  if ( callbackName == "fermenting_page" )
    {
      if ( !isHidden() ) // protect against delayed callbacks
	{
	  hide();
	  dispatcher()->getContent("FERMENTINGPAGE")->show();
	}
      //      printf("Finished callback brewing page: name=%s, index1=%d, index2=%d, value=%s\n", callbackName.c_str(), index1, index2, value.c_str());
      return;
    }
  
  if ( callbackName == "logout" )
    {
      dispatcher()->logout(session);
      return;
    }

  SmartLock lockActions(_actions);
  CallbackRecord cr;
  cr.callbackName = callbackName;
  cr.index = index1;
  cr.value = value;
  _actions.push_back(cr); // perform actions in the main loop to avoid races
}

void BrewingPage::drop(const string& sourceName, int sourceIndex1, int sourceIndex2, 
		       const string& destinationName, int destinationIndex1, int destinationIndex2, 
		       int session)
{
  SmartLock lockActions(_actions);
  CallbackRecord cr;
  cr.callbackName = sourceName; // the sourceName is identical to the add_... command
  cr.index = sourceIndex1;
  cr.destinationIndex = destinationIndex1;
  _actions.push_back(cr); // perform actions in the main loop to avoid races
}

BrewingPage::~BrewingPage()
{
  for ( size_t i = 0; i < _steps.size(); i++ )
    delete _steps[i];
  _steps.clear();
}

MashingStep::MashingStep(BrewingPage* parent, const string& name, const string& loaderName )
  : 
  BrewingStep(parent,name,loaderName) 
{
  _durationMinutes = 30;
  _targetTemp = 67.5; 
  _accomplishedMiliseconds = 0;
}

void MashingStep::renderDescription( ostream& stream )
{
  stream 
    << "Mash <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _durationMinutes << "' size='3' maxlength='3'/>"
    << " min at T = <input type=text onchange=" << _parent->generateCallback(_name,2,1) 
    << " value='" << _targetTemp << "' size='4' maxlength='4'/> &#8451;";
}

void MashingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    _durationMinutes = atol(value.c_str());
  else if ( index == 2 )
    _targetTemp = atof(value.c_str());
}

void MashingStep::start()
{
  _lastMs = millis();
  _accomplishedMiliseconds = 0;
  stirMash();
}

bool MashingStep::step(string& status)
{
  double temp = _parent->dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getMashingTemperature();
  if ( temp > _targetTemp )
    {
      _accomplishedMiliseconds += millis() - _lastMs;
      noHeatMash();
      status = "Mashing, current T=" + to_string(temp) + "&#8451;, accomplished "
	+ to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);
    }
  else if ( temp >= (_targetTemp - HYSTERESIS ) )
    {
      _accomplishedMiliseconds += millis() - _lastMs;
      heatMash();
      status = "Mashing, current T=" + to_string(temp) + "&#8451;, accomplished "
	+ to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);
    }
  else
    {
      heatMash();
      status = "Heating before mashing, current T=" + to_string(temp) + "&#8451;, accomplished "
	+ to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);
    }

  _lastMs = millis();
  return ( _accomplishedMiliseconds / 60000 ) >= _durationMinutes;
}

void MashingStep::finish()
{
  noHeatMash();
  noStirMash();
}

BoilingStep::BoilingStep(BrewingPage* parent, const string& name, const string& loaderName )
  : 
  BrewingStep(parent,name,loaderName) 
{
  _durationMinutes = 90;
  _accomplishedMiliseconds = 0;
  _boilingPowerPercentage = 100;
}

void BoilingStep::renderDescription( ostream& stream )
{
  stream 
    << "Boil <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _durationMinutes << "' size='3' maxlength='3'/>"
    << " min at <input type=text onchange=" << _parent->generateCallback(_name,2,1)
    << " value='" << _boilingPowerPercentage << "' size='4' maxlength='4'/> percent power";
}

void BoilingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    _durationMinutes = atol(value.c_str());
  else if ( index ==  2 )
    {
      double perc = atof(value.c_str());
      if ( perc > 0.0 )
	if ( perc <= 100.0 )
	  _boilingPowerPercentage = perc;
    }
}

void BoilingStep::start()
{
  _parent->dispatcher()->get<HLTController>("HLTCONTROLLER").deactivate();
  _lastMs = millis();
  _accomplishedMiliseconds = 0;
  _stepCounter = 0;
}

bool BoilingStep::step(string& status)
{
  double temp = _parent->dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getBoilerTemperature();
  double volume = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();
  if ( volume < 20 )
    {
      noHeatBoiler();
      status = "Not enough water to start boiling";
    }
  /*
  else if ( temp > BOILINGTEMP )
    {
      _accomplishedMiliseconds += millis() - _lastMs;
      noHeatBoiler();
      status = "Boiling, current T=" + to_string(temp) + "&#8451;, accomplished "
	+ to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);
    }
  */
  else if ( temp >= (BOILINGTEMP - HYSTERESIS ) )
    {
      _accomplishedMiliseconds += millis() - _lastMs;
      status = "Boiling, current T=" + to_string(temp) + "&#8451;, accomplished "
	+ to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);
      _stepCounter++;

      if ( ((_stepCounter % 50) * 2.0) < _boilingPowerPercentage )
	heatBoiler();
      else
	noHeatBoiler();	
    }
  else
    {
      heatBoiler();
      status = "Heating before boiling, current T=" + to_string(temp) + "&#8451;, accomplished "
	+ to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);
    }

  _lastMs = millis();
  return ( _accomplishedMiliseconds / 60000 ) >= _durationMinutes;
}

void BoilingStep::finish()
{
  noHeatBoiler();
}

WaitingStep::WaitingStep(BrewingPage* parent, const string& name, const string& loaderName )
  : 
  BrewingStep(parent,name,loaderName) 
{
  _running = false;
  _elapsedMiliseconds = 0;
}

void WaitingStep::renderDescription( ostream& stream )
{
  if ( _running )
    stream << "<button onclick=" << _parent->generateCallback(_name,1,1) << ">Press this button to continue</button>";
  else
    stream << "Wait for the user";
}

void WaitingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    _running = false;
}

void WaitingStep::start()
{
  _lastMs = millis();
  _elapsedMiliseconds = 0;
  _running = true;
  _parent->invalidate();
}

bool WaitingStep::step(string& status)
{
  _elapsedMiliseconds += millis() - _lastMs;
  _lastMs = millis();
  status = "Waiting, elapsed " + to_string(_elapsedMiliseconds/60000) + ":" + to_string((_elapsedMiliseconds/1000)%60);
  return ! _running;
}

void WaitingStep::finish()
{
  _running = false;
}

void FermentingPage::controlLoop()
{
  while (1)
    {
      sleep(60);

      {
	SmartLock lock(_currentTemp);

	_currentTemp = dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getFermentingTemperature();

	time_t t = time(NULL);
	_time.push_back(t);
	_history.push_back(_currentTemp);
	if ( _history.size() > MAX_HISTORY_SIZE )
	  {
	    _history.pop_front();
	    _time.pop_front();
	  }

	if ( _currentTemp <= _targetTemp )
	  noChillFermenter();
	else if ( _currentTemp > (_targetTemp+HYSTERESIS) )
	  chillFermenter();

	invalidate(); // update the GUI
      }
    }
}

void FermentingPage::render( ostream& stream )
{
  SmartLock lock(_currentTemp);

  stream << "<hr>";
  stream << "Menu: <button onclick=" << generateCallback("brewing_page",1,1) << ">Show brewing</button> ";
  stream << "<button onclick=" << generateCallback("logout",1,1) << ">Logout</button>";
  stream << "<hr>";

  stream << "<h2>Fermenting</h2>";
  stream << "Ferment at T = <input type=text onchange=" << generateCallback("set_fermenting_temp",1,1) 
	 << " value='" << _targetTemp << "' size='4' maxlength='4'/> &#8451;";

  stream << "<h3>Status</h3>";
  stream << "<b>Current T=" << _currentTemp << "</b>";

  stream << "<br><br>";
  stream << renderContent("TILTREADER");

  stream << "<br>";
  stream << renderContent("VOLUMEREADER");

  stream << "<br>";
  stream << renderContent("TEMPERATUREREADER");

  stream << "<h3>History</h3>";
  stream << "<div id='div_history_fermenting'></div>";

  stream << generateScriptStart();
  stream << "{ var g = new Dygraph(document.getElementById('div_history_fermenting'),"
	 << "'Date,Temp[C]\\n' + ";
  for ( size_t i = 0; i < _time.size(); i++ )
    {
      time_t t = _time[i];
      tm *ltm = localtime(&t);
      stream << "'" << (1900 + ltm->tm_year) << "/" << (1 + ltm->tm_mon) << "/" << ltm->tm_mday
	     << " " << ltm->tm_hour << ":" << ltm->tm_min // << ":" << ltm->tm_sec
	     << "," << _history[i] << "\\n' + ";
    }
  stream << "''); g.resize(600,250); }"; // resize is a trick to make the graph visible, because it is initially drawn in the hidden page
  stream << generateScriptEnd();
}

void FermentingPage::callback(const string& callbackName, int index1, int index2, const string& value, int session)
{
  //  printf("Callback fermenting page: name=%s, index1=%d, index2=%d, value=%s\n", callbackName.c_str(), index1, index2, value.c_str());

  if ( callbackName == "brewing_page" )
    {
      if ( !isHidden() ) // protect against delayed callbacks
	{
	  hide();
	  dispatcher()->getContent("BREWINGPAGE")->show();
	}
    }
  else if ( callbackName == "set_fermenting_temp" )
    {
      _targetTemp = atof(value.c_str());
      invalidate();
    }
  else if ( callbackName == "logout" )
    {
      dispatcher()->logout(session);
    }
}

void HLTController::controlLoop()
{
  while (1)
    {
      sleep(1);
      if ( _active )
	{
	  double volume = dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();
	  if ( volume < 20 )
	    noHeatBoiler();	    
	  else if ( dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getBoilerTemperature() > _targetTemp )
	    noHeatBoiler();
	  else
	    heatBoiler();
	}
      invalidate();
    }
}

void HLTController::activate()
{
  _active = true;
  invalidate();
}

void HLTController::deactivate()
{
  _active = false;
  noHeatBoiler();
  invalidate();
}

bool HLTController::isTargetTemperatureReached()
{
  return ( dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getBoilerTemperature() > ( _targetTemp - HYSTERESIS ) );
}

void HLTController::render( ostream& stream )
{
  if ( ! isActive() )
    stream << "HLT heating not active";
  else
    {
      double volume = dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();
      if ( volume < 20 )
	stream << "Not enough water to heat HLT";
      else if ( isTargetTemperatureReached() )
	stream << "Target HLT temperature reached: T=" << dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getBoilerTemperature() << ", target=" << _targetTemp;
      else
	stream << "Target HLT temperature not reached: T=" << dispatcher()->get<TemperatureReader>("TEMPERATUREREADER").getBoilerTemperature() << ", target=" << _targetTemp;

    }}

void FillMashTunStep::renderDescription(ostream& stream )
{
  stream 
    << "Fill <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _targetVolume << "' size='3' maxlength='3'/>"
    << " liters of water to mash tun.";  
}

void FillMashTunStep::setParameter(int index, const string& value)
{
  double val = atof(value.c_str());
  if ( index == 1 )
    if ( val < 50.0 )
      if ( val > 0.0 )
	_targetVolume = val;
}

void FillMashTunStep::start()
{
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::feedWaterToMashTun);  
}

bool FillMashTunStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Filling mash tun: turning the valves";
      return false;
    }

  pumpOn(); // to avoid leak for the barrel valve

  double volume = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeMashTun();
  status = "Filling mash tun: " + to_string(volume) + " of " + to_string(_targetVolume) + " liters";
  return volume >= _targetVolume;
}

void FillMashTunStep::finish()
{
  pumpOff();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

void FillHLTStep::renderDescription(ostream& stream )
{
  stream 
    << "Fill <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _targetVolume << "' size='3' maxlength='3'/>"
    << " liters of water to HLT"
    << " and start heating to T = <input type=text onchange=" << _parent->generateCallback(_name,2,1) 
    << " value='" << _targetTemp << "' size='4' maxlength='4'/> &#8451;";

}

void FillHLTStep::setParameter(int index, const string& value)
{
  double val = atof(value.c_str());

  if ( index == 1 )
    if ( val < 85.0 )
      if ( val > 0.0 )
	_targetVolume = val;

  if ( index == 2 )
    _targetTemp = val;
}

void FillHLTStep::start()
{
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::feedWaterToBoiler);
}

bool FillHLTStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Filling HLT: turning the valves";
      return false;
    }
  
  pumpOn(); // to avoid leak for the barrel valve

  double volume = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();
  status = "Filling HLT: " + to_string(volume) + " of " + to_string(_targetVolume) + " liters";
  if (volume >= _targetVolume)
    {
      _parent->dispatcher()->get<HLTController>("HLTCONTROLLER").setTargetTemperature(_targetTemp);
      return true;
    }
  else
    {
      return false;
    }
}

void FillHLTStep::finish()
{
  pumpOff();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

void ChillingStep::renderDescription(ostream& stream )
{
  stream 
    << "Chill the content of boil kettle using the chiller (valve open "
    << "<input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _valveOpenPercentage << "' size='4' maxlength='4'/> percent)";

}

void ChillingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    {
      double perc = atof(value.c_str());
      if ( perc > 0.0 )
	if ( perc <= 100.0 )
	  _valveOpenPercentage = perc;
    }
}

void ChillingStep::start()
{
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::preChilling);
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::startChilling,
									   (long)(_valveOpenPercentage * VALVETIME / 100.0));
}

bool ChillingStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Chilling: turning the valves";
      return false;
    }
  
  double volume = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();
  status = "Chilling: " + to_string(volume) + " liters remains";
  
  return (volume <= 0);
}

void ChillingStep::finish()
{
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

WhirlpoolingStep::WhirlpoolingStep(BrewingPage* parent, const string& name, const string& loaderName )
  : 
  BrewingStep(parent,name,loaderName) 
{
  _durationMinutes = 10;
  _accomplishedMiliseconds = 0;
  _withCooling = true;
}

void WhirlpoolingStep::renderDescription( ostream& stream )
{
  stream 
    << "Whirlpooling <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _durationMinutes << "' size='3' maxlength='3'/>"
    << " min ";

  stream << "<input type='checkbox' onclick=" << _parent->generateCallback(_name,2,1);
  if ( _withCooling )
    stream << " checked='checked' value='turn_cooling_off'";
  else
    stream << " value='turn_cooling_on'";
  stream << "> with cooling";
}

void WhirlpoolingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    _durationMinutes = atol(value.c_str());
  else if ( index == 2 )
    {
      _withCooling = (value == "turn_cooling_on");
    }
}

void WhirlpoolingStep::start()
{
  if ( _withCooling )
    _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::startWhirlpoolingWithCooling);
  else
    _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::startWhirlpooling);
  
  _lastMs = millis();
  _accomplishedMiliseconds = 0;
}

bool WhirlpoolingStep::step(string& status)
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Whirlpooling: turning the valves";
      _lastMs = millis();
      return false;
    }

  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").isWaterLevelStabilized() )
    {
      status = "Whirlpooling: waiting for water level stabilization";
      _lastMs = millis();
      return false;
    }
  
  pumpOn(); // pump only after the valves have stabilized

  _accomplishedMiliseconds += millis() - _lastMs;

  status = "Whirlpooling: accomplished "
    + to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);

  _lastMs = millis();
  return ( _accomplishedMiliseconds / 60000 ) >= _durationMinutes;
}

void WhirlpoolingStep::finish()
{
  pumpOff();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

void DrainingStep::renderDescription(ostream& stream )
{
  stream 
    << "Drain the content of mash tun to the barrel for "
    << "<input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _durationSeconds << "' size='4' maxlength='4'/> seconds (valve open "
    << "<input type=text onchange=" << _parent->generateCallback(_name,2,1) 
    << " value='" << _valveOpenPercentage << "' size='4' maxlength='4'/> percent)";

  stream << " <input type='checkbox' onclick=" << _parent->generateCallback(_name,3,1);
  if ( _withStirring )
    stream << " checked='checked' value='turn_stirring_off'";
  else
    stream << " value='turn_stirring_on'";
  stream << "> with stirring";

}

void DrainingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    _durationSeconds = atol(value.c_str());
  else if ( index == 2 )
    {
      double perc = atof(value.c_str());
      if ( perc > 0.0 )
	if ( perc <= 100.0 )
	  _valveOpenPercentage = perc;
    }
  else if ( index == 3 )
    {
      _withStirring = (value == "turn_stirring_on");      
    }
}

void DrainingStep::start()
{
  _lastMs = millis();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::preDraining);
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::startDraining,
									   (long)(_valveOpenPercentage * VALVETIME / 100.0));
}

bool DrainingStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Draining: turning the valves";
      _lastMs = millis();
      return false;
    }

  if ( _withStirring )
    stirMash();
  else
    noStirMash();
  
  long accomplished = (millis() - _lastMs) / 1000;
  status = "Draining: accomplished " + to_string(accomplished) + " seconds";
  
  return (accomplished >= _durationSeconds);
}

void DrainingStep::finish()
{
  noStirMash();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

void PumpTheWortStep::renderDescription(ostream& stream )
{
  stream 
    << "Pump the wort from the barrel to the boiling kettle.";

}

void PumpTheWortStep::start()
{
  _previousVolume = 0.0;
  _lastVolumeSampledMs = millis();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::feedWortToBoiler);
}

bool PumpTheWortStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      _previousVolume = 0.0;
      _lastVolumeSampledMs = millis();
      status = "Pumping the wort: turning the valves";
      return false;
    }

  pumpOn();
  
  double volume = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();
  status = "Pumping the wort: " + to_string(volume) + " liters in the boiler";

  if ( volume > 75 ) // max capacity reached
    return true;

  if ( (millis() - _lastVolumeSampledMs) > 10000 ) // check every 10s for the increase of volume
    {
      if ( volume <= _previousVolume )
	return true; // stagnation of volume in the boiler

      _previousVolume = volume;
      _lastVolumeSampledMs = millis();
    }
  
  return false;
}

void PumpTheWortStep::finish()
{
  pumpOff();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

void ReturnTheWortStep::renderDescription(ostream& stream )
{
  stream 
    << "Return the initial wort from the barrel to the mash tun.";

}

void ReturnTheWortStep::start()
{
  _previousVolume = 0.0;
  _lastVolumeSampledMs = millis();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::startReturningWortToMashTun);
}

bool ReturnTheWortStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      _previousVolume = 0.0;
      _lastVolumeSampledMs = millis();
      status = "Returning the wort: turning the valves";
      return false;
    }

  pumpOn();
  
  double volume = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeMashTun();
  status = "Returning the wort: " + to_string(volume) + " liters in the mash tun";

  if ( volume > 45 ) // max capacity reached
    return true;
  
  if ( (millis() - _lastVolumeSampledMs) > 10000 ) // check every 10s for the increase of volume
    {
      if ( volume <= _previousVolume )
	return true; // stagnation of volume in the boiler

      _previousVolume = volume;
      _lastVolumeSampledMs = millis();
    }
  
  return false;
}

void ReturnTheWortStep::finish()
{
  pumpOff();
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

void SpargingStep::renderDescription(ostream& stream )
{
  if ( _spargeAll )
    {
      stream 
	<< "Sparge with <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
	<< " value='" << "all" << "' size='3' maxlength='3'/>"
	<< " liters of water from HLT to mash tun.";
    }
  else
    {
      stream 
	<< "Sparge with <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
	<< " value='" << _targetVolume << "' size='3' maxlength='3'/>"
	<< " liters of water from HLT to mash tun.";
    }
}

void SpargingStep::setParameter(int index, const string& value)
{
  if ( value == "all" )
    {
      _spargeAll = true;
    }
  else
    {
      _spargeAll = false;
      double val = atof(value.c_str());
      if ( index == 1 )
	if ( val < 50.0 )
	  if ( val > 0.0 )
	    _targetVolume = val;
    }
}

void SpargingStep::start()
{
  _step = 0;
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::feedSpargeToMashTun);
  _wasHLTActive = _parent->dispatcher()->get<HLTController>("HLTCONTROLLER").isActive();
}

bool SpargingStep::step( string& status )
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Sparging: turning the valves";
      return false;
    }

  double volumeMashTun = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeMashTun();
  double volumeBoiler = _parent->dispatcher()->get<VolumeReader>("VOLUMEREADER").volumeBoiler();

  // step 0: initialize and check volume
  if ( _step == 0 )
    {
      _initialVolume = volumeMashTun;
      if ( (volumeBoiler < _targetVolume) && !_spargeAll && !_parent->manualMode() )
	{
	  status = "Problem: not enough water in HLT to perform sparging!";
	  _step = 0; // waits forever - user needs to press something to skip the step of change the target volume or fill the water manually
	}
      else
	{
	  status = "Sparging: read initial volume OK";
	  _step = 1; // proceed to next part
	}
      return false;
    }
  
  if ( _step == 1 ) // check the HLT temperature
    {
      if ( !_parent->dispatcher()->get<HLTController>("HLTCONTROLLER").isTargetTemperatureReached() && !_parent->manualMode() )
	{
	  status = "Waiting for HLT to heat up!";
	  _parent->dispatcher()->get<HLTController>("HLTCONTROLLER").activate(); // make sure the water is warm enough
	  _step = 1; // check the temperature again next time
	}
      else
	{
	  status = "Sparging: reading HLT temperature OK";
	  _parent->dispatcher()->get<HLTController>("HLTCONTROLLER").deactivate(); // the HLT might be emptied so we should not heat it during pumping
	  _step = 2; // proceed to next part
	}
      return false;      
    }

  if ( _step == 2 )
    {
      _previousVolume = volumeMashTun;
      _lastVolumeSampledMs = millis();
      _step = 3;
    }

  if ( volumeMashTun > 45 ) // max capacity reached
    return true;
  
  pumpOn();

  double alreadyPumped = volumeMashTun - _initialVolume;
  
  if ( _spargeAll )
    {
      if ( (millis() - _lastVolumeSampledMs) > 10000 ) // check every 10s for the increase of volume
	{
	  if ( volumeMashTun <= _previousVolume )
	    return true; // stagnation of volume in the boiler

	  _previousVolume = volumeMashTun;
	  _lastVolumeSampledMs = millis();
	}

      status = "Sparging all: pumped  " + to_string(alreadyPumped) + " liters";
      return false;
    }
  else
    {
      status = "Sparging: accomplished " + to_string(alreadyPumped) + " of " + to_string(_targetVolume) + " liters";
      return alreadyPumped >= _targetVolume;
    }
}

void SpargingStep::finish()
{
  pumpOff();
  if ( _wasHLTActive )
    _parent->dispatcher()->get<HLTController>("HLTCONTROLLER").activate(); // will turn off if thevolume < 20
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}

RestingStep::RestingStep(BrewingPage* parent, const string& name, const string& loaderName )
  : 
  BrewingStep(parent,name,loaderName) 
{
  _durationMinutes = 10;
  _accomplishedMiliseconds = 0;
}

void RestingStep::renderDescription( ostream& stream )
{
  stream 
    << "Resting <input type=text onchange=" << _parent->generateCallback(_name,1,1) 
    << " value='" << _durationMinutes << "' size='3' maxlength='3'/>"
    << " min";
}

void RestingStep::setParameter(int index, const string& value)
{
  if ( index == 1 )
    _durationMinutes = atol(value.c_str());
}

void RestingStep::start()
{
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
  _lastMs = millis();
  _accomplishedMiliseconds = 0;
}

bool RestingStep::step(string& status)
{
  if ( !_parent->dispatcher()->get<ValveController>("VALVECONTROLLER").areValvesStabilized() )
    {
      status = "Resting: closing the valves";
      _lastMs = millis();
      return false;
    }
  
  _accomplishedMiliseconds += millis() - _lastMs;

  status = "Resting: accomplished "
    + to_string(_accomplishedMiliseconds/60000) + ":" + to_string((_accomplishedMiliseconds/1000)%60);

  _lastMs = millis();
  return ( _accomplishedMiliseconds / 60000 ) >= _durationMinutes;
}

void RestingStep::finish()
{
  _parent->dispatcher()->get<ValveController>("VALVECONTROLLER").addAction(ValveController::closeValves);
}


double PowerMeter::_power = 0.0;

void PowerMeter::isrHandler()
{
  static long prevTime = millis();
  long currentTime = millis();
  long duration = currentTime - prevTime; // 800 impulses per kWh, 1 second corresponds to 4500W, 1 milisecond corresponds to 4500000W

  if ( duration < 200 ) /// filter out peaks, 200 ms, corresponds to 22.5kW
    return;
  prevTime = currentTime;

  _power = 4.5e6 / duration;
}
  
void PowerMeter::controlLoop()
{
  double _oldPower = _power;

  wiringPiSetup();
  
  pinMode(POWER_IN_PIN,INPUT);
  pullUpDnControl(POWER_IN_PIN,PUD_UP);
  wiringPiISR(POWER_IN_PIN,INT_EDGE_FALLING,&isrHandler);

  while (1)
    {
      sleep(1);
      if ( _oldPower != _power )
	{
	  _oldPower = _power;
	  invalidate();
	}
    }
}
  
void PowerMeter::render( ostream& stream )
{
  stream << "Power consumption: " << _power << " W";
}

void BrewingStep::exportStep( ostream& stream )
{
  stream << _loaderName;

  if ( ! _parameters.empty() )
    {
      for ( int index = 1; index <= _parameters.rbegin()->first; index++ ) // ! numbering starts from 1
	{
	  stream << " ";
	  if ( _parameters.find(index) == _parameters.end() )
	    stream << "*";
	  else
	    stream << _parameters[index];
	}
    }

  stream << "\n";
}

void TiltReader::controlLoop()
{
  _temp = 0.0;
  _sg = 1.000;
  
  /*
    The following code utilizes the well known Tilt hydrometer.
    Since we don't want to depend on third party hardware in this release, we comment this out
    and assume default values for the example.
  */
  /*
  while (1)
    {
      sleep(120);
      
      system("sudo gatttool -b 50:65:83:6D:AA:85 --char-read -a 0x37 >/tmp/tilt.txt");
      system("sudo gatttool -b 50:65:83:6D:AA:85 --char-read -a 0x3b >>/tmp/tilt.txt");
      char buff[100];

      FILE *fp = fopen("/tmp/tilt.txt","r");
      if ( fp == NULL )
	continue; // unable to open file

      int nrRead = fread(buff,1,sizeof(buff)-1,fp);
      fclose(fp);
      if ( nrRead < 84 )
	continue; // unable to read the temperature part

      double Fahrenheit = ((fromHexa(buff[36]) * 16.0 + fromHexa(buff[37])) * 16.0 + fromHexa(buff[33])) * 16.0 + fromHexa(buff[34]);
      _temp = (Fahrenheit - 32.0) * 5.0 / 9.0;

      double val = ((fromHexa(buff[82]) * 16.0 + fromHexa(buff[83])) * 16.0 + fromHexa(buff[79])) * 16.0 + fromHexa(buff[80]);
      _sg = val / 1000.0;

      invalidate();
    }
  */
}
  
void TiltReader::render( ostream& stream )
{
  stream << "Tilt temperature=" << _temp << "&#8451;<br>Tilt gravity=" << _sg;
}

void VolumeReader::controlLoop()
{
  _volumeMashTun = 0.0;
  _voltageMashTun = 0.0;
  _volumeBoiler = 0.0;
  _voltageBoiler = 0.0;
  
  /*
    The following code measures the volumes using pressure sensors and ADC module from AB electronics.
    Since we don't want to include third party library in this release, we comment this out
    and assume zero volumes for the example.
  */

  /*
  while (1)
    {
      sleep(1);

      ADCPi adc(0x68, 0x69, 14);
      adc.set_conversion_mode(0);

      _voltageMashTun = adc.read_voltage(1);
      _volumeMashTun = (_voltageMashTun - 1.178) / 0.007805556 + 10.875;
      if ( _volumeMashTun < 0.0 )
	_volumeMashTun = 0.0;
      
      _voltageBoiler = adc.read_voltage(2);
      _volumeBoiler = (_voltageBoiler - 1.01) / 0.00693 + 10.0;
      if ( _volumeBoiler < 0.0 )
	_volumeBoiler = 0.0;
      
      invalidate();
    }
  */
}
  
void VolumeReader::render( ostream& stream )
{
  stream << "Mash tun: volume=" << _volumeMashTun << " l (" << _voltageMashTun << " V)"
	 << "<br>Boiler: volume=" << _volumeBoiler << " l (" << _voltageBoiler << " V)";
}

