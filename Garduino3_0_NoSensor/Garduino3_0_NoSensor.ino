
/* 
  Garduino Project
  Editor: Carlos A. Lopez
  Created: 12/03/2014
  Modified: 05/2/2016

  Objective: The intention of this program is to maintain the life of a indoor plant

  Inputs:
    - Soil Mosture Sensor (Future Sensor)
    - Temperature Sensor (Future Update)
    - PH Sensor (Future Update)
    - Water Tank Level Sensor (Float Type Sensor)
    - Manual/Auto Mode (Push Button)
    
  Output:
    - Water Tank Relay
    - Grow Light Relay
    - Status LED: Program Running. (Blinking)
    - Status LED: Watering Plant.  (ON)
   
  Ethernet Shield:
  The user will interact with the system using a computer or smart phone device. 
    -Send Sensor Data (Mostly for Monitoring)
    -How many times has the robot water the plants in a 24 hr period. 
    -IP Address: 192.168.1.177
    -MAC Address:  DE:AD:BE:EF:FE:ED
    
  SD Card:
  The Arduino Ethernet Shield has an SD card slot so we can log some important data. 
   
  Watering Profile:
  Plant Name: Dumb Cane
  Moisture Level: 1 (Lowest)
  Check Frequency: Once a week (We will start with 1 once a day)
  Special Needs: Soil should remain dry 4-5 days.
   
*/ 
#include <Time.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>


//Set Tags For Inputs
//#define AO_Input_Humidity A0     // Plant Soil Sensor
//#define A1_Input_Temperature A1  // Plant Environment Temperature
//#define A2_Input_PH A2           // Plant PH
//#define A3_InputTank_Level A3    // Tank Water Level
//#define A4_Input A4              // Empty
//#define A5_Input A5              // Empty

//Set Tags For Outputs
//#define D0 0                     // USED BY SERIAL COMMUNICATION            
//#define D1 1                     // USED BY SERIAL COMMUNICATION
#define D2_Output_Program_OK 2     // Blink for pogram state
#define D3_Output_Light_Pump 3     // Water Pump On Signal
//#define D4 4                     // USED BY SD CARD 
#define D5_Output_Light_Relay 5    // LED Light Turn On Relay
#define D6_Output_Pump_Relay 6     // Water Pump Turn On Relay    
#define D7_Input_Manual_Mode 7     // Turn on Water Pump Manually
//#define D8 8                     // Empty
//#define D9 9                     // Empty
//#define D10 10                   // USED BY ETHERNET SHIELD
//#define D11 11                   // USED BY ETHERNET SHIELD
//#define D12 12                   // USED BY ETHERNET SHIELD
//#define D13 13                   // USED BY ETHERNET SHIELD

//Time
const int timeZone = 7;    //Pacific Standard Time (USA)

//Comunication Settings
byte mac[] = {0xDE,0xAD,0xBE,0xFE,0xED };  //MAC address
IPAddress ip(192,168,1,177);           //Ethernet Shield IP Address 192.168.1.177

unsigned int localPort = 8888;         //Local port to listen for UDP packets
IPAddress timeServer(132,163,4,101);   //Time Server IP Address 132.163.4.101
const int NTP_PACKET_SIZE = 48;        // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE ];  //buffer to hold incoming and outgoing packets
EthernetServer server(80);             //Initialize the Ethernet server library (port: 80)
EthernetUDP Udp;                       //A UDP instance to let us send and receive packets over UDP
  
//Internal Variables 
int LED_WATERING_STATE = LOW;
int LED_DRY_STATE = LOW;
int RELAY_PUMP_STATE = HIGH;
int RELAY_LIGHT_STATE = HIGH;
/////////////////////////////////////////////////////////////////////////////////////////////////////

time_t time_set_check_to_water;
time_t time_diff;                               //Temp Variable to Keep Difference between to timestamps
time_t pump_start_time;                         //Store Pump Start Time
const unsigned long max_watering_time = 30;     //30 seconds
const int day_freq_to_water_plant = 1;           //Every two days water plant
int water_counter = 0;

unsigned long time_light_hour_start = 18;       //Time to start Light;
unsigned long time_light_hour_stop = 20;        //Time to stop Light;

time_t prevDisplay = 0; // when the digital clock was displayed

/////////////////////////////////////////////////////////////////////////////////////////////////////

//State Machine Variables
int current_state = 0;      //Set to Startup State
int prev_state = -1;         //Set Pev State

void setup() 
{
  //Initialize Serial Communications (Mostly for Toubleshooting)
  Serial.begin(9600);
  
  //Start Ethernet
  Ethernet.begin(mac, ip);
  
  //Start UDP Service
  Udp.begin(localPort);
  
  //Set Time with NTP Server.
  setSyncProvider(get_NTP_TIME);  
  
  //Use this if ethernet is not connected
  //setTime(18,31,0,27,1,2016);
  
  //Initialize Outputs 
  pinMode(D3_Output_Light_Pump, OUTPUT);  
  pinMode(D2_Output_Program_OK, OUTPUT);
  pinMode(D5_Output_Light_Relay, OUTPUT);
  pinMode(D6_Output_Pump_Relay, OUTPUT);

  digitalWrite(D5_Output_Light_Relay, RELAY_LIGHT_STATE);  //Reset Output
  digitalWrite(D6_Output_Pump_Relay,RELAY_PUMP_STATE);     //Reset Output
  
  //Initialize Inputs
  pinMode(D7_Input_Manual_Mode, INPUT);
  
  update_clock();  
  Serial.println("Start System");
}

void loop() 
{
  switch (current_state) 
  {
    case 0:  //Setup Case
             if(prev_state != current_state) 
             {
               update_clock();
               Serial.println("CASE 0: Setup Case Started");
             }
             time_set_check_to_water = now();    //Set Last Water Time
             time_diff = 0;                      //Temp Variable to Keep Difference between to timestamps
             pump_start_time = 0;                //Store Pump Start Time
             current_state = 1;                  //Change to Idle Case
             //update_clock();
             //Serial.println("CASE 0: Setup Case Finished");
             prev_state = 0;
             break;

    case 1: //Idle Case
            if(prev_state != current_state) 
            {
              update_clock();
              Serial.println("CASE 1: Idle Case Started");            
            }
            blink_out(D2_Output_Program_OK, 500);
            //***CHECK IF PLANT IS READY FOR WATER***//
            time_diff = now() - time_set_check_to_water;  //Check Difference between Last Watering Time and current time
            
            if(day(time_diff)  > day_freq_to_water_plant)    //Use Day To Check how frequent to water plant
            {
              update_clock();
              Serial.println("CASE 1 >> Time to water plant");
              pump_start_time = now();  //Store Pump Turning On Time
              current_state = 2;        //Change to Turn Pump On State
              water_counter++;
              break;
            }
            
            //***CHECK IF PLANT IS READY FOR LIGHT***//
            else if(((hour(now()) >= time_light_hour_start) && (hour(now()) <= time_light_hour_stop)) && RELAY_LIGHT_STATE == HIGH)
            {
              //if(RELAY_LIGHT_STATE != LOW)
              //{
                update_clock();
                Serial.print("CASE 1 >> Time to turn on Lights! ->");
                Serial.println(hour(now()));
              //}
              current_state = 3;  //Change to Turn Light On State
            }
            
            else if(((hour(now()) < time_light_hour_start) || (hour(now()) > time_light_hour_stop)) && RELAY_LIGHT_STATE == LOW)
            {
              //if(RELAY_LIGHT_STATE != HIGH)
              //{
                update_clock();
                Serial.print("CASE 1 >> Time to turn off Lights!");
                Serial.println(hour(now()));
              //}
              current_state = 4;  //Change to Turn Light Off State
            }
            
            //update_clock();            
            //Serial.println("CASE 1: Idle Case Finished");
            prev_state = 1;
            break;
    
    case 2:  //Water Pump On State
            if(prev_state != current_state)
            {
              update_clock();
              Serial.println("CASE 2: Water Pump On Case Started");
            }
            
            time_diff = now() - pump_start_time;
            if (second(time_diff) > max_watering_time)
            {
              update_clock();
              Serial.println("CASE 2 >> Stop Water Pump!");
              RELAY_PUMP_STATE = HIGH;          //Reset Pump 
              LED_WATERING_STATE = LOW;         //Reset Watering LED Signal
              time_set_check_to_water = now();  //Set Last Water Time
              current_state = 1;                //Return to Idle Case
              break;
            }
      
            RELAY_PUMP_STATE = LOW;     //Set Pump
            blink_out(D3_Output_Light_Pump, 500);
            
            //update_clock();            
            //Serial.println("CASE 2: Water Pump On Case Finished");
            prev_state = 2;
            break;            
            
     case 3:  //Turn Light ON State
            if(prev_state != current_state)
            {
              update_clock();
              Serial.println("CASE 3: Light ON State Started");
            }
            RELAY_LIGHT_STATE = LOW;  //Set Light
            current_state = 1;        //Return to Idle Case
            //update_clock();
            //Serial.println("CASE 3: Light ON State Finished");
            prev_state = 3;
            break;
            
     case 4:  //Turn Light OFF State
            update_clock();
            Serial.println("CASE 4: Light OFF State Started");
            RELAY_LIGHT_STATE = HIGH;  //Set Light
            current_state = 1;         //Return to Idle Case
            //update_clock();
            //Serial.println("CASE 4: Light OFF State Finished");
            prev_state = 4;
            break;
     default:
            break;
  }
 
  //SetOut
  Output();
   
  send_NTP_TIME();
}

void blink_out(int pin, int blink_time)
{
  digitalWrite(pin, HIGH);
  delay(blink_time);
  digitalWrite(pin, LOW);
  delay(blink_time);
}

void update_clock() 
{
  // digital clock display of the time
  Serial.print("[");
  Serial.print(month());
  Serial.print("/");
  Serial.print(day());
  Serial.print("-");
  Serial.print(hour());
  Serial.print(":");
  printDigits(minute());
  Serial.print(":");
  printDigits(second());
  Serial.print("] - ");
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void send_NTP_TIME()
{
  //Listen to incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    //an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        //If you've gotten to the end o fthe line (received a newline
        //character) and the line is blank, the http request has ended, 
        //so you can send a reply
        if (c == '\n' && currentLineIsBlank || c == 'G') {
          //Send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
	  //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<b> Welcome to Foli Monitoring System </b><br/>");
          client.print("<b>Humidity Sensor Level: ");
          //client.print(Humidity_Current);
          client.println("</b><br/>");
          client.print("<b>Last Time Foli was water: ");
          client.print(month());
          client.print("/");
          client.print(day());
          client.print("/");
          client.print(year());
          client.print(" ");
          client.print(hour());
          client.print(":");
          client.print(minute());
          client.print(":");
          client.print(second());
          client.print("</b>");
          client.println("<br />");
          client.print("<b>Time Set To Check Water: ");
          client.print(time_set_check_to_water);
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          //you're startin a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          //you've gotten a character on the current line
          currentLineIsBlank = false;
        }
        // give the web browser time to receive the data
        delay(1);
        //close the connection;
        client.stop();
        Serial.println("client disconnected");
      }
    }
  }
}

time_t get_NTP_TIME()
{
//  if(current_state != 0)
//  {
//    update_clock();        
//  }
  sendNTPpacket(timeServer); // send an NTP packet to a time server

  // wait to see if a reply is available
  delay(1000);  
  if ( Udp.parsePacket() ) {  
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  
    //Serial.print("Seconds since Jan 1 1900 = " );
    //Serial.println(secsSince1900);               

    // now convert NTP time into everyday time:
    //update_clock();
    Serial.print("Update Clock... New Unix Time: ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL + timeZone * SECS_PER_HOUR;     
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;  
    // print Unix time:
    Serial.println(epoch);                               
    
    //Sync Time
    return(epoch);

//    // print the hour, minute and second:
//    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
//    Serial.print(':');
//    Serial.print(':');  
//    if ( ((epoch % 3600) / 60) < 10 ) {
//       // In the first 10 minutes of each hour, we'll want a leading '0'
//      Serial.print('0');
//    }
//    Serial.print(':'); 
//    if ( (epoch % 60) < 10 ) {
//      // In the first 10 seconds of each minute, we'll want a leading '0'
//      Serial.print('0');
//    }
  }
  return 0;  
}

// send an NTP request to the time server at the given address 
unsigned long sendNTPpacket(IPAddress& address)
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
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}

void Output()
{
  digitalWrite(D3_Output_Light_Pump,LED_WATERING_STATE);
  digitalWrite(D2_Output_Program_OK,LED_DRY_STATE);
  digitalWrite(D5_Output_Light_Relay, RELAY_LIGHT_STATE);        
  digitalWrite(D6_Output_Pump_Relay,RELAY_PUMP_STATE);
}
