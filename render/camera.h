
#pragma once
#include<glm/glm.hpp>

class Camera
{
public:
    Camera();
    ~Camera()=default;
    Camera(float viewWidth, float viewHeight);

    //position functions
    void SetCameraPosition(float x,float y);
    const glm::vec2& GetCameraPosition() const;

    //ViewSize functions;
    void SetViewSize(float width,float height);
    const glm::vec2& GetViewSize() const;

    const glm::mat4& GetViewProjection() const;

private:
    void RecalculateMatrix();

private:
    glm::vec2 m_Position;

    glm::vec2 m_ViewSize;

    glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);

};
