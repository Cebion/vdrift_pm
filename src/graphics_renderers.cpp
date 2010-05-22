#include "graphics_renderers.h"

#ifdef __APPLE__
#include <GLExtensionWrangler/glew.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "opengl_utility.h"
#include "matrix4.h"
#include "mathvector.h"
#include "model.h"
#include "texture.h"
#include "vertexarray.h"
#include "reseatable_reference.h"
#include "definitions.h"
#include "containeralgorithm.h"
#include "drawable.h"

#include <cassert>

#include <sstream>
using std::stringstream;

#include <string>
using std::string;

#include <iostream>
using std::pair;
using std::endl;

#include <map>
using std::map;

#include <vector>
using std::vector;

#include <algorithm>

void RENDER_INPUT_POSTPROCESS::Render(GLSTATEMANAGER & glstate, std::ostream & error_output)
{
	assert(shader);
	
	OPENGL_UTILITY::CheckForOpenGLErrors("postprocess begin", error_output);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	shader->Enable();
	
	OPENGL_UTILITY::CheckForOpenGLErrors("postprocess shader enable", error_output);
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho( 0, 1, 0, 1, -1, 1 );
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glColor4f(1,1,1,1);
	glstate.SetColor(1,1,1,1);
	glstate.Disable(GL_BLEND);
	glstate.Disable(GL_DEPTH_TEST);
	glstate.Enable(GL_TEXTURE_2D);
	
	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
	
	OPENGL_UTILITY::CheckForOpenGLErrors("postprocess flag set", error_output);
	
	for (unsigned int i = 0; i < source_textures.size(); i++)
	{
		//std::cout << i << ": " << source_textures[i] << std::endl;
		glActiveTexture(GL_TEXTURE0+i);
		if (source_textures[i])
			source_textures[i]->Activate();
	}
	
	OPENGL_UTILITY::CheckForOpenGLErrors("postprocess texture set", error_output);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f( 0.0f,  0.0f,  0.0f);
	glTexCoord2f(1.0, 0.0f); glVertex3f( 1.0f,  0.0f,  0.0f);
	glTexCoord2f(1.0, 1.0); glVertex3f( 1.0f,  1.0f,  0.0f);
	glTexCoord2f(0.0f, 1.0); glVertex3f( 0.0f,  1.0f,  0.0f);
	glEnd();
	
	OPENGL_UTILITY::CheckForOpenGLErrors("postprocess draw", error_output);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glstate.Enable(GL_DEPTH_TEST);
	glstate.Disable(GL_TEXTURE_2D);
	
	OPENGL_UTILITY::CheckForOpenGLErrors("postprocess end", error_output);
}

void RENDER_INPUT_SCENE::SetActiveShader(const SHADER_TYPE & newshader)
{
	if (newshader == activeshader)
		return;

	assert(GLEW_ARB_shading_language_100);

	size_t shaderindex = newshader;

	assert(shaderindex < shadermap.size());
	assert(shadermap[shaderindex] != NULL);

	shadermap[shaderindex]->Enable();
	activeshader = newshader;
}

void RENDER_INPUT_SCENE::Render(GLSTATEMANAGER & glstate, std::ostream & error_output)
{
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	if (orthomode)
	{
		glOrtho(orthomin[0], orthomax[0], orthomin[1], orthomax[1], orthomin[2], orthomax[2]);
		//std::cout << "ortho near/far: " << orthomin[2] << "/" << orthomax[2] << std::endl;
	}
	else
	{
		gluPerspective( camfov, w/(float)h, 0.1f, lod_far );
	}
	glMatrixMode( GL_MODELVIEW );
	float temp_matrix[16];
	(cam_rotation).GetMatrix4(temp_matrix);
	glLoadMatrixf(temp_matrix);
	glTranslatef(-cam_position[0],-cam_position[1],-cam_position[2]);
	ExtractFrustum();

	//send information to the shaders
	if (shaders)
	{
		//camera transform goes in texture3
		glActiveTexture(GL_TEXTURE3);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		(cam_rotation).GetMatrix4(temp_matrix);
		glLoadMatrixf(temp_matrix);
		glTranslatef(-cam_position[0],-cam_position[1],-cam_position[2]);

		//cubemap transform goes in texture2
		glActiveTexture(GL_TEXTURE2);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		QUATERNION <float> camlook;
		camlook.Rotate(3.141593*0.5,1,0,0);
		camlook.Rotate(-3.141593*0.5,0,0,1);
		QUATERNION <float> cuberotation;
		cuberotation = (-camlook) * (-cam_rotation); //experimentally derived
		(cuberotation).GetMatrix4(temp_matrix);
		//(cam_rotation).GetMatrix4(temp_matrix);
		glLoadMatrixf(temp_matrix);
		//glTranslatef(-cam_position[0],-cam_position[1],-cam_position[2]);
		//glLoadIdentity();

		/*glActiveTexture(GL_TEXTURE3);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		(cam_rotation).GetMatrix4(temp_matrix);
		glLoadMatrixf(temp_matrix);
		glTranslatef(-cam_position[0],-cam_position[1],-cam_position[2]);*/

		glActiveTexture(GL_TEXTURE0);
		glMatrixMode(GL_MODELVIEW);

		//send light position to the shaders
		MATHVECTOR <float, 3> lightvec = lightposition;
		(cam_rotation).RotateVector(lightvec);
		//(cuberotation).RotateVector(lightvec);
		shadermap[SHADER_FULL]->UploadActiveShaderParameter3f("lightposition", lightvec[0], lightvec[1], lightvec[2]);
		shadermap[SHADER_FULL]->UploadActiveShaderParameter1f("contrast", contrast);
		shadermap[SHADER_FULLBLEND]->UploadActiveShaderParameter3f("lightposition", lightvec[0], lightvec[1], lightvec[2]);
		shadermap[SHADER_FULLBLEND]->UploadActiveShaderParameter1f("contrast", contrast);
		shadermap[SHADER_SKYBOX]->UploadActiveShaderParameter1f("contrast", contrast);
		/*float lightarray[3];
		for (int i = 0; i < 3; i++)
		lightarray[i] = lightvec[i];
		glLightfv(GL_LIGHT0, GL_POSITION, lightarray);*/

		// if we have no reflection texture supplied, don't touch the TU because
		// someone else may be supplying one
		if (reflection && reflection->Loaded())
		{
			glActiveTexture(GL_TEXTURE2);
			reflection->Activate();
			glActiveTexture(GL_TEXTURE0);
		}
		
		glActiveTexture(GL_TEXTURE3);
		if (ambient && ambient->Loaded())
		{
			ambient->Activate();
		}
		else
		{
			glBindTexture(GL_TEXTURE_CUBE_MAP,0);
			//assert(0);
		}
		glActiveTexture(GL_TEXTURE0);
	}

	if (clearcolor && cleardepth)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else if (clearcolor)
		glClear(GL_COLOR_BUFFER_BIT);
	else if (cleardepth)
		glClear(GL_DEPTH_BUFFER_BIT);
	
	if (depth_mode_equal)
		glDepthFunc( GL_EQUAL );
	else
		glDepthFunc( GL_LEQUAL );
	
	last_transform_valid = false;
	activeshader = SHADER_NONE;
	
	DrawList(glstate, *dynamic_drawlist_ptr, false);
	DrawList(glstate, *static_drawlist_ptr, true);
	
	if (last_transform_valid)
		glPopMatrix();
}

void RENDER_INPUT_SCENE::DrawList(GLSTATEMANAGER & glstate, std::vector <DRAWABLE*> & drawlist, bool preculled)
{
	unsigned int drawcount = 0;
	unsigned int loopcount = 0;
	
	for (vector <DRAWABLE*>::iterator ptr = drawlist.begin(); ptr != drawlist.end(); ptr++, loopcount++)
	{
		DRAWABLE * i = *ptr;
		//if (i->IsDraw())
		{
			//if (loopcount < already_culled || !FrustumCull(*i))
			if (preculled || !FrustumCull(*i))
			{
				drawcount++;
				
				SelectFlags(*i, glstate);

				if (shaders) SelectAppropriateShader(*i);

				SelectTexturing(*i, glstate);

				bool need_pop = SelectTransformStart(*i, glstate);

				//assert(i->GetDraw()->GetVertArray() || i->GetDraw()->IsDrawList() || !i->GetDraw()->GetLine().empty());

				if (i->IsDrawList())
				{
					//cout << "beep" << endl;
					//assert(i->GetDraw()->GetModel()->HaveListID());
					//glCallList(i->GetDraw()->GetModel()->GetListID());
					const unsigned int numlists = i->GetDrawLists().size();
					for (unsigned int n = 0; n < numlists; ++n)
						glCallList(i->GetDrawLists()[n]);
				}
				else if (i->GetVertArray())
				{
					glEnableClientState(GL_VERTEX_ARRAY);

					const float * verts;
					int vertcount;
					i->GetVertArray()->GetVertices(verts, vertcount);
					glVertexPointer(3, GL_FLOAT, 0, verts);

					const float * norms;
					int normcount;
					i->GetVertArray()->GetNormals(norms, normcount);
					glNormalPointer(GL_FLOAT, 0, norms);
					if (normcount > 0)
						glEnableClientState(GL_NORMAL_ARRAY);

					//const float * tc[i->GetDraw()->varray.GetTexCoordSets()];
					//int tccount[i->GetDraw()->varray.GetTexCoordSets()];
					const float * tc[1];
					int tccount[1];
					if (i->GetVertArray()->GetTexCoordSets() > 0)
					{
						i->GetVertArray()->GetTexCoords(0, tc[0], tccount[0]);
						glEnableClientState(GL_TEXTURE_COORD_ARRAY);
						glTexCoordPointer(2, GL_FLOAT, 0, tc[0]);
					}

					const int * faces;
					int facecount;
					i->GetVertArray()->GetFaces(faces, facecount);

					glDrawElements(GL_TRIANGLES, facecount, GL_UNSIGNED_INT, faces);

					glDisableClientState(GL_VERTEX_ARRAY);
					glDisableClientState(GL_NORMAL_ARRAY);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
				else if (!i->GetLine().empty())
				{
					glstate.Enable(GL_LINE_SMOOTH);
					glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
					glLineWidth(i->GetLinesize());
					glBegin(GL_LINE_STRIP);
					const std::vector< MATHVECTOR < float , 3 > > & line = i->GetLine();
					for (std::vector< MATHVECTOR < float , 3 > >::const_iterator i = line.begin(); i != line.end(); ++i)
						glVertex3f((*i)[0],(*i)[1],(*i)[2]);
					glEnd();
				}

				SelectTransformEnd(*i, need_pop);
			}
		}
		/*else if (i->IsTransform())
		{
			//cout << "is transform" << endl;
			float mat[16];
			glPushMatrix();
			MATHVECTOR <float, 3> translation;
			translation = i->GetTransform()->GetTranslation();
			glTranslatef(translation[0],translation[1],translation[2]);
			i->GetTransform()->GetRotation().GetMatrix4(mat);
			glMultMatrixf(mat);
			last_transform_valid = false;
		}
		else if (i->IsEmpty())
		{
			//cout << "is transform pop" << endl;
			glPopMatrix();
		}*/
	}
	
	//std::cout << "drew " << drawcount << " of " << drawlist_static->size() + drawlist_dynamic->size() << " ("  << combined_drawlist_cache.size() << "/" << already_culled << ")" << std::endl;
}

///returns true if the object was culled and should not be drawn
bool RENDER_INPUT_SCENE::FrustumCull(DRAWABLE & tocull)
{
	//return false;

	DRAWABLE * d (&tocull);
	//if (d->GetRadius() != 0.0 && d->parent != NULL && !d->skybox)
	if (d->GetRadius() != 0.0 && !d->GetSkybox() && d->GetCameraTransformEnable())
	{
		//do frustum culling
		MATHVECTOR <float, 3> objpos(d->GetObjectCenter());
		d->GetTransform().TransformVectorOut(objpos[0],objpos[1],objpos[2]);
		float dx=objpos[0]-cam_position[0]; float dy=objpos[1]-cam_position[1]; float dz=objpos[2]-cam_position[2];
		float rc=dx*dx+dy*dy+dz*dz;
		float temp_lod_far = lod_far + d->GetRadius();
		if (rc > temp_lod_far*temp_lod_far)
			return true;
		else if (rc < d->GetRadius()*d->GetRadius())
			return false;
		else
		{
			float bound, rd;
			bound = d->GetRadius();
			for (int i=0; i<6; i++)
			{
				rd=frustum.frustum[i][0]*objpos[0]+
						frustum.frustum[i][1]*objpos[1]+
						frustum.frustum[i][2]*objpos[2]+
						frustum.frustum[i][3];
				if (rd <= -bound)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void RENDER_INPUT_SCENE::SelectAppropriateShader(DRAWABLE & forme)
{
	if (forme.Get2D())
	{
		if (forme.GetDistanceField())
			SetActiveShader(SHADER_DISTANCEFIELD);
		else
			SetActiveShader(SHADER_SIMPLE);
	}
	else
	{
		if (forme.GetSkybox() || !forme.GetLit())
			SetActiveShader(SHADER_SKYBOX);
		else if (forme.GetSmoke())
			SetActiveShader(SHADER_SIMPLE);
		else
		{
			bool blend = (forme.GetDecal() || forme.GetPartialTransparency());
			if (blend)
				SetActiveShader(SHADER_FULLBLEND);
			else
				SetActiveShader(SHADER_FULL);
		}
	}
}

void RENDER_INPUT_SCENE::SelectFlags(DRAWABLE & forme, GLSTATEMANAGER & glstate)
{
	DRAWABLE * i(&forme);
	if (i->GetDecal() || i->GetPartialTransparency())
	{
		glstate.Enable(GL_POLYGON_OFFSET_FILL);
	}
	else
	{
		glstate.Disable(GL_POLYGON_OFFSET_FILL);
	}

	if (i->GetCull())
	{
		glstate.Enable(GL_CULL_FACE);
		if (i->GetCull())
		{
			if (i->GetCullFront())
				glstate.SetCullFace(GL_FRONT);
			else
				glstate.SetCullFace(GL_BACK);
		}
	}
	else
		glstate.Disable(GL_CULL_FACE);

	bool blend = (i->GetDecal() || i->Get2D() ||
			i->GetPartialTransparency() || i->GetDistanceField());

	if (blend && !i->GetForceAlphaTest())
	{
		if (fsaa > 1)
		{
			glstate.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}

		//if (!shaders && i->GetDraw()->GetDistanceField())
		//{
		//	glstate.Enable(GL_ALPHA_TEST);
		//	glAlphaFunc(GL_GREATER, 0.5f);
		//}
		//else
		glstate.Disable(GL_ALPHA_TEST);
		glstate.Enable(GL_BLEND);
		
		//glstate.SetBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		
		/*if (shaders)
		{
			glstate.SetBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		else*/
		{
			glstate.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			if (i->GetSmoke())
				glstate.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
	}
	else
	{
		if (fsaa > 1 && shaders)
		{
			glstate.Enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
			/*glstate.Enable(GL_BLEND);
			glstate.Disable(GL_ALPHA_TEST);

			glstate.Disable(GL_BLEND);
			glstate.Enable(GL_ALPHA_TEST);*/
		/*}
		else
		{*/
			//glstate.Enable(GL_BLEND);
			glstate.Disable(GL_BLEND);
			if (i->GetDistanceField())
				glstate.SetAlphaFunc(GL_GREATER, 0.5f);
			else
				glstate.SetAlphaFunc(GL_GREATER, 0.25f);
			glstate.Enable(GL_ALPHA_TEST);
		//}
	}

	glstate.SetDepthMask(writedepth);
	glstate.SetColorMask(writecolor);

	//if (i->GetDraw()->GetSmoke() || i->GetDraw()->Get2D() || i->GetDraw()->GetSkybox())
	if (i->GetSmoke() || i->Get2D())
		glstate.Disable(GL_DEPTH_TEST);
	else
		glstate.Enable(GL_DEPTH_TEST);

	float r,g,b,a;
	i->GetColor(r,g,b,a);
	glstate.SetColor(r,g,b,a);
}

void RENDER_INPUT_SCENE::SelectTexturing(DRAWABLE & forme, GLSTATEMANAGER & glstate)
{
	DRAWABLE * i(&forme);

	bool enabletex = true;

	const TEXTURE * diffusetexture = i->GetDiffuseMap();

	if (!diffusetexture)
		enabletex = false;
	else
		if (!diffusetexture->Loaded())
			enabletex = false;

	if (!enabletex)
	{
		glstate.BindTexture2D(0,NULL);
		return;
	}

	if (enabletex)
	{
		{
			glstate.BindTexture2D(0,i->GetDiffuseMap());

			if (shaders)
			{
				glstate.BindTexture2D(1, i->GetMiscMap1());
				glstate.BindTexture2D(8, i->GetSelfIllumination() ? i->GetAdditiveMap1() : NULL);
				glstate.BindTexture2D(7, i->GetSelfIllumination() ? i->GetAdditiveMap2() : NULL);
			}
			else
			{
				if (carpainthack)
				//if (true)
				{
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
					// rgb
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
					// alpha
					/*glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);*/
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
					float mycolor[4];
					mycolor[0]=mycolor[1]=mycolor[2]=0.0;
					mycolor[3]=1.0;
					glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, mycolor);
				}
				else
				{
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				}
			}
		}
	}
	else
	{
		glstate.Disable(GL_TEXTURE_2D);
	}
}

///returns true if the matrix was pushed
bool RENDER_INPUT_SCENE::SelectTransformStart(DRAWABLE & forme, GLSTATEMANAGER & glstate)
{
	bool need_a_pop = true;

	DRAWABLE * i(&forme);
	if (i->Get2D())
	{
		if (last_transform_valid)
			glPopMatrix();
		last_transform_valid = false;

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		//glOrtho( 0, 1, 0, 1, -1, 1 );
		glOrtho( 0, 1, 1, 0, -1, 1 );
		//glOrtho( -5, 5, -5, 5, -100, 100 );
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glMultMatrixf(i->GetTransform().GetArray());
	}
	else
	{
		glstate.Enable(GL_DEPTH_TEST);

		{
			if (!i->GetCameraTransformEnable()) //do our own transform only and ignore the camera position / orientation
			{
				if (last_transform_valid)
					glPopMatrix();
				last_transform_valid = false;

				glActiveTexture(GL_TEXTURE1);
				glMatrixMode(GL_TEXTURE);
				glPushMatrix();
				glLoadIdentity();
				glActiveTexture(GL_TEXTURE0);
				glMatrixMode(GL_MODELVIEW);

				glPushMatrix();
				glLoadMatrixf(i->GetTransform().GetArray());
			}
			else if (i->GetSkybox())
			{
				if (last_transform_valid)
					glPopMatrix();
				last_transform_valid = false;

				glPushMatrix();
				float temp_matrix[16];
				cam_rotation.GetMatrix4(temp_matrix);
				glLoadMatrixf(temp_matrix);
				if (i->GetVerticalTrack())
				{
					MATHVECTOR< float, 3 > objpos(i->GetObjectCenter());
					//std::cout << "Vertical offset: " << objpos;
					i->GetTransform().TransformVectorOut(objpos[0],objpos[1],objpos[2]);
					//std::cout << " || " << objpos << endl;
					//glTranslatef(-objpos.x,-objpos.y,-objpos.z);
					//glTranslatef(0,game.cam.position.y,0);
					glTranslatef(0.0,0.0,-objpos[2]);
				}
				glMultMatrixf(i->GetTransform().GetArray());
			}
			else
			{
				bool need_new_transform = !last_transform_valid;
				if (last_transform_valid)
					need_new_transform = (!last_transform.Equals(i->GetTransform()));
				if (need_new_transform)
				{
					if (last_transform_valid)
						glPopMatrix();

					glPushMatrix();
					glMultMatrixf(i->GetTransform().GetArray());
					last_transform = i->GetTransform();
					last_transform_valid = true;

					need_a_pop = false;
				}
				else need_a_pop = false;
			}
		}
	}

	return need_a_pop;
}

void RENDER_INPUT_SCENE::SelectTransformEnd(DRAWABLE & forme, bool need_pop)
{
	DRAWABLE * i(&forme);
	if (i->Get2D() && need_pop)
	{
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}
	else
	{
		if (!i->GetCameraTransformEnable())
		{
			glActiveTexture(GL_TEXTURE1);
			glMatrixMode(GL_TEXTURE);
			glPopMatrix();
			glActiveTexture(GL_TEXTURE0);
			glMatrixMode(GL_MODELVIEW);
		}

		if (need_pop)
		{
			glPopMatrix();
			//cout << "popping" << endl;
		}
		//else cout << "not popping" << endl;
	}
}

FRUSTUM RENDER_INPUT_SCENE::SetCameraInfo(const MATHVECTOR <float, 3> & newpos,
										  const QUATERNION <float> & newrot,
										  float newfov,
										  float newlodfar,
										  float neww,
										  float newh,
										  bool restore_matrices)
{
	cam_position = newpos;
	cam_rotation = newrot;
	camfov = newfov;
	lod_far = newlodfar;
	w = neww;
	h = newh;
	
	glMatrixMode( GL_PROJECTION );
	if (restore_matrices)
		glPushMatrix();
	glLoadIdentity();
	if (orthomode)
	{
		glOrtho(orthomin[0], orthomax[0], orthomin[1], orthomax[1], orthomin[2], orthomax[2]);
	}
	else
	{
		gluPerspective( camfov, w/(float)h, 0.1f, lod_far );
	}
	glMatrixMode( GL_MODELVIEW );
	if (restore_matrices)
		glPushMatrix();
	float temp_matrix[16];
	(cam_rotation).GetMatrix4(temp_matrix);
	glLoadMatrixf(temp_matrix);
	glTranslatef(-cam_position[0],-cam_position[1],-cam_position[2]);
	ExtractFrustum();
	glMatrixMode( GL_PROJECTION );
	if (restore_matrices)
		glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	if (restore_matrices)
		glPopMatrix();
	return frustum;
}

void RENDER_INPUT_SCENE::ExtractFrustum()
{
	float   proj[16];
	float   modl[16];
	float   clip[16];
	float   t;

	/* Get the current PROJECTION matrix from OpenGL */
	glGetFloatv( GL_PROJECTION_MATRIX, proj );

	/* Get the current MODELVIEW matrix from OpenGL */
	glGetFloatv( GL_MODELVIEW_MATRIX, modl );

	/* Combine the two matrices (multiply projection by modelview) */
	clip[ 0] = modl[ 0] * proj[ 0] + modl[ 1] * proj[ 4] + modl[ 2] * proj[ 8] + modl[ 3] * proj[12];
	clip[ 1] = modl[ 0] * proj[ 1] + modl[ 1] * proj[ 5] + modl[ 2] * proj[ 9] + modl[ 3] * proj[13];
	clip[ 2] = modl[ 0] * proj[ 2] + modl[ 1] * proj[ 6] + modl[ 2] * proj[10] + modl[ 3] * proj[14];
	clip[ 3] = modl[ 0] * proj[ 3] + modl[ 1] * proj[ 7] + modl[ 2] * proj[11] + modl[ 3] * proj[15];

	clip[ 4] = modl[ 4] * proj[ 0] + modl[ 5] * proj[ 4] + modl[ 6] * proj[ 8] + modl[ 7] * proj[12];
	clip[ 5] = modl[ 4] * proj[ 1] + modl[ 5] * proj[ 5] + modl[ 6] * proj[ 9] + modl[ 7] * proj[13];
	clip[ 6] = modl[ 4] * proj[ 2] + modl[ 5] * proj[ 6] + modl[ 6] * proj[10] + modl[ 7] * proj[14];
	clip[ 7] = modl[ 4] * proj[ 3] + modl[ 5] * proj[ 7] + modl[ 6] * proj[11] + modl[ 7] * proj[15];

	clip[ 8] = modl[ 8] * proj[ 0] + modl[ 9] * proj[ 4] + modl[10] * proj[ 8] + modl[11] * proj[12];
	clip[ 9] = modl[ 8] * proj[ 1] + modl[ 9] * proj[ 5] + modl[10] * proj[ 9] + modl[11] * proj[13];
	clip[10] = modl[ 8] * proj[ 2] + modl[ 9] * proj[ 6] + modl[10] * proj[10] + modl[11] * proj[14];
	clip[11] = modl[ 8] * proj[ 3] + modl[ 9] * proj[ 7] + modl[10] * proj[11] + modl[11] * proj[15];

	clip[12] = modl[12] * proj[ 0] + modl[13] * proj[ 4] + modl[14] * proj[ 8] + modl[15] * proj[12];
	clip[13] = modl[12] * proj[ 1] + modl[13] * proj[ 5] + modl[14] * proj[ 9] + modl[15] * proj[13];
	clip[14] = modl[12] * proj[ 2] + modl[13] * proj[ 6] + modl[14] * proj[10] + modl[15] * proj[14];
	clip[15] = modl[12] * proj[ 3] + modl[13] * proj[ 7] + modl[14] * proj[11] + modl[15] * proj[15];
	
	/* Extract the numbers for the RIGHT plane */
	frustum.frustum[0][0] = clip[ 3] - clip[ 0];
	frustum.frustum[0][1] = clip[ 7] - clip[ 4];
	frustum.frustum[0][2] = clip[11] - clip[ 8];
	frustum.frustum[0][3] = clip[15] - clip[12];

	/* Normalize the result */
	t = sqrt( frustum.frustum[0][0] * frustum.frustum[0][0] + frustum.frustum[0][1] * frustum.frustum[0][1] + frustum.frustum[0][2] * frustum.frustum[0][2] );
	frustum.frustum[0][0] /= t;
	frustum.frustum[0][1] /= t;
	frustum.frustum[0][2] /= t;
	frustum.frustum[0][3] /= t;

	/* Extract the numbers for the LEFT plane */
	frustum.frustum[1][0] = clip[ 3] + clip[ 0];
	frustum.frustum[1][1] = clip[ 7] + clip[ 4];
	frustum.frustum[1][2] = clip[11] + clip[ 8];
	frustum.frustum[1][3] = clip[15] + clip[12];

	/* Normalize the result */
	t = sqrt( frustum.frustum[1][0] * frustum.frustum[1][0] + frustum.frustum[1][1] * frustum.frustum[1][1] + frustum.frustum[1][2] * frustum.frustum[1][2] );
	frustum.frustum[1][0] /= t;
	frustum.frustum[1][1] /= t;
	frustum.frustum[1][2] /= t;
	frustum.frustum[1][3] /= t;

	/* Extract the BOTTOM plane */
	frustum.frustum[2][0] = clip[ 3] + clip[ 1];
	frustum.frustum[2][1] = clip[ 7] + clip[ 5];
	frustum.frustum[2][2] = clip[11] + clip[ 9];
	frustum.frustum[2][3] = clip[15] + clip[13];

	/* Normalize the result */
	t = sqrt( frustum.frustum[2][0] * frustum.frustum[2][0] + frustum.frustum[2][1] * frustum.frustum[2][1] + frustum.frustum[2][2] * frustum.frustum[2][2] );
	frustum.frustum[2][0] /= t;
	frustum.frustum[2][1] /= t;
	frustum.frustum[2][2] /= t;
	frustum.frustum[2][3] /= t;

	/* Extract the TOP plane */
	frustum.frustum[3][0] = clip[ 3] - clip[ 1];
	frustum.frustum[3][1] = clip[ 7] - clip[ 5];
	frustum.frustum[3][2] = clip[11] - clip[ 9];
	frustum.frustum[3][3] = clip[15] - clip[13];

	/* Normalize the result */
	t = sqrt( frustum.frustum[3][0] * frustum.frustum[3][0] + frustum.frustum[3][1] * frustum.frustum[3][1] + frustum.frustum[3][2] * frustum.frustum[3][2] );
	frustum.frustum[3][0] /= t;
	frustum.frustum[3][1] /= t;
	frustum.frustum[3][2] /= t;
	frustum.frustum[3][3] /= t;

	/* Extract the FAR plane */
	frustum.frustum[4][0] = clip[ 3] - clip[ 2];
	frustum.frustum[4][1] = clip[ 7] - clip[ 6];
	frustum.frustum[4][2] = clip[11] - clip[10];
	frustum.frustum[4][3] = clip[15] - clip[14];

	/* Normalize the result */
	t = sqrt( frustum.frustum[4][0] * frustum.frustum[4][0] + frustum.frustum[4][1] * frustum.frustum[4][1] + frustum.frustum[4][2] * frustum.frustum[4][2] );
	frustum.frustum[4][0] /= t;
	frustum.frustum[4][1] /= t;
	frustum.frustum[4][2] /= t;
	frustum.frustum[4][3] /= t;

	/* Extract the NEAR plane */
	frustum.frustum[5][0] = clip[ 3] + clip[ 2];
	frustum.frustum[5][1] = clip[ 7] + clip[ 6];
	frustum.frustum[5][2] = clip[11] + clip[10];
	frustum.frustum[5][3] = clip[15] + clip[14];

	/* Normalize the result */
	t = sqrt( frustum.frustum[5][0] * frustum.frustum[5][0] + frustum.frustum[5][1] * frustum.frustum[5][1] + frustum.frustum[5][2] * frustum.frustum[5][2] );
	frustum.frustum[5][0] /= t;
	frustum.frustum[5][1] /= t;
	frustum.frustum[5][2] /= t;
	frustum.frustum[5][3] /= t;
}

RENDER_INPUT_SCENE::RENDER_INPUT_SCENE()
:	last_transform_valid(false), 
	shaders(false), 
	clearcolor(false), 
	cleardepth(false), 
	orthomode(false), 
	contrast(1.0), 
	depth_mode_equal(false),
	writecolor(true),
	writedepth(true),
	carpainthack(false)
{
	shadermap.resize(SHADER_NONE, NULL);
	MATHVECTOR <float, 3> front(1,0,0);
	lightposition = front;
	QUATERNION <float> ldir;
				//ldir.Rotate(3.141593*0.4,0,0,1);
				//ldir.Rotate(3.141593*0.5*0.7,1,0,0);
	ldir.Rotate(3.141593*0.5,1,0,0);
	ldir.RotateVector(lightposition);
}

/*SCENEDRAW * PointerTo(const SCENEDRAW & sd)
{
	return const_cast<SCENEDRAW *> (&sd);
}*/

/*unsigned int GRAPHICS_SDLGL::RENDER_INPUT_SCENE::CombineDrawlists()
{
	combined_drawlist_cache.resize(0);
	combined_drawlist_cache.reserve(drawlist_static->size()+drawlist_dynamic->size());
	
	unsigned int already_culled = 0;
	
	if (use_static_partitioning)
	{
		AABB<float>::FRUSTUM aabbfrustum(frustum);
		//aabbfrustum.DebugPrint(std::cout);
		static_partitioning->Query(aabbfrustum, combined_drawlist_cache);
		already_culled = combined_drawlist_cache.size();
	}
	else
		calgo::transform(*drawlist_static, std::back_inserter(combined_drawlist_cache), &PointerTo);
	calgo::transform(*drawlist_dynamic, std::back_inserter(combined_drawlist_cache), &PointerTo);
	
	return already_culled;
}*/
