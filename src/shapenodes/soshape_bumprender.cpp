/**************************************************************************\
 *
 *  This file is part of the Coin 3D visualization library.
 *  Copyright (C) 1998-2003 by Systems in Motion.  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  ("GPL") version 2 as published by the Free Software Foundation.
 *  See the file LICENSE.GPL at the root directory of this source
 *  distribution for additional information about the GNU GPL.
 *
 *  For using Coin with software that can not be combined with the GNU
 *  GPL, and for taking advantage of the additional benefits of our
 *  support services, please contact Systems in Motion about acquiring
 *  a Coin Professional Edition License.
 *
 *  See <URL:http://www.coin3d.org> for  more information.
 *
 *  Systems in Motion, Teknobyen, Abels Gate 5, 7030 Trondheim, NORWAY.
 *  <URL:http://www.sim.no>.
 *
\**************************************************************************/

#include "soshape_bumprender.h"
#include <Inventor/misc/SoState.h>
#include <Inventor/elements/SoBumpMapElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/details/SoPointDetail.h>
#include <Inventor/C/glue/gl.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/misc/SoGL.h>
#include <Inventor/misc/SoGLImage.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/caches/SoPrimitiveVertexCache.h>
#include <Inventor/elements/SoMultiTextureEnabledElement.h>
#include <Inventor/elements/SoMultiTextureCoordinateElement.h>
#include <Inventor/elements/SoGLMultiTextureImageElement.h>
#include <Inventor/elements/SoGLTextureEnabledElement.h>
#include <Inventor/elements/SoBumpMapMatrixElement.h>
#include <Inventor/elements/SoTextureMatrixElement.h>
#include <Inventor/elements/SoMultiTextureMatrixElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoProjectionMatrixElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/errors/SoDebugError.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include <Inventor/C/glue/glp.h>
#include <assert.h>

// Fragment program for bumpmapping
static const char * bumpspecfpprogram = 
"!!ARBfp1.0"
"PARAM u0 = program.env[0];\n" // Specular color (3 floats) 
"PARAM u1 = program.env[1];\n" // Shininess color (1 float)
"PARAM c0 = {2, 0.5, 0, 0};\n"
"TEMP R0;\n"
"TEMP R1;\n"
"TEX R0.xyz, fragment.texcoord[0], texture[0], 2D;\n"
"ADD R0.xyz, R0, -c0.y;\n"
"MUL R0.xyz, R0, c0.x;\n"
"MOV R1.xyz, fragment.texcoord[2];\n"
"ADD R1.xyz, fragment.texcoord[1], R1;\n"
"DP3 R0.w, R1, R1;\n"
"RSQ R0.w, R0.w;\n"
"MUL R1.xyz, R0.w, R1;\n"
"TEX R1.xyz, R1, texture[1], CUBE;\n"
"ADD R1.xyz, R1, -c0.y;\n"
"MUL R1.xyz, R1, c0.x;\n"
"DP3_SAT R0.x, R0, R1;\n"
"POW R0.x, R0.x, u1.x;\n"
"MUL result.color, u0, R0.x;\n"
"END\n";

// Vertex program for directional lights
static const char * directionallightvpprogram = 
"!!ARBvp1.0\n"
"TEMP R0;\n"
"ATTRIB v26 = vertex.texcoord[2];\n"
"ATTRIB v25 = vertex.texcoord[1];\n"
"ATTRIB v24 = vertex.texcoord[0];\n"
"ATTRIB v18 = vertex.normal;\n"
"ATTRIB v16 = vertex.position;\n"
"PARAM c1 = program.env[1];\n" // Light direction
"PARAM c0 = program.env[0];\n" // Eye position
"PARAM c2[4] = { state.matrix.mvp };\n"
" MOV result.texcoord[0].xy, v24;\n"
" DPH result.position.x, v16.xyzz, c2[0];\n"
" DPH result.position.y, v16.xyzz, c2[1];\n"
" DPH result.position.z, v16.xyzz, c2[2];\n"
" DPH result.position.w, v16.xyzz, c2[3];\n"
" DP3 result.texcoord[1].x, v25.xyzx, c0.xyzx;\n"
" DP3 result.texcoord[1].y, v26.xyzx, c0.xyzx;\n"
" DP3 result.texcoord[1].z, v18.xyzx, c0.xyzx;\n"
" ADD R0.yzw, -v16.xxyz, c1.xxyz;\n"
" DP3 R0.x, R0.yzwy, R0.yzwy;\n"
" RSQ R0.x, R0.x;\n"
" MUL R0.xyz, R0.x, R0.yzwy;\n"
" DP3 result.texcoord[2].x, v25.xyzx, R0.xyzx;\n"
" DP3 result.texcoord[2].y, v26.xyzx, R0.xyzx;\n"
" DP3 result.texcoord[2].z, v18.xyzx, R0.xyzx;\n"
"END\n";

// Vertex program for point lights
static const char * pointlightvpprogram = 
"!!ARBvp1.0\n"
"TEMP R0;\n"
"ATTRIB v26 = vertex.texcoord[2];\n"
"ATTRIB v25 = vertex.texcoord[1];\n"
"ATTRIB v24 = vertex.texcoord[0];\n"
"ATTRIB v18 = vertex.normal;\n"
"ATTRIB v16 = vertex.position;\n"
"PARAM c1 = program.env[1];\n" // Light position
"PARAM c0 = program.env[0];\n" // Eye position
"PARAM c2[4] = { program.local[2..5] };\n"
" MOV result.texcoord[0].xy, v24;\n"
" DPH result.position.x, v16.xyzz, c2[0];\n"
" DPH result.position.y, v16.xyzz, c2[1];\n"
" DPH result.position.z, v16.xyzz, c2[2];\n"
" DPH result.position.w, v16.xyzz, c2[3];\n"
" ADD R0.yzw, c0.xxyz, -v16.xxyz;\n"
" DP3 R0.x, R0.yzwy, R0.yzwy;\n"
" RSQ R0.x, R0.x;\n"
" MUL R0.xyz, R0.x, R0.yzwy;\n"
" DP3 result.texcoord[1].x, v25.xyzx, R0.xyzx;\n"
" DP3 result.texcoord[1].y, v26.xyzx, R0.xyzx;\n"
" DP3 result.texcoord[1].z, v18.xyzx, R0.xyzx;\n"
" ADD R0.yzw, c1.xxyz, -v16.xxyz;\n"
" DP3 R0.x, R0.yzwy, R0.yzwy;\n"
" RSQ R0.x, R0.x;\n"
" MUL R0.xyz, R0.x, R0.yzwy;\n"
" DP3 result.texcoord[2].x, v25.xyzx, R0.xyzx;\n"
" DP3 result.texcoord[2].y, v26.xyzx, R0.xyzx;\n"
" DP3 result.texcoord[2].z, v18.xyzx, R0.xyzx;\n"
"END\n";


soshape_bumprender::soshape_bumprender(void)
{
  this->programsinitialized = FALSE;
}

soshape_bumprender::~soshape_bumprender()
{
}

// to avoid warnings from SbVec3f::normalize()
inline void NORMALIZE(SbVec3f &v)
{
  float len = v.length();
  if (len) {
    len = 1.0f / len;
    v[0] *= len;
    v[1] *= len;
    v[2] *= len;
  }
}

void
soshape_bumprender::initPrograms(const cc_glglue * glue)
{

  // FIXME: Should we make the programs static for this class so that
  // every bump map node would share the same program? (20040129
  // handegar)

  glue->glGenProgramsARB(1, &fragmentprogramid); // -- Fragment program
  glue->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragmentprogramid);
  glue->glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           strlen(bumpspecfpprogram), bumpspecfpprogram);

  // FIXME: Maybe a wrapper for catching fragment program errors
  // should be a part of GLUE... (20031204 handegar)
  GLint errorPos;    
  GLenum err = glGetError();
  if (err) {
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    SoDebugError::postWarning("soshape_bumpspecrender::initPrograms",
                              "Error in fragment program! (byte pos: %d) '%s'.\n", 
                              errorPos, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
    
  }
  
  glue->glGenProgramsARB(1, &dirlightvertexprogramid); // -- Directional light program
  glue->glBindProgramARB(GL_VERTEX_PROGRAM_ARB, dirlightvertexprogramid);
  glue->glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           strlen(directionallightvpprogram), directionallightvpprogram);

  err = glGetError();
  if (err) {
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    SoDebugError::postWarning("soshape_bumpspecrender::initPrograms",
                              "Error in vertex program! (byte pos: %d) '%s'.\n", 
                              errorPos, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
    
  }

  glue->glGenProgramsARB(1, &pointlightvertexprogramid); // -- Point light program
  glue->glBindProgramARB(GL_VERTEX_PROGRAM_ARB, pointlightvertexprogramid);
  glue->glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           strlen(directionallightvpprogram), directionallightvpprogram);

  err = glGetError();
  if (err) {
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    SoDebugError::postWarning("soshape_bumpspecrender::initPrograms",
                              "Error in vertex program! (byte pos: %d) '%s'.\n", 
                              errorPos, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
    
  }


  this->programsinitialized = TRUE;

}

void
soshape_bumprender::renderBumpSpecular(SoState * state,
                                       const SoPrimitiveVertexCache * cache,
                                       SoLight * light, const SbMatrix & toobjectspace)
{

  //
  // A check for fragment- and vertex-program support has already
  // been done in SoShape::shouldGLRender().
  //

  const cc_glglue * glue = sogl_glue_instance(state);
  const SbColor spec = SoLazyElement::getSpecular(state); 
  float shininess = SoLazyElement::getShininess(state);

  if (!this->programsinitialized)
    this->initPrograms(glue);    
    
  this->initLight(light, toobjectspace);

  const SbMatrix & oldtexture0matrix = SoTextureMatrixElement::get(state);
  const SbMatrix & oldtexture1matrix = SoMultiTextureMatrixElement::get(state, 1);
  const SbMatrix & bumpmapmatrix = SoBumpMapMatrixElement::get(state);

  int i, lastenabled = -1;
  const SbBool * enabled = SoMultiTextureEnabledElement::getEnabledUnits(state, lastenabled);
  
  // disable texture units 1-n
  for (i = 1; i <= lastenabled; i++) {
    if (enabled[i]) {
      cc_glglue_glActiveTexture(glue, GL_TEXTURE0+i);
      glDisable(GL_TEXTURE_2D);
    }
  }
  SoGLImage * bumpimage = SoBumpMapElement::get(state);
  assert(bumpimage);

  // set up textures
  cc_glglue_glActiveTexture(glue, GL_TEXTURE0);

  if (bumpmapmatrix != oldtexture0matrix) {
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(bumpmapmatrix[0]);
    glMatrixMode(GL_MODELVIEW);
  }

  glEnable(GL_TEXTURE_2D);
  bumpimage->getGLDisplayList(state)->call(state);

  
  // FRAGMENT: Setting up spec. colour and shininess for the fragment program  
  glEnable(GL_FRAGMENT_PROGRAM_ARB);
  glue->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragmentprogramid);    
  glue->glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 
                                   spec[0], spec[1], spec[2], 1.0f); 
  
  glue->glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, 
                                   shininess * 64, 0.0f, 0.0f, 1.0f); 
  
  const SbViewVolume & vv = SoViewVolumeElement::get(state);
  const SbMatrix & vm = SoViewingMatrixElement::get(state);
  
  SbVec3f eyepos = vv.getProjectionPoint();
  SoModelMatrixElement::get(state).inverse().multVecMatrix(eyepos, eyepos);
  
  // VERTEX: Setting up lightprograms
  glEnable(GL_VERTEX_PROGRAM_ARB);
  if (!this->ispointlight) {
    glue->glBindProgramARB(GL_VERTEX_PROGRAM_ARB, dirlightvertexprogramid);  
  }
  else {
    glue->glBindProgramARB(GL_VERTEX_PROGRAM_ARB, pointlightvertexprogramid); 
  }

  glue->glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 0, 
                                     this->lightvec[0], 
                                     this->lightvec[1], 
                                     this->lightvec[2], 1); 
  
  glue->glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 1, 
                                     eyepos[0], 
                                     eyepos[1], 
                                     eyepos[2], 1);
  
  
  cc_glglue_glActiveTexture(glue, GL_TEXTURE1);
  if (oldtexture1matrix != SbMatrix::identity()) {
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity(); // load identity texture matrix
    glMatrixMode(GL_MODELVIEW);
  }
  coin_apply_normalization_cube_map(glue);
  glEnable(GL_TEXTURE_CUBE_MAP);

  cc_glglue_glActiveTexture(glue, GL_TEXTURE0);
 
  const SoPrimitiveVertexCache::Vertex * vptr = cache->getVertices();
  const int n = cache->getNumIndices();
  const SbVec3f * cmptr = this->cubemaplist.getArrayPtr();
  const SbVec3f * tptr = this->tangentlist.getArrayPtr();

  cc_glglue_glVertexPointer(glue, 3, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                            (GLvoid*) &vptr->vertex);
  cc_glglue_glEnableClientState(glue, GL_VERTEX_ARRAY);

  cc_glglue_glTexCoordPointer(glue, 2, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                              (GLvoid*) &vptr->bumpcoord);
  cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);

  cc_glglue_glNormalPointer(glue, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                           (GLvoid*) &vptr->normal);
  cc_glglue_glEnableClientState(glue, GL_NORMAL_ARRAY);

  cc_glglue_glClientActiveTexture(glue, GL_TEXTURE1);
  cc_glglue_glTexCoordPointer(glue, 3, GL_FLOAT, 6*sizeof(float), (GLvoid*) tptr);
  cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);

  cc_glglue_glClientActiveTexture(glue, GL_TEXTURE2);  
  cc_glglue_glTexCoordPointer(glue, 3, GL_FLOAT, 6*sizeof(float), (GLvoid*) (tptr + 1));
  cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);
  
  cc_glglue_glDrawElements(glue, GL_TRIANGLES, n, GL_UNSIGNED_INT, 
                           (const GLvoid*) cache->getIndices());
 
  cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);
  cc_glglue_glClientActiveTexture(glue, GL_TEXTURE1);
  cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);
  cc_glglue_glClientActiveTexture(glue, GL_TEXTURE0);
  cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);
  cc_glglue_glDisableClientState(glue, GL_VERTEX_ARRAY);
  cc_glglue_glDisableClientState(glue, GL_NORMAL_ARRAY);

  glDisable(GL_FRAGMENT_PROGRAM_ARB);
  glDisable(GL_VERTEX_PROGRAM_ARB);
  glDisable(GL_TEXTURE_CUBE_MAP); // unit 1
  
  // reenable texture units 1-n if enabled  
  for (i = 1; i <= lastenabled; i++) {
    if (enabled[i]) {
      cc_glglue_glActiveTexture(glue, GL_TEXTURE0+i);
      glEnable(GL_TEXTURE_2D);
    }
  }

  if (lastenabled >= 1 && enabled[1]) {
    // restore blend mode for texture unit 1
    SoGLMultiTextureImageElement::restore(state, 1);
  }

  if (oldtexture1matrix != SbMatrix::identity()) {
    cc_glglue_glActiveTexture(glue, GL_TEXTURE1);
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(oldtexture1matrix[0]);
    glMatrixMode(GL_MODELVIEW);
  }
  
  cc_glglue_glActiveTexture(glue, GL_TEXTURE0);

  if (bumpmapmatrix != oldtexture0matrix) {
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(oldtexture0matrix[0]);
    glMatrixMode(GL_MODELVIEW);
  }

 
  // disable texturing for unit 0 if not enabled
  if (!SoGLTextureEnabledElement::get(state)) glDisable(GL_TEXTURE_2D);
}
  

void
soshape_bumprender::renderBump(SoState * state,
                               const SoPrimitiveVertexCache * cache,
                               SoLight * light, const SbMatrix & toobjectspace)
{
  this->initLight(light, toobjectspace);
  this->calcTSBCoords(cache, light);

  const SbMatrix & oldtexture0matrix = SoTextureMatrixElement::get(state);
  const SbMatrix & oldtexture1matrix = SoMultiTextureMatrixElement::get(state, 1);
  const SbMatrix & bumpmapmatrix = SoBumpMapMatrixElement::get(state);

  const cc_glglue * glue = sogl_glue_instance(state);
  int i, lastenabled = -1;
  const SbBool * enabled = SoMultiTextureEnabledElement::getEnabledUnits(state, lastenabled);
  
  // disable texture units 1-n
  for (i = 1; i <= lastenabled; i++) {
    if (enabled[i]) {
      cc_glglue_glActiveTexture(glue, GL_TEXTURE0+i);
      glDisable(GL_TEXTURE_2D);
    }
  }
  SoGLImage * bumpimage = SoBumpMapElement::get(state);
  assert(bumpimage);

  // set up textures
  cc_glglue_glActiveTexture(glue, GL_TEXTURE0);

  if (bumpmapmatrix != oldtexture0matrix) {
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(bumpmapmatrix[0]);
    glMatrixMode(GL_MODELVIEW);
  }

  glEnable(GL_TEXTURE_2D);
  bumpimage->getGLDisplayList(state)->call(state);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);

  cc_glglue_glActiveTexture(glue, GL_TEXTURE1);

  if (oldtexture1matrix != SbMatrix::identity()) {
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity(); // load identity texture matrix
    glMatrixMode(GL_MODELVIEW);
  }
  coin_apply_normalization_cube_map(glue);
  glEnable(GL_TEXTURE_CUBE_MAP);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
  glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);

  const SoPrimitiveVertexCache::Vertex * vptr =
    cache->getVertices();
  const int n = cache->getNumIndices();
  const SbVec3f * cmptr = this->cubemaplist.getArrayPtr();

  cc_glglue_glVertexPointer(glue, 3, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                            (GLvoid*) &vptr->vertex);
  cc_glglue_glEnableClientState(glue, GL_VERTEX_ARRAY);
  cc_glglue_glTexCoordPointer(glue, 2, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                              (GLvoid*) &vptr->bumpcoord);
  cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);

  cc_glglue_glClientActiveTexture(glue, GL_TEXTURE1);
  cc_glglue_glTexCoordPointer(glue, 3, GL_FLOAT, 0,
                              (GLvoid*) cmptr);
  cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);

  cc_glglue_glDrawElements(glue, GL_TRIANGLES, n, GL_UNSIGNED_INT,
                           (const GLvoid*) cache->getIndices());

  cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);
  cc_glglue_glClientActiveTexture(glue, GL_TEXTURE0);
  cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);
  cc_glglue_glDisableClientState(glue, GL_VERTEX_ARRAY);

  glDisable(GL_TEXTURE_CUBE_MAP); // unit 1
  
  // reenable texture units 1-n if enabled  
  for (i = 1; i <= lastenabled; i++) {
    if (enabled[i]) {
      cc_glglue_glActiveTexture(glue, GL_TEXTURE0+i);
      glEnable(GL_TEXTURE_2D);
    }
  }

  if (lastenabled >= 1 && enabled[1]) {
    // restore blend mode for texture unit 1
    SoGLMultiTextureImageElement::restore(state, 1);
  }

  if (oldtexture1matrix != SbMatrix::identity()) {
    cc_glglue_glActiveTexture(glue, GL_TEXTURE1);
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(oldtexture1matrix[0]);
    glMatrixMode(GL_MODELVIEW);
  }
  
  cc_glglue_glActiveTexture(glue, GL_TEXTURE0);

  if (bumpmapmatrix != oldtexture0matrix) {
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(oldtexture0matrix[0]);
    glMatrixMode(GL_MODELVIEW);
  }
  // disable texturing for unit 0 if not enabled
  if (!SoGLTextureEnabledElement::get(state)) glDisable(GL_TEXTURE_2D);
}
  
void
soshape_bumprender::renderNormal(SoState * state, const SoPrimitiveVertexCache * cache)
{
  const cc_glglue * glue = sogl_glue_instance(state);

  const SoPrimitiveVertexCache::Vertex * vptr =
    cache->getVertices();
  const int n = cache->getNumIndices();

  int lastenabled = -1;
  const SbBool * enabled = 
    SoMultiTextureEnabledElement::getEnabledUnits(state, lastenabled);
  
  SbBool colorpervertex = cache->colorPerVertex();

  cc_glglue_glVertexPointer(glue, 3, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                            (GLvoid*) &vptr->vertex);
  cc_glglue_glEnableClientState(glue, GL_VERTEX_ARRAY);

  cc_glglue_glNormalPointer(glue, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                            (GLvoid*) &vptr->normal);
  cc_glglue_glEnableClientState(glue, GL_NORMAL_ARRAY);

  cc_glglue_glTexCoordPointer(glue, 4, GL_FLOAT, sizeof(SoPrimitiveVertexCache::Vertex),
                              (GLvoid*) &vptr->texcoord0);
  cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);

  if (colorpervertex) {
    cc_glglue_glColorPointer(glue, 4, GL_UNSIGNED_BYTE,
                             sizeof(SoPrimitiveVertexCache::Vertex),
                             (GLvoid*) &vptr->rgba);
    cc_glglue_glEnableClientState(glue, GL_COLOR_ARRAY);
  }

  int i;
  for (i = 1; i <= lastenabled; i++) {
    if (enabled[i]) {
      cc_glglue_glClientActiveTexture(glue, GL_TEXTURE0 + i);
      cc_glglue_glTexCoordPointer(glue, 4, GL_FLOAT, 0,
                                  (GLvoid*) cache->getMultiTextureCoordinateArray(i));
      cc_glglue_glEnableClientState(glue, GL_TEXTURE_COORD_ARRAY);
    }
  }
  
  cc_glglue_glDrawElements(glue, GL_TRIANGLES, n, GL_UNSIGNED_INT,
                           (const GLvoid*) cache->getIndices());
  
  for (i = 1; i <= lastenabled; i++) {
    if (enabled[i]) {
      cc_glglue_glClientActiveTexture(glue, GL_TEXTURE0 + i);
      cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);
    }
  }
  if (lastenabled >= 1) {
    // reset to default
    cc_glglue_glClientActiveTexture(glue, GL_TEXTURE0); 
  }

  cc_glglue_glDisableClientState(glue, GL_VERTEX_ARRAY);
  cc_glglue_glDisableClientState(glue, GL_NORMAL_ARRAY);
  cc_glglue_glDisableClientState(glue, GL_TEXTURE_COORD_ARRAY);

  if (colorpervertex) {
    cc_glglue_glDisableClientState(glue, GL_COLOR_ARRAY);
  }
}

void
soshape_bumprender::calcTangentSpace(const SoPrimitiveVertexCache * cache)
{
  const SoPrimitiveVertexCache::Vertex * vptr =
    cache->getVertices();
  int i;

  const int numv = cache->getNumVertices();
  const int32_t * idxptr = cache->getIndices();

  this->tangentlist.truncate(0);

  for (i = 0; i < numv; i++) {
    this->tangentlist.append(SbVec3f(0.0f, 0.0f, 0.0f));
    this->tangentlist.append(SbVec3f(0.0f, 0.0f, 0.0f));
  }
  const int numi = cache->getNumIndices();

  SbVec3f sTangent;
  SbVec3f tTangent;

  int idx[3];

  for (i = 0; i < numi; i += 3) {
    idx[0] = idxptr[i];
    idx[1] = idxptr[i+1];
    idx[2] = idxptr[i+2];
    const SoPrimitiveVertexCache::Vertex & v0 = vptr[idx[0]];
    const SoPrimitiveVertexCache::Vertex & v1 = vptr[idx[1]];
    const SoPrimitiveVertexCache::Vertex & v2 = vptr[idx[2]];

    SbVec3f side0 = v1.vertex - v0.vertex;
    SbVec3f side1 = v2.vertex - v0.vertex;

    float deltaT0 = v1.bumpcoord[1] - v0.bumpcoord[1];
    float deltaT1 = v2.bumpcoord[1] - v0.bumpcoord[1];
    sTangent = deltaT1 * side0 - deltaT0 * side1;
    NORMALIZE(sTangent);

    float deltaS0 = v1.bumpcoord[0] - v0.bumpcoord[0];
    float deltaS1 = v2.bumpcoord[0] - v0.bumpcoord[0];
    tTangent = deltaS1 * side0 - deltaS0 * side1;
    NORMALIZE(tTangent);

    for (int j = 0; j < 3; j++) {
      this->tangentlist[idx[j]*2] += sTangent;
      this->tangentlist[idx[j]*2+1] += tTangent;
    }
  }
  for (i = 0; i < numv; i++) {
    this->tangentlist[i*2].normalize();
    this->tangentlist[i*2+1].normalize();
  }
}

void
soshape_bumprender::calcTSBCoords(const SoPrimitiveVertexCache * cache, SoLight * light)
{
  SbVec3f thelightvec;
  SbVec3f tlightvec;

  const SoPrimitiveVertexCache::Vertex * vptr =
    cache->getVertices();
  const int numv = cache->getNumVertices();

  this->cubemaplist.truncate(0);
  for (int i = 0; i < numv; i++) {
    const SoPrimitiveVertexCache::Vertex & v = vptr[i];
    SbVec3f sTangent = this->tangentlist[i*2];
    SbVec3f tTangent = this->tangentlist[i*2+1];
    thelightvec = this->getLightVec(v.vertex);
    tlightvec = thelightvec;
#if 0 // FIXME: I don't think it's necessary to do this test. pederb, 2003-11-20
    SbVec3f tcross = tTangent.cross(sTangent);
    if (tcross.dot(v.normal) < 0.0f) {
      tlightvec = -tlightvec;
    }
#endif // disabled, probably not necessary
    this->cubemaplist.append(SbVec3f(sTangent.dot(tlightvec),
                                     tTangent.dot(tlightvec),
                                     v.normal.dot(thelightvec)));

  }
}

void
soshape_bumprender::initLight(SoLight * light, const SbMatrix & m)
{
  if (light->isOfType(SoPointLight::getClassTypeId())) {
    SoPointLight * pl = (SoPointLight*) light;
    this->lightvec = pl->location.getValue();
    m.multVecMatrix(this->lightvec, this->lightvec);
    this->ispointlight = TRUE;
  }
  else if (light->isOfType(SoDirectionalLight::getClassTypeId())) {
    SoDirectionalLight * dir = (SoDirectionalLight*)light;
    m.multDirMatrix(-(dir->direction.getValue()), this->lightvec);
    this->ispointlight = FALSE;
    NORMALIZE(this->lightvec);
  }
  else if (light->isOfType(SoSpotLight::getClassTypeId())) {
    SoSpotLight * pl = (SoSpotLight*) light;
    this->lightvec = pl->location.getValue();
    m.multVecMatrix(this->lightvec, this->lightvec);
    this->ispointlight = TRUE;

  }
  else {
    this->lightvec = SbVec3f(0.0f, 0.0f, 1.0f);
    this->ispointlight = FALSE;

  }
}


SbVec3f
soshape_bumprender::getLightVec(const SbVec3f & v) const
{
  if (this->ispointlight) {
    SbVec3f tmp = lightvec - v;
    NORMALIZE(tmp);
    return tmp;
  }
  else return this->lightvec;
}
