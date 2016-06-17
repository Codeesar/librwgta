#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <rw.h>

#include "rwgta.h"

using namespace std;

namespace rw {

int32
findPlatform(Clump *c)
{
	FORLIST(lnk, c->atomics){
		Geometry *g = Atomic::fromClump(lnk)->geometry;
		if(g->instData)
			return g->instData->platform;
	}
	return 0;
}

void
switchPipes(Clump *c, int32 platform)
{
	FORLIST(lnk, c->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		if(a->pipeline && a->pipeline->platform != platform){
			uint32 plgid = a->pipeline->pluginID;
			switch(plgid){
			case ID_SKIN:
				a->pipeline = skinGlobals.pipelines[platform];
				break;
			case ID_MATFX:
				a->pipeline = matFXGlobals.pipelines[platform];
				break;
			}
		}
	}
}

}

namespace gta {

void
attachPlugins(void)
{
	// Call before native rasters are registered
	rw::initialize();

	rw::ps2::registerPDSPlugin(40);
	rw::ps2::registerPluginPDSPipes();
	gta::registerPDSPipes();

	rw::ps2::registerNativeRaster();
	rw::xbox::registerNativeRaster();
	rw::d3d::registerNativeRaster();

	rw::registerMeshPlugin();
	rw::registerNativeDataPlugin();
	rw::registerAtomicRightsPlugin();
	rw::registerMaterialRightsPlugin();
	rw::xbox::registerVertexFormatPlugin();
	rw::registerSkinPlugin();
	rw::registerHAnimPlugin();
	gta::registerNodeNamePlugin();
	rw::registerMatFXPlugin();
	rw::registerUVAnimPlugin();
	rw::ps2::registerADCPlugin();
	gta::registerExtraNormalsPlugin();
	gta::registerExtraVertColorPlugin();
	gta::registerEnvSpecPlugin();
	gta::registerBreakableModelPlugin();
	gta::registerCollisionPlugin();
	gta::register2dEffectPlugin();
	gta::registerPipelinePlugin();
}

//
// Frame
//

// Node Name

int32 nodeNameOffset;

static void*
createNodeName(void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	name[0] = '\0';
	return object;
}

static void*
copyNodeName(void *dst, void *src, int32 offset, int32)
{
	char *dstname = PLUGINOFFSET(char, dst, offset);
	char *srcname = PLUGINOFFSET(char, src, offset);
	strncpy(dstname, srcname, 23);
	return dst;
}

static void*
destroyNodeName(void *object, int32, int32)
{
	return object;
}

static Stream*
readNodeName(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream->read(name, len);
	name[len] = '\0';
	//printf("%s\n", name);
	return stream;
}

static Stream*
writeNodeName(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream->write(name, len);
	return stream;
}

static int32
getSizeNodeName(void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	int32 len = strlen(name);
	return len > 0 ? len : 0;
}


void
registerNodeNamePlugin(void)
{
	nodeNameOffset = Frame::registerPlugin(24, ID_NODENAME,
	                                       createNodeName,
	                                       destroyNodeName,
	                                       copyNodeName);
	Frame::registerPluginStream(ID_NODENAME,
	                            readNodeName,
	                            writeNodeName,
	                            getSizeNodeName);
}

char*
getNodeName(Frame *f)
{
	return PLUGINOFFSET(char, f, nodeNameOffset);
}

//
// Geometry
//

// Breakable Model

int32 breakableOffset;

static void*
createBreakableModel(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint8*, object, offset) = 0;
	return object;
}

static void*
destroyBreakableModel(void *object, int32 offset, int32)
{
	uint8 *p = *PLUGINOFFSET(uint8*, object, offset);
	delete[] p;
	return object;
}

static Stream*
readBreakableModel(Stream *stream, int32, void *object, int32 o, int32)
{
	uint32 header[13];
	uint32 hasBreakable = stream->readU32();
	if(hasBreakable == 0)
		return stream;
	stream->read(header, 13*4);
	uint32 size = header[1]*(12+8+4) + header[5]*(6+2) +
	              header[8]*(32+32+12);
	uint8 *p = new uint8[sizeof(Breakable)+size];
	Breakable *breakable = (Breakable*)p;
	*PLUGINOFFSET(Breakable*, object, o) = breakable;
	breakable->position     = header[0];
	breakable->numVertices  = header[1];
	breakable->numFaces     = header[5];
	breakable->numMaterials = header[8];
	p += sizeof(Breakable);
	stream->read(p, size);
	breakable->vertices = (float*)p;
	p += breakable->numVertices*12;
	breakable->texCoords = (float*)p;
	p += breakable->numVertices*8;
	breakable->colors = (uint8*)p;
	p += breakable->numVertices*4;
	breakable->faces = (uint16*)p;
	p += breakable->numFaces*6;
	breakable->matIDs = (uint16*)p;
	p += breakable->numFaces*2;
	breakable->texNames = (char(*)[32])p;
	p += breakable->numMaterials*32;
	breakable->maskNames = (char(*)[32])p;
	p += breakable->numMaterials*32;
	breakable->surfaceProps = (float32(*)[3])p;
	return stream;
}

static Stream*
writeBreakableModel(Stream *stream, int32, void *object, int32 o, int32)
{
	uint32 header[13];
	Breakable *breakable = *PLUGINOFFSET(Breakable*, object, o);
	uint8 *p = (uint8*)breakable;
	if(breakable == NULL){
		stream->writeU32(0);
		return stream;
	}
	stream->writeU32(1);
	memset((char*)header, 0, 13*4);
	header[0] = breakable->position;
	header[1] = breakable->numVertices;
	header[5] = breakable->numFaces;
	header[8] = breakable->numMaterials;
	stream->write(header, 13*4);
	p += sizeof(Breakable);
	stream->write(p, breakable->numVertices*(12+8+4) +
	                       breakable->numFaces*(6+2) +
	                       breakable->numMaterials*(32+32+12));
	return stream;
}

static int32
getSizeBreakableModel(void *object, int32 offset, int32)
{
	Breakable *breakable = *PLUGINOFFSET(Breakable*, object, offset);
	if(breakable == NULL)
		return 0; //4;
	return 56 + breakable->numVertices*(12+8+4) +
	            breakable->numFaces*(6+2) +
	            breakable->numMaterials*(32+32+12);
}

void
registerBreakableModelPlugin(void)
{
	breakableOffset = Geometry::registerPlugin(sizeof(Breakable*),
	                                           ID_BREAKABLE,
	                                           createBreakableModel,
	                                           destroyBreakableModel, NULL);
	Geometry::registerPluginStream(ID_BREAKABLE,
	                               readBreakableModel,
	                               writeBreakableModel,
	                               getSizeBreakableModel);
}

// Extra normals

int32 extraNormalsOffset;

static void*
createExtraNormals(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(float*, object, offset) = NULL;
	return object;
}

static void*
destroyExtraNormals(void *object, int32 offset, int32)
{
	float *extranormals = *PLUGINOFFSET(float*, object, offset);
	delete[] extranormals;
	*PLUGINOFFSET(float*, object, offset) = NULL;
	return object;
}

static Stream*
readExtraNormals(Stream *stream, int32, void *object, int32 offset, int32)
{
	Geometry *geo = (Geometry*)object;
	float **plgp = PLUGINOFFSET(float*, object, offset);
	if(*plgp)
		delete[] *plgp;
	float *extranormals = *plgp = new float[geo->numVertices*3];
	stream->read(extranormals, geo->numVertices*3*4);
//	printf("extra normals\n");

//	for(int i = 0; i < geo->numVertices; i++){
//		float *nx = extranormals+i*3;
//		float *n = geo->morphTargets[0].normals;
//		float len = n[0]*n[0] + n[1]*n[1] + n[2]*n[2];
//		printf("%f %f %f %f\n", n[0], n[1], n[2], len);
//		printf("%f %f %f\n", nx[0], nx[1], nx[2]);
//	}
	return stream;
}

static Stream*
writeExtraNormals(Stream *stream, int32, void *object, int32 offset, int32)
{
	Geometry *geo = (Geometry*)object;
	float *extranormals = *PLUGINOFFSET(float*, object, offset);
	assert(extranormals != NULL);
	stream->write(extranormals, geo->numVertices*3*4);
	return stream;
}

static int32
getSizeExtraNormals(void *object, int32 offset, int32)
{
	Geometry *geo = (Geometry*)object;
	if(*PLUGINOFFSET(float*, object, offset))
		return geo->numVertices*3*4;
	return 0;
}

void
registerExtraNormalsPlugin(void)
{
	extraNormalsOffset = Geometry::registerPlugin(sizeof(void*),
	                                              ID_EXTRANORMALS,
	                                              createExtraNormals,
	                                              destroyExtraNormals,
	                                              NULL);
	Geometry::registerPluginStream(ID_EXTRANORMALS,
	                               readExtraNormals,
	                               writeExtraNormals,
	                               getSizeExtraNormals);
}


// Extra colors

int32 extraVertColorOffset;

void
allocateExtraVertColors(Geometry *g)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, g, extraVertColorOffset);
	colordata->nightColors = new uint8[g->numVertices*4];
	colordata->dayColors = new uint8[g->numVertices*4];
	colordata->balance = 1.0f;
}

static void*
createExtraVertColors(void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	colordata->nightColors = NULL;
	colordata->dayColors = NULL;
	colordata->balance = 0.0f;
	return object;
}

static void*
destroyExtraVertColors(void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	delete[] colordata->nightColors;
	delete[] colordata->dayColors;
	return object;
}

static Stream*
readExtraVertColors(Stream *stream, int32, void *object, int32 offset, int32)
{
	uint32 hasData;
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	hasData = stream->readU32();
	if(!hasData)
		return stream;
	Geometry *geometry = (Geometry*)object;
	colordata->nightColors = new uint8[geometry->numVertices*4];
	colordata->dayColors = new uint8[geometry->numVertices*4];
	colordata->balance = 1.0f;
	stream->read(colordata->nightColors, geometry->numVertices*4);
	if(geometry->colors)
		memcpy(colordata->dayColors, geometry->colors,
		       geometry->numVertices*4);
	return stream;
}

static Stream*
writeExtraVertColors(Stream *stream, int32, void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	stream->writeU32(colordata->nightColors != NULL);
	if(colordata->nightColors){
		Geometry *geometry = (Geometry*)object;
		stream->write(colordata->nightColors, geometry->numVertices*4);
	}
	return stream;
}

static int32
getSizeExtraVertColors(void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	Geometry *geometry = (Geometry*)object;
	if(colordata->nightColors)
		return 4 + geometry->numVertices*4;
	return 0;
}

void
registerExtraVertColorPlugin(void)
{
	extraVertColorOffset = Geometry::registerPlugin(sizeof(ExtraVertColors),
	                                                ID_EXTRAVERTCOLORS,
	                                                createExtraVertColors,
	                                                destroyExtraVertColors,
	                                                NULL);
	Geometry::registerPluginStream(ID_EXTRAVERTCOLORS,
	                               readExtraVertColors,
	                               writeExtraVertColors,
	                               getSizeExtraVertColors);
}

// Environment mat

int32 envMatOffset;

static void*
createEnvMat(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(EnvMat*, object, offset) = NULL;
	return object;
}

static void*
destroyEnvMat(void *object, int32 offset, int32)
{
	EnvMat **envmat = PLUGINOFFSET(EnvMat*, object, offset);
	delete *envmat;
	*envmat = NULL;
	return object;
}

static void*
copyEnvMat(void *dst, void *src, int32 offset, int32)
{
	EnvMat *srcenv = *PLUGINOFFSET(EnvMat*, src, offset);
	if(srcenv == NULL)
		return dst;
	EnvMat *dstenv = new EnvMat;
	dstenv->scaleX = srcenv->scaleX;
	dstenv->scaleY = srcenv->scaleY;
	dstenv->transScaleX = srcenv->transScaleX;
	dstenv->transScaleY = srcenv->transScaleY;
	dstenv->shininess = srcenv->shininess;
	dstenv->texture = NULL;
	*PLUGINOFFSET(EnvMat*, dst, offset) = dstenv;
	return dst;
}

struct EnvStream {
	float scaleX, scaleY;
	float transScaleX, transScaleY;
	float shininess;
	int32 zero;
};

static Stream*
readEnvMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	EnvStream buf;
	EnvMat *env = new EnvMat;
	*PLUGINOFFSET(EnvMat*, object, offset) = env;
	stream->read(&buf, sizeof(buf));
	env->scaleX = (int8)(buf.scaleX*8.0f);
	env->scaleY = (int8)(buf.scaleY*8.0f);
	env->transScaleX = (int8)(buf.transScaleX*8.0f);
	env->transScaleY = (int8)(buf.transScaleY*8.0f);
	env->shininess = (uint8)(buf.shininess*255.0f);
	env->texture = NULL;
	return stream;
}

static Stream*
writeEnvMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	EnvStream buf;
	EnvMat *env = *PLUGINOFFSET(EnvMat*, object, offset);
	buf.scaleX = env->scaleX/8.0f;
	buf.scaleY = env->scaleY/8.0f;
	buf.transScaleX = env->transScaleX/8.0f;
	buf.transScaleY = env->transScaleY/8.0f;
	buf.shininess = env->shininess/255.0f;
	buf.zero = 0;
	stream->write(&buf, sizeof(buf));
	return stream;
}

static int32
getSizeEnvMat(void *object, int32 offset, int32)
{
	EnvMat *env = *PLUGINOFFSET(EnvMat*, object, offset);
	return env ? (int)sizeof(EnvStream) : 0;
}

// Specular mat

int32 specMatOffset;

static void*
createSpecMat(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(SpecMat*, object, offset) = NULL;
	return object;
}

static void*
destroySpecMat(void *object, int32 offset, int32)
{
	SpecMat **specmat = PLUGINOFFSET(SpecMat*, object, offset);
	if(*specmat == NULL)
		return object;
	if((*specmat)->texture)
		(*specmat)->texture->destroy();
	delete *specmat;
	*specmat = NULL;
	return object;
}

static void*
copySpecMat(void *dst, void *src, int32 offset, int32)
{
	SpecMat *srcspec = *PLUGINOFFSET(SpecMat*, src, offset);
	if(srcspec == NULL)
		return dst;
	SpecMat *dstspec = new SpecMat;
	*PLUGINOFFSET(SpecMat*, dst, offset) = dstspec;
	dstspec->specularity = srcspec->specularity;
	dstspec->texture = srcspec->texture;
	dstspec->texture->refCount++;
	return dst;
}

struct SpecStream {
	float specularity;
	char texname[24];
};

static Stream*
readSpecMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	SpecStream buf;
	SpecMat *spec = new SpecMat;
	*PLUGINOFFSET(SpecMat*, object, offset) = spec;
	stream->read(&buf, sizeof(buf));
	spec->specularity = buf.specularity;
	spec->texture = Texture::create(NULL);
	strncpy(spec->texture->name, buf.texname, 24);
	return stream;
}

static Stream*
writeSpecMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	SpecStream buf;
	SpecMat *spec = *PLUGINOFFSET(SpecMat*, object, offset);
	buf.specularity = spec->specularity;
	strncpy(buf.texname, spec->texture->name, 24);
	stream->write(&buf, sizeof(buf));
	return stream;
}

static int32
getSizeSpecMat(void *object, int32 offset, int32)
{
	SpecMat *spec = *PLUGINOFFSET(SpecMat*, object, offset);
	return spec ? (int)sizeof(SpecStream) : 0;
}

void
registerEnvSpecPlugin(void)
{
	envMatOffset = Material::registerPlugin(sizeof(EnvMat*), ID_ENVMAT,
	                                        createEnvMat,
                                                destroyEnvMat,
                                                copyEnvMat);
	Material::registerPluginStream(ID_ENVMAT, readEnvMat,
                                                  writeEnvMat,
                                                  getSizeEnvMat);
	specMatOffset = Material::registerPlugin(sizeof(SpecMat*), ID_SPECMAT,
	                                         createSpecMat,
                                                 destroySpecMat,
                                                 copySpecMat);
	Material::registerPluginStream(ID_SPECMAT, readSpecMat,
                                                   writeSpecMat,
                                                   getSizeSpecMat);
}

// Pipeline

int32 pipelineOffset;

static void*
createPipeline(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, object, offset) = 0;
	return object;
}

static void*
copyPipeline(void *dst, void *src, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, dst, offset) = *PLUGINOFFSET(uint32, src, offset);
	return dst;
}

static Stream*
readPipeline(Stream *stream, int32, void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, object, offset) = stream->readU32();
//	printf("%x\n", *PLUGINOFFSET(uint32, object, offset));
	return stream;
}

static Stream*
writePipeline(Stream *stream, int32, void *object, int32 offset, int32)
{
	stream->writeU32(*PLUGINOFFSET(uint32, object, offset));
	return stream;
}

static int32
getSizePipeline(void *object, int32 offset, int32)
{
	if(*PLUGINOFFSET(uint32, object, offset))
		return 4;
	return 0;
}

void
registerPipelinePlugin(void)
{
	pipelineOffset = Atomic::registerPlugin(sizeof(uint32), ID_PIPELINE,
	                                        createPipeline,
                                                NULL,
                                                copyPipeline);
	Atomic::registerPluginStream(ID_PIPELINE, readPipeline,
	                             writePipeline, getSizePipeline);
}

uint32
getPipelineID(Atomic *atomic)
{
	return *PLUGINOFFSET(uint32, atomic, pipelineOffset);
}

void
setPipelineID(Atomic *atomic, uint32 id)
{
	*PLUGINOFFSET(uint32, atomic, pipelineOffset) = id;
}

// 2dEffect

struct SizedData
{
	uint32 size;
	uint8 *data;
};

int32 twodEffectOffset;

static void*
create2dEffect(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	data->size = 0;
	data->data = NULL;
	return object;
}

static void*
destroy2dEffect(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	delete[] data->data;
	data->data = NULL;
	data->size = 0;
	return object;
}

static void*
copy2dEffect(void *dst, void *src, int32 offset, int32)
{
	SizedData *srcdata, *dstdata;
	dstdata = PLUGINOFFSET(SizedData, dst, offset);
	srcdata = PLUGINOFFSET(SizedData, src, offset);
	dstdata->size = srcdata->size;
	if(dstdata->size != 0){
		dstdata->data = new uint8[dstdata->size];
		memcpy(dstdata->data, srcdata->data, dstdata->size);
	}
	return dst;
}

static Stream*
read2dEffect(Stream *stream, int32 size, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	data->size = size;
	data->data = new uint8[data->size];
	stream->read(data->data, data->size);
	return stream;
}

static Stream*
write2dEffect(Stream *stream, int32, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	stream->write(data->data, data->size);
	return stream;
}

static int32
getSize2dEffect(void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	return data->size;
}

void
register2dEffectPlugin(void)
{
	twodEffectOffset = Geometry::registerPlugin(sizeof(SizedData), ID_2DEFFECT,
	                                            create2dEffect,
                                                    destroy2dEffect,
                                                    copy2dEffect);
	Geometry::registerPluginStream(ID_2DEFFECT, read2dEffect,
	                               write2dEffect, getSize2dEffect);
}

// Collision

int32 collisionOffset;

static void*
createCollision(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	data->size = 0;
	data->data = NULL;
	return object;
}

static void*
destroyCollision(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	delete[] data->data;
	data->data = NULL;
	data->size = 0;
	return object;
}

static void*
copyCollision(void *dst, void *src, int32 offset, int32)
{
	SizedData *srcdata, *dstdata;
	dstdata = PLUGINOFFSET(SizedData, dst, offset);
	srcdata = PLUGINOFFSET(SizedData, src, offset);
	dstdata->size = srcdata->size;
	if(dstdata->size != 0){
		dstdata->data = new uint8[dstdata->size];
		memcpy(dstdata->data, srcdata->data, dstdata->size);
	}
	return dst;
}

static Stream*
readCollision(Stream *stream, int32 size, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	data->size = size;
	data->data = new uint8[data->size];
	stream->read(data->data, data->size);
	return stream;
}

static Stream*
writeCollision(Stream *stream, int32, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	stream->write(data->data, data->size);
	return stream;
}

static int32
getSizeCollision(void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	return data->size;
}

void
registerCollisionPlugin(void)
{
	collisionOffset = Clump::registerPlugin(sizeof(SizedData), ID_COLLISION,
	                                        createCollision,
                                                destroyCollision,
                                                copyCollision);
	Clump::registerPluginStream(ID_COLLISION, readCollision,
	                            writeCollision, getSizeCollision);
}

}
