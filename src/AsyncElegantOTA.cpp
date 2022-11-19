#include <AsyncElegantOTA.h>


#if defined(ESP8266)
    #include "ESP8266WiFi.h"
    #include "ESPAsyncTCP.h"
    #include "flash_hal.h"
    #include "FS.h"
#elif defined(ESP32)
    #include "WiFi.h"
    #include "AsyncTCP.h"
    #include "Update.h"
    #include "esp_int_wdt.h"
    #include "esp_task_wdt.h"
#endif

#include "Hash.h"
#include "FS.h"

#include "elegantWebpage.h"

AsyncElegantOtaClass AsyncElegantOTA;

void AsyncElegantOtaClass::setID(const char* id){
    _id = id;
}

bool AsyncElegantOtaClass::loginValid(AsyncWebServerRequest *request) {
    if (_authRequired) {
        if (!request->authenticate(_username.c_str(), _password.c_str())) {
            request->requestAuthentication();
            return false;
        }
    }

    return true;
}

void AsyncElegantOtaClass::subscribeMethodsOnServer() {
    this->subscribeGetIdentityOnServer();
    this->subscribeGetUpdateOnServer();
    this->subscribePosttUpdateOnServer();
}

void AsyncElegantOtaClass::subscribeGetIdentityOnServer() {
    _server->on("/update/identity", HTTP_GET, [&](AsyncWebServerRequest *request){this->performIdentityRequest(request);});
}

void AsyncElegantOtaClass::performIdentityRequest(AsyncWebServerRequest *request) {
    if (this->loginValid(request)) {
        request->send(200, "application/json", "{\"id\": \""+_id+"\", \"hardware\": \""+ProcessorType+"\"}");
    }
}

void AsyncElegantOtaClass::subscribeGetUpdateOnServer() {
    _server->on("/update", HTTP_GET, [&](AsyncWebServerRequest *request){this->performGetUpdateRequest(request);});
}

void AsyncElegantOtaClass::performGetUpdateRequest(AsyncWebServerRequest *request) {
    if (this->loginValid(request)) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", ELEGANT_HTML, ELEGANT_HTML_SIZE);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }
}

void AsyncElegantOtaClass::subscribePosttUpdateOnServer() {
    _server->on(
        "/update", HTTP_POST, 
        [&](AsyncWebServerRequest *request) {this->performPostUpdateRequest(request);},
        [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            this->performPostUpdateUpload(request, filename, index, data, len, final);
        }
    );
}

void AsyncElegantOtaClass::performPostUpdateRequest(AsyncWebServerRequest *request) {
    if (this->loginValid(request)) {
        // the request handler is triggered after the upload has finished... 
        // create the response, add header, and send response
        AsyncWebServerResponse *response = request->beginResponse((Update.hasError())?500:200, "text/plain", (Update.hasError())?"FAIL":"OK");
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
        restart();
    }
}

void AsyncElegantOtaClass::performPostUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (this->loginValid(request)) {
        if (!index) {
            if (!request->hasParam("MD5", true)) {
                return request->send(400, "text/plain", "MD5 parameter missing");
            }

            if (!Update.setMD5(request->getParam("MD5", true)->value().c_str())) {
                return request->send(400, "text/plain", "MD5 parameter invalid");
            }

            if (!this->beginProcessorDependentUpdate(filename)) {
                Update.printError(Serial);
                return request->send(400, "text/plain", "OTA could not begin");
            } else {
                this->signalStartUpdate(filename);
            }
        }

        // Write chunked data to the free sketch space
        if (len) {
            if (Update.write(data, len) != len) {
                return request->send(400, "text/plain", "OTA could not begin");
            }
        }
            
        if (final) { // if the final flag is set then this is the last frame of data
            if (!Update.end(true)) { //true to set the size to the current progress
                Update.printError(Serial);
                return request->send(400, "text/plain", "Could not end OTA");
            }
        } else {
            return;
        }
    }
}

void AsyncElegantOtaClass::begin(AsyncWebServer *server, std::function<void(String)> startUpdateFct, const char* username, const char* password){
    _server = server;
    this->initializeLoginData(username, password);
    this->subscribeMethodsOnServer();

    // _server->on("/update/identity", HTTP_GET, [&](AsyncWebServerRequest *request){
    //     if(_authRequired){
    //         if(!request->authenticate(_username.c_str(), _password.c_str())){
    //             return request->requestAuthentication();
    //         }
    //     }
    //     request->send(200, "application/json", "{\"id\": \""+_id+"\", \"hardware\": \""+ProcessorType+"\"}");
    // });

    // _server->on("/update", HTTP_GET, [&](AsyncWebServerRequest *request){
    //     if(_authRequired){
    //         if(!request->authenticate(_username.c_str(), _password.c_str())){
    //             return request->requestAuthentication();
    //         }
    //     }
    //     AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", ELEGANT_HTML, ELEGANT_HTML_SIZE);
    //     response->addHeader("Content-Encoding", "gzip");
    //     request->send(response);
    // });

    // _server->on("/update", HTTP_POST, [&](AsyncWebServerRequest *request) {
    //     if(_authRequired){
    //         if(!request->authenticate(_username.c_str(), _password.c_str())){
    //             return request->requestAuthentication();
    //         }
    //     }
    //     // the request handler is triggered after the upload has finished... 
    //     // create the response, add header, and send response
    //     AsyncWebServerResponse *response = request->beginResponse((Update.hasError())?500:200, "text/plain", (Update.hasError())?"FAIL":"OK");
    //     response->addHeader("Connection", "close");
    //     response->addHeader("Access-Control-Allow-Origin", "*");
    //     request->send(response);
    //     restart();
    // }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    //     //Upload handler chunks in data
    //     if(_authRequired){
    //         if(!request->authenticate(_username.c_str(), _password.c_str())){
    //             return request->requestAuthentication();
    //         }
    //     }

    //     if (!index) {
    //         if(!request->hasParam("MD5", true)) {
    //             return request->send(400, "text/plain", "MD5 parameter missing");
    //         }

    //         if(!Update.setMD5(request->getParam("MD5", true)->value().c_str())) {
    //             return request->send(400, "text/plain", "MD5 parameter invalid");
    //         }

    //         if (!this->beginProcessorDependentUpdate(filename)) {
    //             Update.printError(Serial);
    //             return request->send(400, "text/plain", "OTA could not begin");
    //         } else {
    //             this->signalStartUpdate(filename);
    //         }
    //     }

    //     // Write chunked data to the free sketch space
    //     if (len) {
    //         if (Update.write(data, len) != len) {
    //             return request->send(400, "text/plain", "OTA could not begin");
    //         }
    //     }
            
    //     if (final) { // if the final flag is set then this is the last frame of data
    //         if (!Update.end(true)) { //true to set the size to the current progress
    //             Update.printError(Serial);
    //             return request->send(400, "text/plain", "Could not end OTA");
    //         }
    //     } else {
    //         return;
    //     }
    // });
}

void AsyncElegantOtaClass::signalStartUpdate(const String& filename) {
    if (_startUpdateFct != nullptr) {
        _startUpdateFct(filename);
    }
}

void AsyncElegantOtaClass::restart() {
    yield();
    delay(1000);
    yield();
    ESP.restart();
}

String AsyncElegantOtaClass::getID(){
    String id = getProcessorDependentID();
    id.toUpperCase();
    id += " / Version " + getVersion();
    return id;
}

String AsyncElegantOtaClass::getVersion() {
    return "1.0.6";
}

String AsyncElegantOtaClass::getCompileTime() {
    return __TIME__;
}

String AsyncElegantOtaClass::getCompileDate() {
    return __DATE__;
}

String AsyncElegantOtaClass::getCppFileTimeStamp() {
    return __TIMESTAMP__;
}

#if defined(ESP8266)

const String AsyncElegantOtaClass::ProcessorType = "ESP8266";

String AsyncElegantOtaClass::getProcessorDependentID() {
    return String(ESP.getChipId());
}

bool AsyncElegantOtaClass::beginProcessorDependentUpdate(const String& filename) {
    const int cmd = (filename == "filesystem") ? U_FS : U_FLASH;
    Update.runAsync(true);
    const size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
    const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    return Update.begin((cmd == U_FS) ? fsSize : maxSketchSpace, cmd); // Start with max available size
}

#elif defined(ESP32)

const String AsyncElegantOtaClass::ProcessorType = "ESP32";

String AsyncElegantOtaClass::getProcessorDependentID(){
    return String((uint32_t)ESP.getEfuseMac(), HEX);
}

bool AsyncElegantOtaClass::beginProcessorDependentUpdate(const String& filename) {
    const int cmd = (filename == "filesystem") ? U_SPIFFS : U_FLASH;
    return Update.begin(UPDATE_SIZE_UNKNOWN, cmd); // Start with max available size
}

void AsyncElegantOtaClass::initializeLoginData(const char* username, const char* password) {
    if (strlen(username) > 0) {
        _authRequired = true;
        _username = username;
        _password = password;
    } else {
        _authRequired = false;
        _username = "";
        _password = "";
    }
}

#endif
