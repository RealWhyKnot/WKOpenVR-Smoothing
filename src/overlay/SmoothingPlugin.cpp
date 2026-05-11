#include "SmoothingPlugin.h"

#include "Protocol.h"
#include "ShellContext.h"

#include <imgui.h>

#include <algorithm>
#include <exception>
#include <memory>

void SmoothingPlugin::OnStart(openvr_pair::overlay::ShellContext &)
{
	ConnectIfNeeded();
	SendConfig();
}

void SmoothingPlugin::Tick(openvr_pair::overlay::ShellContext &)
{
	if (!ipc_.IsConnected()) ConnectIfNeeded();
}

void SmoothingPlugin::ConnectIfNeeded()
{
	if (ipc_.IsConnected()) return;
	try {
		ipc_.Connect();
		connectError_.clear();
	} catch (const std::exception &e) {
		connectError_ = e.what();
	}
}

void SmoothingPlugin::SendConfig()
{
	if (!ipc_.IsConnected()) return;
	protocol::Request req(protocol::RequestSetFingerSmoothing);
	req.setFingerSmoothing.master_enabled = cfg_.master_enabled;
	cfg_.smoothness = std::clamp(cfg_.smoothness, 0, 100);
	req.setFingerSmoothing.smoothness = (uint8_t)cfg_.smoothness;
	req.setFingerSmoothing.finger_mask = cfg_.finger_mask;
	req.setFingerSmoothing._reserved = 0;
	try {
		ipc_.SendBlocking(req);
		connectError_.clear();
	} catch (const std::exception &e) {
		connectError_ = e.what();
	}
}

void SmoothingPlugin::DrawTab(openvr_pair::overlay::ShellContext &)
{
	bool dirty = false;

	if (!connectError_.empty()) {
		ImGui::TextWrapped("%s", connectError_.c_str());
		if (ImGui::Button("Retry connection")) {
			ConnectIfNeeded();
			SendConfig();
		}
		ImGui::Separator();
	}

	if (ImGui::Checkbox("Enable finger smoothing", &cfg_.master_enabled)) dirty = true;
	ImGui::BeginDisabled(!cfg_.master_enabled);

	int smoothness = cfg_.smoothness;
	if (ImGui::SliderInt("Smoothness", &smoothness, 0, 100, "%d%%")) {
		cfg_.smoothness = smoothness;
		dirty = true;
	}

	const char *fingerLabels[5] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };
	const char *handLabels[2] = { "Left", "Right" };
	if (ImGui::BeginTable("fingers_grid", 6, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("Hand");
		for (int f = 0; f < 5; ++f) ImGui::TableSetupColumn(fingerLabels[f]);
		ImGui::TableHeadersRow();
		for (int hand = 0; hand < 2; ++hand) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(handLabels[hand]);
			for (int finger = 0; finger < 5; ++finger) {
				int bit = hand * 5 + finger;
				ImGui::TableNextColumn();
				bool enabled = ((cfg_.finger_mask >> bit) & 1) != 0;
				ImGui::PushID(bit);
				if (ImGui::Checkbox("##fingerbit", &enabled)) {
					if (enabled) cfg_.finger_mask |= (uint16_t)(1u << bit);
					else cfg_.finger_mask &= (uint16_t)~(1u << bit);
					dirty = true;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}

	if (ImGui::Button("Enable all fingers")) {
		cfg_.finger_mask = protocol::kAllFingersMask;
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable all fingers")) {
		cfg_.finger_mask = 0;
		dirty = true;
	}

	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::TextUnformatted("Prediction smoothing");
	static int predictionSmoothness = 0;
	ImGui::SliderInt("Pose prediction smoothness", &predictionSmoothness, 0, 100, "%d%%");

	if (dirty) {
		SaveConfig(cfg_);
		SendConfig();
	}
}

namespace openvr_pair::overlay {

std::unique_ptr<FeaturePlugin> CreateSmoothingPlugin()
{
	return std::make_unique<SmoothingPlugin>();
}

} // namespace openvr_pair::overlay
