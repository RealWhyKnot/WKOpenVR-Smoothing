#include "UserInterface.h"
#include "BuildStamp.h"
#include "Version.h"

#include <imgui/imgui.h>

namespace fs_ui
{

namespace {

// Per-finger labels in the same bit order as protocol::SmoothingConfig::finger_mask.
const char *const kLabels[10] = {
    "L Thumb", "L Index", "L Middle", "L Ring", "L Pinky",
    "R Thumb", "R Index", "R Middle", "R Ring", "R Pinky",
};

void DrawStatusDot(bool connected)
{
    ImVec4 col = connected ? ImVec4(0.20f, 0.80f, 0.30f, 1.0f) : ImVec4(0.85f, 0.30f, 0.25f, 1.0f);
    ImGui::TextColored(col, connected ? "Driver: connected" : "Driver: disconnected");
}

} // anonymous

bool Render(protocol::SmoothingConfig &cfg, IPCClient & /*ipc*/, bool ipcConnected)
{
    bool dirty = false;

    // Top bar with driver-connection state and version. Mirrors SpaceCalibrator's
    // status-pill pattern so users moving between the two recognize the layout.
    ImGui::Begin("FingerSmoothing", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    DrawStatusDot(ipcConnected);
    ImGui::Separator();

    if (ImGui::Checkbox("Master enable", &cfg.master_enabled)) dirty = true;
    ImGui::SameLine();
    ImGui::TextDisabled("(off = passthrough; on = smoothing applied)");

    if (ImGui::Checkbox("Adaptive (auto-tune to noise floor)", &cfg.adaptive_enabled)) dirty = true;
    ImGui::SameLine();
    ImGui::TextDisabled("(observes stationary variance and tightens cutoff automatically)");

    ImGui::Separator();
    ImGui::Text("One-Euro filter parameters");

    if (ImGui::SliderFloat("mincutoff (Hz)", &cfg.mincutoff, 0.1f, 5.0f, "%.3f")) dirty = true;
    if (ImGui::SliderFloat("beta",           &cfg.beta,      0.0f, 0.05f, "%.4f")) dirty = true;
    if (ImGui::SliderFloat("dcutoff (Hz)",   &cfg.dcutoff,   0.1f, 5.0f, "%.3f")) dirty = true;

    ImGui::Separator();
    ImGui::Text("Per-finger smoothing (uncheck to bypass for that finger)");

    // 5-column grid: thumb..pinky for each hand on its own row.
    if (ImGui::BeginTable("fingers", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV)) {
        for (int row = 0; row < 2; ++row) {
            ImGui::TableNextRow();
            for (int col = 0; col < 5; ++col) {
                int bit = row * 5 + col;
                ImGui::TableNextColumn();
                bool enabled = (cfg.finger_mask >> bit) & 1;
                ImGui::PushID(bit);
                if (ImGui::Checkbox(kLabels[bit], &enabled)) {
                    if (enabled) cfg.finger_mask |= (uint16_t)(1u << bit);
                    else         cfg.finger_mask &= (uint16_t)~(1u << bit);
                    dirty = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    // Visualization placeholder. M4 wires this to a shared-memory ring buffer
    // the driver writes per-frame curl/splay (raw + smoothed) into.
    ImGui::TextDisabled("Live curl/splay visualization will appear here in the next milestone.");

    // Footer
    ImGui::Separator();
    ImGui::Text("FingerSmoothing %s  build %s (%s)", FINGERSMOOTH_VERSION_STRING, FINGERSMOOTH_BUILD_STAMP, FINGERSMOOTH_BUILD_CHANNEL);

    ImGui::End();
    return dirty;
}

} // namespace fs_ui
