#include "euryopa.h"

#ifdef RW_D3D9

using namespace rw;
using namespace d3d;
using namespace d3d9;

enum {
	// common
	REG_transform	= 0,
	REG_ambient	= 4,
	REG_directCol	= 5,	// 7 lights (main + 6 extra)
	REG_directDir	= 12,	//
	REG_matCol	= 19,
	REG_surfProps	= 20,

	REG_shaderParams= 29,
	// DN and UVA
	REG_dayparam	= 30,
	REG_nightparam	= 31,
	REG_texmat	= 32,
	// Env
	REG_fxParams	= 36,
	REG_envXform	= 37,
	REG_envmat	= 38,
};


static void *ps2BuildingVS, *pcBuildingVS, *ps2BuildingFxVS;
static void *simplePS, *ps2EnvPS;

void setVertexShader(void *vs) { d3ddevice->SetVertexShader((IDirect3DVertexShader9*)vs); }
void setPixelShader(void *ps) { d3ddevice->SetPixelShader((IDirect3DPixelShader9*)ps); }

void
getComposedMatrix(Atomic *atm, RawMatrix *combined)
{
	RawMatrix world, compxpos, worldview;
	Camera *cam = (Camera*)engine->currentCamera;
	convMatrix(&world, atm->getFrame()->getLTM());
	RawMatrix::mult(&worldview, &world, &cam->devView);
	RawMatrix::mult(&compxpos, &worldview, &cam->devProj);
	RawMatrix::transpose(combined, &compxpos);
}

// similar to GTA code, some useless stuff
void
getEnvMatrix(Atomic *atomic, Frame *envframe, RawMatrix *envmat)
{
	Matrix inv, env;
	Clump *clump;
	Frame *frame;

	if(envframe == nil)
		envframe = ((Camera*)engine->currentCamera)->getFrame();

	clump = atomic->clump;

	Matrix::invert(&inv, envframe->getLTM());
	frame = clump ? clump->getFrame() : atomic->getFrame();
	Matrix::mult(&env, frame->getLTM(), &inv);
	convMatrix(envmat, &env);
}

static void
buildingRenderCB_PS2(Atomic *atomic, d3d9::InstanceDataHeader *header)
{
	RawMatrix combined, ident;
	Matrix *texmat;
	Geometry *geo = atomic->geometry;
	RawMatrix::setIdentity(&ident);

	d3ddevice->SetStreamSource(0, (IDirect3DVertexBuffer9*)header->vertexStream[0].vertexBuffer,
	                           0, header->vertexStream[0].stride);
	d3ddevice->SetIndices((IDirect3DIndexBuffer9*)header->indexBuffer);
	d3ddevice->SetVertexDeclaration((IDirect3DVertexDeclaration9*)header->vertexDeclaration);

	setVertexShader(gBuildingPipeSwitch == PLATFORM_PC ? pcBuildingVS : ps2BuildingVS);
	setPixelShader(simplePS);

	getComposedMatrix(atomic, &combined);
	d3ddevice->SetVertexShaderConstantF(REG_transform, (float*)&combined, 4);

	float dayparam[4], nightparam[4];
	if(atomic->pipeline->pluginData == gta::RSPIPE_PC_CustomBuilding_PipeID){
		dayparam[0] = dayparam[1] = dayparam[2] = dayparam[3] = 0.0f;
		nightparam[0] = nightparam[1] = nightparam[2] = nightparam[3] = 1.0f;
	}else{
		dayparam[0] = dayparam[1] = dayparam[2] = 1.0f-gDayNightBalance;
		nightparam[0] = nightparam[1] = nightparam[2] = gDayNightBalance;
		dayparam[3] = gWetRoadEffect;
		nightparam[3] = (1.0f-gWetRoadEffect);
	}

	d3ddevice->SetVertexShaderConstantF(REG_dayparam, dayparam, 1);
	d3ddevice->SetVertexShaderConstantF(REG_nightparam, nightparam, 1);

	d3ddevice->SetVertexShaderConstantF(REG_ambient, (float*)&pAmbient->color, 1);

	RawMatrix envmat;
	getEnvMatrix(atomic, nil, &envmat);
	d3ddevice->SetVertexShaderConstantF(REG_envmat, (float*)&envmat, 4);

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		float colorscale = 1.0f;
		if(inst->material->texture && gBuildingPipeSwitch == PLATFORM_PS2)
			colorscale = 255.0f/128.0f;
		d3ddevice->SetVertexShaderConstantF(REG_shaderParams, &colorscale, 1);
		d3ddevice->SetPixelShaderConstantF(0, &colorscale, 1);

		int hasEnv = (*(uint32*)&inst->material->surfaceProps.specular) & 1;
		gta::EnvMat *env = gta::getEnvMat(inst->material);

		d3d::setTexture(0, inst->material->texture);

		SetRenderState(VERTEXALPHA, inst->vertexAlpha || inst->material->color.alpha != 255);

		// Material colour
		rw::RGBAf col;
		convColor(&col, &inst->material->color);
		d3ddevice->SetVertexShaderConstantF(REG_matCol, (float*)&col, 1);
		d3ddevice->SetVertexShaderConstantF(REG_surfProps, (float*)&inst->material->surfaceProps, 1);

		// UV animation
		if(MatFX::getEffects(inst->material) == MatFX::UVTRANSFORM){
			MatFX *matfx = MatFX::get(inst->material);
			matfx->getUVTransformMatrices(&texmat, nil);
			if(texmat)
				d3ddevice->SetVertexShaderConstantF(REG_texmat, (float*)texmat, 4);
			else
				d3ddevice->SetVertexShaderConstantF(REG_texmat, (float*)&ident, 4);
		}else
			d3ddevice->SetVertexShaderConstantF(REG_texmat, (float*)&ident, 4);


		d3d::flushCache();
		d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)header->primType, inst->baseIndex,
		                                0, inst->numVertices,
		                                inst->startIndex, inst->numPrimitives);

		if(hasEnv){
			setVertexShader(ps2BuildingFxVS);
			setPixelShader(ps2EnvPS);

			d3d::setTexture(0, env->texture);

			float envxform[4];
			envxform[0] = envxform[1] = 0.0f;
			envxform[2] = env->getScaleX();
			envxform[3] = env->getScaleY();
			d3ddevice->SetVertexShaderConstantF(REG_envXform, envxform, 1);

			float fxparams[2];
			fxparams[0] = env->getShininess();
			fxparams[1] = 1.0f;
			d3ddevice->SetVertexShaderConstantF(REG_fxParams, fxparams, 1);

			int dst;
			dst = GetRenderState(DESTBLEND);
			SetRenderState(DESTBLEND, BLENDONE);
			SetRenderState(VERTEXALPHA, 1);
			d3d::flushCache();
			d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)header->primType, inst->baseIndex,
			                                0, inst->numVertices,
			                                inst->startIndex, inst->numPrimitives);
			SetRenderState(DESTBLEND, dst);

			d3d::setTexture(0, env->texture);
			setVertexShader(gBuildingPipeSwitch == PLATFORM_PC ? pcBuildingVS : ps2BuildingVS);
			setPixelShader(simplePS);
		}

		inst++;
	}

	d3ddevice->SetVertexShader(nil);
	d3ddevice->SetPixelShader(nil);
}

#define GETEXTRACOLOREXT(g) PLUGINOFFSET(gta::ExtraVertColors, g, gta::extraVertColorOffset)

static void
buildingInstanceCB(Geometry *geo, d3d9::InstanceDataHeader *header)
{
	VertexElement dcl[12];
	gta::ExtraVertColors *extracols = GETEXTRACOLOREXT(geo);
	V3d *extranormals = gta::getExtraNormals(geo);

	VertexStream *s = &header->vertexStream[0];
	s->offset = 0;
	s->managed = 1;
	s->geometryFlags = 0;
	s->dynamicLock = 0;

	int i = 0;
	dcl[i].stream = 0;
	dcl[i].offset = 0;
	dcl[i].type = D3DDECLTYPE_FLOAT3;
	dcl[i].method = D3DDECLMETHOD_DEFAULT;
	dcl[i].usage = D3DDECLUSAGE_POSITION;
	dcl[i].usageIndex = 0;
	i++;
	uint16 stride = 12;
	s->geometryFlags |= 0x2;

	bool isPrelit = (geo->flags & Geometry::PRELIT) != 0;
	if(isPrelit){
		// night
		dcl[i].stream = 0;
		dcl[i].offset = stride;
		dcl[i].type = D3DDECLTYPE_D3DCOLOR;
		dcl[i].method = D3DDECLMETHOD_DEFAULT;
		dcl[i].usage = D3DDECLUSAGE_COLOR;
		dcl[i].usageIndex = 0;
		i++;
		s->geometryFlags |= 0x8;
		stride += 4;

		// day
		dcl[i].stream = 0;
		dcl[i].offset = stride;
		dcl[i].type = D3DDECLTYPE_D3DCOLOR;
		dcl[i].method = D3DDECLMETHOD_DEFAULT;
		dcl[i].usage = D3DDECLUSAGE_COLOR;
		dcl[i].usageIndex = 1;
		i++;
		s->geometryFlags |= 0x8;
		stride += 4;
	}else{
		// we need vertex colors in the shader so force white like on PS2, one set is enough
		dcl[i].stream = 0;
		dcl[i].offset = stride;
		dcl[i].type = D3DDECLTYPE_D3DCOLOR;
		dcl[i].method = D3DDECLMETHOD_DEFAULT;
		dcl[i].usage = D3DDECLUSAGE_COLOR;
		dcl[i].usageIndex = 0;
		i++;
		s->geometryFlags |= 0x8;
		stride += 4;
	}

	for(int32 n = 0; n < geo->numTexCoordSets; n++){
		dcl[i].stream = 0;
		dcl[i].offset = stride;
		dcl[i].type = D3DDECLTYPE_FLOAT2;
		dcl[i].method = D3DDECLMETHOD_DEFAULT;
		dcl[i].usage = D3DDECLUSAGE_TEXCOORD;
		dcl[i].usageIndex = (uint8)n;
		i++;
		s->geometryFlags |= 0x10 << n;
		stride += 8;
	}

	bool hasNormals = (geo->flags & Geometry::NORMALS) != 0;
	if(hasNormals || extranormals){
		dcl[i].stream = 0;
		dcl[i].offset = stride;
		dcl[i].type = D3DDECLTYPE_FLOAT3;
		dcl[i].method = D3DDECLMETHOD_DEFAULT;
		dcl[i].usage = D3DDECLUSAGE_NORMAL;
		dcl[i].usageIndex = 0;
		i++;
		s->geometryFlags |= 0x4;
		stride += 12;
	}
	dcl[i] = D3DDECL_END();
	header->vertexStream[0].stride = stride;

	header->vertexDeclaration = createVertexDeclaration((VertexElement*)dcl);

	s->vertexBuffer = createVertexBuffer(header->totalNumVertex*s->stride, 0, D3DPOOL_MANAGED);

	// TODO: support both vertex buffers
	uint8 *verts = lockVertices(s->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
	for(i = 0; dcl[i].usage != D3DDECLUSAGE_POSITION || dcl[i].usageIndex != 0; i++)
		;
	instV3d(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
		geo->morphTargets[0].vertices,
		header->totalNumVertex,
		header->vertexStream[dcl[i].stream].stride);

	if(isPrelit){
		int j;
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_COLOR || dcl[i].usageIndex != 0; i++)
			;
		for(j = 0; dcl[j].usage != D3DDECLUSAGE_COLOR || dcl[j].usageIndex != 1; j++)
			;

		InstanceData *inst = header->inst;
		uint32 n = header->numMeshes;
		rw::RGBA *dayColors = geo->colors;//extracols->dayColors;
		rw::RGBA *nightColors = extracols->nightColors;
//		if(dayColors == nil) dayColors = geo->colors;
		if(nightColors == nil) nightColors = dayColors;
		while(n--){
			uint32 stride = header->vertexStream[dcl[i].stream].stride;
			inst->vertexAlpha = instColor(vertFormatMap[dcl[i].type],
				verts + dcl[i].offset + stride*inst->minVert,
				nightColors + inst->minVert,
				inst->numVertices,
				stride);
			inst->vertexAlpha |= instColor(vertFormatMap[dcl[j].type],
				verts + dcl[j].offset + stride*inst->minVert,
				dayColors + inst->minVert,
				inst->numVertices,
				stride);
			inst++;
		}
	}else{
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_COLOR || dcl[i].usageIndex != 0; i++)
			;
		InstanceData *inst = header->inst;
		uint32 n = header->numMeshes;
		while(n--){
			inst->vertexAlpha = 0;
			inst++;
		}
		instWhite(vertFormatMap[dcl[i].type],
			verts + dcl[i].offset,
			header->totalNumVertex,
			header->vertexStream[dcl[i].stream].stride);
	}

	for(int32 n = 0; n < geo->numTexCoordSets; n++){
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_TEXCOORD || dcl[i].usageIndex != n; i++)
			;
		instTexCoords(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
			geo->texCoords[n],
			header->totalNumVertex,
			header->vertexStream[dcl[i].stream].stride);
	}

	if(hasNormals || extranormals){
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_NORMAL || dcl[i].usageIndex != 0; i++)
			;
		instV3d(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
			hasNormals ? geo->morphTargets[0].normals : extranormals,
			header->totalNumVertex,
			header->vertexStream[dcl[i].stream].stride);
	}
	unlockVertices(s->vertexBuffer);
}


void
MakeCustomBuildingPipelines(void)
{
	d3d9::ObjPipeline *pipe;

#include "d3d_shaders/ps2BuildingVS.inc"
#include "d3d_shaders/pcBuildingVS.inc"
#include "d3d_shaders/simplePS.inc"
	d3ddevice->CreateVertexShader((DWORD*)ps2BuildingVS_cso, (IDirect3DVertexShader9**)&ps2BuildingVS);
	d3ddevice->CreateVertexShader((DWORD*)pcBuildingVS_cso, (IDirect3DVertexShader9**)&pcBuildingVS);
	d3ddevice->CreatePixelShader((DWORD*)simplePS_cso, (IDirect3DPixelShader9**)&simplePS);

#include "d3d_shaders/ps2BuildingFxVS.inc"
#include "d3d_shaders/ps2EnvPS.inc"
	d3ddevice->CreateVertexShader((DWORD*)ps2BuildingFxVS_cso, (IDirect3DVertexShader9**)&ps2BuildingFxVS);
	d3ddevice->CreatePixelShader((DWORD*)ps2EnvPS_cso, (IDirect3DPixelShader9**)&ps2EnvPS);


	pipe = new d3d9::ObjPipeline(PLATFORM_D3D9);
	pipe->pluginID = gta::RSPIPE_PC_CustomBuilding_PipeID;
	pipe->pluginData = gta::RSPIPE_PC_CustomBuilding_PipeID;
	pipe->instanceCB = buildingInstanceCB;
	pipe->uninstanceCB = nil;
	pipe->renderCB = buildingRenderCB_PS2;
	buildingPipe = pipe;

	pipe = new d3d9::ObjPipeline(PLATFORM_D3D9);
	pipe->pluginID = gta::RSPIPE_PC_CustomBuildingDN_PipeID;
	pipe->pluginData = gta::RSPIPE_PC_CustomBuildingDN_PipeID;
	pipe->instanceCB = buildingInstanceCB;
	pipe->uninstanceCB = nil;
	pipe->renderCB = buildingRenderCB_PS2;
	buildingDNPipe = pipe;
}

#endif