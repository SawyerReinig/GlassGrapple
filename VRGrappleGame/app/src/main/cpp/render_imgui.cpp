/* ------------------------------------------------ *
 * The MIT License (MIT)
 * Copyright (c) 2020 terryky1220@gmail.com
 * ------------------------------------------------ */
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "render_imgui.h"
#include "VRGame.h"

#define DISPLAY_SCALE_X 1
#define DISPLAY_SCALE_Y 1
#define _X(x)       ((float)(x) / DISPLAY_SCALE_X)
#define _Y(y)       ((float)(y) / DISPLAY_SCALE_Y)

static int  s_win_w;
static int  s_win_h;

static ImVec2 s_win_size[10];
static ImVec2 s_win_pos [10];
static int    s_win_num = 0;
static ImVec2 s_mouse_pos;
ImFont* synthFont;


int
init_imgui (int win_w, int win_h)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();


    synthFont = io.Fonts->AddFontFromFileTTF(
            "/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/Audiowide-Regular.ttf",
            12.0f  // Font size in pixels
    );

    if (!synthFont) {
        __android_log_print(ANDROID_LOG_ERROR, "IMGUI", "Failed to load Audiowide-Regular.ttf");
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplOpenGL3_Init(NULL);

    io.DisplaySize = ImVec2 (_X(win_w), _Y(win_h));
    io.DisplayFramebufferScale = {DISPLAY_SCALE_X, DISPLAY_SCALE_Y};

    s_win_w = win_w;
    s_win_h = win_h;

    return 0;
}

void
imgui_mousebutton (int button, int state, int x, int y)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(_X(x), (float)_Y(y));

    if (state)
        io.MouseDown[button] = true;
    else
        io.MouseDown[button] = false;

    s_mouse_pos.x = x;
    s_mouse_pos.y = y;
}

void
imgui_mousemove (int x, int y)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(_X(x), _Y(y));

    s_mouse_pos.x = x;
    s_mouse_pos.y = y;
}

bool
imgui_is_anywindow_hovered ()
{
#if 1
    int x = _X(s_mouse_pos.x);
    int y = _Y(s_mouse_pos.y);
    for (int i = 0; i < s_win_num; i ++)
    {
        int x0 = s_win_pos[i].x;
        int y0 = s_win_pos[i].y;
        int x1 = x0 + s_win_size[i].x;
        int y1 = y0 + s_win_size[i].y;
        if ((x >= x0) && (x < x1) && (y >= y0) && (y < y1))
            return true;
    }
    return false;
#else
    return ImGui::IsAnyWindowHovered();
#endif
}

static void
render_gui (scene_data_t *scn_data, int highscore)  // I Should try messing with fonts :)
{
//    int win_w = 300;
    int win_h = 0;
    int win_x = 0;
    int win_y = 0;

    s_win_num = 0;

    /* Show main window */
    win_y += win_h;
    win_h = 120;
    ImGui::SetNextWindowPos (ImVec2(_X(win_x), _Y(win_y)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
//    ImGui::PushFont(synthFont);  // Use custom font

    ImGui::Begin(".....................................");
    {
        ImGui::SetWindowFontScale(2.5f);  // Scale 1.0 is default

        const char* title = "Welcome To GLASS GRAPPLE";
        float win_width = ImGui::GetWindowSize().x;
        float text_width = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((win_width - text_width) * 0.5f);  // center
        ImGui::Text("%s", title);

        ImGui::Text("Swing Through As Many Walls As You Can");


        char HighscoreText[64];
        snprintf(HighscoreText, sizeof(HighscoreText), "HighScore: %d", highscore);

        float HighscoreText_width = ImGui::CalcTextSize(HighscoreText).x;
        ImGui::SetCursorPosX((win_width - HighscoreText_width) * 0.5f);  // center
        ImGui::Text("%s", HighscoreText);




        //testing out an outline:
//        ImDrawList* draw_list = ImGui::GetWindowDrawList();
//        ImVec2 pos = ImGui::GetCursorScreenPos();
//        const char* text = "Welcome To GLASS GRAPPLE";
//
//        // Glow color
//        ImU32 glow = IM_COL32(0, 255, 255, 150);  // Cyan with transparency
//
//        // Draw multiple offset layers (glow)
//        for (int dx = -1; dx <= 1; dx++) {
//            for (int dy = -1; dy <= 1; dy++) {
//                if (dx == 0 && dy == 0) continue;
//                draw_list->AddText(synthFont, 12.0f, ImVec2(pos.x + dx, pos.y + dy), glow, text);
//            }
//        }
//
//        // Top layer (main text)
//        draw_list->AddText(synthFont, 12.0f, pos, IM_COL32_WHITE, text);
//
//        // Move cursor down so next item doesn't overlap
//        ImGui::Dummy(ImVec2(0, synthFont->FontSize));









//        ImGui::Text("HighScore: %f", PlayerPosOffset.z);

        s_win_pos [s_win_num] = ImGui::GetWindowPos  ();
        s_win_size[s_win_num] = ImGui::GetWindowSize ();
        s_win_num ++;
    }
    ImGui::End();

}

int
invoke_imgui (scene_data_t *scn_data, int highscore)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    render_gui (scn_data, highscore);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return 0;
}


static void render_Debug_gui (scene_data_t *scn_data, XrCompositionLayerProjectionView View)
{
    int win_w = 300;
    int win_h = 0;
    int win_x = 0;
    int win_y = 0;

    s_win_num = 0;

    /* Show main window */
    win_y += win_h;
    win_h = 200;
    ImGui::SetNextWindowPos (ImVec2(_X(win_x), _Y(win_y)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(_X(win_w), _Y(win_h)), ImGuiCond_FirstUseEver);

    win_y += win_h;
    win_h = 200;
    ImGui::SetNextWindowPos (ImVec2(_X(win_x), _Y(win_y)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(_X(win_w), _Y(win_h)), ImGuiCond_FirstUseEver);
    ImGui::Begin("View Info");
    {
//        ImGui::Text("Elapsed  : %d [ms]",    scn_data->elapsed_us / 1000);
        ImGui::Text("FPS : %6.3f", 1000/scn_data->interval_ms);
        ImGui::Text("HeadPosX : %6.3f", View.pose.position.x);
        ImGui::Text("HeadPosY : %6.3f", View.pose.position.y);
        ImGui::Text("HeadPosZ : %6.3f", View.pose.position.z);




//        ImGui::Text("Press Left Menu Button For Menu");


//        s_win_pos [s_win_num] = ImGui::GetWindowPos  ();
//        s_win_size[s_win_num] = ImGui::GetWindowSize ();
        s_win_num ++;
    }
    ImGui::End();
}

int invoke_Debug_imgui (scene_data_t *scn_data, XrCompositionLayerProjectionView View)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    render_Debug_gui (scn_data, View);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return 0;
}
