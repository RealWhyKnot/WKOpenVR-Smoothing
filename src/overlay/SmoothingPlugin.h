#pragma once

#include "Config.h"
#include "FeaturePlugin.h"
#include "IPCClient.h"

#include <string>

class SmoothingPlugin final : public openvr_pair::overlay::FeaturePlugin
{
public:
	const char *Name() const override { return "Smoothing"; }
	const char *FlagFileName() const override { return "enable_smoothing.flag"; }
	const char *PipeName() const override { return OPENVR_PAIRDRIVER_SMOOTHING_PIPE_NAME; }

	void OnStart(openvr_pair::overlay::ShellContext &context) override;
	void Tick(openvr_pair::overlay::ShellContext &context) override;
	void DrawTab(openvr_pair::overlay::ShellContext &context) override;

private:
	SmoothingConfig cfg_ = LoadConfig();
	SmoothingIPCClient ipc_;
	std::string connectError_;

	void ConnectIfNeeded();
	void SendConfig();
};
