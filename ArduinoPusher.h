#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>

enum ConnectionState {
    ConnectionInitialized,
    ConnectionConnecting,
    ConnectionConnected,
    ConnectionUnavailable,
    ConnectionFailed
};

const String connectionStates[] = { "initialized", "connecting", "connected", "unavailable", "failed" };

using PartialHandlerFunc = std::function<void(String data)>;
using HandlerFunc = std::function<void(String eventName, String data)>;
using HandleStateChange = std::function<void(ConnectionState previous, ConnectionState current)>;

struct EventHandler {
    String name;
    PartialHandlerFunc handler;
};

class EventEmitter {
    public:
        EventEmitter();

        void bind(const String& eventName, PartialHandlerFunc listener);
        void bindAll(HandlerFunc listener);

        void checkEvent(const String& eventName, const String& data);
    private:
        EventHandler **events;
        HandlerFunc bindedToAll;
        size_t eventLength;

        void addEvent(const String& eventName, PartialHandlerFunc listener);
        void removeEvent(const String& eventName);
    protected:
        void triggerEvent(const String& eventName, const String& data) const;
};

class Connection: public websockets::WebsocketsClient, public EventEmitter {
    public:
        Connection();

        String socketId;
        ConnectionState state;

        void bind(ConnectionState _state, PartialHandlerFunc handler);
        void bindStateChange(HandleStateChange handler);
        void changeState(ConnectionState _state);
    private:
        HandleStateChange onStateChange;

};

class Channel: public EventEmitter {
    public:
        explicit Channel(String channelName);
        String name;
};

class PrivateChannel: public Channel {

};

class PresenceChannel: public PrivateChannel {

};

enum PusherEvents {
    Connected,
    Disconnected,
    Subscribed,
    Unsubscribed,
    SignInSuccess
};

const String pusherEvents[] = { "pusher:connection_established", "pusher:disconnected", "pusher_internal:subscription_succeeded", "pusher:unsubscribed", "pusher:signin_success" };

struct PusherOptions {
    String appKey;
    String secret;
    String cluster;
    String host;
    int port;
};

class Pusher: public EventEmitter {
    public:
        explicit Pusher(PusherOptions options);
        Connection* connect();
        void disconnect() const;

        void poll() const;

        Channel* subscribe(const String& channelName);
        void unsubscribe(const String& channelName);

         void bind(PusherEvents event, PartialHandlerFunc handler);

        Connection *connection;
    private:
        websockets::PartialMessageCallback onMessage();
        Channel **channels;
        size_t channelLength;

        void addChannel(Channel *channel);
        void removeChannel(const String& channelName);

        PusherOptions options;
};

