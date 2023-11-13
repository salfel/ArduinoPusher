#include <ArduinoPusher.h>
#include "mbedtls/md.h"

EventEmitter::EventEmitter() {
    this->eventLength = 0;
    this->events = new EventHandler*;
}

void EventEmitter::bind(const String& eventName, PartialHandlerFunc listener) {
    this->addEvent(eventName, std::move(listener));
}

void EventEmitter::bindAll(HandlerFunc listener) {
    this->bindedToAll = std::move(listener);
}

void EventEmitter::addEvent(const String& eventName, PartialHandlerFunc listener) {
    auto **newEvents = new EventHandler*[this->eventLength];
    auto *event = new EventHandler{eventName, std::move(listener)};
    for(int i = 0; i < this->eventLength; i++) {
        newEvents[i] = this->events[i];
    }
    newEvents[this->eventLength] = event;
    this->events = newEvents;
    this->eventLength++;
}

void EventEmitter::removeEvent(const String& eventName) {
    auto **newEvents = new EventHandler*[this->eventLength - 1];
    for(int i = 0; i < this->eventLength; i++) {
        if (this->events[i]->name == eventName) {
            continue;
        }
        newEvents[i] = this->events[i];
    }
    this->events = newEvents;
    this->eventLength--;
}

void EventEmitter::checkEvent(const String& eventName, const String& data) {
    if (this->bindedToAll) {
        this->bindedToAll(eventName, data);
    }
    this->triggerEvent(eventName, data);
}

void EventEmitter::triggerEvent(const String& eventName, const String& data) const {
    for (int i = 0; i < eventLength; i++) {
        EventHandler *event = events[i];

        if (event->name == eventName) {
            event->handler(data);
        }
    }
}

Connection::Connection(): state(ConnectionInitialized), onStateChange(nullptr) {
}

void Connection::bind(ConnectionState event, PartialHandlerFunc handler) {
    EventEmitter::bind(connectionStates[event], std::move(handler));
}

void Connection::bindStateChange(HandleStateChange handler) {
    this->onStateChange = std::move(handler);
}

void Connection::changeState(ConnectionState _state) {
    if (this->onStateChange) {
        this->onStateChange(this->state, _state);
    }
    this->state = _state;
    this->checkEvent(connectionStates[state], "");
}

Pusher::Pusher(PusherOptions options) {
    this->connection = new Connection;
    this->options = std::move(options);

    this->channels = new Channel*;
    this->channelLength = 0;
}

Connection* Pusher::connect() {
    this->connection->connect("ws-" + this->options.cluster + ".pusher.com", 80, "/app/" + this->options.appKey + "?protocol=7&client=arduino-ArduinoPusher");
    this->connection->changeState(ConnectionConnecting);

    this->connection->onMessage(this->onMessage());
    return this->connection;
}

void Pusher::disconnect() const {
    this->connection->close();
    this->connection->changeState(ConnectionUnavailable);
    this->triggerEvent(pusherEvents[Disconnected], "");
}

void Pusher::bind(PusherEvents event, PartialHandlerFunc handler) {
    EventEmitter::bind(pusherEvents[event], std::move(handler));
}

void Pusher::poll() const {
    this->connection->poll();
}

Channel* Pusher::subscribe(const String& channelName) {
    String data;
    StaticJsonDocument<200> doc;
    doc["event"] = "pusher:subscribe";
    doc["data"]["channel"] = channelName;
    serializeJson(doc, data);

    this->connection->send(data);
    auto *channel = new Channel(channelName);
    this->addChannel(channel);
    return channel;
}

void Pusher::unsubscribe(const String& channelName) {
    String data;
    StaticJsonDocument<200> doc;
    doc["event"] = "pusher:unsubscribe";
    doc["data"]["channel"] = channelName;
    serializeJson(doc, data);

    this->connection->send(data);
    this->triggerEvent(pusherEvents[Unsubscribed], "");
    this->removeChannel(channelName);
}

void Pusher::addChannel(Channel *channel) {
    auto **newChannels = new Channel*[this->channelLength];
    for(int i = 0; i < this->channelLength; i++) {
        newChannels[i] = this->channels[i];
    }
    newChannels[this->channelLength] = channel;
    this->channels = newChannels;
    this->channelLength++;
}

void Pusher::removeChannel(const String &channelName) {
    auto **newChannels = new Channel*[this->channelLength - 1];
    for(int i = 0; i < this->channelLength; i++) {
        if (this->channels[i]->name == channelName) {
            continue;
        }
        newChannels[i] = this->channels[i];
    }
    this->channels = newChannels;
    this->channelLength--;
}

websockets::PartialMessageCallback Pusher::onMessage() {
    return [this](const websockets::WebsocketsMessage& message) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, message.data());
        String event = doc["event"].as<String>();
        String data = doc["data"].as<String>();
        if (event == pusherEvents[Connected]) {
            StaticJsonDocument<100> _doc;
            deserializeJson(doc, data);
            String socketId = doc["socket_id"].as<String>();

            this->connection->socketId = socketId;
            connection->changeState(ConnectionConnected);
        }
        for (int i = 0; i < channelLength; i++) {
            Channel *channel = channels[i];

            channel->checkEvent(event, data);;
        }
        this->checkEvent(event, data);
    };
}

Channel::Channel(String channelName) : name(std::move(channelName)) {}
