#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "ui.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
int main(int argc, char **argv) {
    std::string openFile, probe, testInput;
    float scale = 1.0f;
    int frames = 0;
    if (auto e = std::getenv("SAPF_UI_SCALE"))
        scale = std::clamp(float(std::atof(e)), .75f, 3.f);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::puts("SAPF Experiments: C++ / Dear ImGui reconstruction\n  --open FILE       open a saved "
                      "document\n  --probe FILE      write widget coordinates/state for integration tests\n  "
                      "--test-input FILE inject test mouse/key events (with --probe)\n  --frames N        "
                      "exit after N frames (smoke test)\n  --scale N         interface "
                      "scale, 0.75..3\nEnvironment: SAPF_ENGINE=/path/to/run-sapf; SAPF_UI_SCALE=2");
            return 0;
        } else if (i + 1 < argc && arg == "--open")
            openFile = argv[++i];
        else if (i + 1 < argc && arg == "--test-input")
            testInput = argv[++i];
        else if (i + 1 < argc && arg == "--probe")
            probe = argv[++i];
        else if (i + 1 < argc && arg == "--frames")
            frames = std::max(1, std::atoi(argv[++i]));
        else if (i + 1 < argc && arg == "--scale")
            scale = std::clamp(float(std::atof(argv[++i])), .75f, 3.f);
        else {
            std::fprintf(stderr, "Unknown/missing argument: %s\n", argv[i]);
            return 2;
        }
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow(
        "SAPF Experiments", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, int(1200 * scale),
        int(850 * scale), SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "Window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    auto gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "OpenGL: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImFontConfig font;
    font.SizePixels = 15 * scale;
    io.Fonts->AddFontDefault(&font);
    ImGui_ImplSDL2_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 150");
    int result = 0;
    try {
        sapfui::UI app(sapfui::findLauncher(), scale);
        app.probe = probe;
        app.testInput = testInput;
        if (!openFile.empty())
            app.loadDocument(openFile);
        bool quit = false;
        int count = 0;
        while (!quit) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT ||
                    (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                     event.window.windowID == SDL_GetWindowID(window)))
                    app.requestClose();
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            app.driveTestInput();
            ImGui::NewFrame();
            app.draw();
            ImGui::Render();
            int width, height;
            SDL_GL_GetDrawableSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(.12f, .12f, .12f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);
            app.writeProbe();
            quit = app.shouldClose() || (frames > 0 && ++count >= frames);
            SDL_Delay(8);
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "SAPF Experiments: %s\n", e.what());
        result = 1;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
