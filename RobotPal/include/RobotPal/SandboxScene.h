#ifndef __SANDBOXSCENE_H__
#define __SANDBOXSCENE_H__
#include "RobotPal/Scene.h"
#include "glm/glm.hpp"
#include <memory>
#include "RobotPal/Core/GraphicsTypes.h"
#include "RobotPal/GlobalComponents.h"
#include "RobotPal/Streaming/IStreamingManager.h"
class VertexArray;
class Shader;

class SandboxScene : public Scene {
public:
    using Scene::Scene;

    void OnEnter() override;

    void OnUpdate(float dt) override;

    void OnExit() override;

    void OnImGuiRender() override;
private:
    static constexpr float kCameraStreamFpsCap = 60.0f;
    bool m_StreamingEnabled = true;
    float m_StreamFrameAccumulator = 0.0f;
    std::shared_ptr<IStreamingManager> m_StreamingManager;
};

#endif