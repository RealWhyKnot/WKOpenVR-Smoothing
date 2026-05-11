// openvr.h must precede anything that may pull in openvr_driver.h via
// Protocol.h (IPCClient -> Protocol). Defining _OPENVR_API early makes
// Protocol.h skip the driver header, so the openvr.h declarations below
// don't redefine the same vr:: symbols.
#include <openvr.h>

#include "SmoothingPlugin.h"

#include "Protocol.h"
#include "ShellContext.h"
#include "ShellFooter.h"

#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <string>

void SmoothingPlugin::OnStart(openvr_pair::overlay::ShellContext &)
{
	ConnectIfNeeded();
	SendConfig();
	ReplayDevicePredictions();
}

void SmoothingPlugin::Tick(openvr_pair::overlay::ShellContext &)
{
	if (!ipc_.IsConnected()) ConnectIfNeeded();
	TickExternalToolDetection();
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
	for (int i = 0; i < 10; ++i) {
		int v = std::clamp(cfg_.per_finger_smoothness[i], 0, 100);
		cfg_.per_finger_smoothness[i] = v;
		req.setFingerSmoothing.per_finger_smoothness[i] = (uint8_t)v;
	}
	req.setFingerSmoothing._reserved2[0] = 0;
	req.setFingerSmoothing._reserved2[1] = 0;
	try {
		ipc_.SendBlocking(req);
		connectError_.clear();
	} catch (const std::exception &e) {
		connectError_ = e.what();
	}
}

void SmoothingPlugin::SendDevicePrediction(uint32_t openVRID, int smoothness)
{
	if (!ipc_.IsConnected()) return;
	if (smoothness < 0) smoothness = 0;
	if (smoothness > 100) smoothness = 100;
	protocol::Request req(protocol::RequestSetDevicePrediction);
	req.setDevicePrediction.openVRID = openVRID;
	req.setDevicePrediction.predictionSmoothness = (uint8_t)smoothness;
	req.setDevicePrediction._reserved[0] = 0;
	req.setDevicePrediction._reserved[1] = 0;
	req.setDevicePrediction._reserved[2] = 0;
	try {
		ipc_.SendBlocking(req);
		connectError_.clear();
	} catch (const std::exception &e) {
		connectError_ = e.what();
	}
}

void SmoothingPlugin::ReplayDevicePredictions()
{
	// On startup / reconnect, walk the saved tracker_smoothness map and push
	// each entry to the driver. Without this, restored values would sit on
	// disk until the user happened to wiggle the slider for that device.
	if (!ipc_.IsConnected() || cfg_.trackerSmoothness.empty()) return;
	auto *vrSystem = vr::VRSystem();
	if (!vrSystem) return;

	char buffer[vr::k_unMaxPropertyStringSize];
	for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
		if (vrSystem->GetTrackedDeviceClass(id) == vr::TrackedDeviceClass_Invalid) continue;
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		vrSystem->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, sizeof buffer, &err);
		if (err != vr::TrackedProp_Success || buffer[0] == 0) continue;
		auto it = cfg_.trackerSmoothness.find(buffer);
		if (it == cfg_.trackerSmoothness.end()) continue;
		SendDevicePrediction(id, it->second);
	}
}

void SmoothingPlugin::DrawTab(openvr_pair::overlay::ShellContext &)
{
	if (!connectError_.empty()) {
		ImGui::TextWrapped("%s", connectError_.c_str());
		if (ImGui::Button("Retry connection")) {
			ConnectIfNeeded();
			SendConfig();
			ReplayDevicePredictions();
		}
		ImGui::Separator();
	}

	if (ImGui::BeginTabBar("smoothing_tabs")) {
		if (ImGui::BeginTabItem("Prediction")) { DrawPredictionTab(); ImGui::EndTabItem(); }
		if (ImGui::BeginTabItem("Fingers"))    { DrawFingersTab();    ImGui::EndTabItem(); }
		ImGui::EndTabBar();
	}

	openvr_pair::overlay::ShellFooterStatus footer;
	footer.driverConnected = ipc_.IsConnected();
	footer.driverLabel = "Smoothing driver";
	openvr_pair::overlay::DrawShellFooter(footer);
}

namespace openvr_pair::overlay {

std::unique_ptr<FeaturePlugin> CreateSmoothingPlugin()
{
	return std::make_unique<SmoothingPlugin>();
}

} // namespace openvr_pair::overlay
