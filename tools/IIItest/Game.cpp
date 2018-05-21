#include "III.h"

#include "Test.h"

eLevelName CGame::currLevel;

void
CGame::InitialiseRW(void)
{
	CTxdStore::Initialize();
	CVisibilityPlugins::Initialise();
	Scene.camera = CameraCreate(640, 480, 1);
	Scene.camera->setFarPlane(2000.0f);
	Scene.camera->setNearPlane(0.9f);
	TheCamera.m_rwcam = Scene.camera;
	TheCamera.m_aspectRatio = (float)globals.width/globals.height;
//	TheCamera.m_target.set(0.0f, 0.0f, 0.0f);
//	TheCamera.m_position.set(-100.0f, -100.0f, 50.0f);
	TheCamera.m_target.set(1155.0f, -190.0f, -18.0f);
	TheCamera.m_position.set(1286.0f, -211.0f, 50.0f);

	Scene.world = rw::World::create();
	Scene.world->addCamera(Scene.camera);

	// DEBUG
	debugCamState = *Scene.camera;

	LightsCreate(Scene.world);
}

int gameTxdSlot;

void
CGame::Initialise(void)
{
	CPools::Initialise();	// in CWorld::Initialise on PS2

	CGame::currLevel = LEVEL_NONE;	// INDUSTRIAL gives problems :/

	printf("--Loading generic textures\n");
	gameTxdSlot = CTxdStore::AddTxdSlot("generic");
	CTxdStore::Create(gameTxdSlot);
	CTxdStore::AddRef(gameTxdSlot);

	printf("--Loading particles\n");
	int part = CTxdStore::AddTxdSlot("particle");
	CTxdStore::LoadTxd(part, "MODELS/PARTICLE.TXD");
	CTxdStore::AddRef(part);
	CTxdStore::SetCurrentTxd(gameTxdSlot);

	printf("--Setup game variables\n");
	CPathFind::AllocatePathFindInfoMem(PATHNODESIZE);
	CWeather::Init();
	CCullZones::Init();
	CTheZones::Init();

	cHandlingDataMgr::Initialise();
	CAnimManager::LoadAnimGroups();	// not in III

	InitModelIndices();
	CModelInfo::Initialise();
	CPedStats::Initialise();

	CdStream::addImage("MODELS\\GTA3.IMG");
	CFileLoader::LoadLevel("DATA\\DEFAULT.DAT");
	CFileLoader::LoadLevel("DATA\\GTA3.DAT");
	CTheZones::PostZoneCreation();

	printf("--Setup water\n");
	CWaterLevel::Initialise("DATA\\WATER.DAT");	// file is unused

	CTimeCycle::Initialise();

	printf("--Setup Streaming\n");
	CStreaming::Init();
	CStreaming::RequestInitialVehicles();
	CStreaming::RequestInitialPeds();
	CStreaming::RequestBigBuildings(LEVEL_NONE);
	CStreaming::LoadAllRequestedModels();

	printf("--Setup game variables\n");
	CTimer::Initialise();
	CClock::Initialise(1000);

	printf("--Load scene\n");
	CCollision::ms_collisionInMemory = CGame::currLevel;

	///
	CTest::Init();
}

void
CGame::Process(void)
{
	CPad::UpdatePads();

	CPad *pad = CPad::GetPad(0);
	if(CPad::IsKeyDown('Q') || CPad::IsKeyDown(KEY_ESC) ||
	   pad->NewState.start && pad->NewState.select){
		isRunning = 0;
		return;
	}
	CStreaming::LoadAllRequestedModels();

	if(!CTimer::m_UserPause && !CTimer::m_CodePause){
		CTheZones::Update();
		CClock::Update();
		CWeather::Update();
		CCollision::Update();
		CTimeCycle::Update();
		TheCamera.Process();

		CTest::Update();
	}
}
