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


static shader_obj_t     s_sobj;
static render_target_t  s_rtarget;

static shader_obj_t     PaintShaderObject;
static shader_obj_t     SkyboxShaderObject;


//    static render_target_t  PaintShaderTarget;

#define UI_WIN_W 300
#define UI_WIN_H 740


struct Bullet {
    Vec3 position = {0,0,0}; // x, y, z
    Vec3 velocity = {0,0,0}; // vx, vy, vz
    float lifetime = 0;    // Time to pop from matrix/destroy
};

struct Wall{
    float WallX;
    float hole;
};


std::vector<Bullet> Bullets;
float timelastFrame;
const float deltaTime = 0.0138;

#define MAX_IMPACTS 100  // Limit number of tracked impacts


std::vector<Vec3> impactPoints;

Vec3 playerPosOffset;
float PlayerYRot = 0.0f; //this is in radians
Vec3 playerVel;

static char s_strVS[] = "                                   \n\
                                                            \n\
attribute vec4  a_Vertex;                                   \n\
attribute vec4  a_Color;                                    \n\
varying   vec4  v_color;                                    \n\
uniform   mat4  u_PMVMatrix;                                \n\
                                                            \n\
void main(void)                                             \n\
{                                                           \n\
    gl_Position = u_PMVMatrix * a_Vertex;                   \n\
    v_color     = a_Color;                                  \n\
}                                                           ";

static char s_strFS[] = "                                   \n\
precision mediump float;                                    \n\
varying   vec4  v_color;                                    \n\
                                                            \n\
void main(void)                                             \n\
{                                                           \n\
    gl_FragColor = v_color;                                 \n\
}                                                           ";

static char PaintFloorVertexShader[] = "                                   \n\
                                                            \n\
attribute vec4  a_Vertex;                                   \n\
attribute vec3  a_Normal;                                   \n\
uniform   mat4  u_PMVMatrix;                                \n\
varying   vec3  v_Normal;                                   \n\
varying   vec3  v_Position;                                 \n\
                                                            \n\
void main(void)                                             \n\
{                                                           \n\
    gl_Position = u_PMVMatrix * a_Vertex;                   \n\
    v_Position = a_Vertex.xyz;                                  \n\
}                                                           ";

static char PaintFloorFragmentShader[] = R"(
precision mediump float;
varying vec3 v_Normal;
varying vec3 v_Position;


void main() {
    vec3 floorColor = vec3(0.0, 0.0, 0.0);
    vec3 finalColor = mix(floorColor, vec3(1.0, 1.0, 1.0), 0.0);
    gl_FragColor = vec4(finalColor, 1.0);

}           )";



//I THINK I NEED TO TRY SUBTRACTING THE PLAYER POSITION FROM THE VERTEX SHADER, SO THE WALLS DON'T SLIDE WHEN MOVING!!!
static char SkyboxPlaneVertexShader[] = R"(
attribute vec4  a_Vertex;
attribute vec3  a_Normal;
uniform   mat4  u_PMVMatrix;
uniform   int   u_FaceID;
varying   vec3  v_Position;

void main(void)
{
//    a_Vertex.xyz = a_Vertex.xyz;
    gl_Position = u_PMVMatrix * a_Vertex;

    //Because of the way the frag shader works. We esentailly need to trick the frag shader into thinking that it is actually along the Z plane.
    vec3 rotated = a_Vertex.xyz;
    if (u_FaceID == 0) { // Ceiling (+Y)
        rotated = vec3(a_Vertex.y, a_Vertex.z, a_Vertex.x);
    }
    else if (u_FaceID == 1) { // Floor (-Y)
        rotated = vec3(-a_Vertex.y, a_Vertex.z, a_Vertex.x);
    }
    else if (u_FaceID == 4) { // Right (+X)
        rotated = vec3(-a_Vertex.z, a_Vertex.y, a_Vertex.x);
    }
    else if (u_FaceID == 5) { // Left (-X)
        rotated = vec3(a_Vertex.z, a_Vertex.y, -a_Vertex.x);
    }
    v_Position = rotated;
}
)";

static char SkyboxPlaneFSCubes[] = "                             \n\
precision mediump float;                                        \n\
varying vec3 v_Position;                                        \n\
uniform float iTime;                                            \n\
                                                                \n\
float safeMod(float a, float b) {                               \n\
    return a - b * floor(a / b);                                \n\
}                                                               \n\
vec2 safeMod(vec2 a, vec2 b) {                                  \n\
    return a - b * floor(a / b);                                \n\
}                                                               \n\
                                                                \n\
float segment(vec2 p, vec2 a, vec2 b) {                         \n\
    p -= a;                                                    \n\
    b -= a;                                                    \n\
    float d = dot(b, b);                                       \n\
    if (d < 0.0001) return length(p);                           \n\
    return length(p - b * clamp(dot(p, b) / d, 0.0, 1.0));      \n\
}                                                               \n\
                                                                \n\
mat2 rot(float a) {                                             \n\
    float c = cos(a);                                           \n\
    float s = sin(a);                                           \n\
    return mat2(c, -s, s, c);                                   \n\
}                                                               \n\
                                                                \n\
float t;                                                        \n\
vec2 T(vec3 p) {                                                \n\
    p.xy *= rot(-t);                                            \n\
    p.xz *= rot(0.785);                                         \n\
    p.yz *= rot(-0.625);                                        \n\
    return p.xy;                                                \n\
}                                                               \n\
                                                                \n\
float fastTanh(float x) {                                       \n\
    float e = exp(-2.0 * abs(x));                               \n\
    float t = (1.0 - e) / (1.0 + e);                            \n\
    return x < 0.0 ? -t : t;                                    \n\
}                                                               \n\
                                                                \n\
void main() {                                                   \n\
    vec2 U = v_Position.yz * 2.0;                               \n\
    vec2 M = vec2(2.0, 2.3);                                    \n\
    vec2 I = floor(U / M) * M;                                  \n\
    vec3 color = vec3(0.0);                                     \n\
                                                                \n\
    float angles[4];                                            \n\
    angles[0] = 0.0;                                            \n\
    angles[1] = 1.57;                                           \n\
    angles[2] = 3.14;                                           \n\
    angles[3] = 4.71;                                           \n\
                                                                \n\
    for (int dx = -1; dx <= 1; dx++) {                          \n\
        for (int dy = -1; dy <= 1; dy++) {                      \n\
            vec2 offset = vec2(float(dx), float(dy)) * M;       \n\
            vec2 J = I + offset;                                \n\
            vec2 X = J;                                         \n\
                                                                \n\
            float check = safeMod(J.x / M.x, 2.0) + safeMod(J.y / M.y, 2.0); \n\
            if (fract(check / 2.0) > 0.5) {                     \n\
                X.y += 1.15;                                    \n\
            }                                                   \n\
                                                                \n\
            t = fastTanh(-0.2 * (J.x + J.y) + safeMod(2.0 * iTime, 23.0) - 9.0) * 0.785; \n\
                                                                \n\
            for (int i = 0; i < 4; i++) {                       \n\
                float a = angles[i];                            \n\
                vec3 A = vec3(cos(a), sin(a), 0.7);             \n\
                vec3 B = vec3(-A.y, A.x, 0.7);                  \n\
                color += smoothstep(0.1, 0.0, segment(U - X, T(A), T(B))); \n\
                                                                \n\
                vec3 B2 = vec3(A.xy, -A.z);                     \n\
                color += smoothstep(0.1, 0.0, segment(U - X, T(A), T(B2))); \n\
                                                                \n\
                A.z = -A.z; B.z = -B.z;                         \n\
                color += smoothstep(0.1, 0.0, segment(U - X, T(A), T(B))); \n\
            }                                                   \n\
        }                                                       \n\
    }                                                           \n\
                                                                \n\
    gl_FragColor = vec4(color, 1.0);                            \n\
}                                                               \n";

//
//static char GridFragShader[] = R"(
//precision mediump float;
//
//varying vec3 v_Position;
//
//uniform float gridSize;      // Size of each cell
//uniform float lineWidth;     // Thickness of lines
//uniform vec3  lineColor;     // Color of the grid lines
//uniform vec3  bgColor;       // Color of background
//
//void main()
//{
//    // Snap to 2D grid based on XZ (like ground plane) or YZ, etc.
//    vec2 pos = v_Position.xz;  // <- use .yz for walls
//
//    // Get fractional offset from grid lines
//    vec2 grid = abs((pos / gridSize - 0.5));
//
//    // Thin lines where either axis is close to 0
//    float line = step(1.0 - lineWidth / gridSize, max(grid.x, grid.y));
//
//    // Mix between lineColor and bgColor
//    vec3 color = mix(lineColor, bgColor, line);
//
//    gl_FragColor = vec4(color, 1.0);
//}
//)";

//Shout out to the shader toy example that I made the base of my skybox:
//https://www.shadertoy.com/view/lcSGDD



//This Function was included in the library code
int
init_gles_scene ()
{
    generate_shader (&s_sobj, s_strVS, s_strFS);
    generate_shader (&PaintShaderObject, PaintFloorVertexShader, PaintFloorFragmentShader);
//    generate_shader (&SkyboxShaderObject, SkyboxPlaneVertexShader, SkyboxPlaneFSCubes);  //currently have the old VS in this.
    generate_shader (&SkyboxShaderObject, SkyboxPlaneVertexShader, SkyboxPlaneFSCubes);  //currently have the old VS in this.



    init_teapot (); // this was in the sample
    init_stage (); //as was this
    init_texplate ();
    init_Sphere();
    init_dbgstr (0, 0);
    init_imgui (UI_WIN_W, UI_WIN_H);

    create_render_target (&s_rtarget, UI_WIN_W, UI_WIN_H, RTARGET_COLOR);

    return 0;
}


//This Function was included in the library code
int draw_line (float *mtxPV, float *p0, float *p1, float *color)
{
    GLfloat floor_vtx[6];
    for (int i = 0; i < 3; i ++)
    {
        floor_vtx[0 + i] = p0[i];
        floor_vtx[3 + i] = p1[i];
    }

    shader_obj_t *sobj = &s_sobj;
    glUseProgram (sobj->program);

    glEnableVertexAttribArray (sobj->loc_vtx);
    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);

    glDisableVertexAttribArray (sobj->loc_clr);
    glVertexAttrib4fv (sobj->loc_clr, color);

    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);

    glEnable (GL_DEPTH_TEST);
    glDrawArrays (GL_LINES, 0, 2);

    return 0;
}


int old_draw_plane (float *mtxPV, float *color)
{
    GLfloat floor_vtx[] ={

            -15,-1.0,-15,
            +15,-1.0,-15,
            -15,-1.0,+15,
            +15,-1.0,-15,
            -15,-1.0,+15,
            +15,-1.0,+15};

    shader_obj_t *sobj = &PaintShaderObject;
    glUseProgram (sobj->program);

    glEnableVertexAttribArray (sobj->loc_vtx);
    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);

    glDisableVertexAttribArray (sobj->loc_clr);
    glVertexAttrib4fv (sobj->loc_clr, color);
    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);

    glEnable (GL_DEPTH_TEST);

    glDrawArrays (GL_TRIANGLES, 0, 6);

    return 0;
}





int DrawWallSkyboxTest(float *mtxPV, float *color, scene_data_t sceneData, Vec3 playerPos)
{
    float SkycubeSize = 30;
    GLfloat floor_vtx[] ={
            //this is using the triangle style, so think about how holes will get made

            SkycubeSize + playerPos.x,-SkycubeSize,-SkycubeSize + playerPos.z,
            SkycubeSize + playerPos.x,-SkycubeSize,+SkycubeSize + playerPos.z,
            SkycubeSize + playerPos.x,SkycubeSize,-SkycubeSize + playerPos.z,
            SkycubeSize + playerPos.x,SkycubeSize,SkycubeSize + playerPos.z,
            SkycubeSize+ playerPos.x,-SkycubeSize,+SkycubeSize+ playerPos.z,
            SkycubeSize+ playerPos.x,SkycubeSize,-SkycubeSize+ playerPos.z

    };


    shader_obj_t *sobj = &SkyboxShaderObject;

    glUseProgram (sobj->program);

    GLint success = 0;
    glGetProgramiv(sobj->program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(sobj->program, 512, NULL, infoLog);
        printf("Program link error: %s\n", infoLog);
    }


    float currentTime = sceneData.elapsed_us * 1e-6;

    glUniform1f(glGetUniformLocation(sobj->program, "iTime"), currentTime);

    glEnableVertexAttribArray (sobj->loc_vtx);
    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);
    glDisableVertexAttribArray (sobj->loc_clr);
    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);


    glEnable (GL_DEPTH_TEST);

    glDrawArrays (GL_TRIANGLES, 0, 6);

    return 0;
}

//
//int DrawWallSkybox(float *mtxPV, float *color, scene_data_t sceneData, Vec3 playerPos)
//{
//    float s = 10.0f;
//
//    // Vertex data for all 6 faces of the cube (each face = 2 triangles = 6 vertices)
//    GLfloat cube_vtx[] = {
//            // +X (right wall)
//            s + playerPos.x, -s, -s + playerPos.z,
//            s + playerPos.x, -s,  s + playerPos.z,
//            s + playerPos.x,  s, -s + playerPos.z,
//            s + playerPos.x,  s, -s + playerPos.z,
//            s + playerPos.x, -s,  s + playerPos.z,
//            s + playerPos.x,  s,  s + playerPos.z,
//
//            // -X (left wall)
//            -s + playerPos.x, -s,  s + playerPos.z,
//            -s + playerPos.x, -s, -s + playerPos.z,
//            -s + playerPos.x,  s,  s + playerPos.z,
//            -s + playerPos.x,  s,  s + playerPos.z,
//            -s + playerPos.x, -s, -s + playerPos.z,
//            -s + playerPos.x,  s, -s + playerPos.z,
//
//            // +Z (back wall)
//            -s + playerPos.x, -s, s + playerPos.z,
//            s + playerPos.x, -s, s + playerPos.z,
//            -s + playerPos.x,  s, s + playerPos.z,
//            -s + playerPos.x,  s, s + playerPos.z,
//            s + playerPos.x, -s, s + playerPos.z,
//            s + playerPos.x,  s, s + playerPos.z,
//
//            // -Z (front wall)
//            s + playerPos.x, -s, -s + playerPos.z,
//            -s + playerPos.x, -s, -s + playerPos.z,
//            s + playerPos.x,  s, -s + playerPos.z,
//            s + playerPos.x,  s, -s + playerPos.z,
//            -s + playerPos.x, -s, -s + playerPos.z,
//            -s + playerPos.x,  s, -s + playerPos.z,
//
//            // +Y (ceiling)
//            -s + playerPos.x, s,  s + playerPos.z,
//            s + playerPos.x, s,  s + playerPos.z,
//            -s + playerPos.x, s, -s + playerPos.z,
//            -s + playerPos.x, s, -s + playerPos.z,
//            s + playerPos.x, s,  s + playerPos.z,
//            s + playerPos.x, s, -s + playerPos.z,
//
//            // -Y (floor)
//            -s + playerPos.x, -s, -s + playerPos.z,
//            s + playerPos.x, -s, -s + playerPos.z,
//            -s + playerPos.x, -s,  s + playerPos.z,
//            -s + playerPos.x, -s,  s + playerPos.z,
//            s + playerPos.x, -s, -s + playerPos.z,
//            s + playerPos.x, -s,  s + playerPos.z,
//    };
//
//
//
//    shader_obj_t *sobj = &SkyboxShaderObject;
//
//    glUseProgram (sobj->program);
//
//    GLint success = 0;
//    glGetProgramiv(sobj->program, GL_LINK_STATUS, &success);
//    if (!success) {
//        char infoLog[512];
//        glGetProgramInfoLog(sobj->program, 512, NULL, infoLog);
//        printf("Program link error: %s\n", infoLog);
//    }
//
//
//    float currentTime = sceneData.elapsed_us * 1e-6;
//
//    glUniform1f(glGetUniformLocation(sobj->program, "iTime"), currentTime);
//    GLint faceLoc = glGetUniformLocation(sobj->program, "u_FaceID");
//    glUniform1i(faceLoc, 2); // For ceiling
//    glEnableVertexAttribArray (sobj->loc_vtx);
//    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, cube_vtx);
//    glDisableVertexAttribArray (sobj->loc_clr);
//    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);
//
//
//    glEnable (GL_DEPTH_TEST);
//
//    glDrawArrays (GL_TRIANGLES, 0, 36);
//
//    return 0;
//}


void DrawSkyboxFace(const GLfloat* vtx, int faceID, float *mtxPV, float currentTime, shader_obj_t *sobj, Vec3 playerpos)
{
    glUseProgram(sobj->program);

    glUniform1f(glGetUniformLocation(sobj->program, "iTime"), currentTime);
    glUniform1i(glGetUniformLocation(sobj->program, "u_FaceID"), faceID);
    glUniform3f(glGetUniformLocation(sobj->program, "PlayerPos"), playerpos.x, playerpos.y, playerpos.z);

    //fragment shader uniforms
    glUniform1f(glGetUniformLocation(sobj->program, "gridSize"), 1.0f);        // 1 unit cells
    glUniform1f(glGetUniformLocation(sobj->program, "lineWidth"), 0.05f);      // thin lines
    glUniform3f(glGetUniformLocation(sobj->program, "lineColor"), 1.0f, 1.0f, 1.0f); // white lines
    glUniform3f(glGetUniformLocation(sobj->program, "bgColor"), 0.1f, 0.1f, 0.1f);  // dark background

    glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, vtx);

    glDisableVertexAttribArray(sobj->loc_clr);  // Assuming no color per vertex
    glEnable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

int DrawWallSkybox(float *mtxPV, float *color, scene_data_t sceneData, Vec3 playerPos)
{
    float s = 20;
    float px = playerPos.x, pz = playerPos.z;

    float currentTime = sceneData.elapsed_us * 1e-6;
    shader_obj_t *sobj = &SkyboxShaderObject;

    // All faces, CCW order to face inward

    GLfloat faceCeiling[] = {
            -s + px, s,  s + pz,
            s + px, s,  s + pz,
            -s + px, s, -s + pz,
            -s + px, s, -s + pz,
            s + px, s,  s + pz,
            s + px, s, -s + pz
    };

    GLfloat faceFloor[] = {
            -s + px, -s, -s + pz,
            s + px, -s, -s + pz,
            -s + px, -s,  s + pz,
            -s + px, -s,  s + pz,
            s + px, -s, -s + pz,
            s + px, -s,  s + pz
    };

    GLfloat faceRight[] = {
            s + px, -s, -s + pz,
            s + px, -s,  s + pz,
            s + px,  s, -s + pz,
            s + px,  s, -s + pz,
            s + px, -s,  s + pz,
            s + px,  s,  s + pz
    };

    GLfloat faceLeft[] = {
            -s + px, -s,  s + pz,
            -s + px, -s, -s + pz,
            -s + px,  s,  s + pz,
            -s + px,  s,  s + pz,
            -s + px, -s, -s + pz,
            -s + px,  s, -s + pz
    };
//
    GLfloat faceBack[] = {
            -s + px, -s,  s + pz,
            s + px, -s,  s + pz,
            -s + px,  s,  s + pz,
            -s + px,  s,  s + pz,
            s + px, -s,  s + pz,
            s + px,  s,  s + pz
    };

    GLfloat faceFront[] = {
            s + px, -s, -s + pz,
            -s + px, -s, -s + pz,
            s + px,  s, -s + pz,
            s + px,  s, -s + pz,
            -s + px, -s, -s + pz,
            -s + px,  s, -s + pz
    };

    DrawSkyboxFace(faceCeiling, 0, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceFloor,   1, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceRight,   2, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceLeft,    3, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceBack,    4, mtxPV, currentTime, sobj, playerPos);
    DrawSkyboxFace(faceFront,   5, mtxPV, currentTime, sobj, playerPos);

    return 0;
}



int DrawWallWithHole(float *mtxPV, float *color, float holePosNorm, float holeWidth, float holeHeight)
{
    float wallX = -15.0f;
    float backWallX = wallX - 1.0f;
    float wallBottom = -1.0f;
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

//    shader_obj_t *sobj = &PaintShaderObject;
    shader_obj_t *sobj = &s_sobj;
    glUseProgram(sobj->program);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, vtx);

    glDisableVertexAttribArray(sobj->loc_clr);

    //doing polygon offset here to fix the lines from clipping
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);


    glVertexAttrib4fv(sobj->loc_clr, color);
    glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);

    glEnable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLES, 0, 96); // 4 quads = 8 triangles = 24 vertices

    glDisable(GL_POLYGON_OFFSET_FILL);


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
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24); // 8 lines = 16 vertices

    return 0;
}



int draw_plane (float *mtxPV, float *color)
{
    GLfloat floor_vtx[] ={

            -15,-1.0,-15,
            +15,-1.0,-15,
            -15,-1.0,+15,
            +15,-1.0,-15,
            -15,-1.0,+15,
            +15,-1.0,+15};

    shader_obj_t *sobj = &PaintShaderObject;
    glUseProgram (sobj->program);

    glEnableVertexAttribArray (sobj->loc_vtx);
    glVertexAttribPointer (sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, floor_vtx);

    glDisableVertexAttribArray (sobj->loc_vtx);
    glVertexAttrib4fv (sobj->loc_clr, color);
    glUniformMatrix4fv (sobj->loc_mtx,  1, GL_FALSE, mtxPV);

    glEnable (GL_DEPTH_TEST);

    glDrawArrays (GL_TRIANGLES, 0, 6);

    return 0;
}


//This Function was included in the library code
int draw_stage (float *matStage)
{
    float col_r[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float col_g[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float col_b[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    float col_gray[] = {0.5f, 0.5f, 0.5f, 1.0f};
    float p0[3]  = {0.0f, 0.0f, 0.0f};
    float py[3]  = {0.0f, 1.0f, 0.0f};

    for (int x = -10; x <= 10; x ++)
    {
        float *col = (x == 0) ? col_b : col_gray;
        float p0[3]  = {1.0f * x, 0.0f, -10.0f};
        float p1[3]  = {1.0f * x, 0.0f,  10.0f};
        draw_line (matStage, p0, p1, col);
    }
    for (int z = -10; z <= 10; z ++)
    {
        float *col = (z == 0) ? col_r : col_gray;
        float p0[3]  = {-10.0f, 0.0f, 1.0f * z};
        float p1[3]  = { 10.0f, 0.0f, 1.0f * z};
        draw_line (matStage, p0, p1, col);
    }

    draw_line (matStage, p0, py, col_g);
//    float white[4] = {1,1,1,1};
    float floor[4] = {0.63,0.82,1,0.5};

    old_draw_plane(matStage, floor);
    draw_plane(matStage, floor);
//    DrawWallWithHole(matStage, floor, 0.5f, 4.0f, 6.0f); // hole in center, 4 wide, 6 tall

    GLASSERT();

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
        invoke_imgui (&sceneData, playerPosOffset);
    }

    /* restore FBO */
    set_render_target (&rtarget0);

    glEnable (GL_DEPTH_TEST);

    {
        float matPVM[16], matT[16];
//        float win_x = 1.0f + sceneData.inputState.stickVal[1].x * 0.5f;
        float win_x = 1.0f;
        float win_y = 1.0f;

//        float win_y = 0.0f + sceneData.inputState.stickVal[1].y * 0.5f;
        float win_z =-2.0f;
        float win_w = 1.0f;
        float win_h = win_w * ((float)UI_WIN_H / (float)UI_WIN_W);
        matrix_identity (matT);
        matrix_translate (matT, win_x, win_y, win_z);
        matrix_rotate (matT, -30.0f, 0.0f, 1.0f, 0.0f);
        matrix_scale (matT, win_w, win_h, 1.0f);
        matrix_mult (matPVM, matPVMbase, matT);
        draw_tex_plate (s_rtarget.texc_id, matPVM, RENDER2D_FLIP_V);
    }

    return 0;
}

Vec3 getForwardVectorFromQuaternion(XrQuaternionf q) {
    Vec3 forward;
    forward.x = 2.0f * (q.x * q.z + q.w * q.y);
    forward.y = 2.0f * (q.y * q.z - q.w * q.x);
    forward.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    return forward;
}


Vec3 TransformPositionToWorldSpace(const XrMatrix4x4f *matC, const Vec3 *position)
{
    Vec3 worldPos;
    worldPos.x = matC->m[0] * position->x + matC->m[4] * position->y + matC->m[8]  * position->z + matC->m[12];
    worldPos.y = matC->m[1] * position->x + matC->m[5] * position->y + matC->m[9]  * position->z + matC->m[13];
    worldPos.z = matC->m[2] * position->x + matC->m[6] * position->y + matC->m[10] * position->z + matC->m[14];
    return worldPos;
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


    glClearColor (0.3f, 0.1f, 0.1f, 1.0f);
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

    draw_stage (matStage);
    float currentTime = sceneData.elapsed_us * 1e-7; // Convert microseconds to seconds
//    deltaTime = currentTime - timelastFrame;
//    timelastFrame = currentTime;   //really should look into why delta time isn't working
    float t = currentTime - floor(currentTime);       // 0 → 1
    float pingPong = fabs(1.0f - 2.0f * t);            // 0 → 1 → 0
    float floor[4] = {0.6,pingPong,pingPong,0.5};

    DrawWallWithHole(matStage, floor, 0.5f, 4.0f, 6.0f); // hole in center, 4 wide, 6 tall
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
        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
    }

    /* teapot */
    Vec3 col = {0.2f, 0.8f, 0.7f};

//    draw_teapot (sceneData.elapsed_us / 1000, col, (float *)&matP, (float *)&matV);
    draw_uiplane ((float *)&matPVM, layerView, sceneData);

    //this is the left hand
    if(sceneData.inputState.triggerVal[0] > 0.8f){
        playerVel -= getForwardVectorFromQuaternion(sceneData.handLoc[0].pose.orientation);

    }
    //right hand shooting
    if(sceneData.inputState.triggerVal[1] > 0.8f){

        playerVel -= getForwardVectorFromQuaternion(sceneData.handLoc[1].pose.orientation);
    }

    //playing around with player movement
    playerPosOffset.x += playerVel.x * deltaTime;
    playerPosOffset.y += playerVel.y * deltaTime;
    playerPosOffset.z += playerVel.z * deltaTime;

    if(length(playerVel) > 5){
        playerVel = normalize(playerVel);
        playerVel *= 5;
    }
    if (playerPosOffset.y > 0.0f) {
        playerVel.y -= 1.0f * deltaTime; // simple gravity
    }
    else {
        playerVel.y = 0;
        playerPosOffset.y = 0.0f;
        playerVel.x *= 0.99f;
        playerVel.z *= 0.99f;
    }
    if(sceneData.inputState.squeezeVal[0] > 0.5f){
        playerVel.x *= 0.95f;
        playerVel.z *= 0.95f;
//        playerPosOffset.y += 0.1;
    }
    if(sceneData.inputState.clickA){
//        playerVel.y += 2.0f * deltaTime;
        playerPosOffset.x += 2.0f * deltaTime;
//        playerPosOffset.y += 0.1;
    }
    if(sceneData.inputState.clickB){
//        playerVel.y -= 2.0f * deltaTime;
        playerPosOffset.x -= 2.0f * deltaTime;
//        playerPosOffset.y -= 0.1;
    }
    //thumbstick movement
//    playerPosOffset.x -= sceneData.inputState.stickVal[0].y * 0.01;
//    playerPosOffset.z -= sceneData.inputState.stickVal[0].x * 0.01;





    /* Axis of hand grip */
    for (XrSpaceLocation loc : sceneData.handLoc)
    {
        XrVector3f    scale = {0.2f, 0.2f, 0.2f};
        XrVector3f    &pos  = loc.pose.position;

        pos.x += playerPosOffset.x;
        pos.y += playerPosOffset.y;
        pos.z += playerPosOffset.z;
        XrQuaternionf &qtn  = loc.pose.orientation;
        XrMatrix4x4f_CreateTranslationRotationScale (&matM, &pos, &qtn, &scale);
        draw_axis ((float *)&matP, (float *)&matV, (float *)&matM);
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




