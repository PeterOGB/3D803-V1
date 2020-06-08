/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

// Code for reading wavefront ".obj" and ".mtl" files saved by blender
#define G_LOG_USE_STRUCTURED
#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <GLES3/gl3.h>
#include <gtk/gtk.h>

#include <math.h>
#include <cglm/cglm.h>
#include "LoadPNG.h"
#include "ObjLoader.h"
#include "Parse.h"

#include "Keyboard.h"

enum parseStates {START=0,STATE_o,STATE_v,STATE_vt,STATE_vn,STATE_usemtl,STATE_f};
enum parseStates parserState;

static MATERIAL *currentMaterial = NULL;

static GList *materialList= NULL;
static GList *objectList = NULL;

static GString *mtlFileName = NULL;

static GSList *freeList = NULL;



OBJECT *currentObject = NULL;
ELEMENTS *currentElements = NULL;

static VERTEX *Vertices;
static unsigned int nextVertex;

static TEXEL *Texels;
static unsigned int nextTexel;

static NORMAL *Normals;
static unsigned int nextNormal;

static unsigned int totalVertices = 0;
static unsigned int totalTexels = 0;
static unsigned int totalNormals = 0;
static unsigned int totalFaces = 0;

static int ECOUNT;

// Handlers for parsing a material file

static int Mnewmtl(int n)
{
    MATERIAL *newMaterial;
    g_debug("%s %s\n",__FUNCTION__,getField(n+1));
    GString *name;

    name = g_string_new(NULL);

    g_string_printf(name,"%s:%s",mtlFileName->str,getField(n+1));
    
    newMaterial = (MATERIAL *) calloc(sizeof(MATERIAL),1);

    materialList = g_list_append(materialList,newMaterial);

    newMaterial->materialIndex = 0;
    newMaterial->materialName = name->str;
    currentMaterial = newMaterial;

    g_string_free(name,FALSE);

    return 1;
}

static int MNs(int n)
{
    g_debug("%s %s\n",__FUNCTION__,getField(n+1));
    sscanf(getField(n+1),"%f",&currentMaterial->Ns);
    return 1;
}

static int MKa(int n)
{
    g_debug("%s (%s,%s,%s)\n",__FUNCTION__,getField(n+1),getField(n+2),getField(n+3));
    sscanf(getField(n+1),"%f",&currentMaterial->KaR);
    sscanf(getField(n+2),"%f",&currentMaterial->KaG);
    sscanf(getField(n+3),"%f",&currentMaterial->KaB);
    return 1;
}

static int MKd(int n)
{
    g_debug("%s (%s,%s,%s)\n",__FUNCTION__,getField(n+1),getField(n+2),getField(n+3));
    sscanf(getField(n+1),"%f",&currentMaterial->KdR);
    sscanf(getField(n+2),"%f",&currentMaterial->KdG);
    sscanf(getField(n+3),"%f",&currentMaterial->KdB);
    return 1;
}

static int MKs(int n)
{
    g_debug("%s (%s,%s,%s)\n",__FUNCTION__,getField(n+1),getField(n+2),getField(n+3));
    sscanf(getField(n+1),"%f",&currentMaterial->KsR);
    sscanf(getField(n+2),"%f",&currentMaterial->KsG);
    sscanf(getField(n+3),"%f",&currentMaterial->KsB);
    return 1;
}

static int MNi(int n)
{
    g_debug("%s %s\n",__FUNCTION__,getField(n+1));
    sscanf(getField(n+1),"%f",&currentMaterial->Ni);
    return 1;
}

static int Md(int n)
{
    g_debug("%s %s\n",__FUNCTION__,getField(n+1));
    sscanf(getField(n+1),"%f",&currentMaterial->d);
    return 1;
}

 static int Millum(int n)
{
    g_debug("%s %s\n",__FUNCTION__,getField(n+1));
    sscanf(getField(n+1),"%d",&currentMaterial->illum);
    return 1;
}

static int Mmap_Kd(int n)
{
    g_debug("%s %s\n",__FUNCTION__,getField(n+1));
    currentMaterial->map_Kd = strdup(getField(n+1));
    return 1;
}

static Token materialTokens[] = {
    
 
    {"newmtl",0,Mnewmtl},
    {"Ns",0,MNs},
    {"Ka",0,MKa},
    {"Kd",0,MKd},
    {"Ks",0,MKs},
    {"Ke",0,NULL},
    {"Ni",0,MNi},
    {"d",0,Md},
    {"illum",0,Millum},
    {"map_Kd",0,Mmap_Kd},  
    {"#",0,NULL},
    {NULL,0,NULL}
};


// Handlers for parsing a ".obj" file
// PASS ONE HANDLERS

/*
Example : "o Circle.001_Circle.006_Material.007"
Initialise a new object and add it to the objects list.
Set currentObject to new object.
*/

static int P1o(__attribute__((unused)) int n)
{
    if((parserState == START) || (parserState == STATE_f) || (parserState == STATE_v) )
    {
	currentObject = calloc(sizeof(OBJECT),1);
	currentObject->name = g_string_new(getField(n+1));
	objectList = g_list_append(objectList,currentObject);
	currentElements = NULL;
	parserState = STATE_o;
	return 1;
    }
    g_error("Unexpected o in state %d\n",parserState);
}

/*
Example: "v -2.253907 2.657431 1.106030"
Increment positions counters in CurrentObject and MasterObject.
*/
static int P1v(__attribute__((unused)) int n)
{
    if((parserState == STATE_o) || (parserState == STATE_v)) 
    {
	currentObject->vertices += 1;
	parserState = STATE_v;
	return 1;
    }
    g_error("Unexpected v in state %d\n",parserState);
}

/*
Example: "vt 1.006911 1.001911"
Increment texels counters in CurrentObject and MasterObject.
*/
static int P1vt(__attribute__((unused)) int n)
{
    if((parserState == STATE_v) || (parserState == STATE_vt))
    {
	currentObject->texels += 1;
	parserState = STATE_vt;
	return 1;
    }
    g_error("Unexpected vt in state %d\n",parserState);
}

/*
Example: "vn -0.2893 -0.1708 0.9419"
Increment normals counters in CurrentObject and MasterObject.
*/
static int P1vn(__attribute__((unused)) int n)
{
    if((parserState == STATE_vt) || (parserState == STATE_vn))
    {
	currentObject->normals += 1;
	parserState = STATE_vn;
	return 1;
    }
    g_error("Unexpected vn in state %d\n",parserState);
}

/*
Example: "usemtl Material.004"
Ignored
*/
static int P1usemtl(__attribute__((unused)) int n)
{

    if((parserState == STATE_vn) || (parserState == STATE_f))
    {
	currentElements = calloc(sizeof(ELEMENTS),1);
	currentElements->maxVertex = currentElements->maxNormal = currentElements->maxTexel = 0;
	currentElements->minVertex = currentElements->minNormal = currentElements->minTexel = 1000000;
	currentObject->ElementsList = g_list_append(currentObject->ElementsList,currentElements);
	parserState = STATE_usemtl;
	return 1;
    }
    g_error("Unexpected usemtl in state %d (%s %s)\n",parserState,getField(n+0),getField(n+1));
}

/*
Example: "f 1/1/1 3/2/1 4/3/1"
Increment faces counters in CurrentObject and MasterObject.
*/
static int P1f(__attribute__((unused)) int n)
{
    unsigned int vert,norm,tex;

    if((parserState == STATE_usemtl) || (parserState == STATE_f))
    {
	for(int nn=0; nn <3; nn++)
	{
	    sscanf(getField(n+nn+1),"%d/%d/%d",&vert,&tex,&norm);
	    if(vert < currentElements->minVertex) currentElements->minVertex = vert;
	    if(vert > currentElements->maxVertex) currentElements->maxVertex = vert;

	    if(tex < currentElements->minTexel) currentElements->minTexel = tex;
	    if(tex > currentElements->maxTexel) currentElements->maxTexel = tex;

	    if(norm < currentElements->minNormal) currentElements->minNormal = norm;
	    if(norm > currentElements->maxNormal) currentElements->maxNormal = norm;
	}
    
	currentObject->faces += 1;
	currentElements->faces += 1;
	parserState = STATE_f;
	return 1;
    }
    g_error("Unexpected f in state %d\n",parserState);
}

/*
Example: "mtllib plane+cube8.mtl"
Set mtlFileName 
*/
static int P1mtllib(int n)
{
    mtlFileName = g_string_new(getField(n+1));
    return 1;
}

static int P1EOF(__attribute__((unused)) int n)
{
    g_debug("P1EOF called\n");
    return 1;
}

static Token passOneTokens[] = {

    {"v",0,P1v},
    {"vn",0,P1vn},
    {"vt",0,P1vt},
    {"f",0,P1f},
    {"usemtl",0,P1usemtl},
    {"mtllib",0,P1mtllib},
    {"o",0,P1o},
    {"#",0,NULL},
    {"s",0,NULL},
    {NULL,0,P1EOF}
};

// PASS TWO HANDLERS

/*
Example : "o Circle.001_Circle.006_Material.007"
Set currentObject to the next one from the objects list.
Clear the lookup table ready for the next object.
*/
static int P2o(__attribute__((unused)) int n)
{
    static GList *obj;

    if(parserState == START)
    {
	 obj = objectList;
    }

    currentObject = (OBJECT *) obj->data;
    obj = obj->next;
    
    currentObject->elementsList = currentObject->ElementsList;
    
    // Set lastVertex for an o ater a v
    if(parserState == STATE_v)
    {
	g_debug("XXX Vertex only o\n");
	currentObject->lastVertex = nextVertex-1;
    }
    
    if((parserState == START) || (parserState == STATE_f) || (parserState == STATE_v))
    {
    
	currentObject->elementsList = currentObject->ElementsList;

	parserState = STATE_o;
	return 1;
    }
    
    g_error("Unexpected o in state %d\n",parserState);
}

/*
Example: "v -2.253907 2.657431 1.106030"
Save the Position (vertex) in masterObject's Positions array.
*/
static int P2v(int n)
{
    VERTEX *vp;

    // Set firstVertex for first v after an o
    if(parserState == STATE_o)
    {
	currentObject->firstVertex = nextVertex;
    }
    
    if((parserState == STATE_o) || (parserState == STATE_v)) 
    {
	vp = &Vertices[nextVertex++];
	sscanf(getField(n+1),"%f",&vp->x);
	sscanf(getField(n+2),"%f",&vp->y);
	sscanf(getField(n+3),"%f",&vp->z);

    	parserState = STATE_v;
	return 1;
    }
    g_error("Unexpected v in state %d\n",parserState);
}

/*
Example: "vt 1.006911 1.001911"
Save the Texel in masterObject's Texels array.
*/
static int P2vt(int n)
{
    TEXEL *tp;
    if((parserState == STATE_v) || (parserState == STATE_vt))
    {
	tp = &Texels[nextTexel++];
	sscanf(getField(n+1),"%f",&tp->u);
	sscanf(getField(n+2),"%f",&tp->v);
	parserState = STATE_vt;
	return 1;
    }
    g_error("Unexpected vt in state %d\n",parserState);
}

/*
Example: "vn -0.2893 -0.1708 0.9419"
Save the Normal in masterObject's Normals array.
 */
static int P2vn(int n)
{
    NORMAL *np;
    if((parserState == STATE_vt) || (parserState == STATE_vn))
    {
	np = &Normals[nextNormal++];
	sscanf(getField(n+1),"%f",&np->x);
	sscanf(getField(n+2),"%f",&np->y);
	sscanf(getField(n+3),"%f",&np->z);
	parserState = STATE_vn;
	return 1;
    }

    g_error("Unexpected vn in state %d\n",parserState);
}

/*
Example: "usemtl Material.007"
Lookup material by name and set currentObject's material pointer.
*/

static int P2usemtl(int n)
{
    MATERIAL *material;
    GList *mlist;
    
    unsigned int vCount,nCount;

    if((parserState == STATE_vn) || (parserState == STATE_f))
    {
	GString *matName;
	matName = g_string_new(NULL);

	g_string_printf(matName,"%s:%s",mtlFileName->str,getField(n+1));
    
	for(mlist = materialList; mlist != NULL; mlist = mlist->next)
	{
	    material = (MATERIAL *)mlist->data;
	    g_debug("***** %s == %s *************\n",material->materialName,matName->str);
	    if(strcmp(material->materialName,matName->str) == 0)
		break;
	}

	g_string_free(matName,TRUE);
       
	currentElements = (ELEMENTS *)currentObject->elementsList->data;

	currentElements->Material = material;

	currentObject->elementsList = currentObject->elementsList->next;

	currentElements->vCount = vCount = currentElements->maxVertex - currentElements->minVertex +1;
	currentElements->nCount = nCount = currentElements->maxNormal - currentElements->minNormal +1;

	currentElements->lookupTable =  calloc(sizeof(int),vCount  * nCount);
	freeList = g_slist_prepend(freeList,currentElements->lookupTable);

	currentElements->Faces = calloc(sizeof(FACE),currentElements->faces);
	currentElements->faces = 0;
  
	parserState = STATE_usemtl;
	return 1;
    }
    g_error("Unexpected usemtl in state %d (%s %s)\n",parserState,getField(n+0),getField(n+1));
}

/*
Example: "f 154/149/290 126/119/290 193/184/290"
*/
static int P2f(int n)
{
    unsigned int vert,norm,tex;

    if((parserState == STATE_usemtl) || (parserState == STATE_f))
    {
	unsigned int (*array)[currentElements->vCount][currentElements->nCount];    // a pointer to a 2D array
	array = currentElements->lookupTable;            // Use the malloced memory for the array

	for(int nn=0; nn <3; nn++)
	{
	    sscanf(getField(n+nn+1),"%d/%d/%d",&vert,&tex,&norm);

	    currentElements->Faces[currentElements->faces].v[nn] = vert;
	    currentElements->Faces[currentElements->faces].n[nn] = norm;
	    currentElements->Faces[currentElements->faces].t[nn] = tex;
	
	    vert -= currentElements->minVertex;
	    norm -= currentElements->minNormal;

	    if( (*array)[vert][norm] == 0)
	    {
		(*array)[vert][norm] = tex;
	    }
	}
	currentElements->faces += 1;
    	parserState = STATE_f;
	return 1;
    }
    g_error("Unexpected f in state %d\n",parserState);
}

static int P2EOF(__attribute__((unused)) int n)
{
    g_debug("P2EOF called\n");
    // At eof Set lastVertex if last was v 
    if(parserState == STATE_v)
    {
	g_debug("XXX Vertex only o\n");
	currentObject->lastVertex = nextVertex-1;
    }
    return 1;
}

static Token passTwoTokens[] = {

    {"v",0,P2v},
    {"vn",0,P2vn},
    {"vt",0,P2vt},
    {"f",0,P2f},
    {"usemtl",0,P2usemtl},
    {"mtllib",0,NULL},
    {"o",0,P2o},
    {"#",0,NULL},
    {"s",0,NULL},
    {NULL,0,P2EOF}
};

GList *loadObjectsFromFile(const GString *path,const char *fname, OBJECTINIT *initObjects)
{
    GList *objects = NULL;
    GString *objectFilename = NULL;
    GString *materialFilename = NULL;


    // Set initial values for counters
    // Reset to empty lists
    objectList = NULL;
    currentObject =  NULL;
    freeList = NULL;

    debugOn();
    
    ECOUNT = 0;

    objectFilename = g_string_new(path->str);
    g_string_append(objectFilename,fname);
    
    g_debug("STARTING PASS ONE\n");
    parserState = START;
    if(parseFile(objectFilename->str,passOneTokens,NULL) == FALSE)
    {
	objectList = NULL;
	goto done;
    }
    
    for(objects=objectList;objects != NULL; objects=objects->next)
    {
	currentObject = (OBJECT *) objects->data;

	totalVertices += currentObject->vertices;
	totalTexels += currentObject->texels;
	totalNormals += currentObject->normals;
	totalFaces += currentObject->faces;
    }

    Vertices = calloc(sizeof(VERTEX),(totalVertices+1));
    Texels = calloc(sizeof(TEXEL),(totalTexels+1));
    Normals = calloc(sizeof(NORMAL),(totalNormals+1));

    freeList = g_slist_prepend(freeList,Vertices);
    freeList = g_slist_prepend(freeList,Texels);
    freeList = g_slist_prepend(freeList,Normals);

    // Vertex/Texel/Normal numbers in face lines start at 1
    nextVertex = nextTexel = nextNormal = 1;
    
    materialFilename = g_string_new(path->str);
    g_string_append(materialFilename,mtlFileName->str);

    if(parseFile(materialFilename->str,materialTokens,NULL) == FALSE)
	return NULL;
    
    currentObject = NULL;
    g_debug("STARTING PASS TWO\n");
    parserState = START;
    if(parseFile(objectFilename->str,passTwoTokens,NULL) ==FALSE)
    {
	objectList = NULL;
	goto done;
    }
    
    g_debug("FINISHED PASS TWO\n");
    
    // A bit kludgy for now.  Will need improving as more objects are added.
    for(objects=objectList;objects != NULL; objects=objects->next)
    {
	OBJECTINIT *initObject;
	gboolean found;
	currentObject = (OBJECT *) objects->data;


	found = FALSE;
	for(initObject = initObjects; initObject->objectName != NULL; initObject++)
	{
	    if(g_strcmp0(initObject->objectName,currentObject->name->str) == 0)
	    {
		*initObject->object = currentObject;
		currentObject->hidden = initObject->hiddenFlag;
		found = TRUE;
		g_debug("Matched %s with %s\n",initObject->objectName,currentObject->name->str); 
		break;
	    }
	}



	vec4 *vp;
	
	if(currentObject->ElementsList == NULL)
	{
	    vp = currentObject->VertexOnly = calloc(sizeof(vec4),currentObject->vertices);
	    for(unsigned int v=0,vv = currentObject->firstVertex;v<currentObject->vertices;v++,vv++)
	    {
		(*vp)[0] = Vertices[vv].x;
		(*vp)[1] = Vertices[vv].y;
		(*vp)[2] = Vertices[vv].z;
		(*vp)[3] = 0.0f;
		vp++;
	    }
	}
	else
	{
	    currentObject->VertexOnly = NULL;
	
	    for(GList *e = currentObject->ElementsList; e != NULL; e = e->next)
	    {
		unsigned int elementCount = 0;
		unsigned int vert,norm,tex,v,n;
		currentElements = (ELEMENTS *) e->data;
		unsigned int (*array)[currentElements->vCount][currentElements->nCount];    // a pointer to a 2D array
		array = currentElements->lookupTable;            // Use the malloced memory for the array
		{
		    unsigned int s,*p;
		    s = currentElements->nCount * currentElements->vCount;
		    p  = currentElements->lookupTable;
		    for(unsigned int nn = 0; nn < s; nn++)
			if(*p++ != 0) elementCount += 1;
		}
	    
		array = currentElements->lookupTable;
	
		g_debug("FINISHED COUNT ECOUNT=%d elementCount=%d\n",ECOUNT,elementCount);
		bzero(currentElements->lookupTable,sizeof(int) * currentElements->vCount * currentElements->nCount);
		g_debug("elementCount=%d\n",elementCount);
		currentElements->elementVertices = calloc(sizeof(VERTEX),elementCount);
		currentElements->elementNormals = calloc(sizeof(NORMAL),elementCount);
		currentElements->elementTexels = calloc(sizeof(TEXEL),elementCount);
		currentElements->elementIndices = calloc(sizeof(GLushort),currentElements->faces * 3);

		{
		    long unsigned int storage;
		    storage  = sizeof(VERTEX) * elementCount;
		    storage += sizeof(NORMAL) * elementCount;
		    storage += sizeof(TEXEL)  * elementCount;
		    storage += sizeof(GLushort) * currentElements->faces * 3;
		}
	    
		currentElements->nextElement = 0;
		currentElements->nextIndex = 0;

		for(unsigned int face = 0; face < currentElements->faces; face+=1)
		{
		    for(int nn=0;nn<3;nn++)
		    {
			vert = currentElements->Faces[face].v[nn];
			tex = currentElements->Faces[face].t[nn];
			norm = currentElements->Faces[face].n[nn];
		
			v = vert - currentElements->minVertex;
			n = norm - currentElements->minNormal;

			if( (*array)[v][n] == 0)
			{
			    (*array)[v][n] = currentElements->nextElement + 1;
		
			    currentElements->elementVertices[currentElements->nextElement] = Vertices[vert];
			    currentElements->elementNormals[currentElements->nextElement]  = Normals[norm];
			    currentElements->elementTexels[currentElements->nextElement] = Texels[tex];		

			    currentElements->elementIndices[currentElements->nextIndex++] =
				(GLushort) currentElements->nextElement++;
			}
			else
			{
			    currentElements->elementIndices[currentElements->nextIndex++] =
				(GLushort) ((*array)[v][n] - 1U);
			}
		    }
		}

#if 0
		// Debug dumps

		g_debug("\n\nVERTICES\n");
	    
		for(unsigned int i = 0; i < elementCount; i++)
		{
		    VERTEX *vtex;
		    vtex = &currentElements->elementVertices[i];
		    g_debug("Vertex %d = (%f,%f,%f)\n",i,(double) vtex->x,(double) vtex->y,(double) vtex->z);
		}

		g_debug("\n\nNORMALS\n");
	    
		for(unsigned int i = 0; i < elementCount; i++)
		{
		    NORMAL *nml;
		    nml = &currentElements->elementNormals[i];
		    g_debug("Vertex %d = (%f,%f,%f)\n",i,(double) nml->x,(double) nml->y,(double) nml->z);
		}

		g_debug("\n\nTEXELS\n");
	    
		for(unsigned int i = 0; i < elementCount; i++)
		{
		    TEXEL *txl;
		    txl = &currentElements->elementTexels[i];
		    g_debug("Texel %d = (%f,%f)\n",i,(double) txl->u,(double) txl->v);
		}
#endif
#if 0
		g_debug("\n\nINDICES\n");
		{
		    int min,max;
		    min = 100000;
		    max = 0;
		    for(unsigned int i = 0; i < currentElements->faces * 3; i++)
		    {
			g_debug("Index %d = %d\n",i,currentElements->elementIndices[i]);
			if(currentElements->elementIndices[i] < min)
			    min = currentElements->elementIndices[i];
			if(currentElements->elementIndices[i] > max)
			    max = currentElements->elementIndices[i];
		    }
		    g_debug("min = %d max = %d %u \n\n",min,max,currentElements->nextElement);
		}
#endif
	    }
	}
    }
done:

    // Tidy up some of the calloced data
    while(freeList)
    {
	free(freeList->data);
	freeList = g_slist_next(freeList);
    }
#if 0    
    g_string_free(LampOnName,TRUE);
    g_string_free(LampOffName,TRUE);
    g_string_free(RedButtonName,TRUE);
    g_string_free(BlackButtonName,TRUE);
    g_string_free(PowerOnButtonName,TRUE);
    g_string_free(PowerOffButtonName,TRUE);
    g_string_free(PowerShroudName,TRUE);
    g_string_free(ShroudName,TRUE);
    g_string_free(OperateBarName,TRUE);
    g_string_free(VolumeName,TRUE);
    g_string_free(ConsoleTopName,TRUE);
    g_string_free(ConsoleFrontName,TRUE);
    g_string_free(HandVertexCloudName,TRUE);
    g_string_free(HandVertexLimitsName,TRUE);
#endif
    // Print number of objects
    if(objectList) g_debug("%d objects\n",g_list_length(objectList));

    debugOff();
    
    return objectList;
}

// First part of loading textures
gboolean loadTextures1(GString *sharedPath,
		      __attribute__((unused)) GString *userPath)
{
    MATERIAL *material;
    GString *filename;

    debugOn();
    
    filename = g_string_new(NULL);
    // Print list of material names
    for(GList *mlist = materialList; mlist != NULL; mlist = mlist->next)
    {
	material = (MATERIAL *)mlist->data;
	g_debug("Material name = %s\n",material->materialName);
	if(material->map_Kd != NULL)
	{
	    g_string_printf(filename,"%sobjects/%s",sharedPath->str,material->map_Kd);
	    g_debug("Loading texture from %s\n",filename->str);
	    printf("Loading texture from %s\n",filename->str);
	    material->texture = loadPNG(filename->str);
	    material->hasTexture = TRUE;
	}
	else
	{
	    material->hasTexture = FALSE;
	}
    }

    debugOff();
    return TRUE;
}

// Second part of texture loading once openGL has been initialised
gboolean loadTextures2(void)
{
    MATERIAL *material;
    GLenum e;

    for(GList *mlist = materialList; mlist != NULL; mlist = mlist->next)
    {
	material = (MATERIAL *)mlist->data;
	g_debug("Material name = %s\n",material->materialName);
	if(material->hasTexture)
	{
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glGenTextures =  %x\n",e);
		return 0;
	    }
	    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	    glPixelStorei(GL_PACK_ALIGNMENT,1);
	    glGenTextures(1, &material->texture->textureId);
       
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glGenTextures =  %x\n",e);
		return 0;
	    }
    
	    glBindTexture(GL_TEXTURE_2D, material->texture->textureId);
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glGenTextures =  %x\n",e);
		return 0;
	    }

	    // Generate 3 mipmap levels
	    if(material->texture->components==3)
		glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGB8,
			       (GLsizei) material->texture->width, (GLsizei) material->texture->height);
	    if(material->texture->components==4)
		glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8,
			       (GLsizei)material->texture->width, (GLsizei) material->texture->height);
    
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug(" glTexStorage2D =  %x\n",e);
		return 0;
	    }

	    if(material->texture->components==3)
		glTexSubImage2D(GL_TEXTURE_2D,0,0,0,
				(GLsizei) material->texture->width,(GLsizei) material->texture->height,
				GL_RGB,GL_UNSIGNED_BYTE,material->texture->pixels);
	    if(material->texture->components==4)
		glTexSubImage2D(GL_TEXTURE_2D,0,0,0,
				(GLsizei) material->texture->width,(GLsizei) material->texture->height,
				GL_RGBA,GL_UNSIGNED_BYTE,material->texture->pixels);
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glTexSubImage2D =  %x\n",e);
		return 0;
	    }

	    glGenerateMipmap(GL_TEXTURE_2D);
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glGenerateMipmap =  %x\n",e);
		return 0;
	    }
    
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glGenTextures =  %x\n",e);
		return 0;
	    }
    
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	    e = glGetError();
	    if(e != 0)
	    {
		g_debug("glGenTextures =  %x\n",e);
		return 0;
	    }
    	}
    }
    return TRUE;
}
