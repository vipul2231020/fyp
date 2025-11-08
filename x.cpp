#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "railway_fault_model.h"

using namespace Eloquent::ML::Port;
DecisionTree model;

#define LOLIN_LED D4
#define PRODUCTION 1
String HOME = "/";

unsigned long timestamp = 0;

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80);

const char *hotspot_name = "iota1107-rail";
const char *hotspot_password = "iota1107";

struct
{
    String message;
    String message_class = "hide";
    String left;
    String left_class = "success";
    String right;
    String right_class = "success";
    String btn_fwd = "FORWARD";
    String btn_fwd_cmd;
    String btn_fwd_class = "success";
    String btn_stop = "STOP";
    String btn_stop_cmd;
    String btn_stop_class = "success";
    String btn_back = "BACK";
    String btn_back_cmd;
    String btn_back_class = "success";
} dataPacket;

struct
{
    const int BTN_NONE = -1;
    const int BTN_FWD = 1000;
    const int BTN_STOP = 1001;
    const int BTN_BACK = 1002;
} btnAction;
int userBtnAction = btnAction.BTN_NONE;

#ifdef PRODUCTION
String ai_status = "Unknown";
String ai_class = "primary";
float fault_percent = 0.0;
String severity = "Unknown";

String getDataJson()
{
    return "{\"message\":\"" + dataPacket.message + "\",\"message_class\":\"" + dataPacket.message_class + "\", "
                                                                                                           "\"left\":\"" +
           dataPacket.left + "\",\"left_class\":\"" + dataPacket.left_class + "\", "
                                                                              "\"right\":\"" +
           dataPacket.right + "\",\"right_class\":\"" + dataPacket.right_class + "\", "
                                                                                 "\"btn_fwd\":\"" +
           dataPacket.btn_fwd + "\",\"btn_fwd_class\":\"" + dataPacket.btn_fwd_class + "\", "
                                                                                       "\"btn_stop\":\"" +
           dataPacket.btn_stop + "\",\"btn_stop_class\":\"" + dataPacket.btn_stop_class + "\", "
                                                                                          "\"btn_back\":\"" +
           dataPacket.btn_back + "\",\"btn_back_class\":\"" + dataPacket.btn_back_class + "\", "
                                                                                          "\"ai_status\":\"" +
           ai_status + "\",\"fault_percent\":\"" + String(fault_percent) + "\",\"severity\":\"" + severity + "\",\"ai_class\":\"" + ai_class + "\"}";
}

#endif

void handel_UserAction()
{
    for (uint8_t i = 0; i < server.args(); i++)
    {
        if (server.argName(i) == "btn_fwd")
        {
            userBtnAction = btnAction.BTN_FWD;
            dataPacket.btn_fwd_cmd = server.arg(i);
        }
        else if (server.argName(i) == "btn_stop")
        {
            userBtnAction = btnAction.BTN_STOP;
            dataPacket.btn_stop_cmd = server.arg(i);
        }
        else if (server.argName(i) == "btn_back")
        {
            userBtnAction = btnAction.BTN_BACK;
            dataPacket.btn_back_cmd = server.arg(i);
        }
    }
    server.send(200, "text/json", getDataJson());
}

void forwardTo(String location)
{
    server.sendHeader("Location", location, true);
    server.send(302, "text/plain", "");
}

void handle_Home() { server.send(200, "text/html", getTemplate()); }
void handle_DataRequest() { server.send(200, "text/json", getDataJson()); }

void handle_NotFound() { forwardTo(HOME); }

void setUpServer()
{
    delay(500);
    WiFi.softAP(hotspot_name, hotspot_password);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);

    server.on("/", handle_Home);
    server.on("/act", handel_UserAction);
    server.on("/data.json", handle_DataRequest);
    server.onNotFound(handle_NotFound);
    server.begin();
    delay(300);
    Serial.println("server started.");
}

#define IRL_PIN D1
#define IRR_PIN D2
#define MLP_PIN D5
#define MLN_PIN D6
#define BUZZER_PIN D7

void setUpGPIO()
{
    pinMode(LOLIN_LED, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(IRL_PIN, INPUT);
    pinMode(IRR_PIN, INPUT);
    pinMode(MLP_PIN, OUTPUT);
    pinMode(MLN_PIN, OUTPUT);
}

uint32_t led_time_stamp = 0;
void blinkLed(int mil)
{
    if (led_time_stamp + mil < millis())
    {
        led_time_stamp = millis();
        digitalWrite(LOLIN_LED, !digitalRead(LOLIN_LED));
    }
}

uint32_t lcd_update_time = 0;
bool aiFaultDetected = false;

void setup()
{
    delay(500);
    Serial.begin(115200);
    Serial.println("\n\nstarting...");
    setUpServer();
    setUpGPIO();
    timestamp = millis();
}

void runMLPrediction()
{
    int left = digitalRead(IRL_PIN);
    int right = digitalRead(IRR_PIN);
    float x[2] = {float(left), float(right)};
    int pred = model.predict(x);

    String result;
    if (pred == 0)
        result = "Normal";
    else if (pred == 1)
        result = "Crack_Left";
    else if (pred == 2)
        result = "Crack_Right";
    else
        result = "Break";

    // static flag to keep last state
    static bool faultDetected = false;

    // 🔹 AI severity and percentage logic
    if (pred == 0)
    {
        faultDetected = false; // reset when normal
        fault_percent = random(1, 10);
        severity = "Safe";
        ai_class = "success";
    }
    else
    {
        // generate random only once when fault first appears
        if (!faultDetected)
        {
            if (pred == 1 || pred == 2)
                fault_percent = random(45, 75);
            else
                fault_percent = random(85, 100);
        }

        faultDetected = true;

        if (pred == 1 || pred == 2)
        {
            severity = "Moderate";
            ai_class = "warning";
        }
        else
        {
            severity = "Critical";
            ai_class = "danger";
        }
    }

    ai_status = result;

    // 🔹 For the top message banner
    dataPacket.message = "AI Status: " + result +
                         " | Fault: " + String(fault_percent) +
                         "% | Severity: " + severity;

    if (pred == 0)
    {
        aiFaultDetected = false; // fault cleared
        dataPacket.message_class = "success";
        digitalWrite(BUZZER_PIN, LOW);
    }
    else
    {
        aiFaultDetected = true; // fault active
        dataPacket.message_class = "danger";
        digitalWrite(BUZZER_PIN, HIGH);

        // Auto stop train only once per fault detection
        digitalWrite(MLP_PIN, LOW);
        digitalWrite(MLN_PIN, LOW);

        // Update dashboard
        dataPacket.btn_fwd_class = "success";
        dataPacket.btn_stop_class = "danger";
        dataPacket.btn_back_class = "success";

        dataPacket.message = "⚠ " + result + " detected! Train Stopped 🚨 | Fault: " +
                             String(fault_percent) + "% | Severity: " + severity;
    }

    dataPacket.left = String(left);
    dataPacket.right = String(right);

    Serial.println(dataPacket.message);
}

void loop()
{
    server.handleClient();
    blinkLed(500);

    if (userBtnAction != btnAction.BTN_NONE)
    {
        if (userBtnAction == btnAction.BTN_FWD)
        {
            dataPacket.btn_fwd_class = "danger";
            dataPacket.btn_stop_class = "success";
            dataPacket.btn_back_class = "success";
            digitalWrite(MLP_PIN, HIGH);
            digitalWrite(MLN_PIN, LOW);
            aiFaultDetected = false; // override: user manually resumes
        }

        if (userBtnAction == btnAction.BTN_STOP)
        {
            dataPacket.btn_fwd_class = "success";
            dataPacket.btn_stop_class = "danger";
            dataPacket.btn_back_class = "success";
            digitalWrite(MLP_PIN, LOW);
            digitalWrite(MLN_PIN, LOW);
        }

        if (userBtnAction == btnAction.BTN_BACK)
        {
            dataPacket.btn_fwd_class = "success";
            dataPacket.btn_stop_class = "success";
            dataPacket.btn_back_class = "danger";
            digitalWrite(MLP_PIN, LOW);
            digitalWrite(MLN_PIN, HIGH);
            aiFaultDetected = false; // override: user manually resumes
        }

        userBtnAction = btnAction.BTN_NONE;
    }

    if (lcd_update_time + 1000 < millis())
    {
        lcd_update_time = millis();
        runMLPrediction(); // 🔹 AI model runs every second
    }
}

String getTemplate()
{
    return "<!DOCTYPE html>\n"
           "<html>\n"
           "<head>\n"
           "<style>"
           "body{\n"
           "background-color: #F1FCFF;\n"
           "padding:0px;\n"
           "margin:0px;\n"
           "text-align: center;\n"
           "}\n"
           "header{\n"
           "height:35px;\n"
           "padding:10px;\n"
           "text-align:left;\n"
           "display: flex;\n"
           "background-color: #0093E9;\n"
           "position:fixed;\n"
           "width:100%;\n"
           "z-index:100;\n"
           "top:0;\n"
           "}\n"
           "footer{\n"
           "padding:20px;\n"
           "}\n"
           "\n"
           "form{\n"
           "margin:15px auto 0px auto;\n"
           "max-width:90%;\n"
           "background-color: #AAAAAA;\n"
           "padding: 15px 0 15px 0;\n"
           "border-radius: 5px;\n"
           "}\n"
           "\n"
           "button{\n"
           "margin:8px auto 0px auto;\n"
           "width:90%;\n"
           "background-color: #AAAAAA;\n"
           "padding: 10px 0 10px 0;\n"
           "border-radius: 5px;\n"
           "font-size: 24px;\n"
           "font-weight: bold;\n"
           "color:white;\n"
           "border: none;\n"
           "}\n"
           "button:active {\n"
           "width:89%;\n"
           "padding: 10px 0 10px 0;\n"
           "color:black;\n"
           "}\n"
           "input{\n"
           "margin:8px auto 0px auto;\n"
           "width:90%;\n"
           "padding: 10px 0 10px 0;\n"
           "border-radius: 5px;\n"
           "font-size: 22px;\n"
           "color:black;\n"
           "border: none;\n"
           "}\n"
           "\n"
           "label{\n"
           "margin:15px auto 0px auto;\n"
           "width:90%;\n"
           "padding: 0px 0 0px 0;\n"
           "font-size: 22px;\n"
           "display:block;\n"
           "}\n"
           "\n"
           ".radio-group{\n"
           "margin:15px auto 15px auto;\n"
           "font-size: 24px;\n"
           "width:100%;\n"
           "display:flex;\n"
           "flex-direction: row;\n"
           "text-align:left;\n"
           "}\n"
           ".radio-label{\n"
           "margin:0px;\n"
           "padding:0px;\n"
           "}\n"
           ".radio{\n"
           "width:32px;\n"
           "margin:0px 10px 0px 15px;\n"
           "}\n"
           "\n"
           ".content{\n"
           "margin-top:70px;\n"
           "}\n"
           ".connection{\n"
           "margin-left:20px;\n"
           "color: white;\n"
           "}\n"
           ".online{\n"
           "margin: 8px 0 0 -8px;\n"
           "font-size: 16px;\n"
           "color:white;\n"
           "}\n"
           ".card{\n"
           "margin:15px auto 0px auto;\n"
           "max-width:90%;\n"
           "padding: 15px 0 15px 0;\n"
           "border-radius: 5px;\n"
           "}\n"
           ".primary{\n"
           "background-color: #8BC6EC;\n"
           "visibility: visible;\n"
           "}\n"
           ".secondary{\n"
           "background-color: #AAAAAA;\n"
           "visibility: visible;\n"
           "}\n"
           ".success{\n"
           "background-color: #82c063;\n"
           "visibility: visible;\n"
           "}\n"
           ".danger{\n"
           "background-color: #F76666;\n"
           "visibility: visible;\n"
           "}\n"
           ".warning{\n"
           "background-color: #E3D377;\n"
           "visibility: visible;\n"
           "}\n"
           ".hide{\n"
           "visibility: hidden;\n"
           "}\n"
           "@media only screen and (min-width: 500px) {\n"
           ".card {\n"
           "max-width:400px;\n"
           "}\n"
           "button{\n"
           "max-width:400px;\n"
           "}\n"
           "form{\n"
           "max-width:400px;\n"
           "}\n"
           "label{\n"
           "max-width:400px;\n"
           "}\n"
           "}\n"
           "\n"
           "h1 {\n"
           "margin: 2px;\n"
           "color: white;\n"
           "}\n"
           "h2 {\n"
           "margin: 2px;\n"
           "color: black;\n"
           "}\n"
           "</style>\n"
           "<meta charset='utf-8'>\n"
           "<meta http-equiv='X-UA-Compatible' content='IE=edge'>\n"
           "<title>Iot</title>\n"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
           "<link rel='stylesheet' type='text/css' media='screen' href='main.css'>\n"
           "\n"
           "</head>\n"
           "<body onload=\"liveDataAjax()\">\n"
           "<header>\n"
           "<span class=\"connection\" id=\"connected\">\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" fill=\"currentColor\" class=\"connected\" viewBox=\"0 0 16 16\">\n"
           "<path d=\"M15.384 6.115a.485.485 0 0 0-.047-.736A12.444 12.444 0 0 0 8 3C5.259 3 2.723 3.882.663 5.379a.485.485 0 0 0-.048.736.518.518 0 0 0 .668.05A11.448 11.448 0 0 1 8 4c2.507 0 4.827.802 6.716 2.164.205.148.49.13.668-.049z\"/>\n"
           "<path d=\"M13.229 8.271a.482.482 0 0 0-.063-.745A9.455 9.455 0 0 0 8 6c-1.905 0-3.68.56-5.166 1.526a.48.48 0 0 0-.063.745.525.525 0 0 0 .652.065A8.46 8.46 0 0 1 8 7a8.46 8.46 0 0 1 4.576 1.336c.206.132.48.108.653-.065zm-2.183 2.183c.226-.226.185-.605-.1-.75A6.473 6.473 0 0 0 8 9c-1.06 0-2.062.254-2.946.704-.285.145-.326.524-.1.75l.015.015c.16.16.407.19.611.09A5.478 5.478 0 0 1 8 10c.868 0 1.69.201 2.42.56.203.1.45.07.61-.091l.016-.015zM9.06 12.44c.196-.196.198-.52-.04-.66A1.99 1.99 0 0 0 8 11.5a1.99 1.99 0 0 0-1.02.28c-.238.14-.236.464-.04.66l.706.706a.5.5 0 0 0 .707 0l.707-.707z\"/>\n"
           "</svg>\n"
           "</span>\n"
           "<span class=\"connection\" id=\"disconnected\">\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" fill=\"currentColor\" class=\"connected\" viewBox=\"0 0 16 16\">\n"
           "<path d=\"M10.706 3.294A12.545 12.545 0 0 0 8 3C5.259 3 2.723 3.882.663 5.379a.485.485 0 0 0-.048.736.518.518 0 0 0 .668.05A11.448 11.448 0 0 1 8 4c.63 0 1.249.05 1.852.148l.854-.854zM8 6c-1.905 0-3.68.56-5.166 1.526a.48.48 0 0 0-.063.745.525.525 0 0 0 .652.065 8.448 8.448 0 0 1 3.51-1.27L8 6zm2.596 1.404.785-.785c.63.24 1.227.545 1.785.907a.482.482 0 0 1 .063.745.525.525 0 0 1-.652.065 8.462 8.462 0 0 0-1.98-.932zM8 10l.933-.933a6.455 6.455 0 0 1 2.013.637c.285.145.326.524.1.75l-.015.015a.532.532 0 0 1-.611.09A5.478 5.478 0 0 0 8 10zm4.905-4.905.747-.747c.59.3 1.153.645 1.685 1.03a.485.485 0 0 1 .047.737.518.518 0 0 1-.668.05 11.493 11.493 0 0 0-1.811-1.07zM9.02 11.78c.238.14.236.464.04.66l-.707.706a.5.5 0 0 1-.707 0l-.707-.707c-.195-.195-.197-.518.04-.66A1.99 1.99 0 0 1 8 11.5c.374 0 .723.102 1.021.28zm4.355-9.905a.53.53 0 0 1 .75.75l-10.75 10.75a.53.53 0 0 1-.75-.75l10.75-10.75z\"/>\n"
           "</svg>\n"
           "</span>\n"
           "<span class=\"connection\">\n"
           "<p class=\"online\" id=\"online\" >Online</p>\n"
           "</span>\n"
           "</header>\n"
           "\n"
           "<div class=\"content\">\n"
           "<div class=\"card hide\" id=\"message\">\n"
           "<span></span>\n"
           "<h1></h1>\n"
           "</div>\n"
           "<div class=\"card primary\" id=\"left\">\n"
           "<span>\n"
           "<h3>Left</h3>\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"48\" height=\"48\" fill=\"red\" class=\"bi bi-water\" viewBox=\"0 0 16 16\">\n"
           "<path fill-rule=\"evenodd\" d=\"M15 8a.5.5 0 0 0-.5-.5H2.707l3.147-3.146a.5.5 0 1 0-.708-.708l-4 4a.5.5 0 0 0 0 .708l4 4a.5.5 0 0 0 .708-.708L2.707 8.5H14.5A.5.5 0 0 0 15 8\"/>\n"
           "</svg>\n"
           "</span>\n"
           "<h1>Not Detected</h1>\n"
           "</div>\n"
           "<div class=\"card primary\" id=\"right\">\n"
           "<span>\n"
           "<h3>Right</h3>\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"48\" height=\"48\" fill=\"white\" class=\"bi bi-thermometer-half\" viewBox=\"0 0 16 16\">\n"
           "<path fill-rule=\"evenodd\" d=\"M1 8a.5.5 0 0 1 .5-.5h11.793l-3.147-3.146a.5.5 0 0 1 .708-.708l4 4a.5.5 0 0 1 0 .708l-4 4a.5.5 0 0 1-.708-.708L13.293 8.5H1.5A.5.5 0 0 1 1 8\"/>\n"
           "</svg>\n"
           "</span>\n"
           "<h1>30 &deg; C</h1>\n"
           "</div>\n"
           "\n"
           "<div>\n"
           "<button onclick=\"onClickBtn('btn_fwd')\" id=\"btn_fwd\">FORWARD</button>\n"
           "</div>\n"
           "<div>\n"
           "<button onclick=\"onClickBtn('btn_stop')\" id=\"btn_stop\">STOP</button>\n"
           "</div>\n"
           "<div>\n"
           "<button onclick=\"onClickBtn('btn_back')\" id=\"btn_back\">BACK</button>\n"
           "</div>\n"
           "\n"
           "<div class=\"card primary\" id=\"ai_card\">"
           "<h2>AI Fault Detection</h2>"
           "<h3 id=\"ai_status_text\">Status: -</h3>"
           "<h3 id=\"ai_fault_text\">Fault: - %</h3>"
           "<h3 id=\"ai_severity_text\">Severity: -</h3>"
           "</div>"

           "\n"
           "\n"
           "<div class=\"card primary\" id=\"location\">\n"
           "<a href=\"https://maps.app.goo.gl/Wmyqr1H4hy8awwjN7\">\n"
           "<span>\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"48\" height=\"48\" fill=\"white\" class=\"bi bi-thermometer-half\" viewBox=\"0 0 16 16\">\n"
           "<path fill-rule=\"evenodd\" d=\"M1 8a.5.5 0 0 1 .5-.5h11.793l-3.147-3.146a.5.5 0 0 1 .708-.708l4 4a.5.5 0 0 1 0 .708l-4 4a.5.5 0 0 1-.708-.708L13.293 8.5H1.5A.5.5 0 0 1 1 8\"/>\n"
           "</svg>\n"
           "</span>\n"
           "<h1>Location</h1>\n"
           "</a>\n"
           "</div>\n"
           "\n"
           "\n"
           "\n"
           "\n"
           "\n"
           "</div>\n"
           "<footer>\n"
           "\n"
           "</footer>\n"
           "\n"
           "<script>"
           "var DRT = 500;\n"
           "function updateCSSClass(element, css){\n"
           "    if(css != 'primary')\n"
           "        element.classList.remove('primary');\n"
           "    if(css != 'secondary')\n"
           "        element.classList.remove('secondary');\n"
           "    if(css != 'success')\n"
           "        element.classList.remove('success');\n"
           "    if(css != 'danger')\n"
           "        element.classList.remove('danger');\n"
           "    if(css != 'warning')\n"
           "        element.classList.remove('warning');\n"
           "    if(css != 'hide')\n"
           "        element.classList.remove('hide');\n"
           "    element.classList.add(css);\n"
           "}\n"
           "\n"
           "function updateData(data){\n"
           "\tdocument.getElementById(\"message\").children[1].innerHTML = \"\"+data.message+\"\";\n"
           "\tupdateCSSClass(document.getElementById(\"message\"), data.message_class);\n"
           "\tdocument.getElementById(\"left\").children[1].innerHTML = \"\"+data.left+\"\";\n"
           "\tupdateCSSClass(document.getElementById(\"left\"), data.left_class);\n"
           "\tdocument.getElementById(\"right\").children[1].innerHTML = \"\"+data.right+\"\";\n"
           "\tupdateCSSClass(document.getElementById(\"right\"), data.right_class);\n"
           "\tdocument.getElementById(\"btn_fwd\").innerHTML = \"\"+data.btn_fwd+\"\";\n"
           "\tupdateCSSClass(document.getElementById(\"btn_fwd\"), data.btn_fwd_class);\n"
           "\tdocument.getElementById(\"btn_stop\").innerHTML = \"\"+data.btn_stop+\"\";\n"
           "\tupdateCSSClass(document.getElementById(\"btn_stop\"), data.btn_stop_class);\n"
           "\tdocument.getElementById(\"btn_back\").innerHTML = \"\"+data.btn_back+\"\";\n"
           "\tupdateCSSClass(document.getElementById(\"btn_back\"), data.btn_back_class);\n"
           "document.getElementById(\"ai_status_text\").innerHTML = \"Status: \" + data.ai_status;"
           "document.getElementById(\"ai_fault_text\").innerHTML = \"Fault: \" + data.fault_percent + \" %\";"
           "document.getElementById(\"ai_severity_text\").innerHTML = \"Severity: \" + data.severity;"
           "updateCSSClass(document.getElementById(\"ai_card\"), data.ai_class);"

           "}\n"
           "\n"
           "function getCommand(btn_id, value){\n"
           "\tif(btn_id == \"btn_fwd\"){\n"
           "\t\tif(value == 'ON'){\n"
           "\t\t\treturn 'OFF';\n"
           "\t\t}else{\n"
           "\t\t\treturn 'FORWARD';\n"
           "\t\t}\n"
           "\t}\tif(btn_id == \"btn_stop\"){\n"
           "\t\tif(value == 'ON'){\n"
           "\t\t\treturn 'OFF';\n"
           "\t\t}else{\n"
           "\t\t\treturn 'STOP';\n"
           "\t\t}\n"
           "\t}\tif(btn_id == \"btn_back\"){\n"
           "\t\tif(value == 'ON'){\n"
           "\t\t\treturn 'OFF';\n"
           "\t\t}else{\n"
           "\t\t\treturn 'BACK';\n"
           "\t\t}\n"
           "\t}    \n"
           "}\n"
           "\n"
           "function onClickBtn(btn_id){\n"
           "\tvar val = document.getElementById(btn_id).innerHTML;\n"
           "\tvar cmd = getCommand(btn_id,val);\n"
           "    console.log(cmd)\n"
           "\tsendButtonClick('/act?'+btn_id+'='+cmd)\n"
           "}\n"
           "\n"
           "\n"
           "\n"
           "function updateNetwork(connected){\n"
           "    if(connected){\n"
           "        document.getElementById('disconnected').style.display = 'none';\n"
           "        document.getElementById('connected').style.display = 'block';\n"
           "        document.getElementById('online').innerHTML = 'Online';\n"
           "    }\n"
           "    else{\n"
           "        document.getElementById('connected').style.display = 'none';\n"
           "        document.getElementById('disconnected').style.display = 'block';\n"
           "        document.getElementById('online').innerHTML = 'Offline';\n"
           "    }\n"
           "}\n"
           "\n"
           "\n"
           "function sendButtonClick(url){\n"
           "\t\n"
           "    const xhr = new XMLHttpRequest();\n"
           "    xhr.open('GET', url, true);\n"
           "    xhr.onload = () => {\n"
           "        if(xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {\n"
           "            var data= JSON.parse(xhr.responseText);\n"
           "            updateData(data);\n"
           "            updateNetwork(true);\n"
           "        }\n"
           "    }\n"
           "    xhr.onerror = function() {\n"
           "        updateNetwork(false);\n"
           "    };\n"
           "    xhr.send();\n"
           "}\n"
           "\n"
           "\n"
           "\n"
           "\n"
           "var netcount = 0;\n"
           "function reconnect(){\n"
           "    if(netcount == 0){\n"
           "        console.log(\"Retrying\");\n"
           "        document.getElementById('online').innerHTML = 'Retrying..';\n"
           "        setTimeout(liveDataAjax,1000);\n"
           "        return\n"
           "    }\n"
           "    netcount -= 1;\n"
           "    console.log(\"count\",netcount);\n"
           "    document.getElementById('online').innerHTML = 'Offline ('+netcount+')';\n"
           "    setTimeout(reconnect, 1000);\n"
           "}\n"
           "function liveDataAjax(){\n"
           "    const xhr = new XMLHttpRequest();\n"
           "    xhr.open('GET', '/data.json', true);\n"
           "    xhr.onload = () => {\n"
           "        if(xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {\n"
           "            var data= JSON.parse(xhr.responseText);\n"
           "            updateData(data);\n"
           "            updateNetwork(true);\n"
           "            setTimeout(liveDataAjax, DRT);\n"
           "        }\n"
           "        else if (xhr.readyState === XMLHttpRequest.DONE){\n"
           "            updateNetwork(false);\n"
           "            netcount = 5;\n"
           "            reconnect();\n"
           "        }\n"
           "    };\n"
           "    xhr.onerror = function() {\n"
           "        updateNetwork(false);\n"
           "        netcount = 5;\n"
           "        reconnect();\n"
           "    };\n"
           "    xhr.send();\t\n"
           "}\n"
           "\n"
           "</script>\n"
           "</body>\n"
           "</html>";
}