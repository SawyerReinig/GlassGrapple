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
//#include "VRGame.cpp"


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
static shader_obj_t     SkyboxShaderObject;


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



Vec3 Amber = Vec3(1.0f, 0.768f, 0.24f);
Vec3 Pink = Vec3(0.937, 0.278, 0.435);
Vec3 Blue = Vec3(0.106, 0.604, 0.667);
Vec3 Green = Vec3(0.024, 0.839, 0.627);
Vec3 Gray = Vec3(0.973, 1, 0.8);
Vec3 Brown = Vec3(0.929, 0.749, 0.522);


Vec3 worldPink = Vec3(0.756f, 0.1, 0.76f);
Vec3 worldYellow = Vec3(0.99f, 0.91, 0.02f);

int lastPickedColor = 0;
std::vector<Vec3> WallColors;

int oldNearestWall = 0;
std::vector<Wall> Walls;
float disBetweenWalls = 12.0f;
int numWalls = 150;

int highscore = 0;

//OboeSinePlayer* m_oboePlayer = new OboeSinePlayer ();
OboeMp3Player* myPlayer;
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


static char s_strFS[] = R"(
precision mediump float;
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
    vec3 fresnelAndRainbow = mix(rainbowColor, envColor, 0.75);
    // Blend environment color into base color based on fresnel
    vec3 color = mix(fresnelAndRainbow, v_color.rgb, 0.5);  // 0.5 = strength of reflection

    gl_FragColor = vec4(color, 0.75);
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
precision mediump float;
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




//I THINK I NEED TO TRY SUBTRACTING THE PLAYER POSITION FROM THE VERTEX SHADER, SO THE WALLS DON'T SLIDE WHEN MOVING!!!
//static char SkyboxPlaneVertexShader[] = R"(
//attribute vec4  a_Vertex;
//attribute vec3  a_Normal;
//uniform   mat4  u_PMVMatrix;
//uniform   int   u_FaceID;
//varying   vec3  v_Position;
//
//void main(void)
//{
////    a_Vertex.xyz = a_Vertex.xyz;
//    gl_Position = u_PMVMatrix * a_Vertex;
//
//    //Because of the way the frag shader works. We esentailly need to trick the frag shader into thinking that it is actually along the Z plane.
//    vec3 rotated = a_Vertex.xyz;
//    if (u_FaceID == 0) { // Ceiling (+Y)
//        rotated = vec3(a_Vertex.y, a_Vertex.z, a_Vertex.x);
//    }
//    else if (u_FaceID == 1) { // Floor (-Y)
//        rotated = vec3(-a_Vertex.y, a_Vertex.z, a_Vertex.x);
//    }
//    else if (u_FaceID == 4) { // Right (+X)
//        rotated = vec3(-a_Vertex.z, a_Vertex.y, a_Vertex.x);
//    }
//    else if (u_FaceID == 5) { // Left (-X)
//        rotated = vec3(a_Vertex.z, a_Vertex.y, -a_Vertex.x);
//    }
//    v_Position = rotated;
//}
//)";



//static char SkyboxPlaneFSCubes[] = R"(
//precision highp float;
//varying vec3 v_Position;
//uniform float iTime;
//uniform vec3 PlayerPos;
//vec3 tempVertPos;
//
//float safeMod(float a, float b) {
//    return a - b * floor(a / b);
//}
//vec2 safeMod(vec2 a, vec2 b) {
//    return a - b * floor(a / b);
//}
//
//float segment(vec2 p, vec2 a, vec2 b) {
//    p -= a;
//    b -= a;
//    float d = dot(b, b);
//    if (d < 0.0001) return length(p);
//    return length(p - b * clamp(dot(p, b) / d, 0.0, 1.0));
//}
//
//mat2 rot(float a) {
//    float c = cos(a);
//    float s = sin(a);
//    return mat2(c, -s, s, c);
//}
//
//float t;
//vec2 T(vec3 p) {
//    p.xy *= rot(-t);
//    p.xz *= rot(0.785);
//    p.yz *= rot(-0.625);
//    return p.xy;
//}
//
//float fastTanh(float x) {
//    float e = exp(-2.0 * abs(x));
//    float t = (1.0 - e) / (1.0 + e);
//    return x < 0.0 ? -t : t;
//}
//
//void main() {
////    tempVertPos = v_Position;
//    tempVertPos = v_Position - PlayerPos;
//    vec2 U = tempVertPos.yz * 2.0;
//    vec2 M = vec2(2.0, 2.3);
//    vec2 I = floor(U / M) * M;
//    vec3 color = vec3(0.0);
//
//    float angles[4];
//    angles[0] = 0.0;
//    angles[1] = 1.57;
//    angles[2] = 3.14;
//    angles[3] = 4.71;
//
//    for (int dx = -1; dx <= 1; dx++) {
//        for (int dy = -1; dy <= 1; dy++) {
//            vec2 offset = vec2(float(dx), float(dy)) * M;
//            vec2 J = I + offset;
//            vec2 X = J;
//
//            float check = safeMod(J.x / M.x, 2.0) + safeMod(J.y / M.y, 2.0);
//            if (fract(check / 2.0) > 0.5) {
//                X.y += 1.15;
//            }
//
//            t = sin(-0.2 * (J.y) + safeMod(2.0 * iTime, 30.0)) * 0.785;
//
//            for (int i = 0; i < 4; i++) {
//                float a = angles[i];
//                vec3 A = vec3(cos(a), sin(a), 0.7);
//                vec3 B = vec3(-A.y, A.x, 0.7);
//                color += smoothstep(0.1, 0.0, segment(U - X, T(A), T(B)));
//
//                vec3 B2 = vec3(A.xy, -A.z);
//                color += smoothstep(0.1, 0.0, segment(U - X, T(A), T(B2)));
//
//                A.z = -A.z; B.z = -B.z;
//                color += smoothstep(0.1, 0.0, segment(U - X, T(A), T(B)));
//            }
//        }
//    }
//
//    gl_FragColor = vec4(color, 1.0);
//}                                                               )";


static char GridVS[] = R"(
attribute vec4  a_Vertex;
attribute vec3  a_Normal;
uniform   mat4  u_PMVMatrix;
uniform   int   u_FaceID;
varying   vec3  v_Position;
uniform float time;


void main(void)
{
    vec4 pos = a_Vertex;

//    float scaleX = 1.0 + 0.02 * cos(3.0 * time);
//    float scaleY = 1.0 + 0.02 * sin(2.0 * time);

//    // Apply scaling to x and y
//    pos.x *= scaleX;
//    pos.y *= scaleY;
//    pos.z *= scaleY;
//    pos.xy *= 1.0+0.4*cos(4.0*time);


    gl_Position = u_PMVMatrix * pos;
    v_Position = pos.xyz;
}
)";

static char GridFragShader[] = R"(
precision highp float;

varying vec3 v_Position;

uniform float gridSize;      // Size of each cell
uniform float lineWidth;     // Thickness of lines
uniform vec3  lineColor;     // Color of the grid lines
uniform vec3  musicLineColor;     // Color of the grid lines when the music is at max
uniform float songBrightness;     // Loudness of the music


uniform vec3  bgColor;       // Color of background
uniform vec2 planeAxes;  // 0,1) for XY, (0,2) for XZ, (2,1) for ZY


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

    vec3 color;
    if(safeMod(coordA, gridSize) < lineWidth || safeMod(coordB, gridSize) < lineWidth){
        color = mix(lineColor, musicLineColor, songBrightness);
    }
    else{
        discard;
    }
    gl_FragColor = vec4(color, 1.0);

}
)";
//
//Shout out to the shader toy example that I made the base of my skybox:
//https://www.shadertoy.com/view/lcSGDD


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
        index = RandomInt(0,5);
    }
    lastPickedColor = index;
    return WallColors.at(index);
}



void initializeWalls(int NW){
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

void reSpawn(){
    int score = roundf(-(playerPosOffset.x + 15) / disBetweenWalls);
    score = clamp(score, 1, (int)Walls.size() - 1);
    if(score > highscore){
        highscore = score;
    }

    playerPosOffset = Vec3(0.0f,1.0f,0.0f);
    playerVel = Vec3(0.0f,0.0f,0.0f);
    initializeWalls(numWalls);
    myPlayer->play();


//    songPlayer->play();

}
//This Function was included in the library code
int
init_gles_scene ()
{
    generate_shader (&s_sobj, s_strVS, s_strFS);
    generate_shader (&RainbowShader, RainbowVertexShader, RainbowFragmentShader);
    generate_shader (&SkyboxShaderObject, GridVS, GridFragShader); //synth shader



    init_teapot (); // this was in the sample
    init_stage (); //as was this
    init_texplate ();
    init_Sphere();
    init_dbgstr (0, 0);
    init_imgui (700, UI_WIN_H);

    Mp3Sound SongLoop = loadMp3File("/sdcard/Android/data/com.example.SwingOut/files/SynthSong.mp3");
    songPlayer = new OboeMp3Player(SongLoop.samples, SongLoop.sampleRate, SongLoop.channels);

    Mp3Sound LoseSound = loadMp3File("/sdcard/Android/data/com.example.SwingOut/files/PigstepLose.mp3");
    myPlayer = new OboeMp3Player(LoseSound.samples, LoseSound.sampleRate, LoseSound.channels);

    create_render_target (&s_rtarget, 700, UI_WIN_H, RTARGET_COLOR);
    WallColors.push_back(Blue);
    WallColors.push_back(Brown);
    WallColors.push_back(Pink);
    WallColors.push_back(Gray);
    WallColors.push_back(Green);
    WallColors.push_back(Amber);

    initializeWalls(numWalls);

    songPlayer->loop();

    return 0;
}


//int draw_line (float *mtxPV, float *p0, float *p1, float *color)
//{
//    GLfloat floor_vtx[6];
//    for (int i = 0; i < 3; i ++)
//    {
//        floor_vtx[0 + i] = p0[i];
//        floor_vtx[3 + i] = p1[i];
//    }
//
//    shader_obj_t *sobj = &s_sobj;
//    glUseProgram (sobj->program);
//
//    glEnableVertexAttribArray (sobj->loc_vtx);
//    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);
//
//    glDisableVertexAttribArray (sobj->loc_clr);
//    glVertexAttrib4fv (sobj->loc_clr, color);
//
//    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);
//
//    glEnable (GL_DEPTH_TEST);
//    glDrawArrays (GL_LINES, 0, 2);
//
//    return 0;
//}

int draw_line (float *mtxPV, const Vec3& p0, const Vec3& p1)
{
    GLfloat floor_vtx[6] = {
            p0.x, p0.y, p0.z,
            p1.x, p1.y, p1.z
    };

    shader_obj_t *sobj = &s_sobj;
    glUseProgram (sobj->program);

    glEnableVertexAttribArray (sobj->loc_vtx);
    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);

//    glDisableVertexAttribArray (sobj->loc_clr);
//    glVertexAttrib4fv (sobj->loc_clr, color);

    Vec3 envColor = worldPink * (1-(songPlayer->MusicBrightness*2)) + worldYellow * songPlayer->MusicBrightness*2;
    glUniform3f(glGetUniformLocation(sobj->program, "viewPos"), playerPosOffset.x,playerPosOffset.y,playerPosOffset.z);        // 1 unit cells
    glUniform3f(glGetUniformLocation(sobj->program, "envColor"), envColor.x, envColor.y, envColor.z);        // 1 unit cells

    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);

    glEnable (GL_DEPTH_TEST);
    glDrawArrays (GL_LINES, 0, 2);

    return 0;
}

void DrawSkyboxFace(const GLfloat* vtx, float axis1, float axis2, float *mtxPV, float currentTime, shader_obj_t *sobj, Vec3 playerpos)
{
    glUseProgram(sobj->program);

    glUniform1f(glGetUniformLocation(sobj->program, "time"), currentTime);
    glUniform3f(glGetUniformLocation(sobj->program, "PlayerPos"), playerpos.x, playerpos.y, playerpos.z);

    //fragment shader uniforms
    glUniform1f(glGetUniformLocation(sobj->program, "gridSize"), 1.0f);        // 1 unit cells
    glUniform1f(glGetUniformLocation(sobj->program, "lineWidth"), 0.05f);      // thin lines
    glUniform3f(glGetUniformLocation(sobj->program, "lineColor"), worldPink.x, worldPink.y, worldPink.z); // pink lines
    glUniform3f(glGetUniformLocation(sobj->program, "musicLineColor"), worldYellow.x, worldYellow.y, worldYellow.z); // yellow lines

    glUniform3f(glGetUniformLocation(sobj->program, "bgColor"), 0.0f, 0.1f, 0.1f);  // dark background

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
int DrawWallSkybox(float *mtxPV, float *color, scene_data_t sceneData, Vec3 playerPos)
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

//    GLfloat faceLeft[] = {
//            -s + px, 0,  s + pz,
//            -s + px, 0, -s + pz,
//            -s + px,  s,  s + pz,
//            -s + px,  s,  s + pz,
//            -s + px, 0, -s + pz,
//            -s + px,  s, -s + pz
//    };
//
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

    DrawSkyboxFace(faceCeiling, 0,2, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceFloor,   0,2, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceRight,   2,1, mtxPV, currentTime, sobj, playerPos);
//    DrawSkyboxFace(faceLeft,    2,1, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceBack,    0,1, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceFront,   0,1, mtxPV, currentTime, sobj, playerPos);

    return 0;
}



int DrawWallWithHole(float *mtxPV, float *color, float holePosNorm, float holeWidth, float holeHeight, float wallDist)
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
            wallX, wallBottom, holeLeft,
            wallX, wallBottom, holeRight,
            backWallX, wallBottom, holeRight,

            backWallX, wallBottom, holeRight,
            backWallX, wallBottom, holeLeft,
            wallX, wallBottom, holeLeft,
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
    glUniform3f(glGetUniformLocation(sobj->program, "viewPos"), playerPosOffset.x,playerPosOffset.y,playerPosOffset.z);        // 1 unit cells
    glUniform3f(glGetUniformLocation(sobj->program, "envColor"), envColor.x, envColor.y, envColor.z);        // 1 unit cells


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
                // Front face
//                wallX + disBetweenWalls, holeBottom, holeLeft, wallX + disBetweenWalls, holeBottom, holeRight,  // Bottom
//                wallX+ disBetweenWalls, holeTopEdge, holeLeft, wallX+ disBetweenWalls, holeTopEdge, holeRight, // Top
//                wallX+ disBetweenWalls, holeBottom, holeLeft, wallX+ disBetweenWalls, holeTopEdge, holeLeft,  // Left
//                wallX+ disBetweenWalls, holeBottom, holeRight, wallX+ disBetweenWalls, holeTopEdge, holeRight, // Right


                wallX , holeBottom, holeLeft, wallX , holeBottom, holeRight,  // Bottom
                wallX, holeTopEdge, holeLeft, wallX, holeTopEdge, holeRight, // Top
                wallX, holeBottom, holeLeft, wallX, holeTopEdge, holeLeft,  // Left
                wallX, holeBottom, holeRight, wallX, holeTopEdge, holeRight, // Right

                // Back face
//                backWallX+ disBetweenWalls, holeBottom, holeLeft, backWallX+ disBetweenWalls, holeBottom, holeRight,
//                backWallX+ disBetweenWalls, holeTopEdge, holeLeft, backWallX+ disBetweenWalls, holeTopEdge, holeRight,
//                backWallX+ disBetweenWalls, holeBottom, holeLeft, backWallX+ disBetweenWalls, holeTopEdge, holeLeft,
//                backWallX+ disBetweenWalls, holeBottom, holeRight, backWallX+ disBetweenWalls, holeTopEdge, holeRight,
                // Connecting the hole squared
//                wallX+ disBetweenWalls, holeBottom, holeLeft, backWallX+ disBetweenWalls, holeBottom, holeLeft,
//                wallX+ disBetweenWalls, holeTopEdge, holeLeft, backWallX+ disBetweenWalls, holeTopEdge, holeLeft,
//                wallX+ disBetweenWalls, holeTopEdge, holeRight, backWallX+ disBetweenWalls, holeTopEdge, holeRight,
//                wallX+ disBetweenWalls, holeBottom, holeRight, backWallX+ disBetweenWalls, holeBottom, holeRight
        };

        //use the rainbow shader


        // Draw line edges
        glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, indicatorHoleEdgeLines);
        glLineWidth(1.0f);
        glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);

        glUniform3f(glGetUniformLocation(sobj->program, "viewPos"), playerPosOffset.x,playerPosOffset.y,playerPosOffset.z);        // 1 unit cells
        glUniform3f(glGetUniformLocation(sobj->program, "envColor"), envColor.x, envColor.y, envColor.z);        // 1 unit cells

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

//Vec3 getForwardVectorFromQuaternion(XrQuaternionf q) {
//    Vec3 forward;
//    forward.x = 2.0f * (q.x * q.z + q.w * q.y);
//    forward.y = 2.0f * (q.y * q.z - q.w * q.x);
//    forward.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
//    return forward;
//}

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
    XrMatrix4x4f_CreateProjectionFov (&matP, GRAPHICS_OPENGL_ES, layerView.fov, 0.05f, 100.0f); //probably clipping planes

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

//    draw_stage (matStage);
    float currentTime = sceneData.elapsed_us * 1e-7; // Convert microseconds to seconds
    float t = currentTime - floor(currentTime);       // 0 → 1
    float pingPong = fabs(1.0f - 2.0f * t);            // 0 → 1 → 0
    float floor[4] = {0.6,pingPong,pingPong,0.5};


    for(int i = 0; i < Walls.size(); i++){
        float WallColor[4] = {Walls[i].WallColor.x,Walls[i].WallColor.y,Walls[i].WallColor.z,1};
        DrawWallWithHole(matStage, WallColor, Walls[i].holeLeftRight, Walls[i].holeWidth, Walls[i].holeHeight, Walls[i].WallX); //draw each of the walls in the list
    }
    DrawWallSkybox(matStage, floor, sceneData, playerPosOffset);


    /* Axis of global origin */
    {
        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
        XrVector3f    pos   = {0.0f, 0.0f, 0.0f};
        XrQuaternionf qtn   = {0.0f, 0.0f, 0.0f, 1.0f};
        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
//        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
    }

    /* Axis of stage origin */
    {
        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
        XrVector3f    &pos  = stagePose.position;
        XrQuaternionf &qtn  = stagePose.orientation;
        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
//        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
    }

    /* teapot */
    Vec3 col = {0.2f, 0.8f, 0.7f};

//    draw_teapot (sceneData.elapsed_us / 1000, col, (float *)&matP, (float *)&matV);
    draw_uiplane ((float *)&matPVM, layerView, sceneData);
//    Vec3 zero = Vec3(0.0f, 0.0f, 0.0f);
//    Vec3 one = Vec3(5.0f, 5.0f, 5.0f);

//    draw_line((float *)&matPVM, one, zero);

    //this is the left hand
//    if(sceneData.inputState.triggerVal[0] > 0.8f){
//        leftHandForward = getDirectionFromQuaternion(sceneData.handLoc[0].pose.orientation, {0.0f, 1.0f, 0.0f});
//        playerVel -= leftHandForward;
//        grapplePointLeft = findGrapplePoint(handPosLeft, -leftHandForward); //calculate the grapple hit pos
//
//        if(!leftGrapplePlaced){
//            leftGrapplePlaced = true;
//            applyGrappleSpring(handPosLeft, grapplePointLeft, &playerVel, deltaTime);
//
//        }
//    }
//    else{
//        leftGrapplePlaced = false;
//        grapplePointLeft = handPosLeft;
//
//    }
    if(sceneData.inputState.triggerVal[0] > 0.8f){
        leftHandForward = getDirectionFromQuaternion(sceneData.handLoc[0].pose.orientation, {0.0f, 1.0f, 0.0f});

        if(!leftGrapplePlaced){
            grapplePointLeft = findGrapplePoint(handPosLeft, -leftHandForward);
            leftGrapplePlaced = true;
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
    //    playerVel -= rightHandForward;

        if(!rightGrapplePlaced){
            grapplePointRight = findGrapplePoint(handPosRight, -rightHandForward);
            rightGrapplePlaced = true;
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
    if(playerPosOffset.z < -13.0f) {
        playerVel.z = 0;
        playerPosOffset.z = -13.0f;
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
    bool inWallX = playerPosOffset.x < wallFrontX + 1 && playerPosOffset.x > wallBackX;
    bool inWallY = playerPosOffset.y < wallTopY && playerPosOffset.y > wallBottomY;
    bool inWallZ = playerPosOffset.z < wallRightZ && playerPosOffset.z > wallLeftZ;

    bool inHoleY = playerPosOffset.y < holeTopY-1.5 && playerPosOffset.y > holeBottomY-1.5;
    bool inHoleZ = playerPosOffset.z < holeRightZ && playerPosOffset.z > holeLeftZ;

    // Final check
    if (inWallX && inWallY && inWallZ && !(inHoleY && inHoleZ)) {   //I still need to fix that this is not the camera pos. Need to add player offset and scene data position
        reSpawn();
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

    /* Axis of hand grip */
//    for (XrSpaceLocation loc : sceneData.handLoc)
//    {
//        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
//        XrVector3f    &pos  = loc.pose.position;
//
//        pos.x += playerPosOffset.x;
//        pos.y += playerPosOffset.y;
//        pos.z += playerPosOffset.z;
//        XrQuaternionf &qtn  = loc.pose.orientation;
//        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
////        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
//        if(grapplePointRight)
//        draw_Grapple ((float *)&matP, (float *)&matV, (float *)&matM);
//        draw_line((float *)&matPVM, handPosLeft, grapplePointLeft);
//        draw_line((float *)&matPVM, handPosRight, grapplePointRight);
//
//
////        float handPos[] = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
////        draw_Sphere(handPos, col, (float *)&matP, (float *)&matV);
//        GLASSERT();
//    }

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
                draw_Hand ((float *)&matP, (float *)&matV, (float *)&matM);

            }
            else{
                draw_Grapple ((float *)&matP, (float *)&matV, (float *)&matM);
            }
        }
        if(i == 1){ //left hand
            if(rightGrapplePlaced){
                draw_line((float *)&matPVM, handPosRight, grapplePointRight);
                draw_Hand ((float *)&matP, (float *)&matV, (float *)&matM);

            }
            else{
                draw_Grapple ((float *)&matP, (float *)&matV, (float *)&matM);
            }
        }


//        float handPos[] = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
//        draw_Sphere(handPos, col, (float *)&matP, (float *)&matV);
        GLASSERT();
    }

#if 0
    /* Axis of hand aim */
    for (XrSpaceLocation loc : sceneData.aimLoc)
    {
        XrVector3f    scale = {0.1f, 0.1f, 0.1f};
        XrVector3f    &pos  = loc.pose.position;
        XrQuaternionf &qtn  = loc.pose.orientation;
        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
        GLASSERT();
    }
#endif

    for (XrHandJointLocationsEXT *loc : sceneData.handJointLoc)
    {
        for (uint32_t i = 0; i < loc->jointCount; i ++)
        {
            XrVector3f    &pos  = loc->jointLocations[i].pose.position;
            XrQuaternionf &qtn  = loc->jointLocations[i].pose.orientation;
            float         rad   = loc->jointLocations[i].radius;
            XrVector3f    scale = {rad, rad, rad};
            XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
            draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);

            GLASSERT();
        }
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




