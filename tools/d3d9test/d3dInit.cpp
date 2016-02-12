#include "d3dUtility.h"
#include <DirectXMath.h>
using namespace DirectX;

#define RW_D3D9
#include <rw.h>
#include "rwgta.h"
#include "math/math.h"
#include "camera.h"

IDirect3DDevice9 *Device = 0;

Camera *camera;

namespace rw {

namespace d3d {

void
setTexture(Texture *tex)
{
	static DWORD filternomip[] = {
		0, D3DTEXF_POINT, D3DTEXF_LINEAR,
		   D3DTEXF_POINT, D3DTEXF_LINEAR,
		   D3DTEXF_POINT, D3DTEXF_LINEAR
	};
	static DWORD wrap[] = {
		0, D3DTADDRESS_WRAP, D3DTADDRESS_MIRROR,
		D3DTADDRESS_CLAMP, D3DTADDRESS_BORDER
	};

	D3dRaster *raster = PLUGINOFFSET(D3dRaster, tex->raster, nativeRasterOffset);
	if(tex->raster && raster->texture){
		Device->SetTexture(0, (IDirect3DTexture9*)raster->texture);
		Device->SetSamplerState(0, D3DSAMP_MAGFILTER, filternomip[tex->filterAddressing & 0xFF]);
		Device->SetSamplerState(0, D3DSAMP_MINFILTER, filternomip[tex->filterAddressing & 0xFF]);
		Device->SetSamplerState(0, D3DSAMP_ADDRESSU, wrap[(tex->filterAddressing >> 8) & 0xF]);
		Device->SetSamplerState(0, D3DSAMP_ADDRESSV, wrap[(tex->filterAddressing >> 12) & 0xF]);
	}else
		Device->SetTexture(0, NULL);
}

void
setMaterial(Material *mat)
{
	D3DMATERIAL9 mat9;
	D3DCOLORVALUE black = { 0, 0, 0, 0 };
	float ambmult = mat->surfaceProps.ambient/255.0f;
	float diffmult = mat->surfaceProps.diffuse/255.0f;
	mat9.Ambient.r = mat->color.red*ambmult;
	mat9.Ambient.g = mat->color.green*ambmult;
	mat9.Ambient.b = mat->color.blue*ambmult;
	mat9.Ambient.a = mat->color.alpha*ambmult;
	mat9.Diffuse.r = mat->color.red*diffmult;
	mat9.Diffuse.g = mat->color.green*diffmult;
	mat9.Diffuse.b = mat->color.blue*diffmult;
	mat9.Diffuse.a = mat->color.alpha*diffmult;
	mat9.Power = 0.0f;
	mat9.Emissive = black;
	mat9.Specular = black;
	Device->SetMaterial(&mat9);
}

}

namespace d3d9 {
using namespace d3d;

void
drawAtomic(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if((geo->geoflags & Geometry::NATIVE) == 0)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;

	Frame *f = atomic->getFrame();
	f->updateLTM();
	Device->SetTransform(D3DTS_WORLD, (D3DMATRIX*)f->ltm);

	Device->SetStreamSource(0, (IDirect3DVertexBuffer9*)header->vertexStream[0].vertexBuffer,
	                        0, header->vertexStream[0].stride);
	Device->SetIndices((IDirect3DIndexBuffer9*)header->indexBuffer);
	Device->SetVertexDeclaration((IDirect3DVertexDeclaration9*)header->vertexDeclaration);

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		if(inst->material->texture)
			setTexture(inst->material->texture);
		else
			Device->SetTexture(0, NULL);
		setMaterial(inst->material);
		Device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(0xFF, 0x40, 0x40, 0x40));
		Device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		Device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		if(geo->geoflags & Geometry::PRELIT)
			Device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
		Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)header->primType, inst->baseIndex,
		                             0, inst->numVertices,
		                             inst->startIndex, inst->numPrimitives);
		inst++;
	}
}
}

namespace d3d8 {
using namespace d3d;

void
drawAtomic(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if((geo->geoflags & Geometry::NATIVE) == 0)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;

	Frame *f = atomic->getFrame();
	f->updateLTM();
	f->updateLTM();
	Device->SetTransform(D3DTS_WORLD, (D3DMATRIX*)f->ltm);

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		if(inst->material->texture)
			setTexture(inst->material->texture);
		else
			Device->SetTexture(0, NULL);
		setMaterial(inst->material);
		Device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(0xFF, 0x40, 0x40, 0x40));
		Device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		Device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		if(geo->geoflags & Geometry::PRELIT)
			Device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);

		Device->SetFVF(inst->vertexShader);
		Device->SetStreamSource(0, (IDirect3DVertexBuffer9*)inst->vertexBuffer, 0, inst->stride);
		Device->SetIndices((IDirect3DIndexBuffer9*)inst->indexBuffer);
		uint32 numPrim = inst->primType == D3DPT_TRIANGLESTRIP ? inst->numIndices-2 : inst->numIndices/3;
		Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)inst->primType, inst->baseIndex,
		                             0, inst->numVertices, 0, numPrim);
		inst++;
	}
}
}

}

rw::Clump *clump;
void (*renderCB)(rw::Atomic*) = NULL;

void
initrw(void)
{
	gta::attachPlugins();
	rw::d3d::registerNativeRaster();
	rw::loadTextures = 1;

	rw::currentTexDictionary = rw::TexDictionary::create();
	rw::Image::setSearchPath("D:\\rockstargames\\ps2\\gta3\\MODELS\\gta3_archive\\txd_extracted\\;"
	                         "D:\\rockstargames\\ps2\\gtavc\\MODELS\\gta3_archive\\txd_extracted\\;"
	                         "D:\\rockstargames\\ps2\\gtasa\\models\\gta3_archive\\txd_extracted\\");

	rw::platform = rw::PLATFORM_D3D9;
	rw::d3d::device = Device;

	if(1){
		char *filename = "D:\\rockstargames\\pc\\gtasa\\models\\gta3_archive\\admiral.txd";
//		char *filename = "D:\\rockstargames\\pc\\gtavc\\models\\gta3_archive\\admiral.txd";
		rw::StreamFile in;
		if(in.open(filename, "rb") == NULL){
			MessageBox(0, "couldn't open file\n", 0, 0);
			printf("couldn't open file\n");
		}
		rw::findChunk(&in, rw::ID_TEXDICTIONARY, NULL, NULL);
		rw::TexDictionary *txd;
		txd = rw::TexDictionary::streamRead(&in);
		assert(txd);
		in.close();
		rw::currentTexDictionary = txd;
	}

//	char *filename = "D:\\rockstargames\\pc\\gtavc\\models\\gta3_archive\\admiral.dff";
//	char *filename = "D:\\rockstargames\\pc\\gta3\\models\\gta3_archive\\kuruma.dff";
//	char *filename = "D:\\rockstargames\\pc\\gtavc\\models\\gta3_archive\\player.dff";
//	char *filename = "D:\\rockstargames\\pc\\gtavc\\models\\gta3_archive\\od_newscafe_dy.dff";
	char *filename = "D:\\rockstargames\\pc\\gtasa\\models\\gta3_archive\\admiral.dff";
//	char *filename = "D:\\rockstargames\\pc\\gtasa\\models\\gta3_archive\\lae2_roads89.dff";
//	char *filename = "D:\\rockstargames\\pc\\gtasa\\models\\gta3_archive\\casinoblock41_nt.dff";
//	char *filename = "D:\\rockstargames\\pc\\gtasa\\models\\cutscene_archive\\csremington92.dff";
//	char *filename = "C:\\gtasa\\test\\hanger.dff";
//	char *filename = "C:\\Users\\aap\\Desktop\\tmp\\out.dff";
//	char *filename = "out2.dff";
//	char *filename = "C:\\Users\\aap\\src\\librw\\tools\\insttest\\out.dff";
	rw::StreamFile in;
	if(in.open(filename, "rb") == NULL){
		MessageBox(0, "couldn't open file\n", 0, 0);
		printf("couldn't open file\n");
	}
	rw::findChunk(&in, rw::ID_CLUMP, NULL, NULL);
	clump = rw::Clump::streamRead(&in);
	assert(clump);
	in.close();

	FORLIST(lnk, clump->atomics){
		rw::Atomic *a = rw::Atomic::fromClump(lnk);
		if(a->pipeline && a->pipeline->platform == rw::PLATFORM_PS2)
			a->pipeline = NULL;
		a->getPipeline()->instance(a);
	}
	if(rw::platform == rw::PLATFORM_D3D8)
		renderCB = rw::d3d8::drawAtomic;
	else if(rw::platform == rw::PLATFORM_D3D9)
		renderCB = rw::d3d9::drawAtomic;

	rw::StreamFile out;
	//out.open("out.dff", "wb");
	//clump->streamWrite(&out);
	//out.close();

	out.open("out.txd", "wb");
	rw::currentTexDictionary->streamWrite(&out);
	out.close();
}

bool
Setup()
{
	D3DLIGHT9 light;
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Diffuse =  { 0.8f, 0.8f, 0.8f, 1.0f };
	light.Specular = { 0.0f, 0.0f, 0.0f, 0.0f };
	light.Ambient =  { 0.0f, 0.0f, 0.0f, 0.0f };
	light.Position = { 0.0f, 0.0f, 0.0f };
	light.Direction = { 0.0f, 0.0f, -1.0f };
	light.Range = 0.0f;
	light.Falloff = 0.0f;
	light.Attenuation0 = 0.0f;
	light.Attenuation1 = 0.0f;
	light.Attenuation2 = 0.0f;
	light.Theta = 0.0f;
	light.Phi = 0.0f;

	initrw();

	Device->SetRenderState(D3DRS_LIGHTING, true);
	Device->SetLight(0, &light);
	Device->LightEnable(0, 1);

	Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	Device->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
	Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	camera = new Camera;
	camera->setAspectRatio(640.0f/480.0f);
	camera->setNearFar(0.1f, 450.0f);
	camera->setTarget(Vec3(0.0f, 0.0f, 0.0f));
//	camera->setPosition(Vec3(0.0f, 5.0f, 0.0f));
//	camera->setPosition(Vec3(0.0f, -70.0f, 0.0f));
	camera->setPosition(Vec3(0.0f, -10.0f, 0.0f));
//	camera->setPosition(Vec3(0.0f, -1.0f, 3.0f));

	return true;
}

void
Cleanup()
{
}

bool
Display(float timeDelta)
{
	if(Device == NULL)
		return true;
	
	Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
	              0xff808080, 1.0f, 0);
	Device->BeginScene();

	camera->look();
	Device->SetTransform(D3DTS_VIEW, (D3DMATRIX*)camera->viewMat.cr);
	Device->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)camera->projMat.cr);

	FORLIST(lnk, clump->atomics){
		rw::Atomic *atomic = rw::Atomic::fromClump(lnk);
		char *name = PLUGINOFFSET(char, atomic->getFrame(),
		                          gta::nodeNameOffset);
		if(strstr(name, "_dam") || strstr(name, "_vlo"))
			continue;
		renderCB(atomic);
	}

	Device->EndScene();

	Device->Present(0, 0, 0, 0);
	return true;
}

LRESULT CALLBACK
d3d::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg){
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		switch(wParam){
		case 'W':
			camera->orbit(0.0f, 0.1f);
			break;
		case 'S':
			camera->orbit(0.0f, -0.1f);
			break;
		case 'A':
			camera->orbit(-0.1f, 0.0f);
			break;
		case 'D':
			camera->orbit(0.1f, 0.0f);
			break;
		case 'R':
			camera->zoom(0.1f);
			break;
		case 'F':
			camera->zoom(-0.1f);
			break;
		case VK_ESCAPE:
			DestroyWindow(hwnd);
			break;
		}
		break;
	case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI
WinMain(HINSTANCE hinstance, HINSTANCE prevInstance,
        PSTR cmdLine, int showCmd)
{
/*	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);*/

	if(!d3d::InitD3D(hinstance, 640, 480, true, D3DDEVTYPE_HAL, &Device)){
		MessageBox(0, "InitD3D() - FAILED", 0, 0);
		return 0;
	}
		
	if(!Setup()){
		MessageBox(0, "Setup() - FAILED", 0, 0);
		return 0;
	}

	d3d::EnterMsgLoop(Display);

	Cleanup();

	Device->Release();

	return 0;
}