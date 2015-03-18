/*************************************************** 
  Adapted from the CC3000 Wifi Breakout buildtest example and 
  https://learn.adafruit.com/wireless-gardening-arduino-cc3000-wifi-modules?view=all
  then changed client.print() to client.fastrprint() as per 
  http://forum.carriots.com/index.php/topic/61-wireless-gardening-with-arduino-cc3000-wifi-modules/page-2
  
  Then modified to include more robust code from
  https://learn.adafruit.com/adafruit-cc3000-wifi-and-xively?view=all
  
  Designed specifically to work with the Adafruit WiFi products:
  ----> https://www.adafruit.com/products/1469
 ****************************************************/
 
// TODO: add a green and red LED
// * turn green on when all ok and red when action required (useful for testing/working out what is good and bad!)
// * blink when doing something
// * indicate when starting up/connecting to wifi (trouble areas)

// TODO:
// * light sensor
// * watering pump & water count

// TODO: improve how wifi connections are handled to ensure it works more regularly (connecting, DHCP, resolve)
// * create separate method with SID and PWD
// * try h2 and h3 networks
// * check networks in range and attempt to connect
// * log data somewhere
// * abandon/reboot after timeout exceeded

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include <Sensirion.h>
#include <stdlib.h>
#include <avr/wdt.h>
#include "DHT.h"
#include "keys.h"

#define DHTPIN 4     // internal temperature sensor (DHT22 - https://www.adafruit.com/products/393)
// pull up wiring -> https://learn.adafruit.com/wifi-weather-station-arduino-cc3000?view=all

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// Soil sensor pins
const uint8_t dataPin  =  6;
const uint8_t clockPin =  7;

// Soil sensor variables
float t;
float h;
float dewpoint;

// character representations to use with fastrprint() methods
char soilTempChar[32];
char soilHumidChar[32];
char airTempChar[32];
char airHumidChar[32];
char lightChar[32];

// Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHTTYPE);
// NOTE: For working with a faster chip, like an Arduino Due or Teensy, you
// might need to increase the threshold for cycle counts considered a 1 or 0.
// You can do this by passing a 3rd parameter for this threshold.  It's a bit
// of fiddling to find the right value, but in general the faster the CPU the
// higher the value.  The default for a 16mhz AVR is a value of 6.  For an
// Arduino Due that runs at 84mhz a value of 30 works.
// Example to initialize DHT sensor for Arduino Due:
//DHT dht(DHTPIN, DHTTYPE, 30);

// Create soil sensor object
Sensirion soilSensor = Sensirion(dataPin, clockPin);

// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed but DI

// hold the resolved IP address for WEBSITE
uint32_t ip;

int buffer_size = 20;

// time (ms) to wait after upload
// you can't use 60*1000 to indicate 60,000, you need to enter 60000
//const long WAIT_BETWEEN_UPLOAD = 60000;  // 1 minute
const long WAIT_BETWEEN_UPLOAD = 600000; // 10 minutes

// photocell https://www.adafruit.com/product/161
// guide https://learn.adafruit.com/photocells?view=all
int photocellPin = 0;     // the cell and 10K pulldown are connected to a0
int photocellReading;     // the analog reading from the sensor divider

/**************************************************************************/
/*!
    @brief  Sets up the HW and the CC3000 module (called automatically
            on startup)
*/
/**************************************************************************/
void setup(void)
{
  Serial.begin(115200);

  Serial.println(F("\nStarting up!\n"));

  // TODO: not sure if this adds value
  displayDriverMode();
  // good to know if there is an issue...
//  Serial.print(F("Free RAM: ")); Serial.println(getFreeRam(), DEC);
  
  /* Initialise the module */
  Serial.print(F("\nInitialising WiFi chip..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while(1);
  }
  Serial.println(F("done!"));

  // TODO: probably not required as we are wiring it all together and know the firmware is ok
/*  uint16_t firmware = checkFirmwareVersion();
  if (firmware < 0x113) {
    Serial.println(F("Wrong firmware version!"));
    for(;;);
  }*/ 

  // TODO: nice to know???  
//  displayMACAddress();
  
  /* Optional: Get the SSID list (not available in 'tiny' mode) */
#ifndef CC3000_TINY_DRIVER
  listSSIDResults();
#endif
  
  // Start watchdog - will reset Arduino if nothing happens for 8 seconds
  wdt_enable(WDTO_8S);

  int attempt = 0;
  
  /* Delete any old connection data on the module */
  Serial.println(F("\nDeleting old connection profiles..."));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    attempt++;
    if (attempt<10)
    {
      // Reset watchdog
      wdt_reset();
    }
  }
  Serial.println(F("done!"));
  
  // Reset watchdog
  wdt_reset();

  /* Attempt to connect to an access point */
  Serial.print(F("\nAttempting to connect to ")); Serial.print(WLAN_SSID); Serial.print(F("..."));
  
  attempt = 0;
  
  // Reset watchdog & disable
  wdt_reset();
  wdt_disable();

  // TODO: check maximum number of retries
  // probably need to set max as 1 and then put the watchdog around this as well to control how long to wait
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY, 5)) {
    Serial.print(F("FAILED!"));
    attempt++;
    if (attempt<50)
    {
      // Reset watchdog
      wdt_reset();
    }
  }   
  Serial.println(F("done!"));
  
  // Start watchdog - will reset Arduino if nothing happens for 8 seconds
  wdt_enable(WDTO_8S);

  // Reset watchdog
  wdt_reset();

  attempt = 0;
  
  /* Wait for DHCP to complete */
  Serial.print(F("Request DHCP")); Serial.print(F("..."));
  while (!cc3000.checkDHCP())
  {
    delay(100);

    // this is where it stalls if it is on the edge of the network
    Serial.print(F("."));
    attempt++;
    if (attempt<500)
    {
      // Reset watchdog 500 times waiting 100ms each time (50 seconds in total)
      wdt_reset();
    }
  }
  Serial.println(F("done!"));

  // Start watchdog - will reset Arduino if nothing happens for 8 seconds
  // Reset watchdog
  wdt_reset();

  /* Display the IP address DNS, Gateway, etc. */  
  attempt = 0;
  while (!displayConnectionDetails()) {
    delay(1000);

    // protect this section from failures with a timeout
    attempt++;
    if (attempt<50)
    {
      // Reset watchdog for 50 times waiting 1000ms each time (50 seconds in total)
      wdt_reset();
    }
  }
  
  // Reset watchdog
  wdt_reset();

  // setup DHT22 sensor
  dht.begin();

  // Reset watchdog & disable
  wdt_reset();
  wdt_disable();
}

void loop(void)
{
  Serial.println(F("Starting loop..."));
  
  // Start watchdog - will reset Arduino if nothing happens for 8 seconds
  wdt_enable(WDTO_8S);
  
  Serial.print(F("Free RAM: ")); Serial.println(getFreeRam(), DEC);

  // Get the website IP & print it
  ip = 0;
  Serial.print(WEBSITE); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  cc3000.printIPdotsRev(ip);
  Serial.println(F(""));
  
  // Reset watchdog
  wdt_reset();

  // Get soil data & transform to integers
  soilSensor.measure(&t, &h, &dewpoint);

  // Convert data to String
  dtostrf(t, 2, 2, soilTempChar);
  dtostrf(h, 2, 2, soilHumidChar);

  // Reset watchdog
  wdt_reset();

  // get DHT22 humidity and temp
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  h = dht.readHumidity();
  // Read temperature as Celsius
  t = dht.readTemperature();
  
  // Reset watchdog
  wdt_reset();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    //return;
    h = 0;
    t = 0;
  }
  
  // Convert data to String
  dtostrf(t, 2, 2, airTempChar);
  dtostrf(h, 2, 2, airHumidChar);

  // Reset watchdog
  wdt_reset();
  
  photocellReading = analogRead(photocellPin);  
  dtostrf(photocellReading, 2, 2, lightChar);
  
  // Prepare JSON for Carriots & get length
  int length = 0;

  String data = "{\"protocol\":\"v2\",\"device\":\""+String(DEVICE)+"\",\"at\":\"now\",\"data\":{\"SoilTemperature\":"+soilTempChar+",\"SoilHumidity\":"+soilHumidChar+",\"AirTemperature\":"+airTempChar+",\"AirHumidity\":"+airHumidChar+",\"Light\":"+lightChar+"}}";

  length = data.length();

  // Reset watchdog
  wdt_reset();
  
  // Print request for debug purposes
  /*Serial.println(F("POST /streams HTTP/1.1"));
  Serial.println(F("Host: api.carriots.com"));
  Serial.println(F("Accept: application/json"));
  Serial.println(F("User-Agent: Arduino-Carriots"));
  Serial.println(F("Content-Type: application/json"));
  Serial.print(F("carriots.apikey: "));
  Serial.println(API_KEY);
  Serial.print(F("Content-Length: "));
  Serial.println(String(length));
  Serial.print(F("Connection: close"));
  Serial.println();*/
  Serial.println(data);
  
  // Check connection to WiFi
  Serial.print(F("Checking WiFi connection ..."));
  if(!cc3000.checkConnected()){while(1){}}
  Serial.println(F("done."));
  wdt_reset();

  // Ping server
  Serial.print(F("Pinging server ..."));
  // TODO: ping or not??
//  if(!cc3000.ping(ip, 1)){while(1){}}
  Serial.println(F("done."));
  wdt_reset();

  // Send request
  Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
  if (client.connected()) {
    Serial.print(F("Connected, sending data..."));
    
    client.fastrprintln(F("POST /streams HTTP/1.1"));
    Serial.print(F("1"));
    client.fastrprintln(F("Host: api.carriots.com"));
    Serial.print(F("2"));
    client.fastrprintln(F("Accept: application/json"));
    Serial.print(F("3"));
    client.fastrprintln(F("User-Agent: Arduino-Carriots"));
    Serial.print(F("4"));
    client.fastrprintln(F("Content-Type: application/json"));
    Serial.print(F("5"));
    client.fastrprint(F("carriots.apikey: ")); 
    Serial.print(F("6"));
    client.fastrprintln(API_KEY);
    Serial.print(F("7"));
    client.fastrprint(F("Content-Length: "));
    Serial.print(F("8"));
    client.println(length);
    Serial.print(F("9"));
    client.fastrprintln(F("Connection: close"));
    Serial.print(F("10"));
    client.println();

    // Reset watchdog
    wdt_reset();
       
    // TODO: test this way of sending data - is it better?
    ///////  for some reason the data string is corrupted by now; i don't think the send method works in its current form
    // Send data
    /*Serial.print(F("Sending data ..."));
    Serial.println(F("\nTHEDATA"));  Serial.println(data);
    client.fastrprintln(F(""));    
    sendData(client,data,buffer_size);  
    client.fastrprintln(F(""));
    Serial.println(F("done."));*/

    Serial.print(F("11"));
    client.fastrprint(F("{\"protocol\":\"v2\",\"device\":\""));
    Serial.print(F("12"));
    client.fastrprint(F("SoilMonitor@phickman.phickman"));
    Serial.print(F("13"));
    client.fastrprint(F("\",\"at\":\"now\",\"data\":{\"SoilTemperature\":"));
    Serial.print(F("14"));
    client.fastrprint(soilTempChar);
    Serial.print(F("15"));
    client.fastrprint(F(",\"SoilHumidity\":"));
    Serial.print(F("16"));
    client.fastrprint(soilHumidChar);
    Serial.print(F("17"));
    client.fastrprint(F(",\"AirTemperature\":"));
    Serial.print(F("a"));
    client.fastrprint(airTempChar);
    Serial.print(F("b"));
    client.fastrprint(F(",\"AirHumidity\":"));
    Serial.print(F("c"));
    client.fastrprint(airHumidChar);
    Serial.print(F("d"));
    client.fastrprint(F(",\"Light\":"));
    Serial.print(F("e"));
    client.fastrprint(lightChar);
    Serial.print(F("f"));
    client.fastrprintln(F("}}"));
    Serial.print(F("18"));

    Serial.print(F("...Sent!\n"));
    
    // Reset watchdog
    wdt_reset();    
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
  
  Serial.println(F("Reading answer...\n-------------------------------------"));
  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }
  client.close();
  Serial.println(F("\n-------------------------------------"));

  // Reset watchdog
  wdt_reset();

  // Close connection and disconnect
  client.close();
  Serial.println(F("Closing connection"));
  
  // Reset watchdog & disable
  wdt_reset();
  wdt_disable();
  
  // Wait until next update
  // TODO: wait function, get working
  Serial.print(F("Sleeping until next time...")); Serial.println(WAIT_BETWEEN_UPLOAD);
  wait(WAIT_BETWEEN_UPLOAD);
}

// TODO: work out if this is a better way to manage the data string and send it
// Send data chunk by chunk
void sendData(Adafruit_CC3000_Client& client, String input, int chunkSize) {
  
  Serial.println(input);
  // Get String length
  int length = input.length();
  int max_iteration = (int)(length/chunkSize);
  int sent = 0;
  
  Serial.println(length);
  for (int i = 0; i <= max_iteration; i++) {
    Serial.print(i); Serial.print(F(":")); Serial.print(sent); Serial.print(F(":")); Serial.println(input.substring(i*chunkSize, (i+1)*chunkSize));
    sent += chunkSize;
    client.print(input.substring(i*chunkSize, (i+1)*chunkSize));
//    client.fastrprint(input.substring(i*chunkSize, (i+1)*chunkSize));
    wdt_reset();
  }
  //Serial.println(F("\nALL SENT!"));
}

// Wait for a given time using the watchdog
void wait(long total_delay) {
  Serial.print(F("Waiting"));// Serial.println(total_delay);
  int number_steps = (int)(total_delay/5000);
  //Serial.println(number_steps);
  wdt_enable(WDTO_8S);
  for (int i = 0; i < number_steps; i++){
    Serial.print(F("."));
    delay(5000);
    wdt_reset();
  }
  wdt_disable();
  Serial.println(F("done!"));
}

/**************************************************************************/
/*!
    @brief  Displays the driver mode (tiny of normal), and the buffer
            size if tiny mode is not being used

    @note   The buffer size and driver mode are defined in cc3000_common.h
*/
/**************************************************************************/
void displayDriverMode(void)
{
  #ifdef CC3000_TINY_DRIVER
    Serial.println(F("CC3000 is configure in 'Tiny' mode"));
  #else
    Serial.print(F("RX Buffer : "));
    Serial.print(CC3000_RX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
    Serial.print(F("TX Buffer : "));
    Serial.print(CC3000_TX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
  #endif
}

/**************************************************************************/
/*!
    @brief  Tries to read the CC3000's internal firmware patch ID
*/
/**************************************************************************/
uint16_t checkFirmwareVersion(void)
{
  uint8_t major, minor;
  uint16_t version;
  
#ifndef CC3000_TINY_DRIVER  
  if(!cc3000.getFirmwareVersion(&major, &minor))
  {
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
    version = 0;
  }
  else
  {
    Serial.print(F("Firmware V. : "));
    Serial.print(major); Serial.print(F(".")); Serial.println(minor);
    version = major; version <<= 8; version |= minor;
  }
#endif
  return version;
}

/**************************************************************************/
/*!
    @brief  Tries to read the 6-byte MAC address of the CC3000 module
*/
/**************************************************************************/
void displayMACAddress(void)
{
  uint8_t macAddress[6];
  
  if(!cc3000.getMacAddress(macAddress))
  {
    Serial.println(F("Unable to retrieve MAC Address!\r\n"));
  }
  else
  {
    Serial.print(F("MAC Address : "));
    cc3000.printHex((byte*)&macAddress, 6);
  }
}


/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

/**************************************************************************/
/*!
    @brief  Begins an SSID scan and prints out all the visible networks
*/
/**************************************************************************/

void listSSIDResults(void)
{
  uint32_t index;
  uint8_t valid, rssi, sec;
  char ssidname[33]; 

  if (!cc3000.startSSIDscan(&index)) {
    Serial.println(F("SSID scan failed!"));
    return;
  }

  Serial.print(F("Networks found: ")); Serial.println(index);
  Serial.println(F("================================================"));

  while (index) {
    index--;

    valid = cc3000.getNextSSID(&rssi, &sec, ssidname);
    
    Serial.print(F("SSID Name    : ")); Serial.print(ssidname);
    Serial.println();
    Serial.print(F("RSSI         : "));
    Serial.println(rssi);
    Serial.print(F("Security Mode: "));
    Serial.println(sec);
    Serial.println();
  }
  Serial.println(F("================================================"));

  cc3000.stopSSIDscan();
}
