#include "defs.h"
#define _WEBSOCKETS_LOGLEVEL_ 0   // 0-4
#define SOCKET_IO_PATH "/api/dev/io/" // do not change

bool _bSocketIOConnected = false;
bool _bInitialConnection = true;
static void (*cbSocketDataReceived)(String jsonStr) = NULL;
static void (*cbSocketConnection)(bool status) = NULL;

// #define RECONNECT_INTERVAL 10000
// #define PING_INTERVAL 3000
// #define PING_TIMEOUT 2000
// #define NO_PONG_COUNT_AS_DISCONNECTED 2
//-------------------------------------------------------------------------
// name  : initSocketIO
void initSocketIO(char *szDeviceNo, void (*ptr)(String jsonStr), void (*ptr2)(bool status))
{
    _ntpClient.begin();
    _socketIO.onEvent(onEvent);

    // try ever 10000 again if connection has failed
    _socketIO.setReconnectInterval(10000);
    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    //_socketIO.enableHeartbeat(PING_INTERVAL, PING_TIMEOUT, NO_PONG_COUNT_AS_DISCONNECTED);
    //_socketIO.enableHeartbeat(3000, 3000, 2);

    char szBuf[256];
    sprintf(szBuf, "%s?sn=%s", SOCKET_IO_PATH, szDeviceNo);

#ifdef USE_SSL
    _socketIO.beginSSL(SERVER_URL, SERVER_PORT, szBuf);
#else
    _socketIO.begin(SERVER_URL, SERVER_PORT, szBuf);
#endif
    cbSocketDataReceived = ptr;
    cbSocketConnection = ptr2;
}
//-------------------------------------------------------------------------
// name  : onEvent
void onEvent(const socketIOmessageType_t &type, uint8_t *payload, const size_t &length)
{
    switch (type)
    {
    case sIOtype_DISCONNECT:
        debugF("%s: %s\n", "[IOc]", "Disconnected");
        _bSocketIOConnected = false;
        (*cbSocketConnection)(0);
        break;
    case sIOtype_CONNECT:
        debugF("%s: %s\n", "[IOc] Connected to url: ", (const char *)payload);
        if (_bInitialConnection)
        {
            _bInitialConnection = false;
            // pubStatus("System Ready");
        }
        (*cbSocketConnection)(1);
        _bSocketIOConnected = true;
        break;
    case sIOtype_EVENT:
        debugF("%s: %s\n", "[IOc] Got event: ", (char *)payload);
        {
            // payload like : ["app-cmd",{"cmd":"sync","content":""}]
            if (payload[2] != 'a') return;            
            String text = ((const char *)&payload[0]);            
            if (cbSocketDataReceived)
                (*cbSocketDataReceived)(text.substring(text.indexOf('{'), text.length() - 1));
        }
        break;
    case sIOtype_ACK:
        debugHexDump("[IOc] Get ack: ", payload, length);
        break;
    case sIOtype_ERROR:
        debugHexDump("[IOc] Get error: ", payload, length);
        break;
    case sIOtype_BINARY_EVENT:
        debugHexDump("[IOc] Get binary: ", payload, length);
        break;
    case sIOtype_BINARY_ACK:
        debugHexDump("[IOc] Get binary ack: ", payload, length);
        break;
    case sIOtype_PING:
        debugF("%s: %s\n", "[IOc]", "Got PING");
        break;
    case sIOtype_PONG:
        debugF("%s: %s\n", "[IOc]", "Got PONG");
        break;
    default:
        break;
    }
}
//-------------------------------------------------------------------------
// name  : publish
uint8_t publish(uint8_t eventType, char *szContent)
{
    if (!_bSocketIOConnected)
    {
        debugF("%s: %s\n", __FUNCTION__, "err: socket is not connected");
        return 1;
    }
    if (eventType != DATA_EVENT && eventType != STATUS_EVENT)
    {
        debugF("%s: %s\n", __FUNCTION__, "err: invalid eventType");
        return 2;
    }
    DynamicJsonDocument doc(1024);
    JsonArray root = doc.to<JsonArray>();
    root.add(eventType == DATA_EVENT ? "dev-data" : "dev-status");

    JsonObject jsonObj = root.createNestedObject();
    if (eventType == DATA_EVENT)
        jsonObj["content"] = serialized(szContent);
    else
        jsonObj["content"] = szContent;
    _ntpClient.update();                                               // update time value
    jsonObj["createdAt"] = (uint64_t)_ntpClient.getEpochTime() * 1000; // set seconds to millisecond value, require 64bit

    String output;
    serializeJson(doc, output); // JSON to String (serializion)
    _socketIO.sendEVENT(output);

    debugF("%s: %s\n", "data Pub: ", output.c_str());
    return 0;
}
//-------------------------------------------------------------------------
// name  : stopSocketIO
void stopSocketIO()
{
    _socketIO.send(sIOtype_DISCONNECT, "");
}

//-------------------------------------------------------------------------
// name  : socketIOLoop
void socketIOLoop()
{
    _socketIO.loop();
}
