#include "camera.h"
#include<glm/gtc/matrix_transform.hpp>
 Camera::Camera()
{

    RecalculateMatrix();
}



 Camera::Camera(float viewWidth, float viewHeight)
{
    m_ViewSize= glm::vec2(viewWidth,viewHeight);
    RecalculateMatrix();
}


void Camera::SetCameraPosition(float x, float y)
{
    m_Position=glm::vec2(x,y);
    RecalculateMatrix();
}

const glm::vec2& Camera::GetCameraPosition() const
{
    return m_Position;
}

void Camera::SetViewSize(float width, float height)
{
    m_ViewSize=glm::vec2(width,height);
    RecalculateMatrix();
}

const glm::vec2& Camera::GetViewSize() const
{
    return m_ViewSize;
}

const glm::mat4& Camera::GetViewProjection() const
{
    return m_ViewProjectionMatrix;
}

void Camera::RecalculateMatrix()
{
     glm::mat4 projection = glm::ortho(
        -m_ViewSize.x * 0.5f,
         m_ViewSize.x  * 0.5f,
        -m_ViewSize.y * 0.5f,
         m_ViewSize.y * 0.5f,
        -1.0f,
         1.0f
    );

    // View matrix (camera position)
    glm::mat4 view = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(-m_Position, 0.0f)
    );

    m_ViewProjectionMatrix = projection * view;
}

