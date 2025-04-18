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

void draw_sphere_vbo(Vec3 col) {
    glUseProgram(s_sobj.program);

    // Bind VBO for positions
    glEnableVertexAttribArray(s_sobj.loc_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
    glVertexAttribPointer(s_sobj.loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Bind VBO for normals
    glEnableVertexAttribArray(s_sobj.loc_nrm);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_nbo);
    glVertexAttribPointer(s_sobj.loc_nrm, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Bind index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ibo);

    // Set color
    glUniform3f(s_loc_color, col.x, col.y, col.z);

    // Draw elements
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(s_sobj.loc_vtx);
    glDisableVertexAttribArray(s_sobj.loc_nrm);
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

int draw_Sphere (Vec3 pos, Vec3 col, float *matP, float *matV)
{
    float matM[16], matVM[16], matPVM[16], matVMI4x4[16], matVMI3x3[9];;

    glUseProgram (s_sobj.program);
    glEnableVertexAttribArray (s_sobj.loc_vtx);
    glEnableVertexAttribArray (s_sobj.loc_nrm);

    glBindBuffer (GL_ARRAY_BUFFER, sphere_vbo);
    glVertexAttribPointer (s_sobj.loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer (GL_ARRAY_BUFFER, sphere_nbo);
    glVertexAttribPointer (s_sobj.loc_nrm, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, sphere_ibo);

    matrix_identity (matM);
//    matrix_translate (matM, 0.0f, 0.0f, -3.0f);
//    matrix_rotate (matM, count*0.1f, 0.0f, 1.0f, 0.0f);
    matrix_translate (matM, pos.x, pos.y, pos.z);
    matrix_scale (matM, 0.02f, 0.02f, 0.02f);


    matrix_mult (matVM, matV, matM);

    matrix_copy (matVMI4x4, matVM);
    matrix_invert   (matVMI4x4);
    matrix_transpose(matVMI4x4);
    matVMI3x3[0] = matVMI4x4[0];
    matVMI3x3[1] = matVMI4x4[1];
    matVMI3x3[2] = matVMI4x4[2];
    matVMI3x3[3] = matVMI4x4[4];
    matVMI3x3[4] = matVMI4x4[5];
    matVMI3x3[5] = matVMI4x4[6];
    matVMI3x3[6] = matVMI4x4[8];
    matVMI3x3[7] = matVMI4x4[9];
    matVMI3x3[8] = matVMI4x4[10];

    matrix_mult (matPVM, matP, matVM);
    glUniformMatrix4fv (ModelViewMatrix,   1, GL_FALSE, matVM );
    glUniformMatrix4fv (s_loc_mtx_pmv,  1, GL_FALSE, matPVM);
    glUniformMatrix3fv (s_loc_mtx_nrm,  1, GL_FALSE, matVMI3x3);

    glUniform3f (s_loc_color, col.x, col.y, col.z);

    glEnable (GL_DEPTH_TEST);
    draw_sphere_vbo (col);

    GLASSERT ();
    return 0;
}




