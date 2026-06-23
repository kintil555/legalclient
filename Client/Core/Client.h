#pragma once

namespace Client::Core {

// Called once from the DLL worker thread after game window is ready.
void initializeClient();

// Called every frame by D3DHook after ImGui::NewFrame().
void renderClient();

// Called on DLL_PROCESS_DETACH.
void shutdownClient();

} // namespace Client::Core
