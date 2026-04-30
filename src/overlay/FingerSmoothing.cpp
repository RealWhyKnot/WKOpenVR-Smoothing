// FingerSmoothing overlay entry point. Minimal desktop window that hosts an
// ImGui UI for the SmoothingConfig the driver consumes via named pipe. The
// SteamVR-overlay rendering path SpaceCalibrator does is intentionally skipped
// for this milestone — we get a normal Windows window first, add the in-VR
// dashboard overlay later.

#include "stdafx.h"
#include "Configuration.h"
#include "EmbeddedFiles.h"
#include "IPCClient.h"
#include "UserInterface.h"
#include "BuildStamp.h"
#include "Version.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <GL/gl3w.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;

static void GLFWErrorCallback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Try to (re)connect to the driver. Non-fatal on failure: the UI still renders
// with a "disconnected" indicator and config edits are saved to disk; they'll
// be picked up the next time the driver reads its persisted state. We retry
// every ~2 seconds until connected.
static bool TryConnect(IPCClient &ipc)
{
    try {
        ipc.Connect();
        return true;
    } catch (const std::exception &e) {
        fprintf(stderr, "[overlay] IPC connect failed: %s\n", e.what());
        return false;
    }
}

static bool SendConfig(IPCClient &ipc, const protocol::SmoothingConfig &cfg)
{
    protocol::Request req(protocol::RequestSetConfig);
    req.setConfig = cfg;
    try {
        ipc.SendBlocking(req);
        return true;
    } catch (const std::exception &e) {
        fprintf(stderr, "[overlay] SendConfig failed: %s\n", e.what());
        return false;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    glfwSetErrorCallback(GLFWErrorCallback);
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow *window = glfwCreateWindow(720, 720, "FingerSmoothing", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (gl3wInit() != 0) {
        fprintf(stderr, "gl3wInit failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't drop imgui.ini next to the exe

    // Embedded font so the overlay doesn't have to ship a separate .ttf.
    ImFontConfig fontCfg{};
    fontCfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryCompressedTTF(
        DroidSans_compressed_data, DroidSans_compressed_size, 18.0f, &fontCfg);

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Load persisted config; this is the in-memory source of truth for the
    // session. The IPC SetConfig is fire-and-forget — driver accepts the new
    // values and applies them next frame.
    protocol::SmoothingConfig cfg = fs_config::Load();

    IPCClient ipc;
    bool ipcConnected = TryConnect(ipc);
    if (ipcConnected) {
        // Push the loaded config to the driver so the driver and overlay agree
        // on state from the moment the user opens the overlay.
        SendConfig(ipc, cfg);
    }

    using clock = std::chrono::steady_clock;
    auto lastReconnectAttempt = clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Periodic reconnect attempts when we don't have a live pipe. SteamVR
        // may not be running when the overlay launches.
        if (!ipcConnected) {
            auto now = clock::now();
            if (now - lastReconnectAttempt > std::chrono::seconds(2)) {
                ipcConnected = TryConnect(ipc);
                if (ipcConnected) SendConfig(ipc, cfg);
                lastReconnectAttempt = now;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Make the FingerSmoothing window fill the GLFW client area.
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2((float)fbW, (float)fbH));

        bool dirty = fs_ui::Render(cfg, ipc, ipcConnected);
        if (dirty) {
            fs_config::Save(cfg);
            if (ipcConnected) {
                if (!SendConfig(ipc, cfg)) ipcConnected = false;
            }
        }

        ImGui::Render();
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
