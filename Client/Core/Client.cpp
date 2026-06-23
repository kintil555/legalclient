#include "Core/ModuleManager.h"
#include "Modules/AutoSprint.h"
#include "Modules/KeystrokeHUD.h"
#include "Modules/AutoJumpOnDamage.h"
#include "Modules/PlayerESP.h"
#include "UI/MenuWindow.h"
#include "Settings/ConfigManager.h"
#include "Events/EventBus.h"

namespace Client::Core {

// Called once when the DLL is injected/attached.
void initializeClient() {
    auto& mgr = ModuleManager::get();

    mgr.registerModule<Modules::AutoSprint>();
    mgr.registerModule<Modules::KeystrokeHUD>();
    mgr.registerModule<Modules::AutoJumpOnDamage>();
    mgr.registerModule<Modules::PlayerESP>();

    mgr.bindEvents();
    Settings::ConfigManager::get().load("default.json");

    // Route raw key events to both the menu (Right Shift toggle) and the
    // module manager (modules like KeystrokeHUD need raw key state too).
    Events::EventBus::get().key.subscribe([](const Events::KeyEvent& evt) {
        UI::MenuWindow::get().onKeyEvent(evt.virtualKey, evt.isDown);
    });
}

// Called every rendered frame, after ImGui::NewFrame().
void renderClient() {
    UI::MenuWindow::get().render();
    Events::EventBus::get().render.dispatch({ /*deltaTime*/ 0.0f });
}

// Called on DLL detach / game shutdown.
void shutdownClient() {
    Settings::ConfigManager::get().save("default.json");
}

} // namespace Client::Core
