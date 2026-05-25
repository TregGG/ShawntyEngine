#pragma once

#include "../service.h"
#include <map>
#include <functional>
#include <memory>

enum class EventType {
    None = 0,
    MousePressed,
    MouseReleased,
    MouseMoved,
    KeyPressed,
    KeyReleased,
    CharTyped
};

struct Event {
    EventType type = EventType::None;
    bool handled = false;
    virtual ~Event() = default;
};

struct MouseEvent : public Event {
    int button;
    double x, y;
    MouseEvent(EventType t, int b, double _x, double _y) : button(b), x(_x), y(_y) { type = t; }
};

struct KeyEvent : public Event {
    int key;
    KeyEvent(EventType t, int k) : key(k) { type = t; }
};

struct CharEvent : public Event {
    unsigned int codepoint;
    explicit CharEvent(unsigned int cp) : codepoint(cp) { type = EventType::CharTyped; }
};

class EventService : public Service {
public:
    using EventCallback = std::function<void(Event&)>;

    void Init() override {}
    void Shutdown() override {}

    int Subscribe(const EventCallback& callback) {
        m_Subscribers[m_NextID] = callback;
        return m_NextID++;
    }

    void Unsubscribe(int id) {
        m_Subscribers.erase(id);
    }

    void Publish(Event& e) {
        for (auto& [id, sub] : m_Subscribers) {
            if (e.handled) break;
            sub(e);
        }
    }

private:
    std::map<int, EventCallback> m_Subscribers;
    int m_NextID = 1;
};
