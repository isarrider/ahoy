//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __AHOYWIFI_H__
#define __AHOYWIFI_H__

#include "../utils/dbg.h"
#include <Arduino.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include "ESPAsyncWebServer.h"

#include "../config/settings.h"

class app;

class ahoywifi {
    public:
        ahoywifi();

        void setup(settings_t *config, uint32_t *utcTimestamp);
        void loop(void);
        bool getNtpTime(void);
        void scanAvailNetworks(void);
        void getAvailNetworks(JsonObject obj);

    private:
        void setupAp(void);
        void setupStation(void);
        void sendNTPpacket(IPAddress& address);
        #if defined(ESP8266)
        void onConnect(const WiFiEventStationModeGotIP& event);
        void onDisconnect(const WiFiEventStationModeDisconnected& event);
        #else
        void onWiFiEvent(WiFiEvent_t event);
        #endif
        void welcome(String msg);

        settings_t *mConfig;

        DNSServer mDns;
        WiFiUDP mUdp; // for time server
        #if defined(ESP8266)
        WiFiEventHandler wifiConnectHandler;
        WiFiEventHandler wifiDisconnectHandler;
        #endif

        bool mConnected, mInitNtp;
        uint8_t mCnt;
        uint32_t *mUtcTimestamp;
};

#endif /*__AHOYWIFI_H__*/