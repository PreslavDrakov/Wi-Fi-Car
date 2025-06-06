#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>

#define LIGHT_PIN 4

const int PWMFreq = 1000; /* 1 KHz */
const int PWMResolution = 8;
const int PWMLightChannel = 3;

//Camera related constants
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char* ssid     = "MyWiFiCar";
const char* encodedPassword = "MTIzNDU2Nzg=";

String decodeBase64(String input) {
  const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String output = "";
  int val = 0, valb = -8;
  for (unsigned char c : input) {
    if (isspace(c) || c == '=') continue;
    const char* p = strchr(lookup, c);
    if (p) {
      val = (val << 6) + (p - lookup);
      valb += 6;
      if (valb >= 0) {
        output += char((val >> valb) & 0xFF);
        valb -= 8;
      }
    }
  }
  return output;
}

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsCarInput("/CarInput");
uint32_t cameraClientId = 0;

const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Wi-Fi Car</title>
    <style>
      * {
        box-sizing: border-box;
      }

      body , html{
        margin: 0;
        padding: 0;
        background-color: white;
        text-align: center;
        overflow-x: hidden;
        height: 100%;
        text-align: center;
        align-items: center;
      }

      .arrows {
        font-size: 40px;
        color: red;
        align-items: center;
      }

      .circularArrows {
        font-size: 50px;
        color: blue;
        align-items: center;
      }

      td.button {
        background-color: black;
        border-radius: 25%;
        box-shadow: 5px 5px #888888;
        cursor: pointer;
        justify-content: center;
        align-items: center; /* Centers the arrows inside the buttons */
        text-align: center;
      }

      td.button:active {
        transform: translate(5px, 5px);
        box-shadow: none;
      }

      .noselect {
        user-select: none;
      }

      table {
        width: 100%;
        max-width: 100%;
        margin: auto;
        table-layout: fixed;
      }

      #cameraImage {
        width: 100%;
        height: auto;
        max-width: 100%;
        transform: rotate(180deg);
      }

      .slidecontainer {
        width: 100%;
      }

      .slider {
        -webkit-appearance: none;
        width: 100%;
        height: 15px;
        border-radius: 5px;
        background: #d3d3d3;
        outline: none;
        opacity: 0.7;
        transition: opacity .2s;
      }

      .slider:hover {
        opacity: 1;
      }

      .slider::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 25px;
        height: 25px;
        border-radius: 50%;
        background: red;
        cursor: pointer;
      }

      .downloadBtn {
        font-size: 15px; 
        color: white; 
        background-color: black; 
        padding: 5% 10%; 
        border-radius: 5px; 
        cursor: pointer;
        width: 100%;
      }

      /* Media Queries */
      @media (max-width: 500px) {
        /* Mobile */
        table {
          max-width: 100%; /* Full width on mobile */
        }

        #cameraImage {
          max-width: 100%;
        }

        .downloadBtn {
          font-size: 15px;
          padding: 5% 5%;
        }
      }

      @media (min-width: 501px) {
        /* Desktop */
        table {
          max-width: 500px; /* Limit max width to 600px on desktop */
        }

        .downloadBtn {
          font-size: 21px;
          padding: 10% 15%;
        }

        #cameraImage {
          max-width: 500px;
        }

    </style>
  </head>
  <body class="noselect">
    <table CELLSPACING="10">
      <tr>
        <td colspan="3">
          <img id="cameraImage" src="" alt="Camera Feed">
        </td>
      </tr> 
      <tr>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","5")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#11017;</span></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","1")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#8679;</span></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","6")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#11016;</span></td>
      </tr>
      <tr>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","3")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#8678;</span></td>
        <td class="button"></td>    
        <td class="button" ontouchstart='sendButtonInput("MoveCar","4")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#8680;</span></td>
      </tr>
      <tr>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","7")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#11019;</span></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","2")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#8681;</span></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","8")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">&#11018;</span></td>
      </tr>
      <tr>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","9")' ontouchend='sendButtonInput("MoveCar","0")'><span class="circularArrows">&#8634;</span></td>
        <td class="button" onclick="downloadImage()"><span class="downloadBtn">Photo</span></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","10")' ontouchend='sendButtonInput("MoveCar","0")'><span class="circularArrows">&#8635;</span></td>
      </tr>
      <tr style="height: 10px;"></tr>
      <tr>
        <td style="text-align:left"><b>Speed:</b></td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendButtonInput("Speed",value)'>
          </div>
        </td>
      </tr>        
      <tr>
        <td style="text-align:left"><b>Light:</b></td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="255" value="0" class="slider" id="Light" oninput='sendButtonInput("Light",value)'>
          </div>
        </td>   
      </tr>
      <tr>
        <td style="text-align:left"><b>Pan:</b></td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput='sendButtonInput("Pan",value)'>
          </div>
        </td>
      </tr>        
      <tr>
        <td style="text-align:left"><b>Tilt:</b></td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput='sendButtonInput("Tilt",value)'>
          </div>
        </td>   
      </tr>      
    </table>
    
    <script>
        var webSocketCameraUrl = "ws:\/\/" + window.location.hostname + "/Camera";
        var webSocketCarInputUrl = "ws:\/\/" + window.location.hostname + "/CarInput";      
        var websocketCamera;
        var websocketCarInput;
        
        function initCameraWebSocket() 
        {
            websocketCamera = new WebSocket(webSocketCameraUrl);
            websocketCamera.binaryType = 'blob';
            websocketCamera.onopen    = function(event){};
            websocketCamera.onclose   = function(event){setTimeout(initCameraWebSocket, 2000);};
            websocketCamera.onmessage = function(event)
            {
                var imageId = document.getElementById("cameraImage");
                imageId.src = URL.createObjectURL(event.data);
            };
        }
        
        function initCarInputWebSocket() 
        {
            websocketCarInput = new WebSocket(webSocketCarInputUrl);
            websocketCarInput.onopen    = function(event)
            {
                sendButtonInput("Speed", document.getElementById("Speed").value);
                sendButtonInput("Light", document.getElementById("Light").value);
                sendButtonInput("Pan", document.getElementById("Pan").value);
                sendButtonInput("Tilt", document.getElementById("Tilt").value);
            };
            websocketCarInput.onclose   = function(event){setTimeout(initCarInputWebSocket, 2000);};
            websocketCarInput.onmessage = function(event){};        
        }
        
        function initWebSocket() 
        {
            initCameraWebSocket ();
            initCarInputWebSocket();
        }
  
        function sendButtonInput(key, value) 
        {
            var data = key + "," + value;
            websocketCarInput.send(data);
        }
        
        function downloadImage() 
        {
            const imgElement = document.getElementById("cameraImage");

            const canvas = document.createElement("canvas");
            const ctx = canvas.getContext("2d");

            const img = new Image();
            img.onload = function () {
              canvas.width = img.width;
              canvas.height = img.height;

              ctx.translate(canvas.width, canvas.height);
              ctx.rotate(Math.PI);

              ctx.drawImage(img, 0, 0);

              const link = document.createElement("a");
              link.href = canvas.toDataURL("image/jpeg");
              link.download = "WiFi_camera_snapshot.jpg";
              document.body.appendChild(link);
              link.click();
              document.body.removeChild(link);
            };

            img.src = imgElement.src;
        }

        window.onload = initWebSocket;
        document.getElementById("mainTable").addEventListener("touchend", function(event){
          event.preventDefault()
        });      
      </script>
  </body>
</html>
)HTMLHOMEPAGE";

void sendCarCommands(std::string inputCommand)
{
  Serial.println(inputCommand.c_str());
}

void handleRoot(AsyncWebServerRequest *request) 
{
  request->send_P(200, "text/html", htmlHomePage);
}

void handleNotFound(AsyncWebServerRequest *request) 
{
    request->send(404, "text/plain", "File Not Found");
}

void onCarInputWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      //Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      //Serial.printf("WebSocket client #%u disconnected\n", client->id());
      sendCarCommands("MoveCar,0");
      sendCarCommands("Pan,90"); 
      sendCarCommands("Tilt,90");
      ledcWrite(PWMLightChannel, 0);  
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        //Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str()); 
        int valueInt = atoi(value.c_str());     
        if (key == "MoveCar" || key == "Speed" || key == "Pan" || key == "Tilt")
        {
          sendCarCommands(myData);    
        }
        else if (key == "Light")
        {
          ledcWrite(PWMLightChannel, valueInt);         
        }     
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      //Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      //Serial.printf("WebSocket client #%u disconnected\n", client->id());
      cameraClientId = 0;
      break;
    case WS_EVT_DATA:
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void setupCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    //Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }  

  if (psramFound())
  {
    heap_caps_malloc_extmem_enable(20000);  
    //Serial.printf("PSRAM initialized. malloc to take memory from psram above this size");    
  }  
}

void sendCameraPicture()
{
  if (cameraClientId == 0)
  {
    return;
  }
  unsigned long  startTime1 = millis();
  //capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) 
  {
      //Serial.println("Frame buffer could not be acquired");
      return;
  }

  unsigned long  startTime2 = millis();
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);
    
  //Wait for message to be delivered
  while (true)
  {
    AsyncWebSocketClient * clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull()))
    {
      break;
    }
    delay(1);
  }
  
  unsigned long  startTime3 = millis();  
  //Serial.printf("Time taken Total: %d|%d|%d\n",startTime3 - startTime1, startTime2 - startTime1, startTime3-startTime2 );
}

void setUpPinModes()
{
  //Set up PWM
  ledcSetup(PWMLightChannel, PWMFreq, PWMResolution);
  pinMode(LIGHT_PIN, OUTPUT);    
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);

  sendCarCommands("MoveCar,0"); 
  sendCarCommands("Pan,90"); 
  sendCarCommands("Tilt,90");  
}

void setup(void) 
{
  setUpPinModes();
  Serial.begin(115200);

  String password = decodeBase64(encodedPassword);
  WiFi.softAP("MyWiFiCar", password.c_str());

  IPAddress IP = WiFi.softAPIP();
  //Serial.print("AP IP address: ");
  //Serial.println(IP);

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
      
  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();
  //Serial.println("HTTP server started");

  setupCamera();
}


void loop() 
{
  wsCamera.cleanupClients(); 
  wsCarInput.cleanupClients(); 
  sendCameraPicture(); 
  //Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
}













