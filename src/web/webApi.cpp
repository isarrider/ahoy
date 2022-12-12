//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "webApi.h"

//-----------------------------------------------------------------------------
webApi::webApi(AsyncWebServer *srv, app *app, settings_t *config, statistics_t *stat, char version[]) {
    mSrv = srv;
    mApp = app;
    mConfig  = config;
    mStat    = stat;
    mVersion = version;

    mTimezoneOffset = 0;
}

//-----------------------------------------------------------------------------
void webApi::setup(void) {
    mSrv->on("/api", HTTP_GET,  std::bind(&webApi::onApi,         this, std::placeholders::_1));
    mSrv->on("/api", HTTP_POST, std::bind(&webApi::onApiPost,     this, std::placeholders::_1)).onBody(
                                std::bind(&webApi::onApiPostBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

    mSrv->on("/get_setup", HTTP_GET,  std::bind(&webApi::onDwnldSetup, this, std::placeholders::_1));
}

//-----------------------------------------------------------------------------
void webApi::loop(void) {
}

//-----------------------------------------------------------------------------
void webApi::ctrlRequest(JsonObject obj) {
    /*char out[128];
    serializeJson(obj, out, 128);
    DPRINTLN(DBG_INFO, "webApi: " + String(out));*/
    DynamicJsonDocument json(128);
    JsonObject dummy = json.to<JsonObject>();
    if(obj[F("path")] == "ctrl")
        setCtrl(obj, dummy);
    else if(obj[F("path")] == "setup")
        setSetup(obj, dummy);
}


//-----------------------------------------------------------------------------
void webApi::onApi(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 8192);
    JsonObject root = response->getRoot();

    Inverter<> *iv = mApp->mSys->getInverterByPos(0, false);
    String path = request->url().substring(5);
    if(path == "html/system")         getHtmlSystem(root);
    else if(path == "html/logout")    getHtmlLogout(root);
    else if(path == "html/save")      getHtmlSave(root);
    else if(path == "system")         getSysInfo(root);
    else if(path == "reboot")         getReboot(root);
    else if(path == "statistics")     getStatistics(root);
    else if(path == "inverter/list")  getInverterList(root);
    else if(path == "menu")           getMenu(root);
    else if(path == "index")          getIndex(root);
    else if(path == "setup")          getSetup(root);
    else if(path == "setup/networks") getNetworks(root);
    else if(path == "live")           getLive(root);
    else if(path == "record/info")    getRecord(root, iv->getRecordStruct(InverterDevInform_All));
    else if(path == "record/alarm")   getRecord(root, iv->getRecordStruct(AlarmData));
    else if(path == "record/config")  getRecord(root, iv->getRecordStruct(SystemConfigPara));
    else if(path == "record/live")    getRecord(root, iv->getRecordStruct(RealTimeRunData_Debug));
    else
        getNotFound(root, F("http://") + request->host() + F("/api/"));

    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Headers", "content-type");
    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onApiPost(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, "onApiPost");
}


//-----------------------------------------------------------------------------
void webApi::onApiPostBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    DPRINTLN(DBG_VERBOSE, "onApiPostBody");
    DynamicJsonDocument json(200);
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 200);
    JsonObject root = response->getRoot();

    DeserializationError err = deserializeJson(json, (const char *)data, len);
    JsonObject obj = json.as<JsonObject>();
    root[F("success")] = (err) ? false : true;
    if(!err) {
        String path = request->url().substring(5);
        if(path == "ctrl")
            root[F("success")] = setCtrl(obj, root);
        else if(path == "setup")
            root[F("success")] = setSetup(obj, root);
        else {
            root[F("success")] = false;
            root[F("error")]   = "Path not found: " + path;
        }
    }
    else {
        switch (err.code()) {
            case DeserializationError::Ok: break;
            case DeserializationError::InvalidInput: root[F("error")] = F("Invalid input");          break;
            case DeserializationError::NoMemory:     root[F("error")] = F("Not enough memory");      break;
            default:                                 root[F("error")] = F("Deserialization failed"); break;
        }
    }

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::getNotFound(JsonObject obj, String url) {
    JsonObject ep = obj.createNestedObject("avail_endpoints");
    ep[F("system")]        = url + F("system");
    ep[F("statistics")]    = url + F("statistics");
    ep[F("inverter/list")] = url + F("inverter/list");
    ep[F("index")]         = url + F("index");
    ep[F("setup")]         = url + F("setup");
    ep[F("live")]          = url + F("live");
    ep[F("record/info")]   = url + F("record/info");
    ep[F("record/alarm")]  = url + F("record/alarm");
    ep[F("record/config")] = url + F("record/config");
    ep[F("record/live")]   = url + F("record/live");
}


//-----------------------------------------------------------------------------
void webApi::onDwnldSetup(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 8192);
    JsonObject root = response->getRoot();

    getSetup(root);

    response->setLength();
    response->addHeader("Content-Type", "application/octet-stream");
    response->addHeader("Content-Description", "File Transfer");
    response->addHeader("Content-Disposition", "attachment; filename=ahoy_setup.json");
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::getSysInfo(JsonObject obj) {
    obj[F("ssid")]         = mConfig->sys.stationSsid;
    obj[F("device_name")]  = mConfig->sys.deviceName;
    obj[F("version")]      = String(mVersion);
    obj[F("build")]        = String(AUTO_GIT_HASH);
    obj[F("ts_uptime")]    = mApp->getUptime();
    obj[F("ts_now")]       = mApp->getTimestamp();
    obj[F("ts_sunrise")]   = mApp->getSunrise();
    obj[F("ts_sunset")]    = mApp->getSunset();
    obj[F("wifi_rssi")]    = WiFi.RSSI();
    obj[F("mac")]          = WiFi.macAddress();
    obj[F("hostname")]     = WiFi.getHostname();
    obj[F("pwd_set")]      = (strlen(mConfig->sys.adminPwd) > 0);

    obj[F("sdk")]          = ESP.getSdkVersion();
    obj[F("cpu_freq")]     = ESP.getCpuFreqMHz();
    obj[F("heap_free")]    = ESP.getFreeHeap();
    obj[F("sketch_total")] = ESP.getFreeSketchSpace();
    obj[F("sketch_used")]  = ESP.getSketchSize() / 1024; // in kb


    getRadio(obj.createNestedObject(F("radio")));

#if defined(ESP32)
    obj[F("heap_total")]    = ESP.getHeapSize();
    obj[F("chip_revision")] = ESP.getChipRevision();
    obj[F("chip_model")]    = ESP.getChipModel();
    obj[F("chip_cores")]    = ESP.getChipCores();
    //obj[F("core_version")]  = F("n/a");
    //obj[F("flash_size")]    = F("n/a");
    //obj[F("heap_frag")]     = F("n/a");
    //obj[F("max_free_blk")]  = F("n/a");
    //obj[F("reboot_reason")] = F("n/a");
#else
    //obj[F("heap_total")]    = F("n/a");
    //obj[F("chip_revision")] = F("n/a");
    //obj[F("chip_model")]    = F("n/a");
    //obj[F("chip_cores")]    = F("n/a");
    obj[F("core_version")]  = ESP.getCoreVersion();
    obj[F("flash_size")]    = ESP.getFlashChipRealSize() / 1024; // in kb
    obj[F("heap_frag")]     = ESP.getHeapFragmentation();
    obj[F("max_free_blk")]  = ESP.getMaxFreeBlockSize();
    obj[F("reboot_reason")] = ESP.getResetReason();
#endif
    //obj[F("littlefs_total")] = LittleFS.totalBytes();
    //obj[F("littlefs_used")] = LittleFS.usedBytes();

#if defined(ESP32)
    obj[F("esp_type")]    = F("ESP32");
#else
    obj[F("esp_type")]    = F("ESP8266");
#endif
}


//-----------------------------------------------------------------------------
void webApi::getHtmlSystem(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("html")] = F("<a href=\"/factory\" class=\"btn\">Factory Reset</a><br/><br/><a href=\"/reboot\" class=\"btn\">Reboot</a>");
}


//-----------------------------------------------------------------------------
void webApi::getHtmlLogout(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("refresh")] = 3;
    obj[F("refresh_url")] = "/";
    obj[F("html")] = F("succesfully logged out");
}


//-----------------------------------------------------------------------------
void webApi::getHtmlSave(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("refresh")] = 2;
    obj[F("refresh_url")] = "/setup";
    obj[F("html")] = F("settings succesfully save");
}


//-----------------------------------------------------------------------------
void webApi::getReboot(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("refresh")] = 10;
    obj[F("refresh_url")] = "/";
    obj[F("html")] = F("reboot. Autoreload after 10 seconds");
}


//-----------------------------------------------------------------------------
void webApi::getStatistics(JsonObject obj) {
    obj[F("rx_success")]     = mStat->rxSuccess;
    obj[F("rx_fail")]        = mStat->rxFail;
    obj[F("rx_fail_answer")] = mStat->rxFailNoAnser;
    obj[F("frame_cnt")]      = mStat->frmCnt;
    obj[F("tx_cnt")]         = mApp->mSys->Radio.mSendCnt;
}


//-----------------------------------------------------------------------------
void webApi::getInverterList(JsonObject obj) {
    JsonArray invArr = obj.createNestedArray(F("inverter"));

    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            JsonObject obj2 = invArr.createNestedObject();
            obj2[F("id")]       = i;
            obj2[F("name")]     = String(iv->config->name);
            obj2[F("serial")]   = String(iv->config->serial.u64, HEX);
            obj2[F("channels")] = iv->channels;
            obj2[F("version")]  = String(iv->fwVersion);

            for(uint8_t j = 0; j < iv->channels; j ++) {
                obj2[F("ch_max_power")][j] = iv->config->chMaxPwr[j];
                obj2[F("ch_name")][j] = iv->config->chName[j];
            }
        }
    }
    obj[F("interval")]          = String(mConfig->nrf.sendInterval);
    obj[F("retries")]           = String(mConfig->nrf.maxRetransPerPyld);
    obj[F("max_num_inverters")] = MAX_NUM_INVERTERS;
}


//-----------------------------------------------------------------------------
void webApi::getMqtt(JsonObject obj) {
    obj[F("broker")] = String(mConfig->mqtt.broker);
    obj[F("port")]   = String(mConfig->mqtt.port);
    obj[F("user")]   = String(mConfig->mqtt.user);
    obj[F("pwd")]    = (strlen(mConfig->mqtt.pwd) > 0) ? F("{PWD}") : String("");
    obj[F("topic")]  = String(mConfig->mqtt.topic);
}


//-----------------------------------------------------------------------------
void webApi::getNtp(JsonObject obj) {
    obj[F("addr")] = String(mConfig->ntp.addr);
    obj[F("port")] = String(mConfig->ntp.port);
}

//-----------------------------------------------------------------------------
void webApi::getSun(JsonObject obj) {
    obj[F("lat")] = mConfig->sun.lat ? String(mConfig->sun.lat, 5) : "";
    obj[F("lon")] = mConfig->sun.lat ? String(mConfig->sun.lon, 5) : "";
    obj[F("disnightcom")] = mConfig->sun.disNightCom;
}


//-----------------------------------------------------------------------------
void webApi::getPinout(JsonObject obj) {
    obj[F("cs")]  = mConfig->nrf.pinCs;
    obj[F("ce")]  = mConfig->nrf.pinCe;
    obj[F("irq")] = mConfig->nrf.pinIrq;
    obj[F("led0")] = mConfig->led.led0;
    obj[F("led1")] = mConfig->led.led1;
}


//-----------------------------------------------------------------------------
void webApi::getRadio(JsonObject obj) {
    obj[F("power_level")] = mConfig->nrf.amplifierPower;
    obj[F("isconnected")] = mApp->mSys->Radio.isChipConnected();
    obj[F("DataRate")] = mApp->mSys->Radio.getDataRate();
    obj[F("isPVariant")] = mApp->mSys->Radio.isPVariant();
}


//-----------------------------------------------------------------------------
void webApi::getSerial(JsonObject obj) {
    obj[F("interval")]       = (uint16_t)mConfig->serial.interval;
    obj[F("show_live_data")] = mConfig->serial.showIv;
    obj[F("debug")]          = mConfig->serial.debug;
}


//-----------------------------------------------------------------------------
void webApi::getStaticIp(JsonObject obj) {
    char buf[16];
    ah::ip2Char(mConfig->sys.ip.ip, buf);      obj[F("ip")]      = String(buf);
    ah::ip2Char(mConfig->sys.ip.mask, buf);    obj[F("mask")]    = String(buf);
    ah::ip2Char(mConfig->sys.ip.dns1, buf);    obj[F("dns1")]    = String(buf);
    ah::ip2Char(mConfig->sys.ip.dns2, buf);    obj[F("dns2")]    = String(buf);
    ah::ip2Char(mConfig->sys.ip.gateway, buf); obj[F("gateway")] = String(buf);
}


//-----------------------------------------------------------------------------
void webApi::getMenu(JsonObject obj) {
    obj["name"][0] = "Live";
    obj["link"][0] = "/live";
    obj["name"][1] = "Serial / Control";
    obj["link"][1] = "/serial";
    obj["name"][2] = "Settings";
    obj["link"][2] = "/setup";
    obj["name"][3] = "-";
    obj["name"][4] = "REST API";
    obj["link"][4] = "/api";
    obj["trgt"][4] = "_blank";
    obj["name"][5] = "-";
    obj["name"][6] = "Update";
    obj["link"][6] = "/update";
    obj["name"][7] = "System";
    obj["link"][7] = "/system";
    obj["name"][8] = "-";
    obj["name"][9] = "Documentation";
    obj["link"][9] = "https://ahoydtu.de";
    obj["trgt"][9] = "_blank";
    if(strlen(mConfig->sys.adminPwd) > 0) {
        obj["name"][10] = "-";
        obj["name"][11] = "Logout";
        obj["link"][11] = "/logout";
    }
}


//-----------------------------------------------------------------------------
void webApi::getIndex(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    getRadio(obj.createNestedObject(F("radio")));
    getStatistics(obj.createNestedObject(F("statistics")));
    obj["refresh_interval"] = mConfig->nrf.sendInterval;

    JsonArray inv = obj.createNestedArray(F("inverter"));
    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            JsonObject invObj = inv.createNestedObject();
            invObj[F("id")]              = i;
            invObj[F("name")]            = String(iv->config->name);
            invObj[F("version")]         = String(iv->fwVersion);
            invObj[F("is_avail")]        = iv->isAvailable(mApp->getTimestamp(), rec);
            invObj[F("is_producing")]    = iv->isProducing(mApp->getTimestamp(), rec);
            invObj[F("ts_last_success")] = iv->getLastTs(rec);
        }
    }

    JsonArray warn = obj.createNestedArray(F("warnings"));
    if(!mApp->mSys->Radio.isChipConnected())
        warn.add(F("your NRF24 module can't be reached, check the wiring and pinout"));
    else if(!mApp->mSys->Radio.isPVariant())
        warn.add(F("your NRF24 module have not a plus(+), please check!"));

    if((!mApp->mqttIsConnected()) && (String(mConfig->mqtt.broker).length() > 0))
        warn.add(F("MQTT is not connected"));

    JsonArray info = obj.createNestedArray(F("infos"));
    if(mApp->getRebootRequestState())
        info.add(F("reboot your ESP to apply all your configuration changes!"));
    if(!mApp->getSettingsValid())
        info.add(F("your settings are invalid"));
    if(mApp->mqttIsConnected())
        info.add(F("MQTT is connected, ") + String(mApp->getMqttTxCnt()) + F(" packets sent"));
}


//-----------------------------------------------------------------------------
void webApi::getSetup(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    getInverterList(obj.createNestedObject(F("inverter")));
    getMqtt(obj.createNestedObject(F("mqtt")));
    getNtp(obj.createNestedObject(F("ntp")));
    getSun(obj.createNestedObject(F("sun")));
    getPinout(obj.createNestedObject(F("pinout")));
    getRadio(obj.createNestedObject(F("radio")));
    getSerial(obj.createNestedObject(F("serial")));
    getStaticIp(obj.createNestedObject(F("static_ip")));
}


//-----------------------------------------------------------------------------
void webApi::getNetworks(JsonObject obj) {
    mApp->getAvailNetworks(obj);
}


//-----------------------------------------------------------------------------
void webApi::getLive(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    JsonArray invArr = obj.createNestedArray(F("inverter"));
    obj["refresh_interval"] = mConfig->nrf.sendInterval;

    uint8_t list[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PF, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_Q};

    Inverter<> *iv;
    uint8_t pos;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            JsonObject obj2 = invArr.createNestedObject();
            obj2[F("name")]               = String(iv->config->name);
            obj2[F("channels")]           = iv->channels;
            obj2[F("power_limit_read")]   = ah::round3(iv->actPowerLimit);
            obj2[F("last_alarm")]         = String(iv->lastAlarmMsg);
            obj2[F("ts_last_success")]    = rec->ts;

            JsonArray ch = obj2.createNestedArray("ch");
            JsonArray ch0 = ch.createNestedArray();
            obj2[F("ch_names")][0] = "AC";
            for (uint8_t fld = 0; fld < sizeof(list); fld++) {
                pos = (iv->getPosByChFld(CH0, list[fld], rec));
                ch0[fld] = (0xff != pos) ? ah::round3(iv->getValue(pos, rec)) : 0.0;
                obj[F("ch0_fld_units")][fld] = (0xff != pos) ? String(iv->getUnit(pos, rec)) : notAvail;
                obj[F("ch0_fld_names")][fld] = (0xff != pos) ? String(iv->getFieldName(pos, rec)) : notAvail;
            }

            for(uint8_t j = 1; j <= iv->channels; j ++) {
                obj2[F("ch_names")][j] = String(iv->config->chName[j-1]);
                JsonArray cur = ch.createNestedArray();
                for (uint8_t k = 0; k < 6; k++) {
                    switch(k) {
                        default: pos = (iv->getPosByChFld(j, FLD_UDC, rec)); break;
                        case 1:  pos = (iv->getPosByChFld(j, FLD_IDC, rec)); break;
                        case 2:  pos = (iv->getPosByChFld(j, FLD_PDC, rec)); break;
                        case 3:  pos = (iv->getPosByChFld(j, FLD_YD, rec));  break;
                        case 4:  pos = (iv->getPosByChFld(j, FLD_YT, rec));  break;
                        case 5:  pos = (iv->getPosByChFld(j, FLD_IRR, rec)); break;
                    }
                    cur[k] = (0xff != pos) ? ah::round3(iv->getValue(pos, rec)) : 0.0;
                    if(1 == j) {
                        obj[F("fld_units")][k] = (0xff != pos) ? String(iv->getUnit(pos, rec)) : notAvail;
                        obj[F("fld_names")][k] = (0xff != pos) ? String(iv->getFieldName(pos, rec)) : notAvail;
                    }
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
void webApi::getRecord(JsonObject obj, record_t<> *rec) {
    JsonArray invArr = obj.createNestedArray(F("inverter"));

    Inverter<> *iv;
    uint8_t pos;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            JsonArray obj2 = invArr.createNestedArray();
            for(uint8_t j = 0; j < rec->length; j++) {
                byteAssign_t *assign = iv->getByteAssign(j, rec);
                pos = (iv->getPosByChFld(assign->ch, assign->fieldId, rec));
                obj2[j]["fld"]  = (0xff != pos) ? String(iv->getFieldName(pos, rec)) : notAvail;
                obj2[j]["unit"] = (0xff != pos) ? String(iv->getUnit(pos, rec)) : notAvail;
                obj2[j]["val"]  = (0xff != pos) ? String(iv->getValue(pos, rec)) : notAvail;
            }
        }
    }
}


//-----------------------------------------------------------------------------
bool webApi::setCtrl(JsonObject jsonIn, JsonObject jsonOut) {
    Inverter<> *iv = mApp->mSys->getInverterByPos(jsonIn[F("id")]);
    if(NULL == iv) {
        jsonOut[F("error")] = F("inverter index invalid: ") + jsonIn[F("id")].as<String>();
        return false;
    }

    if(F("power") == jsonIn[F("cmd")]) {
        iv->devControlCmd = (jsonIn[F("val")] == 1) ? TurnOn : TurnOff;
        iv->devControlRequest = true;
    } else if(F("restart") == jsonIn[F("restart")]) {
        iv->devControlCmd = Restart;
        iv->devControlRequest = true;
    }
    else if(0 == strncmp("limit_", jsonIn[F("cmd")].as<const char*>(), 6)) {
        iv->powerLimit[0] = jsonIn["val"];
        if(F("limit_persistent_relative") == jsonIn[F("cmd")])
            iv->powerLimit[1] = RelativPersistent;
        else if(F("limit_persistent_absolute") == jsonIn[F("cmd")])
            iv->powerLimit[1] = AbsolutPersistent;
        else if(F("limit_nonpersistent_relative") == jsonIn[F("cmd")])
            iv->powerLimit[1] = RelativNonPersistent;
        else if(F("limit_nonpersistent_absolute") == jsonIn[F("cmd")])
            iv->powerLimit[1] = AbsolutNonPersistent;
        iv->devControlCmd = ActivePowerContr;
        iv->devControlRequest = true;
    }
    else {
        jsonOut[F("error")] = F("unknown cmd: '") + jsonIn["cmd"].as<String>() + "'";
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool webApi::setSetup(JsonObject jsonIn, JsonObject jsonOut) {
    if(F("scan_wifi") == jsonIn[F("cmd")])
        mApp->scanAvailNetworks();
    else if(F("set_time") == jsonIn[F("cmd")])
        mApp->setTimestamp(jsonIn[F("val")]);
    else if(F("sync_ntp") == jsonIn[F("cmd")])
        mApp->setTimestamp(0); // 0: update ntp flag
    else if(F("serial_utc_offset") == jsonIn[F("cmd")])
        mTimezoneOffset = jsonIn[F("val")];
    else if(F("discovery_cfg") == jsonIn[F("cmd")])
        mApp->mFlagSendDiscoveryConfig = true; // for homeassistant
    else {
        jsonOut[F("error")] = F("unknown cmd");
        return false;
    }

    return true;
}