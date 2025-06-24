#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <Windows.h>
#include <tchar.h>

#define ROWS 8
#define COLS 8
float fuelMap[ROWS][COLS] = { 0 };
float throttleMap[ROWS][COLS] = { 0 };


HANDLE hSerial = INVALID_HANDLE_VALUE;
char g_serialReadBuffer[1024] = "nothing";

bool OpenSerialPort(const char* portName) {
    hSerial = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) return false;

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        return false;
    }

    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        return false;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        CloseHandle(hSerial);
        return false;
    }

    return true;
}


void SendSerialData(const char* data) {
    if (!hSerial || hSerial == INVALID_HANDLE_VALUE) return;
    DWORD bytesWritten;
    WriteFile(hSerial, data, strlen(data), &bytesWritten, NULL);
}

void ReadSerialData() {
    if (!hSerial || hSerial == INVALID_HANDLE_VALUE) return;

    COMSTAT status;
    DWORD errors;
    DWORD bytesRead;
    char tempBuf[1024];

    // Only proceed if data is waiting
    if (!ClearCommError(hSerial, &errors, &status)) return;
    if (status.cbInQue == 0) return;

    BOOL result = ReadFile(hSerial, tempBuf, sizeof(tempBuf) - 1, &bytesRead, NULL);
    if (result && bytesRead > 0) {
        tempBuf[bytesRead] = '\0';
        strcpy_s(g_serialReadBuffer, sizeof(g_serialReadBuffer), tempBuf);
    }
}


void SendMapOverSerial(float map[ROWS][COLS]) {
    char buffer[1024] = "<STK>";
    char val[16];
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            snprintf(val, sizeof(val), "%.1f,", map[r][c]);
            strcat_s(buffer, val);
        }
    }
    strcat_s(buffer, "<END>");
    SendSerialData(buffer);
}

void DrawFuelMap() {
    ImGui::Text("MAP_Fuelling Table");
    ImGui::BeginChild("TableFrame", ImVec2(0, 400), true, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::BeginTable("MAPTable", COLS + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("MAP \\ RPM");
        for (int col = 0; col < COLS; ++col) {
            char colLabel[16];
            sprintf_s(colLabel, "%d", 1000 + col * 500);
            ImGui::TableSetupColumn(colLabel);
        }

        ImGui::TableHeadersRow();

        for (int row = 0; row < ROWS; ++row) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", 10 + row * 10);

            for (int col = 0; col < COLS; ++col) {
                ImGui::TableSetColumnIndex(col + 1);
                ImGui::PushID(row * COLS + col);
                ImGui::SetNextItemWidth(60);
                ImGui::InputFloat("##val", &fuelMap[row][col], 0, 0, "%.1f", ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("Send to ECU")) {
        SendMapOverSerial(fuelMap);
    }

    ImGui::EndChild();
}

void DrawThrottleMap() {
    ImGui::Text("Throttle Correction Table");

    ImGui::BeginChild("ThrottleTableFrame", ImVec2(0, 400), true, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::BeginTable("ThrottleTable", COLS + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Throttle %");
        for (int col = 0; col < COLS; ++col) {
            char colLabel[16];
            sprintf_s(colLabel, "%d", 1000 + col * 500);
            ImGui::TableSetupColumn(colLabel);
        }

        ImGui::TableHeadersRow();

        for (int row = 0; row < ROWS; ++row) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", row * 10);  // Throttle: 0, 10, ..., 70

            for (int col = 0; col < COLS; ++col) {
                ImGui::TableSetColumnIndex(col + 1);
                ImGui::PushID(1000 + row * COLS + col);
                ImGui::SetNextItemWidth(60);
                ImGui::InputFloat("##throttleVal", &throttleMap[row][col], 0, 0, "%.1f", ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }
    if (ImGui::Button("Send to ECU")) {
        SendMapOverSerial(fuelMap);
    }

    ImGui::EndChild();
}
void DrawHomeTab() {
    static char portName[16] = "COM5";
    static bool isConnected = false;
    static char manualCmd[128] = "";

    isConnected = (hSerial != INVALID_HANDLE_VALUE);

    ImGui::Text("Select Serial Port:");
    ImGui::InputText("##PortName", portName, IM_ARRAYSIZE(portName));
    ImGui::SameLine();

    if (ImGui::Button(isConnected ? "Disconnect" : "Connect")) {
        if (!isConnected) {
            if (OpenSerialPort(portName)) {
                isConnected = true;
            }
        }
        else {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            isConnected = false;
        }
    }

    ImGui::SameLine();
    ImGui::Text("%s", isConnected ? "[Connected ✅]" : "[Disconnected ❌]");

    ImGui::Separator();

    if (ImGui::Button("Request Data")) {
        SendSerialData("<REQ>");
    }
    ImGui::SameLine();
    if (ImGui::Button("Start ECU")) {
        SendSerialData("<START>");
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop ECU")) {
        SendSerialData("<STOP>");
    }

    ImGui::Spacing();
    ImGui::Text("Manual Command:");
    ImGui::InputText("##ManualCmd", manualCmd, IM_ARRAYSIZE(manualCmd));
    ImGui::SameLine();
    if (ImGui::Button("Send")) {
        SendSerialData(manualCmd);
    }

    ImGui::Spacing();
    ImGui::Text("Last Response:");
    if (isConnected) {
        ReadSerialData();  // Safe only if connected
    }
    ImGui::TextWrapped("%s", g_serialReadBuffer);
}



int main(int, char**) {
    OpenSerialPort("COM5");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("ECU Tuner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f);

    // Light theme with violet accents
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsLight();

    style.WindowRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.ScrollbarRounding = 5.0f;
    style.FramePadding = ImVec2(12, 6);
    style.ItemSpacing = ImVec2(10, 8);

        ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]        = ImVec4(0.95f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]          = ImVec4(0.6f, 0.5f, 0.9f, 0.65f);
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.6f, 0.5f, 0.9f, 0.85f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.5f, 0.4f, 0.8f, 1.00f);
    colors[ImGuiCol_Button]          = ImVec4(0.6f, 0.5f, 0.9f, 0.60f);
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.7f, 0.6f, 1.0f, 0.85f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.5f, 0.4f, 0.8f, 1.00f);
    colors[ImGuiCol_TableRowBg]      = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]   = ImVec4(0.95f, 0.95f, 0.95f, 0.50f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Main", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus
        );

        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Home")) {
				DrawHomeTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("MAP_Fuelling")) {
                DrawFuelMap();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Throttle Correction")) {
                DrawThrottleMap();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.95f, 0.95f, 0.98f, 1.0f);  // light background
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
