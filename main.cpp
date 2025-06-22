#include "imgui.h"                      // Core ImGui API
#include "backends/imgui_impl_sdl2.h"   // SDL2 bindings
#include "backends/imgui_impl_opengl3.h" // OpenGL3 bindings

#include <SDL.h>                       // SDL2 core
#include <SDL_opengl.h>  
#include <stdio.h>
#include <Windows.h>
#include <tchar.h>

// Example static headers
#define ROWS 8
#define COLS 8
float fuelMap[ROWS][COLS] = {0};  // Default initialized




HANDLE hSerial = INVALID_HANDLE_VALUE; // Handle to the serial port
// OpenGL for rendering
char g_serialReadBuffer[1024] = "nothing"; // Buffer for reading serial data

bool OpenSerialPort(const char* portName)
{
	hSerial = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSerial == INVALID_HANDLE_VALUE)
	{
		//fprintf(stderr, "Error opening serial port %s: %s\n", portName, GetLastError());
		return false;
	}
	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	if (!GetCommState(hSerial, &dcbSerialParams))
	{
		//fprintf(stderr, "Error getting serial port state: %s\n", GetLastError());
		CloseHandle(hSerial);
		return false;
	}
	dcbSerialParams.BaudRate = CBR_9600; // Set baud rate
	dcbSerialParams.ByteSize = 8; // Set byte size
	dcbSerialParams.StopBits = ONESTOPBIT; // Set stop bits
	dcbSerialParams.Parity = NOPARITY; // Set parity
	if (!SetCommState(hSerial, &dcbSerialParams))
	{
		//fprintf(stderr, "Error setting serial port state: %s\n", GetLastError());
		CloseHandle(hSerial);
		return false;
	}
	//printf("Serial port %s opened successfully.\n", portName);
	return true;
	}
void SendSerialData(const char* data) {
	if (!hSerial || hSerial == INVALID_HANDLE_VALUE) return;

	DWORD bytesWritten;
	WriteFile(hSerial, data, strlen(data), &bytesWritten, NULL);
}
void ReadSerialData() {
	if (!hSerial || hSerial == INVALID_HANDLE_VALUE) return;

	char tempBuf[1024];
	DWORD bytesRead;
	BOOL result = ReadFile(hSerial, tempBuf, sizeof(tempBuf) - 1, &bytesRead, NULL);

	if (result && bytesRead > 0) {
		tempBuf[bytesRead] = '\0';
        // Replace unsafe strcpy with strcpy_s for secure string copy
        // Ensure the destination buffer size is passed to strcpy_s
        strcpy_s(g_serialReadBuffer, sizeof(g_serialReadBuffer), tempBuf);
		g_serialReadBuffer[sizeof(g_serialReadBuffer) - 1] = '\0';
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
	SendSerialData(buffer); // your existing function
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
			ImGui::Text("%d", 10 + row * 10);  // e.g. MAP: 10, 20...

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




int main(int, char** argv)
{
	OpenSerialPort("COM5"); // Change to your serial port name
	
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		//fprintf(stderr, "Error: %s\n", SDL_GetError());
		return 1;
	}
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


	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::StyleColorsLight();

	style.WindowRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 2.0f;
	style.ScrollbarRounding = 3.0f;

	style.WindowBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;

	style.FramePadding = ImVec2(10, 6);
	style.ItemSpacing = ImVec2(10, 6);


	ImVec4* colors = style.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.70f, 0.85f, 1.00f, 0.65f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.85f, 1.00f, 0.85f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.65f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.70f, 0.80f, 0.95f, 0.60f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.80f, 0.95f, 0.85f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.70f, 0.90f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.95f, 0.95f, 0.95f, 0.50f);


	style.WindowRounding = 6.0f;
	style.FrameRounding = 6.0f;
	style.ScrollbarRounding = 5.0f;
	style.FramePadding = ImVec2(12, 6);
	style.ItemSpacing = ImVec2(10, 8);



	ImGui::StyleColorsClassic();

	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 330");

	bool running = true;
	SDL_Event event;
	while (running) 
	{
	     // Read serial data in the main loop
		while(SDL_PollEvent(&event)) 
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT) 
			{
				running = false;
			}
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
			ImGui::SetNextWindowSize(ImVec2(1024, 768), ImGuiCond_Always);
			ImGui::Begin("ECU Tuner", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

			if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
				if (ImGui::BeginTabItem("Home")) {
					ImGui::Text("Welcome to ECU Tuner GUI.");
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("MAP_Fuelling")) {
					DrawFuelMap(); // We'll define this
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Throttle Correction")) {
					//DrawThrottleMap(); // We'll define this
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::End();



			
			
			
		
			ImGui::Render();
			glViewport(0, 0, 1280,800);
			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			SDL_GL_SwapWindow(window);
		}
	}

	return 0;




}