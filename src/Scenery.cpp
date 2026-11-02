#include "Scenery.hpp"

#include <optional>
#include <string>
#include <vector>

#include "GlobalNamespace/BloomPrePassRendererSO.hpp"
#include "GlobalNamespace/MainCameraCullingMask.hpp"
#include "GlobalNamespace/ToneMapping.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "main.hpp"
#include "view_logic.hpp"

namespace QuestBrowser::Scenery {

namespace {

using UnityEngine::SceneManagement::SceneManager;

bool hideAll = false;  // this song
std::vector<SafePtrUnity<UnityEngine::Renderer>> hiddenRenderers;  // to show them again
// The gameplay camera and the culling mask the game gave it (optional: SafePtrUnity
// can not be assigned).
std::optional<SafePtrUnity<UnityEngine::Camera>> gameplayCamera;
int gameplayMask = 0;

void ApplyCullingMask() {
    if (!gameplayCamera || !*gameplayCamera) return;
    int mask = View::CullingMaskFor(gameplayMask, hideAll);
    (*gameplayCamera)->cullingMask = mask;
    logger.info("Gameplay camera culling mask 0x{:x} (game: 0x{:x})", mask, gameplayMask);
}

// Renderers stay disabled until the scene unloads at the end of the song.
void HideEnvironmentRenderers() {
    int count = 0;
    for (int i = 0; i < SceneManager::get_sceneCount(); ++i) {
        auto scene = SceneManager::GetSceneAt(i);
        if (!scene.isLoaded) continue;
        std::string name = scene.name;
        if (!View::IsSongEnvironmentScene(name)) continue;
        for (auto root : scene.GetRootGameObjects()) {
            if (!root) continue;
            for (auto renderer : root->GetComponentsInChildren<UnityEngine::Renderer*>(true)) {
                if (!renderer || !renderer->enabled) continue;
                renderer->enabled = false;
                hiddenRenderers.emplace_back(renderer);
                ++count;
            }
        }
        logger.info("Hide scenery (All): {} renderers in {}", count, name);
    }
}

void ShowEnvironmentRenderers() {
    for (auto& renderer : hiddenRenderers)
        if (renderer) renderer->enabled = true;
    logger.info("Hide scenery (All) off: showed {} renderers", hiddenRenderers.size());
    hiddenRenderers.clear();
}

}  // namespace

void SetHideAll(bool value) {
    if (value == hideAll) return;
    hideAll = value;
    if (hideAll) HideEnvironmentRenderers();
    else ShowEnvironmentRenderers();
    ApplyCullingMask();
}

void BeginSong(bool value) {
    hideAll = false;
    hiddenRenderers.clear();
    SetHideAll(value);
    ApplyCullingMask();
}

void EndSong() {
    hideAll = false;
    hiddenRenderers.clear();  // the scene is gone
    // A camera that lives on (the menu camera) gets the game's mask back.
    if (gameplayCamera && *gameplayCamera) (*gameplayCamera)->cullingMask = gameplayMask;
    gameplayCamera.reset();
}

MAKE_HOOK_MATCH(MainCameraCullingMask_Start, &GlobalNamespace::MainCameraCullingMask::Start, void,
                GlobalNamespace::MainCameraCullingMask* self) {
    MainCameraCullingMask_Start(self);
    auto camera = self->_camera;
    if (!camera) return;
    gameplayCamera.emplace(camera.ptr());
    gameplayMask = camera->cullingMask;
    ApplyCullingMask();
}

// The game draws every light into the bloom pre-pass texture, and note, bomb and fog
// shaders sample it. Camera layers do not apply to it, so give the shaders black.
MAKE_HOOK_MATCH(BloomPrePassRendererSO_SetDataToShaders, &GlobalNamespace::BloomPrePassRendererSO::SetDataToShaders, void,
                float_t stereoCameraEyeOffset, UnityEngine::Vector2 textureToScreenRatio, UnityEngine::Texture* bloomFogTexture,
                GlobalNamespace::ToneMapping toneMapping) {
    if (hideAll && gameplayCamera && *gameplayCamera) bloomFogTexture = UnityEngine::Texture2D::get_blackTexture().ptr();
    BloomPrePassRendererSO_SetDataToShaders(stereoCameraEyeOffset, textureToScreenRatio, bloomFogTexture, toneMapping);
}

void InstallHooks() {
    INSTALL_HOOK(logger, MainCameraCullingMask_Start);
    INSTALL_HOOK(logger, BloomPrePassRendererSO_SetDataToShaders);
}

}  // namespace QuestBrowser::Scenery
