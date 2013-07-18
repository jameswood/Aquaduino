//Includes
#include <SPI.h>
#include <Time.h> 
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <LiquidCrystal.h>

//Constants
const unsigned int timeZone = 10;
const unsigned int NTPTimeout = 5000;
const unsigned int earthtoolsTimeout = 5000;
unsigned int lcdPin = 3;
unsigned int ledPin = 5;
const unsigned int lcdNightBrightness = 10;
const unsigned int lcdDayBrightness = 255;
const unsigned int ledNightBrightness = 0;
const unsigned int ledDayBrightness = 255;
const String latitude = "-33.909508";
const String longitude = "151.22162";
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0xE4, 0x0B };
unsigned int localPort = 8888;
IPAddress timeServer(203, 0, 178, 191);
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
IPAddress myIP(10, 0, 36, 200); // fallback local IP (no DHCP)
const char serverName[] = "www.earthtools.org";
const int timeOffset = 0; //minutes
const int sunriseDelay = 2; //hours
const int sunsetDelay = 5; //hours

//Variables
int sunDate = 0;
int syncDate = 0;
int ledBrightness = ledNightBrightness;
int lcdBrightness = lcdNightBrightness;
time_t dawn;
time_t sunrise;
time_t sunset;
time_t twilight;
String dawnText;
String sunriseText;
String sunsetText;
String twilightText;
unsigned int lastAttemptTime = 0;
int sunStatus = 0; // 1: down. 2: rising. 3: up. 4: setting.
int prevSunStatus = 0;
int prevLcdBrightness = -1;
int prevLedBrightness = -1;
long timeNowOffset = 0;
int secondsDisplayed = 0;

//Characters
byte lamp[8] = {
  B01110,
  B10001,
  B10001,
  B10001,
  B01110,
  B01110,
  B00100,
};

byte up[8] = {
  B00100,
  B10101,
  B01110,
  B11111,
  B01110,
  B10101,
  B00100,
};

byte down[8] = {
  B01100,
  B00110,
  B00011,
  B00011,
  B00011,
  B00110,
  B01100,
};

byte rising[8] = {
  B00000,
  B00111,
  B00011,
  B00101,
  B01000,
  B10000,
  B00000,
};

byte setting[8] = {
  B00000,
  B10000,
  B01000,
  B00101,
  B00011,
  B00111,
  B00000,
};

byte logo[8] = {
  B00000,
  B00000,
  B01000,
  B11010,
  B11010,
  B00010,
  B00000,
};

//Initialisation
LiquidCrystal lcd(8, 9, 4, 2, 6, 7);
EthernetClient earthtools;
EthernetUDP Udp;

void setup(){
  //Initialise Hardware
  Serial.begin(9600);
  Serial.println("_______________________________");
  Serial.println("Start serial (setup())");
  
  lcd.begin(16, 2);
  pinMode (ledPin, OUTPUT);
  pinMode (lcdPin, OUTPUT);
  analogWrite(ledPin, ledNightBrightness);
  analogWrite(lcdPin, lcdNightBrightness);
  lcd.createChar(0, lamp);
  lcd.createChar(1, down);
  lcd.createChar(2, rising);
  lcd.createChar(3, up);
  lcd.createChar(4, setting);
  lcd.createChar(5, logo);
  lcd.setCursor(8,0);
  lcd.write(byte(5));
  lcd.setCursor(0,1);
  lcd.print("DHCP... ");
  Serial.println("Attempting to get an IP address using DHCP:");
  if (!Ethernet.begin(mac)) {
    Serial.println("failed to get an IP address using DHCP, trying manually");
    lcd.print("Manual");
    Ethernet.begin(mac, myIP);
  } else {
    Serial.println("DHCP Success");
    lcd.print("ok");
    delay(1000);
  }
  Serial.print("IP Address: ");
  lcd.setCursor(0,1);
  lcd.print("IP: ");
  Serial.println(Ethernet.localIP());
  lcd.print(Ethernet.localIP());
  delay(1000);
  Udp.begin(localPort);
  Serial.print("Starting NTP sync... ");
  setSyncProvider(getNtpTime);
  setSyncInterval(43200);
  Serial.println("End setup()");
  lcd.clear();
}

void loop(){
  if(timeStatus() != timeNotSet ){ //wait until you know what time it is.
    timeNowOffset = now() + (timeOffset * 60);
    if(sunDate == day()) {
      calculateBrightness();
      updateLED();
      updateLCD();
    } else {
      getSuntimes();
    }
  } else {
    lcd.clear();
    lcd.print("no clock sync...");
    getNtpTime(); //get the time if you don't know what time it is.
    delay(500);
  }
}

void getSuntimes() {
  connectToEarthtools();
  while (!earthtools.available()) {
    Serial.println("Waiting for Earthtools...");
    delay(500); 
  }
  Serial.println("Looking for Sun Times...");
  if(earthtools.find("<sunrise>")) {
    sunriseText = "";
    while (!sunriseText.endsWith("<")) {
      char inChar = earthtools.read();
      sunriseText += inChar;
    }
    sunriseText.replace("<", "");
    sunrise = formatTime(sunriseText, sunriseDelay);
    Serial.println("Sunrise: " + sunriseText);
  }

  if(earthtools.find("<astronomical>")) {
    dawnText = "";
    while (!dawnText.endsWith("<")) {
      char inChar = earthtools.read();
      dawnText += inChar;
    }
    dawnText.replace("<", "");
    dawn = formatTime(dawnText, sunriseDelay);
    Serial.println("Dawn: " + dawnText);
  }

  if(earthtools.find("<sunset>")) {
    sunsetText = "";
    while (!sunsetText.endsWith("<")) {
      char inChar = earthtools.read();
      sunsetText += inChar;
    }
    sunsetText.replace("<", "");
    sunset = formatTime(sunsetText, sunsetDelay);
    Serial.println("Sunset: " + sunsetText);
  }

  if(earthtools.find("<astronomical>")) {
    twilightText = "";
    while (!twilightText.endsWith("<")) {
      char inChar = earthtools.read();
      twilightText += inChar;
    }
    twilightText.replace("<", "");
    twilight = formatTime(twilightText, sunsetDelay);
    Serial.println("Twilight: " + twilightText);
  }

  Serial.println("Sun times OK at " + String(hour()) + ":" + String(minute()) + ":" + String(second())); 
  earthtools.flush();
  earthtools.stop();
  sunDate = day();
}

time_t formatTime(String timeIn, int offset){
  time_t timeout;
  TimeElements tempTime;
  String event_hour = timeIn.substring( 0 , timeIn.indexOf( ":" ) );
  String event_minute = timeIn.substring( timeIn.indexOf(":")+1, timeIn.indexOf( ":", timeIn.indexOf(":")+1 ) );
  String event_second = timeIn.substring( timeIn.indexOf(":", timeIn.indexOf(":")+1)+1);
  tempTime.Second = event_second.toInt();
  tempTime.Minute = event_minute.toInt();
  tempTime.Hour = event_hour.toInt() + offset;
  tempTime.Day = day();
  tempTime.Month = month();
  tempTime.Year = year();
  tempTime.Year = tempTime.Year - 1970;
  timeout = makeTime(tempTime);
  return(timeout);
}

void connectToEarthtools() {
  Serial.print("Connecting to Earthtools... ");
  if (earthtools.connect(serverName, 80)) {
    earthtools.println("GET /sun-1.0/" + latitude + "/" + longitude + "/" + String(day()) + "/" + String(month()) + "/" + timeZone + "/0 HTTP/1.1");
    earthtools.println("HOST: " + String(serverName));
    earthtools.println("Connection: close");
    earthtools.println();
    Serial.println("Connected to Earthtools");
  }
}

void calculateBrightness() {  
  if (timeNowOffset >= dawn && timeNowOffset < sunrise) { // Sun is Rising
    ledBrightness = map(timeNowOffset, dawn, sunrise, ledNightBrightness, ledDayBrightness);
    lcdBrightness = map(timeNowOffset, dawn, sunrise, lcdNightBrightness, lcdDayBrightness);
    sunStatus = 2;
  }

  else if (timeNowOffset >= sunrise && timeNowOffset < sunset) { // Sun is Up
    ledBrightness = ledDayBrightness;
    lcdBrightness = lcdDayBrightness;
    sunStatus = 3;
  }

  else if (timeNowOffset >= sunset  && timeNowOffset < twilight) { // Sun is Setting
    ledBrightness = map(timeNowOffset, sunset, twilight, ledDayBrightness, ledNightBrightness);
    lcdBrightness = map(timeNowOffset, sunset, twilight, lcdDayBrightness, lcdNightBrightness);
    sunStatus = 4;
  }

  else { // Sun is Down
    ledBrightness = ledNightBrightness;
    lcdBrightness = lcdNightBrightness;
    sunStatus = 1;
  }
}

void updateLED(){
  analogWrite(ledPin, ledBrightness);
}

void updateLCD(){
  analogWrite(lcdPin, lcdBrightness);
  if (prevSunStatus != sunStatus) {
    Serial.println("Sun is " + String(sunStatus) + " (1: down. 2: rising. 3: up. 4: setting)");
    lcd.setCursor(12,0);
    lcd.write(byte(0)); //lamp icon
    lcd.setCursor(10,0);
    lcd.write(byte(sunStatus)); //Sun state icon
    prevSunStatus = sunStatus;
  }
  if (prevLedBrightness != ledBrightness) {
    lcd.setCursor(13,0);
    if (ledBrightness < 10) { lcd.print("0"); }
    if (ledBrightness < 100) { lcd.print("0"); }
    lcd.print(ledBrightness);
    prevLedBrightness = ledBrightness;
  }
  if (secondsDisplayed != second()) {
    lcd.setCursor(0, 0);
    lcd.print(hour());
    printDigits(minute());
    printDigits(second());
    if(hour()<10) { lcd.print(" "); }
    secondsDisplayed = second();
    switch (sunStatus) { //1: down. 2: rising. 3: up. 4: setting.
      case 1: //down
        lcd.setCursor(0,1); lcd.write(byte(4)); //setting icon
        lcd.setCursor(2,1); lcd.print(String(hour(twilight)) + ":" + String(minute(twilight)) + " ");
        lcd.setCursor(9,1); lcd.write(byte(2)); //rising icon
        lcd.setCursor(11,1); lcd.print(String(hour(dawn)) + ":" + String(minute(dawn)) + " ");
        break;

      case 2: //rising
        lcd.setCursor(0,1); lcd.write(byte(1)); //down icon
        lcd.setCursor(2,1); lcd.print(String(hour(dawn)) + ":" + String(minute(dawn)) + " ");
        lcd.setCursor(9,1); lcd.write(byte(3)); //up icon
        lcd.setCursor(11,1); lcd.print(String(hour(sunrise)) + ":" + String(minute(sunrise)) + " ");
        break;

      case 3: //up
        lcd.setCursor(0,1);  lcd.write(byte(2)); //rising icon
        lcd.setCursor(2,1);  lcd.print(String(hour(sunrise)) + ":" + String(minute(sunrise)) + " ");
        lcd.setCursor(9,1);  lcd.write(byte(4)); //setting icon
        lcd.setCursor(11,1); lcd.print(String(hour(sunset)) + ":" + String(minute(sunset)) + " ");
        break;

      case 4: //setting
        lcd.setCursor(0,1); lcd.write(byte(3)); //day icon
        lcd.setCursor(2,1); lcd.print(String(hour(sunset)) + ":" + String(minute(sunset)) + " ");
        lcd.setCursor(9,1); lcd.write(byte(1)); //night icon
        lcd.setCursor(11,1); lcd.print(String(hour(twilight)) + ":" + String(minute(twilight)) + " ");
        break;
    }
  }
}

void printDigits(int digits){
  lcd.print(":");
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}


time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print("Transmitting NTP Request... ");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("NTP synced");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("NTP sync failed.");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
