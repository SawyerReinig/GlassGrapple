//
// Created by Sawyer on 2/11/2025.
//
#include <stdio.h>
#include <GLES2/gl2.h>
#include "assertgl.h"
#include "util_shader.h"
#include "util_matrix.h"
#include <math.h>
#include <stdlib.h>
#include "render_scene.h"
#include "VRGame.h"
#include <cmath>


#define PI 3.14159265358979323846

static GLuint sphere_vbo, sphere_nbo, sphere_ibo;
static int sphereIndexCount;

static shader_obj_t s_sobj;
//static GLuint s_vtx_id, s_nrm_id, g_idx_id;
static GLint  ModelViewMatrix;
static GLint  s_loc_mtx_pmv;
static GLint  s_loc_mtx_nrm;
static GLint  s_loc_color;


static char VertexShaderCode[] = "                          \n\
                                                            \n\
attribute vec4  a_Vertex;                                   \n\
attribute vec3  a_Normal;                                   \n\
uniform   mat4  u_PMVMatrix;                                \n\
uniform   mat4  u_MVMatrix;                                 \n\
uniform   mat3  u_ModelViewIT;                              \n\
varying   vec3  v_diffuse;                                  \n\
varying   vec3  v_specular;                                 \n\
const     float shiness = 16.0;                             \n\
const     vec3  LightPos = vec3(4.0, 4.0, 4.0);             \n\
const     vec3  LightCol = vec3(1.0, 1.0, 1.0);             \n\
                                                            \n\
void DirectionalLight (vec3 normal, vec3 eyePos)            \n\
{                                                           \n\
    vec3  lightDir = normalize (LightPos);                  \n\
    vec3  halfV    = normalize (LightPos - eyePos);         \n\
    float dVP      = max(dot(normal, lightDir), 0.0);       \n\
    float dHV      = max(dot(normal, halfV   ), 0.0);       \n\
                                                            \n\
    float pf = 0.0;                                         \n\
    if(dVP > 0.0)                                           \n\
        pf = pow(dHV, shiness);                             \n\
                                                            \n\
    v_diffuse += dVP * LightCol;                            \n\
    v_specular+= pf  * LightCol;                            \n\
}                                                           \n\
                                                            \n\
void main(void)                                             \n\
{                                                           \n\
    gl_Position = u_PMVMatrix * a_Vertex;                   \n\
    vec3 normal = normalize(u_ModelViewIT * a_Normal);      \n\
    vec3 eyePos = vec3(u_MVMatrix * a_Vertex);              \n\
                                                            \n\
    v_diffuse  = vec3(0.0);                                 \n\
    v_specular = vec3(0.0);                                 \n\
    DirectionalLight(normal, eyePos);                       \n\
}                                                           ";

static char FragmentShaderCode[] = "                                   \n\
precision mediump float;                                    \n\
                                                            \n\
uniform vec3    u_color;                                    \n\
varying vec3    v_diffuse;                                  \n\
varying vec3    v_specular;                                 \n\
void main(void)                                             \n\
{                                                           \n\
    vec3 color = u_color * 0.1;                             \n\
    color += (u_color * v_diffuse);                         \n\
    color += v_specular;                                    \n\
    gl_FragColor = vec4(color, 1.0);                        \n\
}                                                           ";


//this is where the points of a sphere are made
void generate_sphere_data(float radius, int latDivs, int longDivs, GLfloat **vertices, GLfloat **normals, GLushort **indices, int *numVertices, int *numIndices) {
    int i, j;
    int vertexCount = (latDivs + 1) * (longDivs + 1);
    int indexCount = latDivs * longDivs * 6;

    *vertices = (GLfloat*)malloc(vertexCount * 3 * sizeof(GLfloat));
    *normals = (GLfloat*)malloc(vertexCount * 3 * sizeof(GLfloat));
    *indices = (GLushort*)malloc(indexCount * sizeof(GLushort));

    int vIndex = 0, iIndex = 0;
    for (i = 0; i <= latDivs; i++) {
        float theta = i * M_PI / latDivs;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (j = 0; j <= longDivs; j++) {
            float phi = j * 2 * M_PI / longDivs;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;
            //Normals are easy for a sphere, they are just the position on the sphere.

            (*vertices)[vIndex] = radius * x;
            (*normals)[vIndex++] = x;
            (*vertices)[vIndex] = radius * y;
            (*normals)[vIndex++] = y;
            (*vertices)[vIndex] = radius * z;
            (*normals)[vIndex++] = z;
        }
    }

    for (i = 0; i < latDivs; i++) {
        for (j = 0; j < longDivs; j++) {
            int first = (i * (longDivs + 1)) + j;
            int second = first + longDivs + 1;

            (*indices)[iIndex++] = first;
            (*indices)[iIndex++] = second;
            (*indices)[iIndex++] = first + 1;

            (*indices)[iIndex++] = second;
            (*indices)[iIndex++] = second + 1;
            (*indices)[iIndex++] = first + 1;
        }
    }

    *numVertices = vertexCount;
    *numIndices = indexCount;
}

int init_Sphere() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    generate_shader(&s_sobj, VertexShaderCode, FragmentShaderCode);
    ModelViewMatrix  = glGetUniformLocation(s_sobj.program, "u_MVMatrix");
    s_loc_mtx_pmv = glGetUniformLocation(s_sobj.program, "u_PMVMatrix");
    s_loc_mtx_nrm = glGetUniformLocation(s_sobj.program, "u_ModelViewIT");
    s_loc_color   = glGetUniformLocation(s_sobj.program, "u_color");

    // Generate sphere vertices, normals, and indices
    int numVertices, numIndices;
    GLfloat *sphereVertices, *sphereNormals;
    GLushort *sphereIndices;

    generate_sphere_data(1.0f, 12, 12, &sphereVertices, &sphereNormals, &sphereIndices, &numVertices, &numIndices);
    sphereIndexCount = numIndices; // Store for rendering

    // Generate and upload vertex buffer
    glGenBuffers(1, &sphere_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
    glBufferData(GL_ARRAY_BUFFER, numVertices * 3 * sizeof(GLfloat), sphereVertices, GL_STATIC_DRAW);

    // Generate and upload normal buffer
    glGenBuffers(1, &sphere_nbo);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_nbo);
    glBufferData(GL_ARRAY_BUFFER, numVertices * 3 * sizeof(GLfloat), sphereNormals, GL_STATIC_DRAW);

    // Generate and upload index buffer
    glGenBuffers(1, &sphere_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(GLushort), sphereIndices, GL_STATIC_DRAW);

    GLASSERT();
    return 0;
}



void copyAssetToExternal(AAssetManager* mgr, const char* assetName, const char* destPath) {
    // Skip if the file already exists
    std::ifstream test(destPath);
    if (test.good()) {
        return;
    }

    AAsset* asset = AAssetManager_open(mgr, assetName, AASSET_MODE_STREAMING);
    if (!asset) {
        __android_log_print(ANDROID_LOG_ERROR, "MP3COPY", "Failed to open asset: %s", assetName);
        return;
    }

    FILE* outFile = fopen(destPath, "wb");
    if (!outFile) {
        __android_log_print(ANDROID_LOG_ERROR, "MP3COPY", "Failed to open destination: %s", destPath);
        AAsset_close(asset);
        return;
    }

    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    int bytesRead = 0;

    while ((bytesRead = AAsset_read(asset, buffer, bufferSize)) > 0) {
        fwrite(buffer, 1, bytesRead, outFile);
    }

    fclose(outFile);
    AAsset_close(asset);
}


void copyAllAssets(AAssetManager* mgr, const char* packageName) {
    AAssetDir* assetDir = AAssetManager_openDir(mgr, "");
    if (!assetDir) {
        __android_log_print(ANDROID_LOG_ERROR, "ASSETCOPY", "Failed to open asset directory");
        return;
    }

    const char* filename = nullptr;
    while ((filename = AAssetDir_getNextFileName(assetDir)) != nullptr) {
        std::string name(filename);

        // Check file extension
        if (name.size() > 4) {
            std::string ext = name.substr(name.size() - 4);

            if (ext == ".mp3" || ext == ".obj") {
                std::string destPath = "/sdcard/Android/data/";
                destPath += packageName;
                destPath += "/files/";
                destPath += name;

                __android_log_print(ANDROID_LOG_INFO, "ASSETCOPY", "Copying: %s → %s", name.c_str(), destPath.c_str());

                copyAssetToExternal(mgr, name.c_str(), destPath.c_str());
            }
        }
    }

    AAssetDir_close(assetDir);
}



#include <limits> // for std::numeric_limits

bool intersectRayPlane(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& planeNormal, float planeD, float& outT)
{
    float denom = dot(planeNormal, rayDir);
    if (fabs(denom) < 1e-6f) {
        return false; // Ray is parallel to the plane
    }

    float t = -(dot(planeNormal, rayOrigin) + planeD) / denom;
    if (t < 0.0f) {
        return false; // Intersection behind the ray
    }

    outT = t;
    return true;
}

Vec3 findGrapplePoint(const Vec3& handPos, const Vec3& handDir)
{
    float bestT = std::numeric_limits<float>::max();
    Vec3 bestPoint = handPos;

    // Define each wall: normal vector + D offset
    struct Plane { Vec3 normal; float d; };

    Plane planes[] = {
            {{ 1, 0, 0 }, -15.0f},  // x = 15 (positive X wall)
            {{-1, 0, 0 }, -15.0f},  // x = -15 (negative X wall)
            {{ 0, 1, 0 },  0.0f },  // y = 0  (floor)
            {{ 0,-1, 0 }, 13.0f },  // y = 13 (ceiling)
            {{ 0, 0, 1 }, -15.0f},  // z = 15 (positive Z wall)
            {{ 0, 0,-1 }, -15.0f},  // z = -15 (negative Z wall)
    };

    for (const auto& plane : planes)
    {
        float t;
        if (intersectRayPlane(handPos, handDir, plane.normal, plane.d, t))
        {
            if (t < bestT)
            {
                bestT = t;
                bestPoint = {
                        handPos.x + t * handDir.x,
                        handPos.y + t * handDir.y,
                        handPos.z + t * handDir.z
                };
            }
        }
    }

    return bestPoint;
}


void applyGrappleSpring(Vec3 playerPos, Vec3 grapplePoint, Vec3 *playerVel, float deltaTime)
{
    const float springK = 3.0f;      // Spring stiffness
    const float damping = 0.0f;        // Damping to avoid infinite swinging
    const float restLength = 3.0f;     // How long the rope wants to be (0 = fully pulled)

    // Direction from player to grapple
    Vec3 delta = {
            grapplePoint.x - playerPos.x,
            grapplePoint.y - playerPos.y,
            grapplePoint.z - playerPos.z
    };

    float distance = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

    // Normalize delta
    Vec3 direction = {0, 0, 0};
    if (distance > 0.0001f) {
        direction.x = delta.x / distance;
        direction.y = delta.y / distance;
        direction.z = delta.z / distance;
    }

    // Spring force (Hooke's Law)
    float stretch = distance - restLength;
    if (stretch < 0) stretch = 0;
    Vec3 springForce = {
            -springK * stretch * direction.x,
            -springK * stretch * direction.y,
            -springK * stretch * direction.z
    };

    // Damping force
    float velAlongDir = playerVel->x * direction.x + playerVel->y * direction.y + playerVel->z * direction.z;
    Vec3 dampingForce = {
            -damping * velAlongDir * direction.x,
            -damping * velAlongDir * direction.y,
            -damping * velAlongDir * direction.z
    };

    // Total force
    Vec3 totalForce = {
            springForce.x + dampingForce.x,
            springForce.y + dampingForce.y,
            springForce.z + dampingForce.z
    };

    // Apply force to velocity
    playerVel->x -= totalForce.x * deltaTime;
    playerVel->y -= totalForce.y * deltaTime;
    playerVel->z -= totalForce.z * deltaTime;
}



Vec3 ColorFrom255(float r, float g, float b){
    return Vec3(r/255.0f, g/255.0f, b/255.0f);
}

float Mod(float NumberToMod, float Modder){
    return std::fmod(NumberToMod, Modder);
}


float MirrorMod(float value, float max) {
    float mod = fmod(value, max * 2.0f);
    if (mod < max)
        return mod;
    else
        return max * 2.0f - mod;
}



