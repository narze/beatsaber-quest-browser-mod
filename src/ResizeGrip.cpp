#include "ResizeGrip.hpp"
#include "BrowserWindow.hpp"

#include "GlobalNamespace/VRController.hpp"
#include "HMUI/ImageView.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "icons_logic.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, ResizeGrip);

using namespace UnityEngine;

namespace QuestBrowser {

namespace {

constexpr float kCornerSize = 12.0f;  // UI units
constexpr float kEdgeLength = 22.0f;
constexpr float kEdgeThickness = 7.0f;

Color const kIdle{1.0f, 1.0f, 1.0f, 0.45f};
Color const kActive{0.25f, 0.85f, 1.0f, 1.0f};

Resize::Vec3 ToVec(Vector3 v) { return {v.x, v.y, v.z}; }

Sprite* IconSprite(Icons::Kind kind) {
    constexpr int n = 64;
    auto px = Icons::Draw(kind, n);
    ArrayW<uint8_t> data(static_cast<il2cpp_array_size_t>(px.size()));
    std::copy(px.begin(), px.end(), data.begin());
    auto tex = Texture2D::New_ctor(n, n, TextureFormat::RGBA32, false);
    tex->LoadRawTextureData(data);
    tex->Apply(false);
    return BSML::Lite::TextureToSprite(tex);
}

// Turn of the bottom-left bracket for each corner, and of the horizontal bar for each edge.
float GripRotation(int axisX, int axisY) {
    if (axisX != 0 && axisY != 0) {
        if (axisX < 0 && axisY < 0) return 0.0f;
        if (axisX > 0 && axisY < 0) return 90.0f;
        if (axisX > 0 && axisY > 0) return 180.0f;
        return 270.0f;
    }
    return axisX != 0 ? 90.0f : 0.0f;
}

}  // namespace

ResizeGrip* ResizeGrip::Create(Transform* window, int axisX, int axisY, std::function<void(Resize::Dimensions, bool)> onResized) {
    // Corners: an L bracket. Edges: a short pill along the edge.
    bool corner = axisX != 0 && axisY != 0;
    Vector2 size = corner ? Vector2(kCornerSize, kCornerSize) : Vector2(kEdgeLength, kEdgeThickness);
    auto image = BSML::Lite::CreateImage(window, IconSprite(corner ? Icons::Kind::CornerBracket : Icons::Kind::EdgeBar), {0, 0}, size);
    image->gameObject->name = "QuestBrowserResizeGrip";
    auto rect = image->rectTransform;
    Vector2 anchor{axisX < 0 ? 0.0f : axisX > 0 ? 1.0f : 0.5f, axisY < 0 ? 0.0f : axisY > 0 ? 1.0f : 0.5f};
    rect->anchorMin = anchor;
    rect->anchorMax = anchor;
    // Centered on the corner or the edge middle; the rotation turns the shape around that point.
    rect->pivot = {0.5f, 0.5f};
    float inset = corner ? kCornerSize / 2.0f - 1.0f : kEdgeThickness / 2.0f;
    rect->anchoredPosition = {-axisX * inset, -axisY * inset};
    rect->sizeDelta = size;
    rect->localRotation = Quaternion::Euler(0.0f, 0.0f, GripRotation(axisX, axisY));
    image->color = kIdle;
    image->raycastTarget = true;

    auto grip = image->gameObject->AddComponent<ResizeGrip*>();
    grip->window = window;
    grip->axisX = axisX;
    grip->axisY = axisY;
    grip->onResized = std::move(onResized);
    return grip;
}

ResizeGrip* ResizeGrip::CreateDepth(Transform* window, std::function<void()> onMoved) {
    auto button = BSML::Lite::CreateUIButton(window, "Depth", Vector2(0, 0), Vector2(24, 8), []() {});
    button->gameObject->name = "QuestBrowserDepthHandle";
    auto rect = button->gameObject->GetComponent<RectTransform*>();
    rect->anchorMin = {1, 0};
    rect->anchorMax = {1, 0};
    rect->pivot = {1, 0};
    rect->anchoredPosition = {-11, 1};
    rect->sizeDelta = {24, 8};
    button->gameObject->AddComponent<CanvasGroup*>()->alpha = 0.65f;
    auto grip = button->gameObject->AddComponent<ResizeGrip*>();
    grip->window = window;
    grip->depthMode = true;
    grip->onMoved = std::move(onMoved);
    return grip;
}

std::optional<Resize::Vec3> ResizeGrip::PointerOnWindowPlane() {
    UnityW<GlobalNamespace::VRController> controller = pointer->lastSelectedVrController;
    if (!controller || !window) return std::nullopt;
    return Resize::RayPlaneHit(ToVec(controller->position), ToVec(controller->forward), ToVec(window->position),
                               ToVec(window->forward));
}

void ResizeGrip::Update() {
    if (!window) return;
    // Grow with distance so the grips stay easy to hit.
    float controls = 1.0f;
    if (auto camera = Camera::get_main()) {
        auto offset = Vector3::op_Subtraction(window->position, camera->transform->position);
        controls = Resize::ControlScale(offset.magnitude, window->lossyScale.x);
    }
    transform->localScale = Vector3(controls, controls, controls);
    // Each scene has its own pointer; find it again when the old one is gone or disabled.
    if (!pointer || !pointer->isActiveAndEnabled) {
        pointer = Object::FindAnyObjectByType<VRUIControls::VRPointer*>();
        if (!pointer) return;
    }
    UnityW<GlobalNamespace::VRController> controller = pointer->lastSelectedVrController;
    if (dragging && dragController) controller = dragController;
    if (!controller) return;
    auto edge = trigger.Update(controller->triggerValue);
    if (!depthMode) {
        // Blue while pointed at or dragged.
        if (auto image = GetComponent<HMUI::ImageView*>()) {
            UnityW<GameObject> hovered = pointer->pointingOver;
            bool hot = dragging || (hovered && hovered.ptr() == gameObject.ptr());
            image->color = hot ? kActive : kIdle;
        }
    }

    if (!dragging) {
        UnityW<GameObject> over = pointer->pointingOver;
        if (edge != Resize::Edge::Pressed || !over || !over->transform->IsChildOf(transform)) return;
        dragController = controller;
        if (depthMode) {
            auto camera = Camera::get_main();
            if (!camera) return;
            depthOrigin = camera->transform->position;
            auto offset = Vector3::op_Subtraction(window->position, depthOrigin);
            startDepth = offset.magnitude;
            if (startDepth < 0.001f) return;
            depthAxis = Vector3::Normalize(offset);
            startController = controller->position;
            dragging = true;
            return;
        }
        auto hit = PointerOnWindowPlane();
        if (!hit) return;
        dragging = true;
        auto rect = window->GetComponent<RectTransform*>()->rect;
        startSize = {rect.width, rect.height};
        startPoint = window->InverseTransformPoint(Vector3(hit->x, hit->y, hit->z));
        return;
    }

    if (!trigger.IsHeld()) {
        EndDrag();
        return;
    }
    if (depthMode) {
        auto delta = Vector3::op_Subtraction(controller->position, startController);
        float distance = Resize::DepthFromDrag(startDepth, ToVec(delta), ToVec(depthAxis), Resize::kDepthGain);
        window->position = Vector3(depthOrigin.x + depthAxis.x * distance,
                                   depthOrigin.y + depthAxis.y * distance,
                                   depthOrigin.z + depthAxis.z * distance);
        return;
    }
    if (auto hit = PointerOnWindowPlane()) {
        auto local = window->InverseTransformPoint(Vector3(hit->x, hit->y, hit->z));
        auto size = Resize::DimensionsFromDrag(startSize, local.x - startPoint.x, local.y - startPoint.y,
                                               axisX, axisY, Resize::AspectFor(Window::GetAspectPreset()),
                                               Window::GetControlScale(), Window::GetChromeHeight());
        if (onResized) onResized(size, false);
    }
}

void ResizeGrip::OnDisable() {
    if (dragging) EndDrag();
    trigger = {};
}

void ResizeGrip::EndDrag() {
    dragging = false;
    dragController = nullptr;
    if (!window) return;
    if (depthMode) {
        if (onMoved) onMoved();
        return;
    }
    auto rect = window->GetComponent<RectTransform*>()->rect;
    logger.info("Resized browser window to {:.1f} x {:.1f}", rect.width, rect.height);
    if (onResized) onResized({rect.width, rect.height}, true);
}

}  // namespace QuestBrowser
