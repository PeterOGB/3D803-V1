/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

#define G_LOG_USE_STRUCTURED
#define _GNU_SOURCE
#include <stdio.h>
#include <gtk/gtk.h>
#include <GLES3/gl3.h>
#include <cglm/cglm.h>

#include  "ObjLoader.h"
#include "Keyboard.h"
#include "Hands.h"
#include "ShaderDefinitions.h"
#include "3D.h"

static GLenum e;
#define CHECK(n) if((e=glGetError())!=0){printf("%d Error %s %x\n",__LINE__,n,e);} 

// lists of objects for drawing hands.
GList *hand0List = NULL;
GList *hand1List = NULL;


OBJECT *HandVertexCloudObject = NULL;
OBJECT *HandVertexLimitsObject = NULL;

OBJECTINIT handObjects[] = {
    {"VertCloud_Vert",&HandVertexCloudObject,TRUE},
    {"VertLimits_Vert.001",&HandVertexLimitsObject,TRUE},
    {NULL,NULL,FALSE}
};
    


HandInfo LeftHandInfo  = {"Left Hand",
			  (vec4) {-3.123580f,2.420521f,2.608409f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  NULL,FALSE,
			  TRUE,
			  TRUE,
			  NOT_PUSHED,
			  NULL,
			  0,
			  TRUE,
			  WG_TOP,WG_TOP,0,
			  0.0f};

HandInfo RightHandInfo = {"Right Hand",
			  (vec4) {3.074589f,2.290603f,2.649102f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  (vec4) {0.0f,0.0f,0.0f,0.0f},
			  NULL,FALSE,
			  FALSE,
			  TRUE,
			  NOT_PUSHED,
			  NULL,
			  0,
			  TRUE,
			  WG_TOP,WG_TOP,0,
			  0.0f};

HandInfo *Hands[2] = {&LeftHandInfo,&RightHandInfo};

HandInfo *ActiveHand = &RightHandInfo;
HandInfo *InactiveHand = &LeftHandInfo;
vec4 *HandOutlineLeft = NULL;
vec4 *HandOutlineRight = NULL;

gboolean HandsInit(  GString *sharedPath,
		     __attribute__((unused))GString *userPath)
{
    GString *directory;

    directory = g_string_new(sharedPath->str);
    g_string_append(directory,"objects/");
    
    hand0List = loadObjectsFromFile(directory,"hand1.obj",handObjects);
    hand1List = loadObjectsFromFile(directory,"hand2.obj",handObjects);

    g_string_free(directory,TRUE);
    
    if((hand0List == NULL) || (hand1List == NULL))
	return FALSE;
    
    LeftHandInfo.WayPoints = g_queue_new();
    RightHandInfo.WayPoints = g_queue_new();

    ResetLeft = LeftHandInfo;
    ResetRight = RightHandInfo;
    
    return TRUE;
}

void DrawHands(void)
{
    float handAngle,armAngle;
    mat4 ArmRotate,HandRotate;
    vec4 ArmTranslate;
    GList *Object;
    OBJECT *currentObject = NULL;
    ELEMENTS *currentElements = NULL;

    //  DRAW THE HANDS

    glUseProgram(HandProg);
    
    for(int h=0;h<2;h++)
    {
	HandInfo *hand = Hands[h];

	// Add the offsets to the Finger position
	glm_vec4_copy(hand->FingerAtXYZ,
		     hand->DrawAtXYZ);
	glm_vec4_addadd(hand->PushedOffsetXYZ,hand->SurfaceOffsetXYZ,
			hand->DrawAtXYZ);

	// Try and draw a hand
       
	if(hand->LeftHand)
	{
	    // Left hand and arm angles changes over left hand side of console
	    if(hand->DrawAtXYZ[0] < 0.0f)
	    {
		// LEFT side
		// Hand is straight to allow multiple buttons to be presses
		// Arm is straight at left edge and at 30degrees at the middle
		armAngle =  -30.0f * (4.0f + hand->DrawAtXYZ[0])/4.0f;
		handAngle = 0.0f;
	    }
	    else
	    {
		// RIGHT side
		// Arm is fixed at 30 degrees
		// Hand goes from 0 degrees at center to 60 decrees at right edge
		armAngle  = -30.0f;
		handAngle = -30.0f * hand->DrawAtXYZ[0] / 2.0f;
	    }
	}
	else
	{
	    // Right hand and arm angles changes over right hand side of console
	    if(hand->DrawAtXYZ[0] >  0.0f)
	    {
		// RIGHT side
		// Hand is straight to allow multiple buttons to be presses
		// Arm is straight at left edge and at 30degrees at the middle
		armAngle =  30.0f * (4.0f - hand->DrawAtXYZ[0])/4.0f;
		handAngle = 0.0f;
	    }
	    else
	    {
		// LEFT side
		// Arm is fixed at 30 degrees
		// Hand goes from 0 degrees at center to 60 decrees at right edge
		armAngle  = 30.0f;
		handAngle = -30.0f * hand->DrawAtXYZ[0] / 2.0f;
	    }

	}

	// Compute arm and hand rotation matrix.
	armAngle = 2.0f *  M_PIf32 * armAngle / 360.0f;
	glm_rotate_y(GLM_MAT4_IDENTITY, armAngle, ArmRotate);

	handAngle = 2.0f *  M_PIf32 * handAngle / 360.0f;
	glm_rotate_y(GLM_MAT4_IDENTITY, handAngle, HandRotate);
	
	/* 
	   The whole hand has the index finger close to the object's origin so hand rotations
	   are applied around the origin, and arm rotations are applied around (0,0,2) which is 
	   the wrist.  Rotations are applied in the fragment shader.
	*/
#if 0
	// Move the hand to the position to track the mouse
	if(NearestButton == NULL)
	    glm_vec4_add(HandXYZ,HandOffset,HandTranslate);
	else
	    glm_vec4_add(NearestButton->TranslateUp,HandOffset,HandTranslate);

	HandTranslate[1] += 0.12f;
#endif	
	//printf("[%f,%f,%f]\n",(double)HandTranslate[0],(double)HandTranslate[1],(double)HandTranslate[2]);
	

	
	glm_vec4_copy(hand->DrawAtXYZ,ArmTranslate);
	
	if(hand->LeftHand)
	{
	    if(ArmTranslate[0] >= 0.0f)
	    {
		// Move the arm/hand joining point of the arm by the same vector
		// as the rotated arm/hand jointing point on the hand has moved
		ArmTranslate[0] -= 2.0f * HandRotate[0][2];
		ArmTranslate[2] +=  (2.0f * HandRotate[0][0]) - 2.0f;
	    }
	    
	}
	else
	{
	    if(ArmTranslate[0] <= 0.0f)
	    {
		// Move the arm/hand joining point of the arm by the same vector
		// as the rotated arm/hand jointing point on the hand has moved
		ArmTranslate[0] -= 2.0f * HandRotate[0][2];
		ArmTranslate[2] +=  (2.0f * HandRotate[0][0]) - 2.0f;
	    }
	}

#if 0	
	// Set ArmTranslate so it joins up to the hand
	if(XwiresXYZ[0] < 0.0f)
	{
	    glm_vec4_copy(HandTranslate,ArmTranslate);
	}
	else
	{
	    glm_vec4_copy(HandTranslate,ArmTranslate);

	    // Move the arm/hand joining point of the arm by the same vector
	    // as the rotated arm/hand jointing point on the hand has moved
	    ArmTranslate[0] -= 2.0f * HandRotate[0][2];
	    ArmTranslate[2] +=  (2.0f * HandRotate[0][0]) - 2.0f;
	}	
#endif	
	// Choose the appropriate object.
	if(hand->NearestButton != NULL)
	{
	    if(hand->fingersPressed)
	    {
		switch(hand->NearestButton->finger)
		{
		    /*
		      case ONE_FINGER:
		      Object=hand1List;
		      break;
		      case THREE_FINGERS:
		      Object=hand3List;
		      break;
		    */
		default:
		    Object=hand1List;
		    break;
		}
	    }
	    else
	    {
		switch(hand->NearestButton->finger)
		{
		    /*
		      case ONE_FINGER:
		      Object=hand0List;
		      break;
		      case THREE_FINGERS:
		      Object=hand2List;
		      break;
		    */
		default:
		    Object=hand0List;
		    break;
		}
	    }
	}
	else
	{
	    Object=hand0List;
	}

	glUseProgram(HandProg);
	for( ;Object != NULL; Object=Object->next)
	{
	    currentObject = (OBJECT *) Object->data;
	    if(currentObject->hidden) continue;
	    //printf("\nObject name = %s %p \n",currentObject->name->str,currentObject->ElementsList);	

	    for(GList *ele = currentObject->ElementsList; ele != NULL; ele = ele->next)
	    {
		currentElements = (ELEMENTS *) ele->data;
		//printf("hasTexture=%s\n",currentElements->Material->hasTexture == TRUE ? "TRUE" : "FALSE");

		if(currentElements->Material->hasTexture == TRUE)
		{ // Hand objects don't have textures.
		 
		}
		else
		{
		    // Load the vertex data
		    glVertexAttribPointer(HandPositionLoc, 3, GL_FLOAT, GL_FALSE, 0,
					  currentElements->elementVertices);

		    CHECK("glVertexAttribPointer(HandPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, \
				      currentElements->elementVertices);");
	
		    glEnableVertexAttribArray(HandPositionLoc);
		    CHECK("	glEnableVertexAttribArray(HandPositionLoc);");

		    glVertexAttribPointer (HandNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );
		    CHECK("glVertexAttribPointer (HandNormalsLoc, 3, GL_FLOAT, GL_FALSE, 0, currentElements->elementNormals );");
		    glEnableVertexAttribArray (HandNormalsLoc );
		    CHECK("glEnableVertexAttribArray (HandNormalsLoc );");
			
		    // Load the MVP matrix
		    glUniformMatrix4fv(HandMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);
		    CHECK("glUniformMatrix4fv(HandMvpLoc, 1, GL_FALSE, ( GLfloat * ) mvpMatrix);");

		    glUniform4f(HandColourLoc ,
				currentElements->Material->KdR,
				currentElements->Material->KdG,
				currentElements->Material->KdB,
				1.0);

		    // Load the rotation matrices
		    glUniformMatrix4fv( ArmRotateLoc, 1, GL_FALSE, (GLfloat*) &ArmRotate[0] );
		    glUniformMatrix4fv(HandRotateLoc, 1, GL_FALSE, (GLfloat*) &HandRotate[0] );
		    glUniform4fv(HandLightPositionLoc , 1,  (GLfloat *) &WGLightPosition[0]);
		    glUniform4fv(HandCameraPositionLoc , 1,  UserXYZ);


		    glUniform4fv(HandTranslateLoc , 1,  (GLfloat *) &hand->DrawAtXYZ[0]);
		    glUniform4fv( ArmTranslateLoc , 1,  (GLfloat *) &ArmTranslate[0]);

		    if(hand->LeftHand)
		    {
			glUniform1f(HandMirrorXLoc,1.0f);
		    }
		    else
		    {
			glUniform1f(HandMirrorXLoc,-1.0f);
			glFrontFace(GL_CW);
			//glCullFace(GL_FRONT);
		    }

		    glDrawRangeElements ( GL_TRIANGLES,
					  0, currentElements->nextElement -1 ,
					  currentElements->nextIndex , GL_UNSIGNED_SHORT,
					  currentElements->elementIndices );

		    if(!hand->LeftHand)
		    {
			glFrontFace(GL_CCW);
			//glCullFace(GL_BACK);
		    }

		    // glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT,
		    //		     currentElements->elementIndices );
		    CHECK("glDrawRangeElements ( GL_TRIANGLES, currentElements->nextIndex , GL_UNSIGNED_SHORT, \
				 currentElements->elementIndices );");
		}
	    }
	}
    }
}

// Move vertex cloud to the position the hand is drawn at.
void transformHandOutlines(void)
{
    float handAngle,armAngle;
    vec4 ArmTranslate; 
    mat4 ArmRotate,HandRotate;
    
    // Transform the handoutline for each hand for collision detection
    if(HandOutlineLeft == NULL)
    {
	HandOutlineLeft  = calloc(sizeof(vec4),HandVertexCloudObject->vertices); 
	HandOutlineRight = calloc(sizeof(vec4),HandVertexCloudObject->vertices);
    }

    
    for(int h=0;h<2;h++)
    {
	vec3 tmp;
	HandInfo *hand = Hands[h];

	if(hand->LeftHand)
	{
	    if(hand->DrawAtXYZ[0] < 0.0f)
	    {
		// LEFT side
		// Hand is straight to allow multiple buttons to be presses
		// Arm is straight at left edge and at 30degrees at the middle
		armAngle =  -30.0f * (4.0f + hand->DrawAtXYZ[0])/4.0f;
		handAngle = 0.0f;
	    }
	    else
	    {
		// RIGHT side
		// Arm is fixed at 30 degrees
		// Hand goes from 0 degrees at center to 60 decrees at right edge
		armAngle  = -30.0f;
		handAngle = -30.0f * hand->DrawAtXYZ[0] / 2.0f;
	    }
	}
	else
	{
	    // Right hand and arm angles changes over right hand side of console
	    if(hand->DrawAtXYZ[0] >  0.0f)
	    {
		// RIGHT side
		// Hand is straight to allow multiple buttons to be presses
		// Arm is straight at left edge and at 30degrees at the middle
		armAngle =  30.0f * (4.0f - hand->DrawAtXYZ[0])/4.0f;
		handAngle = 0.0f;
	    }
	    else
	    {
		// LEFT side
		// Arm is fixed at 30 degrees
		// Hand goes from 0 degrees at center to 60 decrees at right edge
		armAngle  = 30.0f;
		handAngle = -30.0f * hand->DrawAtXYZ[0] / 2.0f;
	    }
	}
	
	// Compute arm and hand rotation matrix.
	armAngle = 2.0f *  M_PIf32 * armAngle / 360.0f;
	glm_rotate_y(GLM_MAT4_IDENTITY, armAngle, ArmRotate);

	handAngle = 2.0f *  M_PIf32 * handAngle / 360.0f;
	glm_rotate_y(GLM_MAT4_IDENTITY, handAngle, HandRotate);

	glm_vec4_copy(hand->DrawAtXYZ,ArmTranslate);

	if(hand->LeftHand)
	{
	    if(ArmTranslate[0] >= 0.0f)
	    {
		// Move the arm/hand joining point of the arm by the same vector
		// as the rotated arm/hand jointing point on the hand has moved
		ArmTranslate[0] -= 2.0f * HandRotate[0][2];
		ArmTranslate[2] +=  (2.0f * HandRotate[0][0]) - 2.0f;
	    }
	}
	else
	{
	    if(ArmTranslate[0] <= 0.0f)
	    {
		// Move the arm/hand joining point of the arm by the same vector
		// as the rotated arm/hand jointing point on the hand has moved
		ArmTranslate[0] -= 2.0f * HandRotate[0][2];
		ArmTranslate[2] +=  (2.0f * HandRotate[0][0]) - 2.0f;
	    }
	}
	
	for(unsigned int n=0; n< HandVertexCloudObject->vertices; n++)
	{
	    glm_vec4_copy3(HandVertexCloudObject->VertexOnly[n],tmp);
	    
	    if(h==1)
		tmp[0] *= -1.0f;
	    if(tmp[2] > 2.0f)
	    {
		tmp[2] -= 2.0f;
		
		
		glm_vec3_rotate_m4(ArmRotate, tmp, tmp);
		glm_vec3_add(ArmTranslate, tmp,tmp);

		tmp[2] += 2.0f;
	    }
	    else
	    {
		glm_vec3_rotate_m4(HandRotate, tmp, tmp);
		glm_vec3_add(hand->DrawAtXYZ, tmp,tmp);
	    }

	    if(h==0)
		glm_vec4(tmp,0.0f,(HandOutlineLeft)[n]);
	    else
		glm_vec4(tmp,0.0f,(HandOutlineRight)[n]);
	
	}
    }
}


float HandCollisionDetect2(void)
{
    float right = 1000.0f,left = -1000.0f,x;
    
    for(unsigned int vl = 0; vl < HandVertexCloudObject->vertices; vl++)
    {
	x = HandOutlineLeft[vl][0];
	if(x > left) left = x;
	x = HandOutlineRight[vl][0];
	if(x < right) right = x;
    }
    return(right - left);
}


