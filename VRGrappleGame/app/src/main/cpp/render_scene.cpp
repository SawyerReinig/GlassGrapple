#include <GLES3/gl31.h>
#include <common/xr_linear.h>
#include "util_egl.h"
#include "util_oxr.h"
#include "util_shader.h"
#include "util_matrix.h"
#include "util_debugstr.h"
#include "util_render_target.h"
#include "teapot.h"

#include "VRGame.h"
//#include "VRGame.cpp" 🧩


#include "render_scene.h"
#include "render_stage.h"
#include "render_texplate.h"
#include "render_imgui.h"
#include "assertgl.h"
#include <vector>
#include <random>
#include "OboePlayer.h"


#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>



static shader_obj_t     s_sobj;
static render_target_t  s_rtarget;

static shader_obj_t     RainbowShader;
static shader_obj_t     GrappleShader;

static shader_obj_t     SkyboxShaderObject;
static shader_obj_t     PortalShaderObject;


// Engine and output mix
SLObjectItf engineObject = nullptr;
SLEngineItf engineEngine = nullptr;
SLObjectItf outputMixObject = nullptr;

//    static render_target_t  PaintShaderTarget;

#define UI_WIN_W 300
#define UI_WIN_H 740


struct Wall{
    float WallX;          //Walls start at -15
    float holeLeftRight;  //should be between 0.1 and 0.9
    float holeHeight;
    float holeWidth;
    Vec3 WallColor;
};



//Vec3 Amber = Vec3(1.0f, 0.768f, 0.24f);
//Vec3 Pink = Vec3(0.937, 0.278, 0.435);
//Vec3 Blue = Vec3(0.106, 0.604, 0.667);
//Vec3 Green = Vec3(0.024, 0.839, 0.627);
//Vec3 Gray = Vec3(0.973, 1, 0.8);
//Vec3 Brown = Vec3(0.929, 0.749, 0.522);


//testing out synthwave colors.
Vec3 WallColor1 = ColorFrom255(255,153,0); //orange
Vec3 WallColor2 = ColorFrom255(255,97,198); // pink
Vec3 WallColor3 = ColorFrom255(92,236,207); // light blue
Vec3 WallColor4 = ColorFrom255(244,255,9); //yellow
//Vec3 Gray = ColorFrom255(255,0,0);
//Vec3 Brown = ColorFrom255(255,0,0);

//Vec3 worldPink = Vec3(0.756f, 0.1, 0.76f);
//Vec3 worldPink = Vec3(1.0f, 0.36, 0.63f);
//Vec3 worldYellow = VColorFrom255(255,255,20);
//Vec3 worldYellow = ColorFrom255(229,150,51);
//Vec3 worldPink = ColorFrom255(255,90,190);
//Vec3 worldPink = ColorFrom255(255,255,20);
Vec3 worldPink = WallColor1;
Vec3 worldYellow = WallColor3;

int lastPickedColor = 0;
std::vector<Vec3> WallColors;

std::vector<Wall> Walls;
float disBetweenWalls = 12.0f;
int numWalls = 150;

int highscore = 0;
int currentLevel = 1;
int totalNumLevels = 2;

bool fastRespawn = false;

float clippingplane = 0;

//OboeSinePlayer* m_oboePlayer = new OboeSinePlayer ();
OboeMp3Player* pigSoundPlayer;
OboeMp3Player* winSoundPlayer;

OboeMp3Player* grappleSoundPlayer;

OboeMp3Player* songPlayer;


const float deltaTime = 0.0138;

Vec3 playerPosOffset;
Vec3 playerVel;

Vec3 grapplePointRight, grapplePointLeft;
Vec3 handPosRight, handPosLeft;
Vec3 leftHandForward, rightHandForward;

bool rightGrapplePlaced, leftGrapplePlaced;

//static char s_strVS[] = "                                   \n\
//                                                            \n\
//attribute vec4  a_Vertex;                                   \n\
//attribute vec4  a_Color;                                    \n\
//varying   vec4  v_color;                                    \n\
//uniform   mat4  u_PMVMatrix;                                \n\
//                                                            \n\
//void main(void)                                             \n\
//{                                                           \n\
//    gl_Position = u_PMVMatrix * a_Vertex;                   \n\
//    v_color     = a_Color;                                  \n\
//}                                                           ";

//static char s_strFS[] = "                                   \n\
//precision mediump float;                                    \n\
//varying   vec4  v_color;                                    \n\
//                                                            \n\
//void main(void)                                             \n\
//{                                                           \n\
//    gl_FragColor = vec4(v_color.rgb, 0.75);                                 \n\
//}                                                           ";


static char s_strVS[] = R"(
attribute vec4  a_Vertex;
attribute vec4  a_Color;
varying   vec4  v_color;
varying   vec3  v_Position;
uniform   mat4  u_PMVMatrix;

void main(void)
{
    gl_Position = u_PMVMatrix * a_Vertex;
    v_color     = a_Color;
    v_Position  = a_Vertex.xyz; //not sure if we are doing the right matrix things here, think its fine
}
)";

//trying to do raysasting of the grid here.
static char s_strFS[] = R"(
precision highp float;
varying vec3 v_Position;
varying vec4 v_color;

uniform vec3 viewPos; // camera world position
uniform vec3 envColor; // Basic Environment color



uniform float gridSize;           // Size of each cell
uniform float lineWidth;          // Thickness of lines
uniform vec3  lineColor;          // Color of the grid lines
uniform vec3  musicLineColor;     // Color of the grid lines when the music is at max
uniform float songBrightness;     // Loudness of the music
uniform vec3  bgColor;            // Color of background

//return a color based on a position and grid parameters. Used for reflections.
vec3 getGridColorAtPoint(
    vec3 pos,
    float gridSize,
    float lineWidth,
    vec3 lineColor,
    vec3 musicLineColor,
    float songBrightness,
    vec3 bgColor,
    vec2 planeAxes  // (0,1)=XY, (0,2)=XZ, (2,1)=ZY

) {
    // Project the 3D point into the 2D plane of the grid
    float coordA = (planeAxes.x < 0.5) ? pos.x : ((planeAxes.x < 1.5) ? pos.y : pos.z);
    float coordB = (planeAxes.y < 0.5) ? pos.x : ((planeAxes.y < 1.5) ? pos.y : pos.z);

    // Compute distances to the nearest grid line
    float dA = min(mod(coordA, gridSize), gridSize - mod(coordA, gridSize));
    float dB = min(mod(coordB, gridSize), gridSize - mod(coordB, gridSize));

    float glowSize = gridSize * 0.05;

    float glowA = exp(-pow(dA / glowSize, 1.04));
    float glowB = exp(-pow(dB / glowSize, 1.04));

    float coreA = smoothstep(lineWidth + 0.005, lineWidth - 0.005, dA);
    float coreB = smoothstep(lineWidth + 0.005, lineWidth - 0.005, dB);

    float glowStrength = clamp(coreA + coreB + glowA + glowB, 0.0, 1.0);

    vec3 glowColor = mix(lineColor, musicLineColor, clamp(songBrightness - 0.4, 0.0, 1.0));
    return mix(bgColor, glowColor, glowStrength);
}

bool intersectPlane(
    vec3 rayOrigin, vec3 rayDir,
    int axis, float planeCoord,
    out float t, out vec3 hitPos
) {
    float dir = rayDir[axis];
    if (abs(dir) < 0.0001)
        return false;

    t = (planeCoord - rayOrigin[axis]) / dir;
    if (t < 0.0)
        return false;

    hitPos = rayOrigin + rayDir * t;
    return true;
}


void main() {
    vec3 normal = normalize(vec3(-1.0, 0.0, 0.0));  // X-facing wall
    vec3 viewDir = normalize(viewPos - v_Position);

    if (dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }

    // Fake reflection amount based on angle
    float fresnel = pow(dot(normal, viewDir), 3.0);

    // Fake rainbow based on fresnel
    vec3 rainbowColor = vec3(
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.0)),
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.333)),
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.666))
    );
    vec3 fresnelAndRainbow = mix(rainbowColor, envColor, 0.5); //this is usually 0.75

    vec3 reflectedDir = reflect(-viewDir, normal);
    vec3 bestHit = vec3(0.0);
    vec2 planeAxisID;
    float bestT = 1e9;
    bool found = false;

    vec3 tempHit;
    float t;

    // Floor (y = 0)
    if (intersectPlane(v_Position, reflectedDir, 1, 0.0, t, tempHit)) {
        if (tempHit.y >= 0.0 && tempHit.y <= 15.0 && abs(tempHit.z) <= 15.0 && t < bestT) {
            bestT = t;
            bestHit = tempHit;
            found = true;
            planeAxisID = vec2(0.0,2.0);
        }
    }

    // Ceiling (y = 15)
    if (intersectPlane(v_Position, reflectedDir, 1, 15.0, t, tempHit)) {
        if (tempHit.y >= 0.0 && tempHit.y <= 15.0 && abs(tempHit.z) <= 15.0 && t < bestT) {
            bestT = t;
            bestHit = tempHit;
            found = true;
            planeAxisID = vec2(0.0,2.0);
        }
    }

    // Back wall (x = 15)
    if (intersectPlane(v_Position, reflectedDir, 0, 15.0, t, tempHit)) {
        if (tempHit.y >= 0.0 && tempHit.y <= 15.0 && abs(tempHit.z) <= 15.0 && t < bestT) {
            bestT = t;
            bestHit = tempHit;
            found = true;
            planeAxisID = vec2(2.0,1.0);

        }
    }

    // Front/Back grid (z = ±15)
    float zTarget = reflectedDir.z > 0.0 ? 15.0 : -15.0;
    if (intersectPlane(v_Position, reflectedDir, 2, zTarget, t, tempHit)) {
        if (tempHit.y >= 0.0 && tempHit.y <= 15.0 && t < bestT) {
            bestT = t;
            bestHit = tempHit;
            found = true;
            planeAxisID = vec2(0.0,1.0);

        }
    }

//    bool validHit = found && length(bestHit - v_Position) < 40.0;
    bool validHit = found;

    vec3 reflectedColor = validHit
    ? getGridColorAtPoint(bestHit, gridSize, lineWidth, lineColor, musicLineColor, songBrightness, bgColor, planeAxisID)
    : envColor;


    // Blend environment color into base color based on fresnel
    vec3 color = mix(fresnelAndRainbow, v_color.rgb, 0.5);  // 0.5 = strength of reflection
//    vec3 color = mix(v_color.rgb, v_color.rgb, 1.0);  // removing the fresnel to see if I actually like it.

    vec3 colorWithReflections = mix(color,reflectedColor , 0.2);

    gl_FragColor = vec4(colorWithReflections, 0.75);
}
)";


static char RainbowVertexShader[] = R"(
attribute vec4  a_Vertex;
attribute vec4  a_Color;
varying   vec4  v_color;
varying   vec3  v_Position;
uniform   mat4  u_PMVMatrix;

void main(void)
{
    gl_Position = u_PMVMatrix * a_Vertex;
    v_color     = a_Color;
    v_Position  = a_Vertex.xyz; //not sure if we are doing the right matrix things here, think its fine
}
)";


static char RainbowFragmentShader[] = R"(
precision highp float;
varying vec3 v_Position;
varying vec4 v_color;

uniform vec3 viewPos; // camera world position
uniform vec3 envColor; // Basic Environment color

void main() {
    vec3 normal = normalize(vec3(-1.0, 0.0, 0.0));  // X-facing wall
    vec3 viewDir = normalize(viewPos - v_Position);

    if (dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }

    // Fake reflection amount based on angle
    float fresnel = pow(dot(normal, viewDir), 3.0);

    // Fake rainbow based on fresnel
    vec3 rainbowColor = vec3(
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.0)),
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.333)),
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.666))
    );
    // Blend environment color into base color based on fresnel
    vec3 color = mix(rainbowColor, v_color.rgb, 0.5);  // 0.5 = strength of reflection

    gl_FragColor = vec4(rainbowColor, 1.0);
}
)";


static char GrappleVS[] = R"(
attribute vec4  a_Vertex;
attribute vec4  a_Color;
varying   vec4  v_color;
varying   vec3  v_Position;
uniform   mat4  u_PMVMatrix;

void main(void)
{
    gl_Position = u_PMVMatrix * a_Vertex;
    v_color     = a_Color;
    v_Position  = a_Vertex.xyz;
}
)";


static char GrappleFS[] = R"(
precision highp float;
varying vec3 v_Position;
varying vec4 v_color;

uniform vec3 viewPos; // camera world position
uniform vec3 envColor; // Basic Environment color

void main() {
    vec3 normal = normalize(vec3(-1.0, 0.0, 0.0));  // X-facing wall
    vec3 viewDir = normalize(viewPos - v_Position);

    if (dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }

    // Fake reflection amount based on angle
    float fresnel = pow(dot(normal, viewDir), 3.0);

    // Fake rainbow based on fresnel
    vec3 rainbowColor = vec3(
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.0)),
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.333)),
        0.5 + 0.5 * sin(6.2831 * (fresnel + 0.666))
    );
    // Blend environment color into base color based on fresnel
    vec3 color = mix(rainbowColor, v_color.rgb, 0.5);  // 0.5 = strength of reflection

    gl_FragColor = vec4(rainbowColor, 1.0);
}
)";


//Using version 320 to get fwidth so I can so antialiasing.
static char GridVS[] = R"(
#version 320 es

// Input attributes
in vec4 a_Vertex;
in vec3 a_Normal;

// Uniforms
uniform mat4 u_PMVMatrix;
uniform int u_FaceID;
uniform float time;

// Output to fragment shader
out vec3 v_Position;

void main(void)
{
    vec4 pos = a_Vertex;
    gl_Position = u_PMVMatrix * pos;
    v_Position = pos.xyz;
}
)";


static char GridFragShader[] = R"(
#version 320 es

precision highp float;

// Updated for GLSL 3.20
in vec3 v_Position;
out vec4 fragColor;

uniform float gridSize;           // Size of each cell
uniform float lineWidth;          // Thickness of lines
uniform vec3  lineColor;          // Color of the grid lines
uniform vec3  musicLineColor;     // Color of the grid lines when the music is at max
uniform float songBrightness;     // Loudness of the music
uniform vec3  bgColor;            // Color of background
uniform vec2  planeAxes;          // (0,1)=XY, (0,2)=XZ, (2,1)=ZY

// Safe mod helper
float safeMod(float a, float b) {
    return a - b * floor(a / b);
}
vec2 safeMod(vec2 a, vec2 b) {
    return a - b * floor(a / b);
}

void main()
{

float coordA = v_Position[int(planeAxes.x)];
float coordB = v_Position[int(planeAxes.y)];

float dA = min(safeMod(coordA, gridSize), gridSize - safeMod(coordA, gridSize));
float dB = min(safeMod(coordB, gridSize), gridSize - safeMod(coordB, gridSize));

float glowSize = gridSize * 0.05;
float aa = max(fwidth(coordA), fwidth(coordB));

float glowA = exp(-pow(dA / glowSize, 1.04));
float glowB = exp(-pow(dB / glowSize, 1.04));

float coreA = smoothstep(lineWidth + aa, lineWidth - aa, dA);
float coreB = smoothstep(lineWidth + aa, lineWidth - aa, dB);

float combinedA = max(coreA, glowA);
float combinedB = max(coreB, glowB);

float glowStrength = combinedA + combinedB;
glowStrength = clamp(glowStrength, 0.0, 1.0);



vec3 glowColor = mix(lineColor, musicLineColor, songBrightness-0.4);
vec3 color = mix(bgColor, glowColor, glowStrength);

fragColor = vec4(color, 1.0);

}
)";


//we will see if this needs version 320, it might make adapting shader toy shaders easier
static char PortalVS[] = R"(
#version 320 es

// Input attributes
in vec4 a_Vertex;
in vec3 a_Normal;

// Uniforms
uniform mat4 u_PMVMatrix;
uniform int u_FaceID;
uniform float time;

// Output to fragment shader
out vec3 v_Position;

void main(void)
{
    vec4 pos = a_Vertex;

    // Optional animation:
    // pos.xy *= 1.0 + 0.4 * cos(4.0 * time);

    gl_Position = u_PMVMatrix * pos;
    v_Position = pos.xyz;
}
)";


static char PortalFS[] = R"(
#version 320 es

precision highp float;

// Updated for GLSL 3.20
in vec3 v_Position;
out vec4 fragColor;


//left over from the grid, need to clean some of these up.
uniform float time;
uniform vec2 HolePos;

uniform vec3  bgColor;            // Color of background
uniform vec2  planeAxes;          // (0,1)=XY, (0,2)=XZ, (2,1)=ZY

mat2 rotate2d(float angle){
    return mat2(cos(angle),-sin(angle),
                sin(angle),cos(angle));
}

float variation(vec2 v1, vec2 v2, float strength, float speed) {
	return sin(
        dot(normalize(v1) * speed, normalize(v2)) * strength + time * speed
    ) / 100.0;
}

vec3 paintCircle (vec2 uv, vec2 center, float rad, float width) {

    vec2 diff = center-uv;
    float len = length(diff);

    len += variation(diff, vec2(0.0, 1.0), 5.0, 5.0);
    len -= variation(diff, vec2(1.0, 0.0), 5.0, 5.0);

    float circle = smoothstep(rad-width, rad, len) - smoothstep(rad, rad+width, len);
    return vec3(circle);
}

void main()
{
    //I wonder if it would be best to place this plane in the last hole in the walls.
    float coordA = v_Position[int(planeAxes.x)];
    float coordB = v_Position[int(planeAxes.y)];
    vec2 uv_raw = vec2(coordA, coordB);

    // Normalize to [0, 1]
    vec2 planeMin = vec2(-15.0, -15.0);  // Set based on your plane bounds
    vec2 planeMax = vec2(15.0, 15.0);
    vec2 uv = (uv_raw - planeMin) / (planeMax - planeMin);

    vec3 color;
    float radius = 0.04;

    vec2 center;
    center.x = HolePos.x;
    center.y = HolePos.y;

    //paint color circle
    color = paintCircle(uv, center, radius, 0.01);

//    //color with gradient
    vec2 v = rotate2d(time*40.0) * uv;
    color *= vec3(v.x, v.y, 0.7-v.y*v.x);

    //paint white circle
    color += paintCircle(uv, center, radius, 0.001);

    fragColor = vec4(color, 1.0);


}
)";



float RandomFloat(float min, float max)
{
    static std::mt19937 rng(std::random_device{}()); // Random engine
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}
int RandomInt(int min, int max)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

Vec3 PickFromPallet(){
    int index = 0;
    while (index == lastPickedColor){
        index = RandomInt(0,WallColors.size()-1);
    }
    lastPickedColor = index;
    return WallColors.at(index);
}



void initializeEndlessWalls(int NW){
    Walls.clear();
    Wall Wall0;
    Wall0.WallX = -15.0f;
    Wall0.holeLeftRight = 0.5f;
    Wall0.holeWidth = 6.0f;
    Wall0.holeHeight = 6.0f;
    Wall0.WallColor = PickFromPallet();
    Walls.push_back(Wall0);

    for(int i = 1; i < NW; i++){
        Wall NewWall;
        NewWall.WallX = -15.0f - (disBetweenWalls * i);
        NewWall.holeLeftRight = RandomFloat(0.1, 0.9);
        NewWall.holeWidth = RandomFloat(3, 8);
        NewWall.holeHeight = RandomFloat(3, 8);
        NewWall.WallColor = PickFromPallet();
        Walls.push_back(NewWall);
    }
}



void initializeWallsFromCSV(const char* filename) {
    Walls.clear();
    Wall Wall0;
    Wall0.WallX = -15.0f;
    Wall0.holeLeftRight = 0.5f;
    Wall0.holeWidth = 6.0f;
    Wall0.holeHeight = 6.0f;
    Wall0.WallColor = PickFromPallet();
    Walls.push_back(Wall0);
    int lineNumber = 1;

    std::string fullPath = "/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/";
    fullPath += filename;

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        __android_log_print(ANDROID_LOG_ERROR, "WALLLOAD", "❌ Failed to open: %s", fullPath.c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string value;
        std::vector<float> values;
//
        while (std::getline(ss, value, ',')) {
            try {
                values.push_back(std::stof(value));
            } catch (...) {
                __android_log_print(ANDROID_LOG_ERROR, "WALLLOAD", "⚠️ Invalid float in line: %s", line.c_str());
            }
        }

        if (values.size() >= 3) {
            Wall newWall;
            newWall.WallX = -15.0f - (disBetweenWalls * lineNumber);
            newWall.holeLeftRight = values[0];
            newWall.holeWidth     = values[1];
            newWall.holeHeight    = values[2];
            newWall.WallColor     = PickFromPallet();
            Walls.push_back(newWall);
            lineNumber++;

        } else {
            __android_log_print(ANDROID_LOG_ERROR, "WALLLOAD", "⚠️ Skipped malformed line: %s", line.c_str());
        }
    }

    file.close();
    __android_log_print(ANDROID_LOG_INFO, "WALLLOAD", "✅ Loaded %zu walls from %s", Walls.size(), filename);
}




void BeatLevel(int JustBeatLevel){
    rightGrapplePlaced = false;
    leftGrapplePlaced = false;
    grapplePointRight = handPosRight;
    grapplePointLeft = handPosLeft;
    playerPosOffset = Vec3(0.0f,1.0f,0.0f);
    playerVel = Vec3(0.0f,0.0f,0.0f);
    if(JustBeatLevel < totalNumLevels) {
        std::stringstream ss;
        ss << "Level" << (JustBeatLevel + 1) << ".csv";
        std::string nextLevel = ss.str();
        initializeWallsFromCSV(nextLevel.c_str());
        currentLevel = JustBeatLevel + 1;
        winSoundPlayer->play(); //change this to a winning sound
        fastRespawn = true;
        clippingplane = 3.0;
    }
    else{
        fastRespawn = true;
        clippingplane = 3.0;
        //they have beat the game, so load endless mode.
        currentLevel = 0;
        initializeEndlessWalls(numWalls);
        winSoundPlayer->play(); //maybe a bigger win sound for beating the game.
    }

}


void reSpawn(int level){
    int score = roundf(-(playerPosOffset.x + 15) / disBetweenWalls);
    score = clamp(score, 0, (int)Walls.size() - 1);
    if(score > highscore){
        highscore = score;
    }
    rightGrapplePlaced = false;
    leftGrapplePlaced = false;
    grapplePointRight = handPosRight;
    grapplePointLeft = handPosLeft;
    playerPosOffset = Vec3(0.0f,1.0f,0.0f);
    playerVel = Vec3(0.0f,0.0f,0.0f);
    if(level == 0){
        initializeEndlessWalls(numWalls);
    }
    pigSoundPlayer->play();

}
//This Function was included in the library code
int
init_gles_scene ()
{
    generate_shader (&s_sobj, s_strVS, s_strFS);
    generate_shader (&RainbowShader, RainbowVertexShader, RainbowFragmentShader);
    generate_shader (&GrappleShader, GrappleVS, GrappleFS); //grapple shader with normals

    generate_shader (&SkyboxShaderObject, GridVS, GridFragShader); //synth shader
    generate_shader (&PortalShaderObject, PortalVS, PortalFS); //synth shader



    init_teapot (); // this was in the sample
    init_stage (); //as was this
    init_texplate ();
    init_Sphere();
    init_dbgstr (0, 0);
    init_imgui (700, UI_WIN_H);

//    Mp3Sound SongLoop = loadMp3File("/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/SynthSong.mp3");
//    songPlayer = new OboeMp3Player(SongLoop.samples, SongLoop.sampleRate, SongLoop.channels);

    Mp3Sound SongLoop = loadMp3File("/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/WatrSong.mp3");
    songPlayer = new OboeMp3Player(SongLoop.samples, SongLoop.sampleRate, SongLoop.channels);

    Mp3Sound LoseSound = loadMp3File("/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/SimpleLose.mp3");
    pigSoundPlayer = new OboeMp3Player(LoseSound.samples, LoseSound.sampleRate, LoseSound.channels);

//    Mp3Sound grappleSound = loadMp3File("/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/synthcowbell.mp3");
//    grappleSoundPlayer = new OboeMp3Player(grappleSound.samples, grappleSound.sampleRate, grappleSound.channels);

    Mp3Sound grappleSound = loadMp3File("/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/Clink2.mp3");
    grappleSoundPlayer = new OboeMp3Player(grappleSound.samples, grappleSound.sampleRate, grappleSound.channels);

    Mp3Sound WinSound = loadMp3File("/sdcard/Android/data/com.DRHudooken.GlassGrapple/files/SimpleWin.mp3");
    winSoundPlayer = new OboeMp3Player(WinSound.samples, WinSound.sampleRate, WinSound.channels);


    create_render_target (&s_rtarget, 700, UI_WIN_H, RTARGET_COLOR);
    WallColors.push_back(WallColor1);
    WallColors.push_back(WallColor2);
    WallColors.push_back(WallColor3);
    WallColors.push_back(WallColor4);
//    WallColors.push_back(Brown);
//    WallColors.push_back(Gray);


//    initializeEndlessWalls(numWalls);
    initializeWallsFromCSV("Level1.csv");

    songPlayer->loop();

    return 0;
}

int draw_line (float *mtxPV, const Vec3& p0, const Vec3& p1)
{
    GLfloat floor_vtx[6] = {
            p0.x, p0.y, p0.z,
            p1.x, p1.y, p1.z
    };

    shader_obj_t *sobj = &RainbowShader;
    glUseProgram (sobj->program);

    glEnableVertexAttribArray (sobj->loc_vtx);
    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);

//    glDisableVertexAttribArray (sobj->loc_clr);
//    glVertexAttrib4fv (sobj->loc_clr, color);
    glLineWidth(1.5f);

    Vec3 envColor = worldPink * (1-(songPlayer->MusicBrightness*2)) + worldYellow * songPlayer->MusicBrightness*2;
    glUniform3f(glGetUniformLocation(sobj->program, "viewPos"), playerPosOffset.x,playerPosOffset.y,playerPosOffset.z);        // 1 unit cells
    glUniform3f(glGetUniformLocation(sobj->program, "envColor"), envColor.x, envColor.y, envColor.z);        // 1 unit cells

    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);

    glEnable (GL_DEPTH_TEST);
    glDrawArrays (GL_LINES, 0, 2);
    glLineWidth(1.0f);

    return 0;
}


void DrawEndPortal(float *mtxPV, float currentTime, float xdist, float planex, float planey)
{
    shader_obj_t *sobj = &PortalShaderObject;
    float s = 15;
    float axis1 = 2.0;
    float axis2 = 1.0;
    float halfwallwidth = 0.5;
    GLfloat vtx[] = {
            xdist - halfwallwidth, 0,  s,
            xdist - halfwallwidth, 0, -s,
            xdist - halfwallwidth,  s,  s,
            xdist - halfwallwidth,  s,  s,
            xdist - halfwallwidth, 0, -s,
            xdist - halfwallwidth,  s, -s
    };
    glUseProgram(sobj->program);

    glUniform1f(glGetUniformLocation(sobj->program, "time"), currentTime);
    glUniform2f(glGetUniformLocation(sobj->program, "HolePos"), planex, planey);

    //fragment shader uniforms
    glUniform3f(glGetUniformLocation(sobj->program, "bgColor"), 0.0f, 0.0f, 0.0f);  // dark background
    glUniform2f(glGetUniformLocation(sobj->program, "planeAxes"), axis1, axis2);  // For XZ plane

    glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, vtx);

    glDisableVertexAttribArray(sobj->loc_clr);  // Assuming no color per vertex
    glEnable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}


void DrawSkyboxFace(const GLfloat* vtx, float axis1, float axis2, float *mtxPV, float currentTime, shader_obj_t *sobj)
{
    glUseProgram(sobj->program);

    glUniform1f(glGetUniformLocation(sobj->program, "time"), currentTime);
    //fragment shader uniforms
    glUniform1f(glGetUniformLocation(sobj->program, "gridSize"), 1.0f);        // 1 unit cells
    glUniform1f(glGetUniformLocation(sobj->program, "lineWidth"), 0.04f);      // thin lines
    glUniform3f(glGetUniformLocation(sobj->program, "lineColor"), worldPink.x, worldPink.y, worldPink.z); // pink lines
    glUniform3f(glGetUniformLocation(sobj->program, "musicLineColor"), worldYellow.x, worldYellow.y, worldYellow.z); // yellow lines

    glUniform3f(glGetUniformLocation(sobj->program, "bgColor"), 0.0f, 0.0f, 0.0f);  // dark background

    glUniform2f(glGetUniformLocation(sobj->program, "planeAxes"), axis1, axis2);  // For XZ plane

    glUniform1f(glGetUniformLocation(sobj->program, "songBrightness"), songPlayer->MusicBrightness * 4);        // 1 unit cells

    glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, vtx);

    glDisableVertexAttribArray(sobj->loc_clr);  // Assuming no color per vertex
    glEnable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}


//draw the pink and music pulsing grid around the world.
int DrawWallSkybox(float *mtxPV, float *color, scene_data_t sceneData)
{
    float s = 15;
    float px = 0.0f, pz = 0.0f;
    float LL = numWalls * disBetweenWalls;

    float currentTime = sceneData.elapsed_us * 1e-6;
    shader_obj_t *sobj = &SkyboxShaderObject;

    // All faces, CCW order to face inward

    GLfloat faceCeiling[] = {
            -s-LL + px, s,  s + pz,
            s + px, s,  s + pz,
            -s-LL + px, s, -s + pz,
            -s-LL + px, s, -s + pz,
            s + px, s,  s + pz,
            s + px, s, -s + pz
    };

    GLfloat faceFloor[] = {
            -s-LL + px, -0, -s + pz,
            s + px, -0, -s + pz,
            -s-LL + px, -0,  s + pz,
            -s-LL + px, -0,  s + pz,
            s + px, -0, -s + pz,
            s + px, -0,  s + pz
    };

    GLfloat faceRight[] = {
            s + px, 0, -s + pz,
            s + px, 0,  s + pz,
            s + px,  s, -s + pz,
            s + px,  s, -s + pz,
            s + px, 0,  s + pz,
            s + px,  s,  s + pz
    };

    GLfloat faceBack[] = {
            -s-LL + px, 0,  s + pz,
            s + px, 0,  s + pz,
            -s-LL + px,  s,  s + pz,
            -s-LL + px,  s,  s + pz,
            s + px, 0,  s + pz,
            s + px,  s,  s + pz
    };

    GLfloat faceFront[] = {
            s + px, 0, -s + pz,
            -s-LL + px, 0, -s + pz,
            s + px,  s, -s + pz,
            s + px,  s, -s + pz,
            -s-LL + px, 0, -s + pz,
            -s-LL + px,  s, -s + pz
    };

    DrawSkyboxFace(faceCeiling, 0,2, mtxPV, currentTime, sobj);
    DrawSkyboxFace(faceFloor,   0,2, mtxPV, currentTime, sobj);
    DrawSkyboxFace(faceRight,   2,1, mtxPV, currentTime, sobj);

    DrawSkyboxFace(faceBack,    0,1, mtxPV, currentTime, sobj);
    DrawSkyboxFace(faceFront,   0,1, mtxPV, currentTime, sobj);

    return 0;
}



int DrawWallWithHole(float *mtxPV, float *color, float holePosNorm, float holeWidth, float holeHeight, float wallDist, XrCompositionLayerProjectionView view)
{

    float wallX = wallDist;
    float backWallX = wallX - 1.0f;
    float wallBottom = 0.0f;
    float wallTop = 15.0f;
    float wallLeft = -15.0f;
    float wallRight = 15.0f;
    float wallWidth = wallRight - wallLeft;

    // Calculate hole center position along Z
    float holeCenterZ = wallLeft + holePosNorm * wallWidth;
    float holeHalfWidth = holeWidth * 0.5f;
    float holeHalfHeight = holeHeight * 0.5f;

    float holeLeft = holeCenterZ - holeHalfWidth;
    float holeRight = holeCenterZ + holeHalfWidth;
    float holeBottom = (wallBottom + wallTop) * 0.5f - holeHalfHeight;
    float holeTopEdge = holeBottom + holeHeight;

    // Vertices for the 4 rectangles around the hole
    GLfloat vtx[] = {
            // LEFT of hole
            wallX, wallBottom, wallLeft,
            wallX, wallBottom, holeLeft,
            wallX, wallTop, holeLeft,

            wallX, wallTop, holeLeft,
            wallX, wallTop, wallLeft,
            wallX, wallBottom, wallLeft,

            // RIGHT of hole
            wallX, wallBottom, holeRight,
            wallX, wallBottom, wallRight,
            wallX, wallTop, wallRight,

            wallX, wallTop, wallRight,
            wallX, wallTop, holeRight,
            wallX, wallBottom, holeRight,

            // TOP of hole
            wallX, holeTopEdge, holeLeft,
            wallX, holeTopEdge, holeRight,
            wallX, wallTop, holeRight,

            wallX, wallTop, holeRight,
            wallX, wallTop, holeLeft,
            wallX, holeTopEdge, holeLeft,

            // BOTTOM of hole
            wallX, wallBottom, holeLeft,
            wallX, wallBottom, holeRight,
            wallX, holeBottom, holeRight,

            wallX, holeBottom, holeRight,
            wallX, holeBottom, holeLeft,
            wallX, wallBottom, holeLeft,

            //JUST THE FRONT WALL AGAIN, BUT MOVED BACK
            backWallX, wallBottom, wallLeft,
            backWallX, wallBottom, holeLeft,
            backWallX, wallTop, holeLeft,

            backWallX, wallTop, holeLeft,
            backWallX, wallTop, wallLeft,
            backWallX, wallBottom, wallLeft,

            // RIGHT of hole
            backWallX, wallBottom, holeRight,
            backWallX, wallBottom, wallRight,
            backWallX, wallTop, wallRight,

            backWallX, wallTop, wallRight,
            backWallX, wallTop, holeRight,
            backWallX, wallBottom, holeRight,

            // TOP of hole
            backWallX, holeTopEdge, holeLeft,
            backWallX, holeTopEdge, holeRight,
            backWallX, wallTop, holeRight,

            backWallX, wallTop, holeRight,
            backWallX, wallTop, holeLeft,
            backWallX, holeTopEdge, holeLeft,

            // BOTTOM of hole
            backWallX, wallBottom, holeLeft,
            backWallX, wallBottom, holeRight,
            backWallX, holeBottom, holeRight,

            backWallX, holeBottom, holeRight,
            backWallX, holeBottom, holeLeft,
            backWallX, wallBottom, holeLeft,
            //CONNECTING THE HOLE AND WALL EDGES:
            // LEFT wall edge
            wallX, wallBottom, wallLeft,
            backWallX, wallBottom, wallLeft,
            backWallX, wallTop, wallLeft,

            backWallX, wallTop, wallLeft,
            wallX, wallTop, wallLeft,
            wallX, wallBottom, wallLeft,

// RIGHT wall edge
            wallX, wallBottom, wallRight,
            backWallX, wallBottom, wallRight,
            backWallX, wallTop, wallRight,

            backWallX, wallTop, wallRight,
            wallX, wallTop, wallRight,
            wallX, wallBottom, wallRight,

// TOP wall edge
            wallX, wallTop, wallLeft,
            backWallX, wallTop, wallLeft,
            backWallX, wallTop, wallRight,

            backWallX, wallTop, wallRight,
            wallX, wallTop, wallRight,
            wallX, wallTop, wallLeft,

// BOTTOM wall edge
            wallX, wallBottom, wallLeft,
            wallX, wallBottom, wallRight,
            backWallX, wallBottom, wallRight,

            backWallX, wallBottom, wallRight,
            backWallX, wallBottom, wallLeft,
            wallX, wallBottom, wallLeft,

// Connect hole edges

// HOLE LEFT edge
            wallX, wallBottom, holeLeft,
            backWallX, wallBottom, holeLeft,
            backWallX, holeTopEdge, holeLeft,

            backWallX, holeTopEdge, holeLeft,
            wallX, holeTopEdge, holeLeft,
            wallX, wallBottom, holeLeft,

// HOLE RIGHT edge
            wallX, wallBottom, holeRight,
            backWallX, wallBottom, holeRight,
            backWallX, holeTopEdge, holeRight,

            backWallX, holeTopEdge, holeRight,
            wallX, holeTopEdge, holeRight,
            wallX, wallBottom, holeRight,

// HOLE TOP edge
            wallX, holeTopEdge, holeLeft,
            backWallX, holeTopEdge, holeLeft,
            backWallX, holeTopEdge, holeRight,

            backWallX, holeTopEdge, holeRight,
            wallX, holeTopEdge, holeRight,
            wallX, holeTopEdge, holeLeft,

// HOLE BOTTOM edge
            wallX, holeBottom, holeLeft,
            wallX, holeBottom, holeRight,
            backWallX, holeBottom, holeRight,

            backWallX, holeBottom, holeRight,
            backWallX, holeBottom, holeLeft,
            wallX, holeBottom, holeLeft,
    };

    shader_obj_t *sobj = &s_sobj;
    glUseProgram(sobj->program);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, vtx);

    glDisableVertexAttribArray(sobj->loc_clr);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //doing polygon offset here to fix the lines from clipping
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);


    glVertexAttrib4fv(sobj->loc_clr, color);
    glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);
    // do a mix between pink and yellow to create the environemnt color. Same at the shader. x×(1−a)+y×a
    Vec3 envColor = worldPink * (1-(songPlayer->MusicBrightness*2)) + worldYellow * songPlayer->MusicBrightness*2;
    glUniform3f(glGetUniformLocation(sobj->program, "viewPos"), view.pose.position.x,view.pose.position.y,view.pose.position.z);        // 1 unit cells
    glUniform3f(glGetUniformLocation(sobj->program, "envColor"), envColor.x, envColor.y, envColor.z);        // 1 unit cells

    //passing all the grid info for reflections.
    glUniform1f(glGetUniformLocation(sobj->program, "gridSize"), 1.0f);        // 1 unit cells
    glUniform1f(glGetUniformLocation(sobj->program, "lineWidth"), 0.04f);      // thin lines
    glUniform3f(glGetUniformLocation(sobj->program, "lineColor"), worldPink.x, worldPink.y, worldPink.z); // pink lines
    glUniform3f(glGetUniformLocation(sobj->program, "musicLineColor"), worldYellow.x, worldYellow.y, worldYellow.z); // yellow lines
    glUniform3f(glGetUniformLocation(sobj->program, "bgColor"), 0.0f, 0.0f, 0.0f);  // dark background
    glUniform1f(glGetUniformLocation(sobj->program, "songBrightness"), songPlayer->MusicBrightness * 4);        // 1 unit cells


    glEnable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLES, 0, 96); // 4 quads = 8 triangles = 24 vertices

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);


    //DRAWING THE LINES OF THE HOLE
    GLfloat holeEdgeLines[] = {
            // Front face
            wallX, holeBottom, holeLeft,     wallX, holeBottom, holeRight,  // Bottom
            wallX, holeTopEdge, holeLeft,    wallX, holeTopEdge, holeRight, // Top
            wallX, holeBottom, holeLeft,     wallX, holeTopEdge, holeLeft,  // Left
            wallX, holeBottom, holeRight,    wallX, holeTopEdge, holeRight, // Right

            // Back face
            backWallX, holeBottom, holeLeft,     backWallX, holeBottom, holeRight,
            backWallX, holeTopEdge, holeLeft,    backWallX, holeTopEdge, holeRight,
            backWallX, holeBottom, holeLeft,     backWallX, holeTopEdge, holeLeft,
            backWallX, holeBottom, holeRight,    backWallX, holeTopEdge, holeRight,
            // Connecting the hole squared
            wallX, holeBottom, holeLeft,     backWallX, holeBottom, holeLeft,
            wallX, holeTopEdge, holeLeft,    backWallX, holeTopEdge, holeLeft,
            wallX, holeTopEdge, holeRight,     backWallX, holeTopEdge, holeRight,
            wallX, holeBottom, holeRight,    backWallX, holeBottom, holeRight
    };
   //the color of the walls lines, should be the inverse of the color of the wall.
    float outlineColor[4] = {
            1.0f - color[0],
            1.0f - color[1],
            1.0f - color[2],
            1.0f
    };

    glVertexAttrib4fv(sobj->loc_clr, outlineColor);

// Draw line edges
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, holeEdgeLines);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, 24); // 8 lines = 16 vertices

    glDisable(GL_DEPTH_TEST);

    if(wallDist < -18 && abs(playerPosOffset.x - wallDist) < 30) {
        shader_obj_t *sobj_Rainbow = &RainbowShader;
        glUseProgram(sobj_Rainbow->program);
        //DRAWING THE INDICATING LINES FOR THE NEXT WALLS HOLE
        GLfloat indicatorHoleEdgeLines[] = {

                wallX , holeBottom, holeLeft, wallX , holeBottom, holeRight,  // Bottom
                wallX, holeTopEdge, holeLeft, wallX, holeTopEdge, holeRight, // Top
                wallX, holeBottom, holeLeft, wallX, holeTopEdge, holeLeft,  // Left
                wallX, holeBottom, holeRight, wallX, holeTopEdge, holeRight, // Right

        };

        //use the rainbow shader


        // Draw line edges
        glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, indicatorHoleEdgeLines);
        glLineWidth(1.0f);
        glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);

//        glUniform3f(glGetUniformLocation(sobj->program, "viewPos"), playerPosOffset.x,playerPosOffset.y,playerPosOffset.z);        // 1 unit cells
//        glUniform3f(glGetUniformLocation(sobj->program, "envColor"), envColor.x, envColor.y, envColor.z);        // 1 unit cells

        glDrawArrays(GL_LINES, 0, 8); // 8 lines = 16 vertices

    }
    glEnable(GL_DEPTH_TEST);

    return 0;
}

//This Function was included in the library code
int draw_uiplane (float *matPVMbase,
              XrCompositionLayerProjectionView &layerView,
              scene_data_t &sceneData)
{
    /* save current FBO */
    render_target_t rtarget0;
    get_render_target (&rtarget0);

    /* render to UIPlane-FBO */
    set_render_target (&s_rtarget);
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    glClear (GL_COLOR_BUFFER_BIT);

    {
        static uint32_t prev_us = 0;
        if (sceneData.viewID == 0)
        {
            sceneData.interval_ms = (sceneData.elapsed_us - prev_us) / 1000.0f;
            prev_us = sceneData.elapsed_us;
        }

        sceneData.gl_version = glGetString (GL_VERSION);
        sceneData.gl_vendor  = glGetString (GL_VENDOR);
        sceneData.gl_render  = glGetString (GL_RENDERER);
        sceneData.viewport   = layerView.subImage.imageRect;
        invoke_imgui (&sceneData, highscore);
        invoke_Debug_imgui(&sceneData, layerView);
    }

    /* restore FBO */
    set_render_target (&rtarget0);

    glEnable (GL_DEPTH_TEST);

    {
        float matPVM[16], matT[16];
//        float win_x = 1.0f + sceneData.inputState.stickVal[1].x * 0.5f;
        float win_x = -5.0f;
        float win_y = 0.5f;

//        float win_y = 0.0f + sceneData.inputState.stickVal[1].y * 0.5f;
        float win_z =-2.0f;
        float win_w = 4.0f;
        float win_h = win_w * ((float)UI_WIN_H / (float)UI_WIN_W);
        matrix_identity (matT);
        matrix_translate (matT, win_x, win_y, win_z);
        matrix_rotate (matT, 90.0f, 0.0f, 1.0f, 0.0f);
        matrix_scale (matT, win_w, win_h, 1.0f);
        matrix_mult (matPVM, matPVMbase, matT);
        draw_tex_plate (s_rtarget.texc_id, matPVM, RENDER2D_FLIP_V);
    }

    return 0;
}


Vec3 getDirectionFromQuaternion(XrQuaternionf q, Vec3 baseDirection)
{
    // Rotate baseDirection by quaternion q
    Vec3 out;
    float num = q.x * 2.0f;
    float num2 = q.y * 2.0f;
    float num3 = q.z * 2.0f;
    float num4 = q.x * num;
    float num5 = q.y * num2;
    float num6 = q.z * num3;
    float num7 = q.x * num2;
    float num8 = q.x * num3;
    float num9 = q.y * num3;
    float num10 = q.w * num;
    float num11 = q.w * num2;
    float num12 = q.w * num3;

    out.x = (1.0f - (num5 + num6)) * baseDirection.x + (num7 - num12) * baseDirection.y + (num8 + num11) * baseDirection.z;
    out.y = (num7 + num12) * baseDirection.x + (1.0f - (num4 + num6)) * baseDirection.y + (num9 - num10) * baseDirection.z;
    out.z = (num8 - num11) * baseDirection.x + (num9 + num10) * baseDirection.y + (1.0f - (num4 + num5)) * baseDirection.z;

    return out;
}


//some of This Function was included in the library code
int render_gles_scene (XrCompositionLayerProjectionView &layerView,
                   render_target_t                  &rtarget,
                   XrPosef                          &viewPose,
                   XrPosef                          &stagePose,
                   scene_data_t                     &sceneData)
{
    float view_x = layerView.subImage.imageRect.offset.x;
    float view_y = layerView.subImage.imageRect.offset.y;
    float view_w = layerView.subImage.imageRect.extent.width;
    float view_h = layerView.subImage.imageRect.extent.height;

    glBindFramebuffer(GL_FRAMEBUFFER, rtarget.fbo_id);

    glViewport(view_x, view_y, view_w, view_h);
//    glViewport(view_y, view_x, view_w, view_h);


    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable (GL_DEPTH_TEST);

    /* ------------------------------------------- *
     *  Matrix Setup
     *    (matPV)  = (proj) x (view)
     *    (matPVM) = (proj) x (view) x (model)
     * ------------------------------------------- */
    XrMatrix4x4f matP, matV, matC, matM, matPV, matPVM;

    /* Projection Matrix */
    if(clippingplane < 100.0f && fastRespawn){
        clippingplane += 0.5f;
    }
    else if(clippingplane < 10.0f){
        clippingplane += 0.03f;
    }
    else if(clippingplane < 100.0f){
        clippingplane += 1.0f;
    }
    else{
        fastRespawn = false;
    }
    XrMatrix4x4f_CreateProjectionFov (&matP, GRAPHICS_OPENGL_ES, layerView.fov, 0.05f, clippingplane); //probably clipping planes

    /* View Matrix (inverse of Camera matrix) */
    XrVector3f scale = {1.0f, 1.0f, 1.0f};
//    const auto &vewPose = layerView.pose;   //this was a const, but I want to move the player forehead
    auto &vewPose = layerView.pose;


    //Changing the actual render position based on the position offset.
    vewPose.position.x += playerPosOffset.x;
    vewPose.position.y += playerPosOffset.y;
    vewPose.position.z += playerPosOffset.z;


    XrMatrix4x4f_CreateTranslationRotationScale (&matC, &vewPose.position, &vewPose.orientation, &scale);
    XrMatrix4x4f_InvertRigidBody (&matV, &matC);

    /* Stage Space Matrix */
    XrMatrix4x4f_CreateTranslationRotationScale (&matM, &stagePose.position, &stagePose.orientation, &scale);

    XrMatrix4x4f_Multiply (&matPV, &matP, &matV);
    XrMatrix4x4f_Multiply (&matPVM, &matPV, &matM);


    /* ------------------------------------------- *
     *  Render
     * ------------------------------------------- */
    float *matStage = reinterpret_cast<float*>(&matPVM);

    float currentTime = sceneData.elapsed_us * 1e-7; // Convert microseconds to seconds
    float t = currentTime - floor(currentTime);       // 0 → 1
    float pingPong = fabs(1.0f - 2.0f * t);            // 0 → 1 → 0
    float floor[4] = {0.6,pingPong,pingPong,0.5};

    //this need to be changed to make the level system, sike no it didn't lol.
    for(int i = 0; i < Walls.size(); i++){
        float WallColor[4] = {Walls[i].WallColor.x,Walls[i].WallColor.y,Walls[i].WallColor.z,1};
        DrawWallWithHole(matStage, WallColor, Walls[i].holeLeftRight, Walls[i].holeWidth, Walls[i].holeHeight, Walls[i].WallX, layerView); //draw each of the walls in the list
    }
    DrawWallSkybox(matStage, floor, sceneData);
    int lastWall = Walls.size() - 1;
    DrawEndPortal( matStage, currentTime, Walls[lastWall].WallX, Walls[lastWall].holeLeftRight, 0.75);


    /* Axis of global origin */
//    {
//        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
//        XrVector3f    pos   = {0.0f, 0.0f, 0.0f};
//        XrQuaternionf qtn   = {0.0f, 0.0f, 0.0f, 1.0f};
//        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
//        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
//    }

    /* Axis of stage origin */
//    {
//        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
//        XrVector3f    &pos  = stagePose.position;
//        XrQuaternionf &qtn  = stagePose.orientation;
//        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
//        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
//    }

    Vec3 col = {0.2f, 0.8f, 0.7f};

    draw_uiplane ((float *)&matPVM, layerView, sceneData);


    if(sceneData.inputState.triggerVal[0] > 0.8f){
        leftHandForward = getDirectionFromQuaternion(sceneData.handLoc[0].pose.orientation, {0.0f, 1.0f, 0.0f});

        if(!leftGrapplePlaced){
            grapplePointLeft = findGrapplePoint(handPosLeft, -leftHandForward);
            leftGrapplePlaced = true;
            if(sceneData.inputState.squeezeVal[0] < 0.8f){
                grappleSoundPlayer->play();
            }
        }
    }
    if((sceneData.inputState.squeezeVal[0] > 0.8f)){
        leftGrapplePlaced = false;
    }
    if(leftGrapplePlaced){
        applyGrappleSpring(handPosLeft, grapplePointLeft, &playerVel, deltaTime);  //handle the velocity changes for grappling.
    }
    else{
        grapplePointLeft = handPosLeft;
    }

    //right hand shooting
    if(sceneData.inputState.triggerVal[1] > 0.8f){
        rightHandForward = getDirectionFromQuaternion(sceneData.handLoc[1].pose.orientation, {0.0f, 1.0f, 0.0f});

        if(!rightGrapplePlaced){
            grapplePointRight = findGrapplePoint(handPosRight, -rightHandForward);
            rightGrapplePlaced = true;
            if(sceneData.inputState.squeezeVal[1] < 0.8f){
                grappleSoundPlayer->play();
            }
        }
    }
    if((sceneData.inputState.squeezeVal[1] > 0.8f)){
        rightGrapplePlaced = false;
        grapplePointRight = handPosRight;
    }
    if(rightGrapplePlaced){
        applyGrappleSpring(handPosRight, grapplePointRight, &playerVel, deltaTime);  //handle the velocity changes for grappling.
    }
    else{
        grapplePointRight = handPosRight;
    }


    //playing around with player movement
    playerPosOffset.x += playerVel.x * deltaTime;
    playerPosOffset.y += playerVel.y * deltaTime;
    playerPosOffset.z += playerVel.z * deltaTime;


    //checking for collision with the world borders

    if(length(playerVel) > 8){
        playerVel = normalize(playerVel);
        playerVel *= 8;
    }
    if(playerPosOffset.y > 0.0f) {
        playerVel.y -= 2.0f * deltaTime; // simple gravity
    }
    else {
        playerVel.y = 0;
        playerPosOffset.y = 0.0f;
        playerVel.x *= 0.99f;
        playerVel.z *= 0.99f;
    }
    if(playerPosOffset.z < -14.0f) {
        playerVel.z = 0;
        playerPosOffset.z = -14.0f;
    }
    if(playerPosOffset.z > 14.0f) {
        playerVel.z = 0;
        playerPosOffset.z = 14.0f;
    }
    if(playerPosOffset.y > 14.0f) {
        playerVel.y = 0;
        playerPosOffset.y = 14.0f;
    }
    if(playerPosOffset.x > 15.0f) {
        playerVel.x = 0;
        playerPosOffset.x = 15.0f;
    }
    if(playerPosOffset.x < Walls[Walls.size()-1].WallX) {
        //win the level
        BeatLevel(currentLevel);
    }

    //use the player position to index into the walls array.
    Wall nearestWall;
    int nearWallIndex = roundf((-playerPosOffset.x - 15.0f) / disBetweenWalls);
    nearWallIndex = clamp(nearWallIndex, 0, (int)Walls.size() - 1);
    nearestWall = Walls.at(nearWallIndex);

    float wallFrontX = nearestWall.WallX;
    float wallBackX = wallFrontX - 1.0f;

    float wallBottomY = -1.0f;
    float wallTopY = 15.0f;

    float wallLeftZ = -15.0f;
    float wallRightZ = 15.0f;
    float wallWidthZ = wallRightZ - wallLeftZ;

    // Hole
    float holeCenterZ = wallLeftZ + nearestWall.holeLeftRight * wallWidthZ;
    float holeHalfWidth = nearestWall.holeWidth * 0.5f;
    float holeLeftZ = holeCenterZ - holeHalfWidth;
    float holeRightZ = holeCenterZ + holeHalfWidth;

    float holeHalfHeight = nearestWall.holeHeight * 0.5f;
    float holeCenterY = (wallBottomY + wallTopY) * 0.5f;
    float holeBottomY = holeCenterY - holeHalfHeight;
    float holeTopY = holeBottomY + nearestWall.holeHeight;

    // Check bounds
    bool inWallX = layerView.pose.position.x < wallFrontX && layerView.pose.position.x > wallBackX;
//    bool inWallY = layerView.pose.position.y < wallTopY && layerView.pose.position.y > wallBottomY;
    bool inWallZ = layerView.pose.position.z < wallRightZ && layerView.pose.position.z > wallLeftZ;

    bool inHoleY = layerView.pose.position.y < holeTopY && layerView.pose.position.y > holeBottomY;
    bool inHoleZ = layerView.pose.position.z < holeRightZ && layerView.pose.position.z > holeLeftZ;

    // Final check
    if (inWallX && inWallZ && !(inHoleY && inHoleZ)) {   //I still need to fix that this is not the camera pos. Need to add player offset and scene data position
        reSpawn(currentLevel);
    }



    if(sceneData.inputState.squeezeVal[0] > 0.5f){
        playerVel.x *= 0.95f;
        playerVel.z *= 0.95f;
//        playerPosOffset.y += 0.1;
    }

    //I am aware that this is horid code, but I couldn't think of a way to do it in the loop. Hope it's right lol.
    handPosLeft.x = sceneData.handLoc[0].pose.position.x + playerPosOffset.x;
    handPosLeft.y = sceneData.handLoc[0].pose.position.y + playerPosOffset.y;
    handPosLeft.z = sceneData.handLoc[0].pose.position.z + playerPosOffset.z;
    handPosRight.x = sceneData.handLoc[1].pose.position.x + playerPosOffset.x;
    handPosRight.y = sceneData.handLoc[1].pose.position.y + playerPosOffset.y;
    handPosRight.z = sceneData.handLoc[1].pose.position.z + playerPosOffset.z;


    for (int i = 0; i < sceneData.handLoc.size(); i++)
    {
        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
        XrVector3f    &pos  = sceneData.handLoc[i].pose.position;

        pos.x += playerPosOffset.x;
        pos.y += playerPosOffset.y;
        pos.z += playerPosOffset.z;
        XrQuaternionf &qtn  = sceneData.handLoc[i].pose.orientation;
        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
        if(i == 0){ //left hand
            if(leftGrapplePlaced){
                draw_line((float *)&matPVM, handPosLeft, grapplePointLeft);
                draw_Hand ((float *)&matP, (float *)&matV, (float *)&matM,GrappleShader, playerPosOffset);

            }
            else{
                draw_Grapple ((float *)&matP, (float *)&matV, (float *)&matM,GrappleShader, playerPosOffset);
            }
        }
        if(i == 1){ //left hand
            if(rightGrapplePlaced){
                draw_line((float *)&matPVM, handPosRight, grapplePointRight);
                draw_Hand ((float *)&matP, (float *)&matV, (float *)&matM,GrappleShader, playerPosOffset);

            }
            else{
                draw_Grapple ((float *)&matP, (float *)&matV, (float *)&matM,GrappleShader, playerPosOffset);
            }
        }

        GLASSERT();
    }



    /* UI plane always view front */
    {
        XrVector3f    scale = {1.0f, 1.0f, 1.0f};
        XrVector3f    &pos  = viewPose.position;
        XrQuaternionf &qtn  = viewPose.orientation;
        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);

        XrMatrix4x4f matPVM;
        XrMatrix4x4f_Multiply (&matPVM, &matPV, &matM);

//        draw_uiplane ((float *)&matPVM, layerView, sceneData);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return 0;
}




