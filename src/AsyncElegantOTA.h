#ifndef AsyncElegantOTA_h
#define AsyncElegantOTA_h

#include "Arduino.h"
#include "stdlib_noniso.h"
#include "ESPAsyncWebServer.h"


class AsyncElegantOtaClass{

    public:
        void setID(const char* id);
        void begin(AsyncWebServer *server, std::function<void(String)> startUpdateFct = nullptr, const char* username = "", const char* password = "");
        void restart();
            
        void signalStartUpdate(const String& filename);
        
        static String getVersion();
        static String getCompileTime();
        static String getCompileDate();
        static String getCppFileTimeStamp();
        static String getHeaderFileTimeStamp() {return __TIMESTAMP__;};

    private:
        AsyncWebServer *_server;

        String getID();
        String getProcessorDependentID();
        bool beginProcessorDependentUpdate(const String& filename);

        String _id = getID();
        String _username = "";
        String _password = "";
        bool _authRequired = false;
        std::function<void(String)> _startUpdateFct = nullptr;

        static const String ProcessorType;

};

extern AsyncElegantOtaClass AsyncElegantOTA;

#endif
