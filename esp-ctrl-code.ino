#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h> // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <ArduinoJson.h> // library for parsing json
#include <PubSubClient.h>

//#include "BluetoothSerial.h"
#include <Preferences.h>
#include "TOD.h"

Preferences preferences;

//test network: Os Silva Wi Fi
//test pass: c8906d2932
String network = "Os Silva Wi Fi";
String password= "c8906d2932";
bool conncted = false;

volatile int interruptCounter = 0;

// ******* MQTT VARIABLEs *****************
// mqtt broker currently running on Heroku
const char* mqttServer = "soldier-01.cloudmqtt.com";
const int mqttPort= 18488;
const char* mqttUser = "ehvtsbga";
const char* mqttPassword = "5n7anyeMnoLR";

const char* subscribeCtrlPath = "system/control/dispense";
const char* subscribeTimingPath = "system/app/timing";
const char* publishTempPath = "system/hardware/temp";
const char* publishLevelPath = "system/hardware/level";
const char* testPath = "system/ping";


WiFiClient espClient;
PubSubClient client(espClient);

// ********************* ******************



//****** SETUP OF RTC MODULE ***************

template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Sets up serial streaming Serial<<"text"<<variable;
TOD RTC; //Instantiate Time of Day class TOD as RTC
uint8_t lastminute=0;
uint8_t lastsecond=0;
char printbuffer[50];
bool TODvalid=false;

//****** /SETUP OF RTC MODULE ***************

// ******* API-RETRIEVED SYSTEM VARIABLES ****

char title[50] = "---";
const char * description= NULL;
float intervals[] = {0.0, 0.0, 0.0, 0.0, 0.0};
double amount;

bool hasMode = false; // set true if we have a loaded mode

// ******* /API-RETRIEVED SYSTEM VARIABLES ****

// ***** TIME SCHEDULE VARIABLES *******
int current_interval = 0;
uint8_t deadline_hour = 0;
uint8_t deadline_minute = 0;

// ***** /TIME SCHEDULE VARIABLES *******

int availableNets = 0;
String inData = "";


hw_timer_t * timer = NULL;
portMUX_TYPE timerMux =  portMUX_INITIALIZER_UNLOCKED;
bool toggleLed = 0;

String baseURL = "https://watering-system-api.herokuapp.com/api/v1.0"; // address for accesing the API 

// function to execute when interruption triggers
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux); // lock main thread for executing critical code
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

//BluetoothSerial SerialBT;

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Initialization...");
    
    pinMode(LED_BUILTIN, OUTPUT);

    connectToSavedNetwork();
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);

    
}

// ***************** LOOP *********************************
void loop() {

  if(interruptCounter > 0){
    // since the ocunter is shared with ISR, we should
    // change it inside a critical section:
    portENTER_CRITICAL(&timerMux); 
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
    toggleLed = !toggleLed;
    digitalWrite(LED_BUILTIN, toggleLed);
    
    
  }

  if(Serial.available() > 0){
    char input = Serial.read(); 
    char inp[12]= {input};
    int in = atoi(inp); // convert from ascII to int
    getInput(in);
    
  }

  checkNetStatus();
  //getRequest("/order/1");
  //postRequest("/orders"); // change this endpoint when we have the actual resources 
  //delay(500);
  countTime(); // perform continous check of time
  reconnectToBroker();

  // making a lil testing
  if(publishMsg(testPath, "1000")){
    Serial.println("test message published");
  }
  

  
  
}
// ***************** /LOOP *********************************

// ************** PUBLISH TO MQTT BROKER *******************

/*
 publish message on the specified topic in the MQTT broker
 return false if client isnt connected
*/
bool publishMsg(const char* path, const char* msg){
  
  if(client.connected()){

    client.publish(path, msg);
    return true;
  }

  else{

    Serial.println("cliente não conectado");
    return false;
    
  }
  
  
}

// *********************************************************

// ***** CALLBACK FOR SUBSCRIPTIONS FROM MQTT BROKER ********

// prints messages retrieved from MQTT BROKER
// TODO: do something more useful with those messages
void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


// **********************************************************



// ************** CONNECTION TO MQTT BROKER ****************

// TODO: FINISH BUILDING THE MQTT CLIENT
// link to tutorial: https://www.arduinoecia.com.br/2019/02/enviando-mensagens-mqtt-modulo-esp32-wifi.html
bool reconnectToBroker(){
  // attempt to connection or reconnection to the broker

  int attemptsAcc = 0; // attempts to connect
  client.setServer(mqttServer, mqttPort);

  // set function to handle the callback from subscription route
  client.setCallback(callback);

  /* code will make 15 attempts to connect to MQTT broker
     (this is because we'll be using the CloudMQTT service
     of Heroku and thus, it goes to sleep when it isnt used for
     30 mins, causing slow waking. Upon successful connection with
     server, client will subscribe to the global topics for 
     timing and control.
  */
  while(!client.connected() && (attemptsAcc < 30)){
    
    Serial.print("Conectando com o broker ");
    Serial.println(mqttServer);
    
    if(client.connect("ESP32Client",mqttUser,mqttPassword)){
      
      Serial.println("Conectado ao Broker!");
      client.subscribe(subscribeTimingPath);
      client.subscribe(subscribeCtrlPath);
      return true;
      
    } else {
      
      Serial.print("Estado cliente: ");
      Serial.println(client.state());
       
    }
    
    
    delay(500);
    attemptsAcc++;
    
  }

  
}

// **************************************************


// ************** CLIENT REQUESTS *******************

/*
 * Gets information from API 
 * regarding the currently active 
 * order.
 * 
 * request: endpoint(URL) for the get request
 * ex: '/order/1'
 * returns NULL
 * 
 */
void getRequest(String request){
  
  HTTPClient http;
  
  // configure url
  http.begin(baseURL + request);
  Serial.println("[HTTP] GET...");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0){
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK){
      String payload = http.getString();
      Serial.println(payload);

      DynamicJsonBuffer JSONBuffer(300); //Memory pool
      JsonObject& order = JSONBuffer.parseObject(payload); // parse root of our object 
      //JsonObject& order = root.createNestedObject("order"); // get our object  

      if(!order.success()){ // check for errors in parsing
        Serial.println("Parsing failed!");
        return;
 
      } else{

        // putting the JSON field values into global variables
        /*Serial.println("Successfully parsed");
        Serial.println("Title");
        title = order["order"]["title"]; 
        Serial.println(title);
        Serial.println("description");
        description = order["order"]["description"]; 
        Serial.println(description);
        for(int i = 0; i < 4; i++){
          intervals[i]= order["order"]["interval"][i];
          
        }
        amount = order["order"]["amount"];*/
        processOrder(order); // process the data received
         
        
      }
      
    }
    else {
      Serial.printf("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
    }

    http.end();
    delay(500); // remove this delay later

  }
  
}

/*
 * Builds a JSON object containing information 
 * on the current status of the system.
 * Builds and sends a POST request to the server.
 * 
 * request: endpoint for the post request
 * returns NULL
 * 
 */
void postRequest(String request){

   

  HTTPClient http;
  
  char t[20];
  if(hasMode){
    strcpy(t,title);
  }else {
    strcpy(t, "null");
  }
 
  http.begin(baseURL + request); //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json"); //Specify content-type header
  char info[60];
  // hardcoding a goddamn JSON package
  strcat(info, "{\"connected\":\"true\",\"title\":");
  strcat(info, "\"");
  strcat(info, t);
  strcat(info, "\"}");
  Serial.println();
  Serial.println(info);

  int httpResponseCode = http.POST(info); //Send the actual POST request with the 'info' JSON objg

  if(httpResponseCode > 0){
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpResponseCode);
    if(httpResponseCode == HTTP_CODE_CREATED){
      String response = http.getString();  //Get the response to the request
 
      Serial.println(httpResponseCode);   //Print return code
      Serial.println(response);           //Print request answer
      
    } else {
  
      Serial.printf("[HTTP] POST... failed, error: %s", http.errorToString(httpResponseCode).c_str());
    
    }
  }

  
    

  http.end();
  
  
}

// ************** /CLIENT REQUESTS *******************


// ***************** RTC TIME REGISTERING AND PUMP CONTROL **********
/**
 * Processes a JSON object with data for a 
 * pumping schedule. Assigns the values to 
 * the proper global variables.
 * 
 * order: JSON Object
 * Returns VOID
 */
void processOrder(JsonObject& order){

  // comparing the orders if our current data isnt NULL
  if(strcmp(order["order"]["title"],title) == 0){
    // do something if our data is the same as the api
    
    /*Serial.println(title);
    String a = order["order"]["title"];
    Serial.println(a);
    Serial.println(strcmp(order["order"]["title"],title));
    if(strcmp(order["order"]["title"],title) == 0){
       Serial.println("no change");
    }*/
    Serial.println("no change");
    return;
    
  } else {
    // putting the JSON field values into global variables
    Serial.println("Assigning values:");
    //title = order["order"]["title"];
    strcpy(title, order["order"]["title"]);
    //strcpy(description, order["order"]["description"]);
    description = order["order"]["description"];
    for(int i = 0; i < 4; i++){
          intervals[i]= order["order"]["interval"][i];
          
    }
    amount = order["order"]["amount"];
    Serial.print("title:");
    Serial.println(title);
    Serial.print("description:");
    Serial.println(description);
    Serial.print("amount:");
    Serial.println(amount);
    hasMode = true; // set flag hasMode true, our system now has a mode running

    // setting the first interval as deadline
    current_interval = 0;
    setNextTime(intervals[current_interval]);// set the the first interval as next time
    
    
  }

  
  
  
}

/**
 * Processes the number of hours and minutes until
 * next irrigation process and assigns those values 
 * accordingly to global variables deadline_hour and
 * deadline_minute
 * 
 * nextTime : float - time until next irrigation
 * returns void
 */
void setNextTime(float nextTime){

  // extract hours and minutes separately from the float
  int hoursToDeadline = int(nextTime);
  int minutesToDeadline = int((nextTime - hoursToDeadline)*100);
  printTimeRemaining(hoursToDeadline, minutesToDeadline);

  if(TODvalid){
    // assigns deadlines
    deadline_hour =  RTC.hour() + hoursToDeadline;
    deadline_minute = RTC.minute() + minutesToDeadline;

    // correct hours and minutes for the next
    // day or hour
    if(deadline_hour > 23){
      deadline_hour = deadline_hour -24;
      
    }

    if(deadline_minute > 59){
      deadline_minute = deadline_minute - 60;
      deadline_hour++;
    }

     Serial.println("New deadline:");
     Serial.print(deadline_hour);
     Serial.print(":");
     Serial.print(deadline_minute);
     Serial.println();
  }
   
  
    
}

void printTimeRemaining(int hours, int mins){
  Serial.println("Time until next watering:");
  Serial.print(hours);
  Serial.print(":");
  Serial.print(mins);
  Serial.println();
}

/**
 * Continuously checks the time to see if the deadline
 * for the next irrigation has been reached. If so, sends
 * info data to the server, gets newest data, performs
 * irrigation and sets next irrigation deadline.
 * 
 * returns void
 */
void countTime(){

  if(RTC.second()!=lastsecond && TODvalid) //We want to perform this loop on the second, each second
  {
    lastsecond=RTC.second();
    sprintf(printbuffer,"   UTC Time:%02i:%02i:%02i.%03i\n", RTC.hour(), RTC.minute(),RTC.second(),RTC.millisec());
    Serial<<printbuffer; 
  }

  // every minute check the schedule
  if(RTC.minute() != lastminute && TODvalid){
    lastminute=RTC.minute();
    // if we've got an order pending
    Serial.println("check routine");
    if(hasMode){

      // if we've reached the deadline or have passed it by some minutes
      if((RTC.hour() == deadline_hour) && ((RTC.minute() == deadline_minute) || (RTC.minute() > deadline_minute))){
          current_interval++;

          // if we reached the end of the interval array or if no more deadlines for today
          if(current_interval == 5 || intervals[current_interval] == 0.0){
            current_interval = 0;
          }
          setNextTime(intervals[current_interval]); // set next deadline
          //pump(); // do some pumping (OH YEA)
          postRequest("/infos"); // send info post to API
          getRequest("/order/1"); // get newest info from API
       
      }
    
    }  else getRequest("/order/1"); // try to get the newest order
    
  }
    
  if(!TODvalid) //if we still havent got a valid TOD, hit the NIST time server 
  {
     char ssid[20];
     char pwd[20];
     network.toCharArray(ssid,20);
     password.toCharArray(pwd,20);
     if(RTC.begin(ssid,pwd))TODvalid=true; 
   }
}
  

// ***************** /RTC TIME REGISTERING AND PUMP CONTROL **********


// ***************** CONNECTION STATUS ******************************
void checkConnection(String net, String pwd){
    int attemptsAcc = 0; // attempts to connect
  
    while((WiFi.status() != WL_CONNECTED) && (attemptsAcc < 10)){
      delay(500);
      Serial.print("connectando com wifi ");
      Serial.println(net);
      attemptsAcc++;
      
    }
  
    /* the following code lights up the esp's built-in led if it manages to connect
    to a wireless network, otherwise, it'll start an interruption routine that blinks
    the led every half second, announcing that we cant connect, the blinking it assynchronous
    because if we cant connect to wifi, we'll start bluetooth service to let the user input
    data for connecting to another network*/
    if(WiFi.status() == WL_CONNECTED){
      conncted = true;
      Serial.print("Connectado com a rede ");
      Serial.println(net);
      digitalWrite(LED_BUILTIN, HIGH); 
      
      
      // if we have our timer enabled, disable it 
      if(timer != NULL){
        timerAlarmDisable(timer);
        timerDetachInterrupt(timer);
      }
      saveCredentials(net, pwd); // if connection successful, save details to memory
      
      char ssid[20];
      char pwd[20];
      network.toCharArray(ssid,20);
      password.toCharArray(pwd,20);
      if(RTC.begin(ssid,pwd))TODvalid=true;   //.begin(ssid,password) requires SSID and password of your home access point
                                               //All the action is in this one function.  It connects to access point, sends
                                               //an NTP time query, and receives the time mark. Returns TRUE on success.
      lastminute=RTC.minute();

         
    } else{
      conncted = false;
      Serial.println("Failed to connect to network");
      timer = timerBegin(0,80,true); // prescaler value
      /* below attaching timer interrupt with timer variable as first arg
      onTimer function as second ard and true for interrupt 
      generated on edge type */
      timerAttachInterrupt(timer, &onTimer, true);
      timerAlarmWrite(timer,500000, true); // generate an interrupt every 0.5 second
      timerAlarmEnable(timer); // enable interruption 
      scanNets();
      scanNets();
    }
  
}

void checkNetStatus(){
  // if we are still connected just light the builtin led
  // since not everytime the connecting routine lights it up
  if(conncted){
    
    if(WiFi.status() == WL_CONNECTED){
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      // if not, turn it off and attempt to connect to previously
      // connected network, then check connection again
      conncted = false;
      digitalWrite(LED_BUILTIN, LOW);
      connectToSavedNetwork();
    }
    
  }
  
  
}

// ***************** /CONNECTION STATUS ******************************


// ***************** SCAN FOR NETWORKS *******************************
void scanNets(){
  
  // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    availableNets = n;
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
            delay(10);
        }
    }
    Serial.println("");
      
}
// ***************** /SCAN FOR NETWORKS *******************************

void getInput(int input){

  // here we send some report through the bt
  if(input == 9){
    sendReport(); 
  }
  
  /* if we arent connected the input is handling
  connection */
  if(WiFi.status() != WL_CONNECTED){
    if(input <= availableNets){
      
      //if the selected network is open, try connect straight away
      if(WiFi.encryptionType(input-1) == WIFI_AUTH_OPEN){
        WiFi.begin(WiFi.SSID(input-1).c_str());
        checkConnection(WiFi.SSID(input-1), ""); // check if connection attempt is successful 
      
      }
      
      // or else, we'll need the password to connect to it 
      else {
        Serial.print("Password for network: ");
        Serial.println(WiFi.SSID(input-1));
        // wait for password 
        String pwd = "";
        while(pwd == ""){
          //delay(10);
          pwd = Serial.readString();
           
        }
        
        char net[60];
        char pass[60]; 
        pwd.toCharArray(pass, 60);
        WiFi.SSID(input-1).toCharArray(net, 60);
        Serial.print("password input was: ");
        Serial.println(pass);
        WiFi.begin(net, pass); // mock attempt to check
        checkConnection(net, pass); // check if connection is successful 
        if(WiFi.scanComplete() > -2){ // if WiFi.scanComplete() is -2 then it hanst been triggered
          WiFi.scanDelete();
        }
             
      }
           
    }
  }
  // later on we'll add more handling to this 
  
  
  
}

// START DISCONNECT
void disconnection(){
  if(WiFi.status() == WL_CONNECTED){
    WiFi.disconnect();
    Serial.println("disconnected from network");
  } else Serial.println("device is not connected to any network");
}
// END DISCONNECT


// START CONNECTING TO MEMORY-SAVED NETWORK
void connectToSavedNetwork(){ 
    // using home connection
    //getLastSavedCredentials(); // get details from last saved network
    if(network != "---" && password != "---"){ // if we have connection details in store
      char net[60];
      network.toCharArray(net, 60);
      if(password.length() > 0){
        char pass[60];
        password.toCharArray(pass,60);
        WiFi.begin(net, pass);
      } else WiFi.begin(net);

      checkConnection(network,password);
    }
  
}
// END CONNECTING TO MEMORY-SAVED NETWORK


// START SAVING NETWORK CREDENTIALS ON MEMORY
void saveCredentials(String net, String pwd){
  Serial.print("saving ");
  Serial.print(net);
  Serial.println(" to memory");
  
  preferences.begin("my-app",false);
  preferences.putString("network",net);
  delay(500);
  preferences.putString("password", pwd);
  delay(500);
  preferences.end();

  Serial.println("Netowork saved");

  
}
// END SAVING NETWORK CREDENTIALS ON MEMORY

// START GETTING LAST SAVED CREDENTIALS FROM MEMORY
void getLastSavedCredentials(){
  preferences.begin("my-app",false);
  network = preferences.getString("network","---");
  password = preferences.getString("password","---");
  Serial.print("last saved net: ");
  Serial.println(network);
  Serial.print("last saved password: ");
  Serial.println(password);
  preferences.end();
}
// END GETTING LAST SAVED CREDENTIALS FROM MEMORY

// START SENDING STATUS REPORT THROUGH SERIAL
void sendReport(){
  // in the future this report might be sent online
  Serial.print("Connected:");
  Serial.println(conncted);
  
}
// END SENDING STATUS REPORT THROUGH SERIAL

// code for interruption: https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/
