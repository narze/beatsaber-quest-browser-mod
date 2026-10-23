#pragma once

#include <functional>

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "custom-types/shared/macros.hpp"
#include "resize_logic.hpp"

// Grip in the bottom-right corner of the browser window. Point at it, hold the
// trigger, and drag to make the window bigger or smaller.
DECLARE_CLASS_CODEGEN(QuestBrowser, ResizeGrip, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Transform>, window);
    DECLARE_INSTANCE_FIELD(UnityW<VRUIControls::VRPointer>, pointer);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_DEFAULT_CTOR();

   public:
    // onResized gets the new window scale when a drag ends.
    static ResizeGrip* Create(UnityEngine::Transform* window, std::function<void(float)> onResized);

   private:
    std::optional<Resize::Vec3> PointerOnWindowPlane();
    void EndDrag();

    std::function<void(float)> onResized;
    Resize::TriggerLatch trigger;
    bool dragging = false;
    float startScale = 0.0f;
    float startDistance = 0.0f;
};
