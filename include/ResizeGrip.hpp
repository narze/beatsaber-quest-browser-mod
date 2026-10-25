#pragma once

#include <functional>

#include "UnityEngine/MonoBehaviour.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/Transform.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "custom-types/shared/macros.hpp"
#include "resize_logic.hpp"

// Edge or corner grip. Hold the trigger and drag to resize the window in UI units.
DECLARE_CLASS_CODEGEN(QuestBrowser, ResizeGrip, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::Transform>, window);
    DECLARE_INSTANCE_FIELD(UnityW<VRUIControls::VRPointer>, pointer);
    DECLARE_INSTANCE_FIELD(UnityW<GlobalNamespace::VRController>, dragController);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_DEFAULT_CTOR();

   public:
    // onResized receives dimensions during the drag; final=true saves them on release.
    static ResizeGrip* Create(UnityEngine::Transform* window, int axisX, int axisY,
                              std::function<void(Resize::Dimensions, bool)> onResized);

   private:
    std::optional<Resize::Vec3> PointerOnWindowPlane();
    void EndDrag();

    std::function<void(Resize::Dimensions, bool)> onResized;
    Resize::TriggerLatch trigger;
    bool dragging = false;
    int axisX = 1, axisY = -1;
    Resize::Dimensions startSize;
    UnityEngine::Vector3 startPoint;
};
