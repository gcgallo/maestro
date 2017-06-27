/* Cut a CAN bus in 2 and insert a device to filter the messages.
 *
 * This app uses an Electron with 2 CAN controllers to reads CAN
 * messages from one side of the bus to forward them to the other side
 * of the bus. When certain CAN message IDs match, it modifies or drops
 * the message
 */
//#include <Adafruit_SSD1306.h>
/* Transmit the value of some knobs connected to the Carloop as CAN messages
 */

#include "application.h"
#include "clickButton.h"
#include "Serial4/Serial4.h"
//#include "carloop.h"
//#include "socketcan_serial.h"

SYSTEM_THREAD(ENABLED);

#define LIST_LEN 700

struct dataInfo {
  bool enable = false;
  bool modify = false;
  uint8_t current = 0x00;
  uint8_t previous = 0x00;
  uint8_t modified = 0x00;
};

struct IDinfo {
  bool enable = false;
  bool modify = false;
  int id;
  int frequency; 
  dataInfo data[8];
};

IDinfo CAN1_IDlist[LIST_LEN];
IDinfo CAN2_IDlist[LIST_LEN];


bool filterCAN(CANMessage &m);
CANMessage modifyData(int i, IDinfo (&IDlist)[LIST_LEN], int SER);
void transmitCAN(CANChannel can, IDinfo (&IDlist)[LIST_LEN], int IDcount, int SER);
int getMode(int BUS);
bool appendToList(CANMessage &m, IDinfo (&IDlist)[LIST_LEN], int IDcount, int BUS);
void sortIDs(IDinfo (&IDlist)[LIST_LEN], int IDcount);
void socketcanReceiveMessages(CANChannel can, int SER);
void printReceivedMessage(const CANMessage &message, int SER);
void checkButtons(IDinfo (&IDlist)[LIST_LEN], int IDcount);
void doEncoderA();
void doEncoderB();
void enableAll(IDinfo (&IDlist)[LIST_LEN], int IDcount);
void listIDs(int i, IDinfo (&IDlist)[LIST_LEN], int IDcount);
void refresh(IDinfo (&IDlist)[LIST_LEN], int IDcount);
void displayMenu(IDinfo (&IDlist)[LIST_LEN], int IDcount);
void recvWithStartEndMarkers();
//void serialSwitch(Stream &serial);
//void showNewData();


int encoderA = D4;
int encoderB = D5;

volatile bool A_set = false;
volatile bool B_set = false;
volatile int encoderPos = 0;
int prevPos = 0;
int updown = 0;

int selectButtonPin = 3;
ClickButton selectButton(selectButtonPin, LOW, CLICKBTN_PULLUP);
int selection = 0;
int led = D7;

#define RST "\033[0m"
#define SEL "\033[1;93;41m"
#define CHANGING "\033[1;93;31m"
#define INVERT "\033[7m"
#define BLINK "\033[5m"

#define REFESH_RATE 500
#define SCREEN_BUFFER 26

#define BUS_1 1
#define BUS_2 2

#define SER_1 1
#define SER_2 2

static int sel = 0;
static int data_select = 0;

long timeOfLastRefresh = 0;

// The 2 CAN controllers on the Electron
CANChannel can1(CAN_C4_C5);
CANChannel can2(CAN_D1_D2);

const char NEW_LINE = '\r';
char inputBuffer[40];
unsigned inputPos = 0;

CANMessage spoofM;

int can1mode = 2;
int can2mode = 1;

// Modify if your CAN bus has a different speed / baud rate
const auto speed = 500000; // bits/s
PMIC _pmic;

bool enableAllState = true;

int CAN1_IDcount = 0;
int CAN2_IDcount = 0;

const size_t READ_BUF_SIZE = 64;
size_t readBufOffset = 0;

/* every
 * Helper than runs a function at a regular millisecond interval
 */
template <typename Fn>
void every(unsigned long intervalMillis, Fn fn) {
  static unsigned long last = 0;
  if (millis() - last > intervalMillis) {
    last = millis();
    fn();
  }
}

void setup() {

  // Connect to CAN
  can1.begin(speed);
  //carloop.begin();    // for recieving in socketcan 
  can2.begin(speed);  // for transmitting from deciever

  // Setup serial connections 

  Serial.begin(9600);      //"Serial" is used only for sniffing via socketcan 
  USBSerial1.begin(9600);  //"USBSerial1" is used only to display filtering interface, except when both are in sniffer mode
  Serial4.begin(9600);     //"Serial4" is used for mode management from RasPi python script

  while (!USBSerial1) delay(100);

  _pmic.disableCharging();

  // Setup encoder knobs
  pinMode(encoderA, INPUT_PULLUP);
  pinMode(encoderB, INPUT_PULLUP);
  attachInterrupt(encoderA, doEncoderA, CHANGE);
  attachInterrupt(encoderB, doEncoderB, CHANGE);

  // Setup buttons
  pinMode(D3, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  selectButton.debounceTime   = 30;   // Debounce timer in ms
  selectButton.multiclickTime = 250;  // Time limit for multi clicks
  selectButton.longClickTime  = 1000; // time until "held-down clicks" register

  // Refresh menu interface
  refresh(CAN1_IDlist, CAN1_IDcount);

}

void loop() {
  
  CANMessage m;
  CANMessage m2;

  //Serial4.print(0);
  //Serial4.print(1);

  selectButton.Update();

  //serialSwitch(USBSerial1);

  recvWithStartEndMarkers();
  //showNewData();
  
  can1mode = getMode(BUS_1);
  //USBSerial1.println(can1mode);


  switch (can1mode){

      case 0 : // OFF
          USBSerial1.print("\033[2J"); // clear screen
        //USBSerial1.print("\033[H"); // cursor to home
          USBSerial1.printf("OFF\r");

          break;

      case 1 : // cansniffer mode

          /*if( can2mode == 2 )
              socketcanReceiveMessages(can1, SER_2);
          else*/
              socketcanReceiveMessages(can1, SER_1);

          break;

      case 2 : // input side of filter mode
          if (can1.receive(m)){

              appendToList(m, CAN1_IDlist, CAN1_IDcount, BUS_1);
              //sortIDs(CAN1_IDlist);

          }

          checkButtons(CAN1_IDlist, CAN1_IDcount);

          if( (millis() - timeOfLastRefresh) > REFESH_RATE){
              refresh(CAN1_IDlist, CAN1_IDcount);
          }

          break;

      case 3 : // output side of filter mode

          socketcanReceiveMessages(can1, SER_2);
          transmitCAN(can1, CAN2_IDlist, CAN2_IDcount, SER_2);

          break;

      default :
          USBSerial1.printf("BAD\r");
          //USBSerial1.print(can1mode);

  }

  //can2mode = getMode(BUS_2);
  //can2mode = 1;

  switch (can2mode){

      case 0 : // OFF

          Serial.println("OFF\r");

          break;

      case 1 : // cansniffer mode

          socketcanReceiveMessages(can2, SER_2);

          break;

      case 2 : // input side of filter mode

          if (can2.receive(m)){

              appendToList(m, CAN2_IDlist, CAN2_IDcount, BUS_2);
              //sortIDs(CAN2_IDlist);
          }

          checkButtons(CAN2_IDlist, CAN2_IDcount);

          if( (millis() - timeOfLastRefresh) > REFESH_RATE){
              refresh(CAN2_IDlist, CAN2_IDcount);
          }

          break;

      case 3 : // output side of filter mode

          socketcanReceiveMessages(can2, SER_2);
          transmitCAN(can2, CAN1_IDlist, CAN1_IDcount, SER_2);

          break;

      default :
          Serial.println("BAD MODE");

  }
    
}

// Modify or drop the messages intercepted by the man-in-the-middle device
/*bool filterCAN(CANMessage &m, IDinfo IDlist) {
  // return false to drop the message
  for( int i = 0 ; i < IDcount ; i++){
    if( m.id == IDlist[i].id ){
      return IDlist[i].enable;
    }
  }
  return false;
}*/

CANMessage modifyData(int i, IDinfo (&IDlist)[LIST_LEN], int SER){
  CANMessage m;
  m.id = IDlist[i].id;
  m.len = 8;
    for( int j = 0 ; j < 8 ; j++){
      if( IDlist[i].data[j].enable ){
        m.data[j] = IDlist[i].data[j].modified;
      }else{
        m.data[j] = IDlist[i].data[j].current;
      }
    }
    printReceivedMessage(m, SER);
    return m;
}


/*

void spoofData(){
  // write to the data array to modify the message
    spoofM.id =  0x456;
    spoofM.len = 8;
    spoofM.data[0] = 0x80;
    spoofM.data[1] = 0x80;
    spoofM.data[2] = 0x80;
    spoofM.data[3] = 0x80;
    spoofM.data[4] = 0xFF;
    spoofM.data[5] = 0x80;
    spoofM.data[6] = 0x80;
    spoofM.data[7] = 0x80;
    //if(spoofM.id >= 0x7FF){
      //spoofM.id = 0x000;
    //}
}
*/

/*void serialSwitch(Stream &serial){

  serial.print("serial switched");

}*/

void transmitCAN(CANChannel can, IDinfo (&IDlist)[LIST_LEN], int IDcount, int SER) {
  //every(10, [] {

    for( int i = 0 ; i < IDcount ; i++){
      if( IDlist[i].enable ){
          can.transmit(modifyData(i, IDlist, SER));
      }

  //});
  }
}

const byte numChars = 90;
char receivedChars[numChars];

boolean newData = false;

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial4.available() > 0 && newData == false) { // <<== NEW - get all bytes from buffer
    rc = Serial4.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

/*void showNewData() {
  if (newData == true) {
    USBSerial1.print("This just in ... ");
    USBSerial1.println(receivedChars);
    newData = false;
  }
}*/

int getMode( int BUS ){

  uint8_t check;
  uint8_t mode;

  check = receivedChars[0];
  mode = receivedChars[1];
  //USBSerial1.print(check);
  //USBSerial1.println(mode);

  /*if(check == 49 && BUS == 2){  // received 01, but looking for 02
    return can2mode;            // return current mode

  }else if(check == 50 && BUS == 1){ // received 02, but looking for 01
    return can1mode;                 // return current mode
  }*/

  if(mode == 79) // ASCII for O 
      return 0;
  else if(mode == 83) //ASCII for S
      return 1;
  else if(mode == 73) //ASCII for I
      return 2;
  else if(mode == 70) //ASCII for F
      return 3;
  else if(mode == 82) //ASCII for R
      return 1;
  else if(mode == 80) //ASCII for P
      return 1;
  /*else
      if(BUS == 1)
          return can1mode;
      else if (BUS == 2)
          return can2mode;*/

}


bool appendToList(CANMessage &m, IDinfo (&IDlist)[LIST_LEN], int IDcount, int BUS){
    
    int match = 0;
    int trip = 1;

    if( IDcount > LIST_LEN){
      return false;
    }

    for ( int i = 0 ; i < IDcount ; i++){
      if(m.id == IDlist[i].id){
          trip--;
          match = i;
      }
    }
    if (trip == 1){
          IDcount++;
          match = IDcount-1;
          IDlist[match].id = m.id;
    }
    IDlist[match].frequency++;
    for( int j = 0 ; j < 8 ; j++){
        IDlist[match].data[j].current = m.data[j];
    }
    sortIDs(IDlist, IDcount);

    if ( BUS == 1 ){
      //CAN1_IDlist = IDlist;
      CAN1_IDcount = IDcount;
    }else{
      //CAN2_IDlist = IDlist;
      CAN2_IDcount = IDcount;
    }

    return true;

}

void sortIDs(IDinfo (&IDlist)[LIST_LEN], int IDcount){
  //Sort by priority
  struct IDinfo IDswap;
  int i, j;
  for ( i = 0 ; i < IDcount - 1 ; i++){
      for ( j = 0 ; j < IDcount - i - 1 ; j++){
        if(IDlist[j].id > IDlist[j+1].id){
            IDswap = IDlist[j];
            IDlist[j] = IDlist[j+1];
            IDlist[j+1] = IDswap;
        }
      }

  }
  //Sort by frequency
  /*for ( i = 0 ; i < IDcount - 1 ; i++){
      for ( j = 0 ; j < IDcount - i - 1 ; j++){
        if(IDlist[j].frequency < IDlist[j+1].frequency){
            IDswap = IDlist[j];
            IDlist[j] = IDlist[j+1];
            IDlist[j+1] = IDswap;
        }
      }
  }*/
}


// from socketcan_serial.cpp

void socketcanReceiveMessages(CANChannel can, int SER){
  CANMessage message;
  while(can.receive(message))
  {
    printReceivedMessage(message, SER);
    //applicationCanReceiver(message);
  }
}

void printReceivedMessage(const CANMessage &message, int SER){

    if( SER == 1){
        USBSerial1.printf("t%03x%d", message.id, message.len);
        for(auto i = 0; i < message.len; i++) {
          USBSerial1.printf("%02x", message.data[i]);
        }
        USBSerial1.write(NEW_LINE);

    }

    if( SER == 2){
        Serial.printf("t%03x%d", message.id, message.len);
        for(auto i = 0; i < message.len; i++) {
          Serial.printf("%02x", message.data[i]);
        }
        Serial.write(NEW_LINE);

    }
  
}


// Encoder logic (should probably move a header, especially if more than one)
void doEncoderA(){
    if( digitalRead(encoderA) != A_set ){
        A_set = !A_set;
        // adjust counter + if A leads B
        if ( A_set && !B_set ) 
          encoderPos += 1;
    }
}

// Interrupt on B changing state, same as A above
void doEncoderB(){
    if( digitalRead(encoderB) != B_set ) {
        B_set = !B_set;
        //  adjust counter - 1 if B leads A
        if( B_set && !A_set ) 
          encoderPos -= 1;
    }
}

void checkButtons(IDinfo (&IDlist)[LIST_LEN], int IDcount){

  //selectButton.Update();

  if(selectButton.clicks == 1){
      selection = 1;
      displayMenu(IDlist, IDcount);
  }

  if( prevPos != encoderPos ) {
      if( prevPos > encoderPos ){
          updown = 1;
      }else{
          updown = 0;
      }
      prevPos = encoderPos;
      displayMenu(IDlist, IDcount);
  }

}


bool scroll_data = false;
bool scroll_list = true;


void enableAll(IDinfo (&IDlist)[LIST_LEN], int IDcount){

  for( int i = 0 ; i < IDcount ; i++){
      if( enableAllState ){
        IDlist[i].enable = IDlist[i].enable ? IDlist[i].enable : !IDlist[i].enable;
      }else{
        IDlist[i].enable = IDlist[i].enable ? !IDlist[i].enable : IDlist[i].enable;
      }
  }
  enableAllState = !enableAllState;

}

void listIDs(int i, IDinfo (&IDlist)[LIST_LEN], int IDcount){

    if( sel == i+1 ){
        USBSerial1.print(SEL);    //highlight selected line
        if( data_select == 0 ){
            USBSerial1.print(INVERT); //highlight selected ID
        }
    }
    USBSerial1.print(IDlist[i].enable ? "   [X]   " : "   [ ]   ");
    USBSerial1.printf("      %03x    ", IDlist[i].id);
    USBSerial1.print(RST);

    if(!IDlist[i].enable){ 
        for( int j = 0 ; j < 8 ; j++){
            USBSerial1.print(RST);
            USBSerial1.printf("%02x ", IDlist[i].data[j].previous);
        }
    }else{
        for( int j = 0 ; j < 8 ; j++){
            if(!IDlist[i].data[j].enable){
              if( sel == i+1 && data_select == j+1 ){
                  USBSerial1.print(INVERT); //highlight selected byte
              }
              if(IDlist[i].data[j].current != IDlist[i].data[j].previous){
                  USBSerial1.print(CHANGING);
                  USBSerial1.printf("%02x ", IDlist[i].data[j].current);
              }else{
                 USBSerial1.print(sel == i+1 && scroll_data == 1 ? SEL : RST);
                 USBSerial1.printf("%02x ", IDlist[i].data[j].current);
              }
              IDlist[i].data[j].previous = IDlist[i].data[j].current;
            }else{
              USBSerial1.print(INVERT);  //highlight modified data
              USBSerial1.printf("%02x ", IDlist[i].data[j].modified);
              //USBSerial1.print(sel == i+1 ? SEL : RST);
            }
        USBSerial1.print(RST);
        }
    }
  USBSerial1.print(RST);
  USBSerial1.println();
      
}

void refresh(IDinfo (&IDlist)[LIST_LEN], int IDcount) {

    USBSerial1.print("\033[2J"); // clear screen
    USBSerial1.print("\033[H"); // cursor to home
    
    if( sel == 0 ){
        USBSerial1.print(SEL);
        USBSerial1.print(INVERT);
    }

    USBSerial1.print(enableAllState ? " [ ] " : " [X] ");
    USBSerial1.print("Enabled ");
    USBSerial1.print(RST);
    USBSerial1.println("|  ID  |          Data           ");

    if( sel < SCREEN_BUFFER ){
        for( int i = 0 ; i < SCREEN_BUFFER && i < IDcount ; i++ ){
            listIDs(i, IDlist, IDcount);
        }
    }else{
        for( int i = 0 ; i < SCREEN_BUFFER && i < IDcount + i - SCREEN_BUFFER ; i++ ){
            listIDs(sel + i - SCREEN_BUFFER, IDlist, IDcount);
        }
    }
    USBSerial1.print(RST);
    timeOfLastRefresh = millis();
}


void displayMenu(IDinfo (&IDlist)[LIST_LEN], int IDcount) {

    if(selection == 1){  // push button pushed

        if( sel == 0 ){  
            enableAll(IDlist, IDcount); 
        }else{
            if( scroll_list ){ 

                if( !IDlist[sel-1].enable ){

                    scroll_list = 0;
                    scroll_data = 1;
                    IDlist[sel-1].enable = 1;

                }else 
                if( IDlist[sel-1].enable ){

                    IDlist[sel-1].enable = 0;
                }

            }else 
            if ( scroll_data ){

                if( data_select == 0){
                    scroll_list = 1;
                    scroll_data = 0;
                }else
                if( !IDlist[sel-1].data[data_select-1].modify && !IDlist[sel-1].data[data_select-1].enable ){
                    IDlist[sel-1].data[data_select-1].modify = 1;
                    IDlist[sel-1].data[data_select-1].enable = 1;
                    IDlist[sel-1].data[data_select-1].modified = IDlist[sel-1].data[data_select-1].current;
                }else 
                if( !IDlist[sel-1].data[data_select-1].modify && IDlist[sel-1].data[data_select-1].enable ){
                    IDlist[sel-1].data[data_select-1].enable = 0;
                }
                else 
                if( IDlist[sel-1].data[data_select-1].modify && IDlist[sel-1].data[data_select-1].enable){
                    IDlist[sel-1].data[data_select-1].modify = 0;
                }
            }
        }
        selection = 0;

    }else if( updown == 1 ){ // going up list
        
        if( scroll_list ){

            sel = sel <= 0 ? 0 : (sel - 1);

        }else 
        if ( scroll_data ){

            if(IDlist[sel-1].data[data_select-1].modify){
                if(IDlist[sel-1].data[data_select-1].modified > 0x00)
                    IDlist[sel-1].data[data_select-1].modified -= 0x01;
            }else{
                data_select = data_select <= 0 ? 0 : (data_select - 1);
            }
        }

    }else if( updown == 0 ){ // going down list

        if( scroll_list ){

            sel = sel >= IDcount ? IDcount : (sel + 1);

        }else 
        if( scroll_data ){

            if( data_select == 0){
                data_select = 1;
            }else
            if(IDlist[sel-1].data[data_select-1].modify){
                if(IDlist[sel-1].data[data_select-1].modified < 0xFF)
                    IDlist[sel-1].data[data_select-1].modified += 0x01;
            }
            else{
                data_select = data_select >= 8 ? 8 : (data_select + 1);
            }
        }
        
    }

    refresh(IDlist, IDcount);
    
}