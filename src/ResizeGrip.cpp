#include "ResizeGrip.hpp"

#include "GlobalNamespace/VRController.hpp"
#include "HMUI/ImageView.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "main.hpp"

DEFINE_TYPE(QuestBrowser, ResizeGrip);

using namespace UnityEngine;

namespace QuestBrowser {

namespace {

constexpr float kGripSize = 9.0f;  // UI units
constexpr float kMinScale = 0.006f;
constexpr float kMaxScale = 0.03f;

Resize::Vec3 ToVec(Vector3 v) { return {v.x, v.y, v.z}; }

// A 32 x 32 icon: three diagonal lines in the bottom-right corner, on a faint triangle.
Sprite* GripSprite() {
    constexpr int n = 32;
    ArrayW<uint8_t> pixels(static_cast<il2cpp_array_size_t>(n * n * 4));
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            // Distance from the bottom-right corner along the diagonal (texture row 0 is the bottom).
            int u = (n - 1 - x) + y;
            uint8_t alpha = 0;
            if (u < 27) alpha = 70;
            if ((u >= 4 && u <= 6) || (u >= 12 && u <= 14) || (u >= 20 && u <= 22)) alpha = 235;
            int i = (y * n + x) * 4;
            pixels[i] = 255;
            pixels[i + 1] = 255;
            pixels[i + 2] = 255;
            pixels[i + 3] = alpha;
        }
    }
    auto tex = Texture2D::New_ctor(n, n, TextureFormat::RGBA32, false);
    tex->LoadRawTextureData(pixels);
    tex->Apply(false);
    return BSML::Lite::TextureToSprite(tex);
}

}  // namespace

ResizeGrip* ResizeGrip::Create(Transform* window, std::function<void(float)> onResized) {
    auto image = BSML::Lite::CreateImage(window, GripSprite(), {0, 0}, {kGripSize, kGripSize});
    image->gameObject->name = "QuestBrowserResizeGrip";
    auto rect = image->rectTransform;
    rect->anchorMin = {1.0f, 0.0f};
    rect->anchorMax = {1.0f, 0.0f};
    rect->pivot = {1.0f, 0.0f};
    rect->anchoredPosition = {-0.5f, 0.5f};
    rect->sizeDelta = {kGripSize, kGripSize};
    image->raycastTarget = true;

    auto grip = image->gameObject->AddComponent<ResizeGrip*>();
    grip->window = window;
    grip->onResized = std::move(onResized);
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
    // Each scene has its own pointer; find it again when the old one is gone or disabled.
    if (!pointer || !pointer->isActiveAndEnabled) {
        pointer = Object::FindAnyObjectByType<VRUIControls::VRPointer*>();
        if (!pointer) return;
    }
    UnityW<GlobalNamespace::VRController> controller = pointer->lastSelectedVrController;
    if (!controller) return;
    auto edge = trigger.Update(controller->triggerValue);

    if (!dragging) {
        UnityW<GameObject> over = pointer->pointingOver;
        if (edge != Resize::Edge::Pressed || !over || over.ptr() != gameObject.ptr()) return;
        auto hit = PointerOnWindowPlane();
        if (!hit) return;
        dragging = true;
        startScale = window->localScale.x;
        startDistance = Resize::Length(Resize::Sub(*hit, ToVec(window->position)));
        return;
    }

    if (!trigger.IsHeld()) {
        EndDrag();
        return;
    }
    if (auto hit = PointerOnWindowPlane()) {
        float distance = Resize::Length(Resize::Sub(*hit, ToVec(window->position)));
        float scale = Resize::ScaleFromDrag(startScale, startDistance, distance, kMinScale, kMaxScale);
        window->localScale = Vector3(scale, scale, scale);
    }
}

void ResizeGrip::OnDisable() {
    if (dragging) EndDrag();
    trigger = {};
}

void ResizeGrip::EndDrag() {
    dragging = false;
    if (!window) return;
    float scale = window->localScale.x;
    logger.info("Resized browser window to scale {:.4f}", scale);
    if (onResized) onResized(scale);
}

}  // namespace QuestBrowser
