#pragma  once

class Service {
public:
    virtual ~Service() = default;
    virtual void Init() = 0;
    virtual void Update(float dt) {};
    virtual void Shutdown() = 0;
};
