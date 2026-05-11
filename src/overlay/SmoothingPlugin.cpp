#include "SmoothingPlugin.h"

#include "Icons.h"
#include "Protocol.h"
#include "ShellContext.h"
#include "Widgets.h"

#include <imgui.h>

#include <algorithm>
#include <exception>
#include <memory>

const char *SmoothingPlugin::IconGlyph() const
{
	return ICON_PAIR_SMOOTH;
}

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

bool SmoothingPlugin::IpcStatusOk(openvr_pair::overlay::ShellContext &) const
{
	return ipc_.IsConnected();
}

void SmoothingPlugin::DrawTab(openvr_pair::overlay::ShellContext &)
{
	if (!connectError_.empty()) {
		openvr_pair::ui::Card("Driver connection", nullptr, [&]() {
			openvr_pair::ui::StatusDot(connectError_.c_str(), openvr_pair::ui::Status::Danger);
			if (ImGui::Button("Retry connection")) {
				ConnectIfNeeded();
				SendConfig();
			}
		});
	}

	openvr_pair::ui::Card("Finger smoothing", nullptr, [&]() {
		if (openvr_pair::ui::ToggleSwitch("Enable finger smoothing", &cfg_.master_enabled)) dirty_ = true;
		ImGui::BeginDisabled(!cfg_.master_enabled);

		const char *finger_labels[5] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };
		const char *hand_labels[2] = { "Left", "Right" };
		if (ImGui::BeginTable("fingers_grid", 6,
			ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Hand");
			for (int f = 0; f < 5; ++f) ImGui::TableSetupColumn(finger_labels[f]);
			ImGui::TableHeadersRow();
			for (int hand = 0; hand < 2; ++hand) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(hand_labels[hand]);
				for (int finger = 0; finger < 5; ++finger) {
					const int bit = hand * 5 + finger;
					ImGui::TableNextColumn();
					bool enabled = ((cfg_.finger_mask >> bit) & 1) != 0;
					ImGui::PushID(bit);
					if (ImGui::Checkbox("##fingerbit", &enabled)) {
						if (enabled) cfg_.finger_mask |= static_cast<uint16_t>(1u << bit);
						else cfg_.finger_mask &= static_cast<uint16_t>(~(1u << bit));
						dirty_ = true;
					}
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}

		if (ImGui::Button("Enable all")) {
			cfg_.finger_mask = protocol::kAllFingersMask;
			dirty_ = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Disable all")) {
			cfg_.finger_mask = 0;
			dirty_ = true;
		}

		int smoothness = cfg_.smoothness;
		if (ImGui::SliderInt("Smoothness", &smoothness, 0, 100, "%d%%")) {
			cfg_.smoothness = smoothness;
			dirty_ = true;
		}

		ImGui::EndDisabled();

		ImGui::BeginDisabled(!dirty_);
		if (ImGui::Button("Apply")) {
			SaveConfig(cfg_);
			SendConfig();
			dirty_ = false;
		}
		ImGui::EndDisabled();
	});
}

namespace openvr_pair::overlay {

std::unique_ptr<FeaturePlugin> CreateSmoothingPlugin()
{
	return std::make_unique<SmoothingPlugin>();
}

} // namespace openvr_pair::overlay
