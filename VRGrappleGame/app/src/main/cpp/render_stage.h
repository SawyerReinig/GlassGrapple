#ifndef RENDER_STAGE_H_
#define RENDER_STAGE_H_


int init_stage ();
int draw_stage (float *mtxPV);
int draw_axis  (float *matP, float *matV, float *matM);
int draw_Grapple  (float *matP, float *matV, float *matM);
int draw_Hand (float *matP, float *matV, float *matM); //for now this just draws a sphere

int draw_bone  (float *matP, float *matV, float *matM, float radius, float *color);

#endif
