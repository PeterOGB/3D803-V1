/*  This file is part of the Elliott 803 emulator.

    Copyright © 2020  Peter Onion

    See LICENCE file. 
*/

typedef struct material
{
    int materialIndex;
    char *materialName;
    float Ns;
    float KaR,KaG,KaB;
    float KdR,KdG,KdB;
    float KsR,KsG,KsB;
    float Ni;
    float d;
    int illum;
    char *map_Kd;
    struct pngData *texture;
    //GLuint texture;
    gboolean hasTexture;
    
} MATERIAL;

typedef struct
{
    GLfloat x;
    GLfloat y;
    GLfloat z;
} VERTEX;

typedef struct
{
    GLfloat x;
    GLfloat y;
    GLfloat z;
} NORMAL;

typedef struct
{
    GLfloat u;
    GLfloat v;
} TEXEL;

typedef struct
{
    unsigned int v[3];
    unsigned int n[3];
    unsigned int t[3];
} FACE;


typedef struct
{
    VERTEX *elementVertices; 
    NORMAL *elementNormals;
    TEXEL *elementTexels;
    GLushort *elementIndices;
    unsigned int IndicesCount;
    unsigned int nextElement;
    int nextIndex;
    
    MATERIAL *Material;
    unsigned int minVertex,maxVertex;
    unsigned int minNormal,maxNormal;
    unsigned int minTexel,maxTexel;
    unsigned int vCount,nCount;
    
    void *lookupTable;
    unsigned int faces;
    FACE *Faces;
} ELEMENTS;


typedef struct objModel
{
    GString *name;
    unsigned int vertices;
    unsigned int texels;
    unsigned int normals;
    unsigned int faces;
    GList *ElementsList,*elementsList;
    gboolean hidden;
    
    vec4 *VertexOnly;  // Scaned vertexlist for vertex clouds.
    unsigned int firstVertex,lastVertex; 
}
OBJECT;

typedef struct
{
    GList *objects;
} GLESdata;

typedef struct 
{
    const gchar *objectName;
    OBJECT **object;
    gboolean hiddenFlag;
} OBJECTINIT;

GList *loadObjectsFromFile(const GString *path,const char *fname, OBJECTINIT *objects);
gboolean loadTextures1(GString *sharedPath,
		      __attribute__((unused)) GString *userPath);
gboolean loadTextures2(void);




