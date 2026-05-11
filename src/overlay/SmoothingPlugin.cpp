#include "SmoothingPlugin.h"

#include "Protocol.h"
#include "ShellContext.h"

#include <openvr.h>

#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <ctime>
#include <exception>
#include <memory>
#include <string>

namespace {

// External "smooth tracking" tools we treat as conflicting with our driver's
// own pose-prediction suppression. When one is running we surface a banner
// asking the user to close it before using our per-tracker sliders -- the
// two systems fight in unpredictable ways.
struct ExternalSmoothingTool
{
	const wchar_t *exeName;
	const char *humanName;
};

const ExternalSmoothingTool kKnownTools[] = {
	{ L"OpenVR-SmoothTracking.exe", "OpenVR-SmoothTracking" },
	{ L"OpenVRSmoothTracking.exe",  "OpenVR-SmoothTracking" },
	{ L"OVR-SmoothTracking.exe",    "OVR-SmoothTracking" },
	{ L"OVRSmoothTracking.exe",     "OVR-SmoothTracking" },
	{ L"ovr_smooth_tracking.exe",   "OVR-SmoothTracking" },
};

struct SubstringSmoothingPattern
{
	const wchar_t *requireA;
	const wchar_t *requireB;
};

// Substring fallback for renamed/repacked variants. Requires BOTH substrings
// to keep false positives down -- a generic match on just "smooth" or "track"
// would catch a lot of unrelated software.
const SubstringSmoothingPattern kSubstringTools[] = {
	{ L"smooth", L"track" },
};

bool DetectExternalSmoothingTool(std::string &outName)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return false;

	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof pe;
	bool found = false;

	if (Process32FirstW(snap, &pe)) {
		do {
			for (const auto &tool : kKnownTools) {
				if (_wcsicmp(pe.szExeFile, tool.exeName) == 0) {
					outName = tool.humanName;
					found = true;
					break;
				}
			}
			if (found) break;

			std::wstring lower = pe.szExeFile;
			for (auto &c : lower) c = (wchar_t)towlower(c);
			for (const auto &pat : kSubstringTools) {
				if (lower.find(pat.requireA) != std::wstring::npos &&
				    lower.find(pat.requireB) != std::wstring::npos) {
					int n = WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
					if (n > 0) {
						outName.assign((size_t)(n - 1), '\0');
						WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, outName.data(), n, nullptr, nullptr);
					}
					found = true;
					break;
				}
			}
		} while (!found && Process32NextW(snap, &pe));
	}

	CloseHandle(snap);
	return found;
}

double MonotonicSeconds()
{
	LARGE_INTEGER freq, ctr;
	if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&ctr)) return 0.0;
	return (double)ctr.QuadPart / (double)freq.QuadPart;
}

// Map common upstream tracking-system identifiers to short user-friendly
// labels. Mirrors SC's GetPrettyTrackingSystemName so the per-tracker rows
// read the same way across both overlays.
const char *PrettyTrackingSystem(const char *raw)
{
	if (!raw || !*raw) return "?";
	if (_stricmp(raw, "lighthouse") == 0) return "lighthouse";
	if (_stricmp(raw, "oculus") == 0) return "oculus";
	if (_stricmp(raw, "indexcontroller") == 0) return "lighthouse";
	return raw;
}

} // namespace

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
	// disk until the user happened to wiggle the slider for that device. We
	// match map entries to live OpenVR IDs by serial; trackers not currently
	// connected are skipped and will be replayed on the next reconnect.
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

void SmoothingPlugin::TickExternalToolDetection()
{
	const double now = MonotonicSeconds();
	if (now - lastExternalScanSeconds_ < 5.0) return;
	lastExternalScanSeconds_ = now;

	std::string detectedName;
	const bool detected = DetectExternalSmoothingTool(detectedName);
	if (detected != externalSmoothingDetected_ ||
	    detectedName != externalSmoothingToolName_) {
		externalSmoothingDetected_ = detected;
		externalSmoothingToolName_ = detected ? detectedName : std::string();
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
		if (ImGui::BeginTabItem("Prediction")) {
			DrawPredictionTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Fingers")) {
			DrawFingersTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void SmoothingPlugin::DrawPredictionTab()
{
	// Detection status box. Always visible so the user can tell at a glance
	// whether we've noticed an external smoothing tool, even when none is
	// running.
	{
		const char *tool = externalSmoothingToolName_.empty()
			? "an external smoothing tool"
			: externalSmoothingToolName_.c_str();
		char statusBuf[256];
		const char *statusText;
		ImVec4 bgColor;
		if (externalSmoothingDetected_) {
			snprintf(statusBuf, sizeof statusBuf, "DETECTED: %s is running.", tool);
			statusText = statusBuf;
			bgColor = ImVec4(0.55f, 0.40f, 0.10f, 1.0f);
		} else {
			statusText = "No external smoothing tool detected.";
			bgColor = ImVec4(0.20f, 0.40f, 0.25f, 1.0f);
		}

		const ImVec2 textSize = ImGui::CalcTextSize(statusText);
		const ImVec2 padding(10.0f, 6.0f);
		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImVec2 rectMin = cursor;
		ImVec2 rectMax(cursor.x + ImGui::GetContentRegionAvail().x,
		               cursor.y + textSize.y + padding.y * 2.0f);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(rectMin, rectMax, ImGui::GetColorU32(bgColor), 6.0f);
		dl->AddText(ImVec2(rectMin.x + padding.x, rectMin.y + padding.y),
		            ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f)), statusText);
		ImGui::Dummy(ImVec2(0, textSize.y + padding.y * 2.0f));
	}

	if (externalSmoothingDetected_) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		const float h = ImGui::GetFrameHeightWithSpacing() * 3.0f;
		if (ImGui::BeginChild("##ext_warn", ImVec2(ImGui::GetContentRegionAvail().x, h),
		                      ImGuiChildFlags_Border)) {
			const char *tool = externalSmoothingToolName_.empty()
				? "An external smoothing tool"
				: externalSmoothingToolName_.c_str();
			ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.95f, 1.0f),
				"%s is running.  We don't support working alongside it -- our smoothing\n"
				"and its smoothing will fight, and the result is unpredictable.\n"
				"Please close it and use the per-tracker smoothness sliders below instead.",
				tool);
		}
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(2);
	}

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Prediction smoothness scales each tracker's reported velocity / acceleration "
		"down toward zero. 0 leaves the pose untouched (raw motion, sharp response). 100 "
		"fully zeros velocity, which defeats SteamVR's pose extrapolation entirely (smoothest "
		"motion at the cost of a tiny lag). Pick a value between to trade response for jitter.\n\n"
		"The HMD is locked at 0 -- suppressing it would cause judder in your view. If you're "
		"running Space Calibrator, do not suppress your calibration reference or target "
		"trackers either: doing so corrupts the calibration math that reads their velocity.");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Per-tracker smoothness");
	ImGui::TextWrapped(
		"Settings stick to a tracker by serial number, so a device that disconnects and "
		"reconnects keeps its slider value.");
	ImGui::Spacing();

	auto *vrSystem = vr::VRSystem();
	if (!vrSystem) {
		ImGui::TextDisabled("(VR system not available)");
		return;
	}

	bool anyShown = false;
	bool dirty = false;
	char buffer[vr::k_unMaxPropertyStringSize];
	for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
		const auto deviceClass = vrSystem->GetTrackedDeviceClass(id);
		if (deviceClass == vr::TrackedDeviceClass_Invalid) continue;

		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		vrSystem->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, sizeof buffer, &err);
		if (err != vr::TrackedProp_Success || buffer[0] == 0) continue;
		std::string serial = buffer;

		vrSystem->GetStringTrackedDeviceProperty(id, vr::Prop_RenderModelName_String, buffer, sizeof buffer, &err);
		std::string model = (err == vr::TrackedProp_Success) ? buffer : "";

		vrSystem->GetStringTrackedDeviceProperty(id, vr::Prop_TrackingSystemName_String, buffer, sizeof buffer, &err);
		std::string sys = (err == vr::TrackedProp_Success) ? PrettyTrackingSystem(buffer) : "";

		const bool isHmd = (deviceClass == vr::TrackedDeviceClass_HMD);

		int smoothness = 0;
		auto it = cfg_.trackerSmoothness.find(serial);
		if (it != cfg_.trackerSmoothness.end()) smoothness = it->second;
		if (isHmd) smoothness = 0;

		ImGui::PushID(("trk_" + serial).c_str());
		ImGui::Text("%s  [%s]  %s",
			model.empty() ? "(unknown model)" : model.c_str(),
			sys.empty() ? "?" : sys.c_str(),
			serial.c_str());
		if (isHmd) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.55f, 1.0f), "[HMD, locked]");
		}

		ImGui::BeginDisabled(isHmd);
		if (ImGui::SliderInt("smoothness##slider", &smoothness, 0, 100, "%d%%")) {
			if (smoothness <= 0) cfg_.trackerSmoothness.erase(serial);
			else cfg_.trackerSmoothness[serial] = smoothness;
			SendDevicePrediction(id, smoothness);
			dirty = true;
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) {
			if (isHmd) {
				ImGui::SetTooltip("Locked to 0. Suppressing HMD prediction would cause judder in your view.");
			} else {
				ImGui::SetTooltip(
					"0 = raw motion (no suppression).\n"
					"100 = fully suppressed (matches the old binary 'freeze' behaviour).\n"
					"Try around 50-75 for IMU-based trackers that feel jittery.");
			}
		}
		ImGui::Spacing();
		ImGui::PopID();
		anyShown = true;
	}
	if (!anyShown) ImGui::TextDisabled("(No tracked devices found.)");

	if (dirty) SaveConfig(cfg_);
}

void SmoothingPlugin::DrawFingersTab()
{
	bool dirty = false;

	ImGui::TextDisabled("(Index Knuckles only -- smooths per-frame finger bone updates before VRChat sees them.)");
	ImGui::Spacing();

	if (ImGui::Checkbox("Enable finger smoothing", &cfg_.master_enabled)) dirty = true;
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Master kill switch. When off, the driver passes Knuckles bone\n"
			"arrays through untouched -- exactly the same behaviour as a build\n"
			"without the finger-smoothing feature compiled in.");
	}

	ImGui::Spacing();
	ImGui::BeginDisabled(!cfg_.master_enabled);

	int smoothness = cfg_.smoothness;
	if (ImGui::SliderInt("Strength##fingers", &smoothness, 0, 100, "%d%%")) {
		if (smoothness < 0) smoothness = 0;
		if (smoothness > 100) smoothness = 100;
		cfg_.smoothness = smoothness;
		dirty = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"0   = no smoothing (each frame snaps to incoming bones).\n"
			"50  = moderate (good starting point).\n"
			"100 = heavy lag (slerp factor 0.05 per frame). Never fully freezes.\n"
			"Drag the slider live and feel the change immediately in-game.");
	}

	ImGui::Spacing();
	ImGui::Text("Per-finger toggles (uncheck to bypass that finger only)");
	ImGui::TextDisabled("Useful for isolating a finger whose smoothing produces an artifact.");

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

	ImGui::Spacing();
	if (ImGui::Button("Enable all fingers")) {
		if (cfg_.finger_mask != protocol::kAllFingersMask) {
			cfg_.finger_mask = protocol::kAllFingersMask;
			dirty = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable all fingers")) {
		if (cfg_.finger_mask != 0) {
			cfg_.finger_mask = 0;
			dirty = true;
		}
	}

	ImGui::EndDisabled();

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
