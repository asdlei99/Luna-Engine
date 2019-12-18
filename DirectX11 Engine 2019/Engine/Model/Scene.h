#pragma once

#include <array>
#include <vector>

// Assimp
#include "Vendor/Assimp/Importer.hpp"
#include "Vendor/Assimp/scene.h"
#include "Vendor/Assimp/postprocess.h"

// Types
#include <DirectXMath.h>
#include "Engine Includes/Types.h"
#include "Other/FloatTypeMath.h"

// ECS
#include "ECS/ECSComponent.hpp"
#include "ECS/ECSSystem.hpp"
#include "ECS/ECS.hpp"

// Other
#include "Engine/Utility/Utils.h"
#include "Engine/RendererFlags.h"
#include "Engine/ScopedMapper.h"
#include "Other/FileSystem.h"
#include "Engine/States/PipelineState.h"

// Components
#include "Components/Components.h"

struct CameraData {
    CameraComponent    *cCam;
    TransformComponent *cTransf;

    inline void SetView(mfloat4x4 mView) {
        cCam->mView = mView;
    }
    
    inline void SetProj(mfloat4x4 mProj) {
        cCam->mProj = mProj;
    }

    void UpdateCB(ConstantBuffer* cb) {
        // Copy data to CB
        ScopeMapConstantBufferCopy<CameraBuff> map(cb, (void*)&cCam->mView);
    }

    void BuildView() {
        using namespace DirectX;

        // 
        mfloat3 EyePos = { cTransf->vPosition.x, cTransf->vPosition.y, cTransf->vPosition.z },
                Focus  = { 0.f, 0.f, 1.f },
                Up     = { 0.f, 1.f, 0.f };

        // Set the yaw (Y axis), pitch (X axis), and roll (Z axis) rotations in radians.
        float pitch = cTransf->vRotation.x * 0.0174532925f;
        float yaw   = cTransf->vRotation.y * 0.0174532925f;
        float roll  = cTransf->vRotation.z * 0.0174532925f;

        // Create the rotation matrix from the yaw, pitch, and roll values.
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

        // Transform the lookAt and up vector by the rotation matrix so the view is correctly rotated at the origin.
        mfloat3 lookAt = XMVector3TransformCoord(Focus, rotationMatrix);
        mfloat3 up = XMVector3TransformCoord(Up, rotationMatrix);

        // Translate the rotated camera position to the location of the viewer.
        lookAt = EyePos + lookAt;

        // Build view matrix
        cCam->mView = XMMatrixLookAtLH(EyePos, lookAt, Up);
    }

    void BuildProj() {
        using namespace DirectX;

        if( cCam->bOrtho ) {
            cCam->mProj = XMMatrixOrthographicOffCenterLH(0.f, cCam->fWidth, cCam->fHeight, 0.f, cCam->fNear, cCam->fFar);
        } else {
            cCam->mProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(cCam->fFOV_X / cCam->fAspect), 
                                                   cCam->fAspect, cCam->fFar, cCam->fNear);
        }
    }

    inline void Build() {
        BuildView();
        BuildProj();
    }

    inline void ViewIdentity() { SetView(DirectX::XMMatrixIdentity()); }
    inline void ProjIdentity() { SetProj(DirectX::XMMatrixIdentity()); }
};

class AnimatedMeshRenderSystem: public BaseECSSystem {
public:
    AnimatedMeshRenderSystem(): BaseECSSystem() {
        AddComponentType(TransformComponent::_ID); // 0
        AddComponentType(MeshAnimatedComponent::_ID); // 1
    }

    void UpdateComponents(float dt, BaseECSComponent** comp) override {
        TransformComponent* transform = (TransformComponent*)comp[0];
        MeshAnimatedComponent* mesh   = (MeshAnimatedComponent*)comp[1];

        uint32_t flags = ieee_uint32(dt);


    }

} *gAnimatedMeshRenderSystem;

class VelocityIntegrationSystem: public BaseECSSystem {
public:
    VelocityIntegrationSystem();

    void UpdateComponents(float dt, BaseECSComponent** comp) override;

} *gVelocityIntegrationSystem;

class StaticMeshRenderSystem: public BaseECSSystem {
public:
    StaticMeshRenderSystem();

    void UpdateComponents(float dt, BaseECSComponent** comp) override;
} *gStaticMeshRenderSystem;

/*class StaticMeshRenderSystem: public BaseECSSystem {
public:
    StaticMeshRenderSystem(): BaseECSSystem() {
        AddComponentType(TransformComponent::_ID);  // 0
        AddComponentType(MeshStaticComponent::_ID); // 1
    }

    void UpdateComponents(float dt, BaseECSComponent** comp) override {
        TransformComponent* transform = (TransformComponent*)comp[0];
        MeshStaticComponent* mesh     = (MeshStaticComponent*)comp[1];

        uint32_t flags = ieee_uint32(dt);
        uint32_t low = (flags >> 24);

        uint32_t slot           = low >> Shader::Count;
        Shader::ShaderType type = Shader::ShaderType(low & ((1 << Shader::Count) - 1));
        //(type | (slot << Shader::Count));

        mesh->Bind();
        //transform->Build();
        transform->Bind(Scene::Current()->cbTransform, Shader::Vertex, 0u);

        gDirectX->gContext->DrawIndexed(mesh->mIndexBuffer.GetNumber(), 0, 0);
    }

} *gStaticMeshRenderSystem;*/

/*class VelocityIntegrationSystem: public BaseECSSystem {
public:
    VelocityIntegrationSystem() {
        AddComponentType(TransformComponent::_ID); // 0
        AddComponentType(VelocityComponent::_ID);  // 1
    }

    void UpdateComponents(float dt, BaseECSComponent** comp) override {
        TransformComponent* transform = (TransformComponent*)comp[0];
        VelocityComponent* velocity   = (VelocityComponent*)comp[1];

        // TODO: Fix +=
        transform->vPosition += velocity->vDirection * velocity->fVelocity * dt;
        velocity->fVelocity  += velocity->fAcceleration * dt;

        velocity->fAcceleration *= .9f;
        velocity->fVelocity     *= .9f;
    }

} *gVelocityIntegrationSystem;*/

typedef std::vector<EntityHandle> EntityHandleList;

#define SCENE_MAX_CAMERA_COUNT 8
class Scene: public PipelineState<Scene> {
private:
    friend StaticMeshRenderSystem;

    EntityHandleList mOpaque{};
    EntityHandleList mCubemaps{};
    EntityHandleList mTransparent{};

    //std::vector<EntityHandle> mParticleEmitters;
    //std::vector<EntityHandle> mParticleSystems;

    std::array<EntityHandle, SCENE_MAX_CAMERA_COUNT> mCamera{};

    ECSSystemList mRenderOpaqueList{}, mRenderCubemapList{}, mRenderTransparentList{};
    ECSSystemList mUpdateList{};

    ECS mECS{};

    std::array<CameraData*, SCENE_MAX_CAMERA_COUNT> mCameraData{};
    std::array<ConstantBuffer*, SCENE_MAX_CAMERA_COUNT> cbCameraData{};
    ConstantBuffer* cbTransform = nullptr;
    ConstantBuffer* cbMaterial = nullptr;

    uint32_t mMainCamera{};
    uint32_t bUpdatedCameraLists{};
    uint32_t bIsTransparentPass{};

    EntityHandleList LoadModelExternalStatic(const char* fname) {
        //ScopedFileAccessRM file(widen(fname).c_str());

        MaterialComponent mat{};
        mat._UseVertexColor = false;
        mat._FlipNormals    = false; // !!!!!!!!!!!!!!!!!!!MUST TODO!!!!!!!!!!!!!!!!!!!!!!!!
        mat._IsTransparent  = false; // Remove StaticOpaque...System and merge them into single one
                                     // Set flag for Opaque pass
                                     //          for Transparent pass
                                     // And compare those values
                                     // It's simply better   
        mat._Alpha = 1.f;
        mat._AlbedoMul = 1.f;
        mat._NormalMul = 1.f;
        mat._MetallnessMul = 1.f;
        mat._RoughnessMul = 1.f;
        mat._AmbientOcclusionMul = 1.f;
        mat._EmissionMul = 1.f;

        TransformComponent transform;
        transform.mWorld = DirectX::XMMatrixIdentity();
        transform.vPosition = {};
        transform.vRotation = {};
        transform.vScale = { 1.f, 1.f, 1.f };

        // Load model
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(fname, aiProcess_Triangulate | aiProcess_CalcTangentSpace
                                                      | aiProcess_GenSmoothNormals);

        if( !scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode ) {
            std::cout << "[Scene::LoadModelExternal]: Can't load model! (" << fname << ")" << std::endl;
            return {  };
        }

        // Process scene
        std::vector<MeshStaticComponent> mMeshList;
        ProcessNodeStatic(scene->mRootNode, scene, &mMeshList);

        // Some error occured
        /*if( file.berror() ) return NULL_HANDLE;

        size_t sz;
        char *line = new char[1024];
        float x, y, z;
        uint32_t i0, i1, i2,  i3, i4, i5,  i6, i7, i8;

        line = file.getline(sz);

        enum NowReading {
            rVertex, 
            rUV, 
            rNormal, 

            rIndex
        } rState;

        while( sz && !file.is_eof() ) {
            //printf_s("[OBJ]: %s\n", line);

            // Remove comments
            //uint32_t p = ; // 

            // 
            sscanf_s(line, "v %f %f %f", &x, &y, &z);
            printf_s("v %f %f %f\n", x, y, z);

            // Read next line
            line = file.getline(sz);
        }

        // WIP
        delete[] line;
        return NULL_HANDLE;*/

        // Done
        if( mMeshList.size() == 1 ) {
            return { mECS.MakeEntity(transform, mMeshList[0], mat) };
        }

        // Return list of entites
        EntityHandleList list;
        list.reserve(mMeshList.size());
        for( auto e : mMeshList ) {
            list.push_back(mECS.MakeEntity(transform, e, mat));
        }

        // Done
        return list;
    }

    void ProcessNodeStatic(aiNode* node, const aiScene* scene, std::vector<MeshStaticComponent>* MeshList) {
        // Process meshes
        for( size_t i = 0; i < node->mNumMeshes; i++ ) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            MeshList->push_back(ProcessMeshStatic(mesh, scene));
        }

        // Process children
        for( size_t i = 0; i < node->mNumChildren; i++ ) {
            ProcessNodeStatic(node->mChildren[i], scene, MeshList);
        }
    }

    MeshStaticComponent ProcessMeshStatic(aiMesh* inMesh, const aiScene* scene) {
        MeshStaticComponent mesh;

        // 
        std::vector<float3> Position;
        std::vector<float2> Texcoord;
        std::vector<float3> Normal;

        std::vector<uint32_t> Index;
        
        // Process vertices
        Position.reserve(inMesh->mNumVertices);
        Texcoord.reserve(inMesh->mNumVertices);
        Normal.reserve(  inMesh->mNumVertices);

        for( size_t i = 0; i < inMesh->mNumVertices; i++ ) {
            Position.push_back({ inMesh->mVertices[i].x, inMesh->mVertices[i].y, inMesh->mVertices[i].z });
            Normal  .push_back({ inMesh->mNormals [i].x, inMesh->mNormals [i].y, inMesh->mNormals [i].z });

            if( inMesh->mTextureCoords[0] ) {
                Texcoord.push_back({ inMesh->mTextureCoords[0][i].x, inMesh->mTextureCoords[0][i].y });
            } else {
                Texcoord.push_back({ 0, 0 });
            }
        }

        // Process indices
        uint32_t IndexNum = 0;
        Index.reserve(inMesh->mNumFaces * 3); // Assume that each face has at least 3 indices
        for( size_t i = 0; i < inMesh->mNumFaces; i++ ) {
            aiFace face = inMesh->mFaces[i];
            IndexNum += face.mNumIndices;

            for( size_t j = 0; j < face.mNumIndices; j++ ) {
                Index.push_back(face.mIndices[j]);
            }
        }

        // Create buffers
        mesh.mIndexBuffer = new IndexBuffer();
        mesh.mVBPosition  = new VertexBuffer();
        mesh.mVBTexcoord  = new VertexBuffer();
        mesh.mVBNormal    = new VertexBuffer();

        mesh.mVBPosition->CreateDefault(Position.size(), sizeof(float3), &Position[0]);
        mesh.mVBTexcoord->CreateDefault(Texcoord.size(), sizeof(float2), &Texcoord[0]);
        mesh.mVBNormal->CreateDefault(  Normal  .size(), sizeof(float3), &Normal  [0]);

        mesh.mIndexBuffer->CreateDefault(IndexNum, &Index[0]);
        
        // Done
        return mesh;
    }

public:
    Scene() {
        char buff[128]{};
        bUpdatedCameraLists = false;

        for( uint32_t i = 0; i < SCENE_MAX_CAMERA_COUNT; i++ ) {
            //SecureZeroMemory(&buff[0], 128);
            sprintf_s(&buff[0], 128, "[Scene::Camera]: Constant Buffer #%u", i);

            cbCameraData[i] = new ConstantBuffer();
            cbCameraData[i]->CreateDefault(sizeof(CameraBuff));
            cbCameraData[i]->SetName(&buff[0]);

            mCameraData[i] = new CameraData();
        }

        cbMaterial = new ConstantBuffer();
        cbMaterial->CreateDefault(sizeof(MaterialBuff));
        cbMaterial->SetName("[Scene::Material]: ConstantBuffer");

        cbTransform = new ConstantBuffer();
        cbTransform->CreateDefault(sizeof(TransformBuff));
        cbTransform->SetName("[Scene::MeshTransform]: Constant Buffer");

        gVelocityIntegrationSystem = new VelocityIntegrationSystem;
        gAnimatedMeshRenderSystem  = new AnimatedMeshRenderSystem;
        gStaticMeshRenderSystem    = new StaticMeshRenderSystem;

        // 
        ResetLists();
    }

    ~Scene() {
        for( auto cb : cbCameraData )
            if( cb ) {
                cb->Release();
                delete cb;
            }

        for( auto cd : mCameraData )
            if( cd ) delete cd;

        //SAFE_RELEASE_N(cbCameraData, SCENE_MAX_CAMERA_COUNT);
        SAFE_RELEASE(cbTransform);

        //SAFE_DELETE_N(mCameraData, SCENE_MAX_CAMERA_COUNT);
        SAFE_DELETE(gVelocityIntegrationSystem);
        SAFE_DELETE(gAnimatedMeshRenderSystem );
        SAFE_DELETE(gStaticMeshRenderSystem   );

        // ECS
        for( uint32_t i = 0; i < SCENE_MAX_CAMERA_COUNT; i++ ) 
            mECS.RemoveEntity(mCamera[i]);

        auto ReleaseMesh = [&](EntityHandle e, std::string_view name) {
            auto static_mesh = GetComponent<MeshStaticComponent>(e);
            auto anim_mesh   = GetComponent<MeshAnimatedComponent>(e);
            //auto // TODO: Release material

            // Unload mesh
            if( static_mesh ) {
                SAFE_RELEASE(static_mesh->mIndexBuffer);

                SAFE_RELEASE(static_mesh->mVBPosition);
                SAFE_RELEASE(static_mesh->mVBTexcoord);
                SAFE_RELEASE(static_mesh->mVBNormal);
            } else if( anim_mesh ) {
                SAFE_RELEASE(anim_mesh->mIndexBuffer);

                SAFE_RELEASE(anim_mesh->mVBPosition);
                SAFE_RELEASE(anim_mesh->mVBTexcoord);
                SAFE_RELEASE(anim_mesh->mVBNormal);

                SAFE_RELEASE(anim_mesh->mVBWeights);
                SAFE_RELEASE(anim_mesh->mVBJoints);
            } else {
                printf_s("[Scene::~Scene]: Error occured during unloading %s mesh %u\n", name, e);
            }

            mECS.RemoveEntity(e);
        };

        for( auto e : mOpaque ) ReleaseMesh(e, "opaque");
        for( auto e : mCubemaps ) ReleaseMesh(e, "cubemap");
        for( auto e : mTransparent ) ReleaseMesh(e, "transparent");

        mOpaque.clear();
        mCubemaps.clear();
        mTransparent.clear();

        mRenderOpaqueList.Clear();
        mRenderCubemapList.Clear();
        mRenderTransparentList.Clear();

        mUpdateList.Clear();
    }

    bool IsTransparentPass() const { return bIsTransparentPass; };

    // Set current scene as active
    // Everything will refere to active scene
    // Renderer, Update events, etc...
    inline void SetAsActive() { gState = this; }

    // Get component from local ECS
    template<typename T>
    T* GetComponent(EntityHandle handle) { return mECS.GetComponent<T>(handle); }

    EntityHandleList Instantiate(EntityHandleList list) {
        EntityHandleList new_list;
        new_list.reserve(list.size());

        for( EntityHandle e : list ) {
            // Copy components & instantiate new Entity
            MeshStaticComponent* static_mesh = GetComponent<MeshStaticComponent>(e);
            MeshAnimatedComponent* anim_mesh = GetComponent<MeshAnimatedComponent>(e);
            TransformComponent transf       = *GetComponent<TransformComponent>(e);
            MaterialComponent mat           = *GetComponent<MaterialComponent>(e);

            if( static_mesh != NULL_HANDLE ) new_list.push_back(mECS.MakeEntity(transf, *static_mesh, mat));
            else if( anim_mesh != NULL_HANDLE ) new_list.push_back(mECS.MakeEntity(transf, *anim_mesh, mat));
        }

        return new_list;
    }

    // TODO: 
    uint32_t AddOpaqueStaticInstance(EntityHandle handle, mfloat4x4 mWorld) {
        return 0; // Instance id
    }

    void AddOpaque(EntityHandleList list) {
        mOpaque.resize(list.size() + list.size());

        for( auto e : list ) 
            mOpaque.push_back(e);
    }
    
    void AddCubemaps(EntityHandleList list) {
        mCubemaps.resize(mCubemaps.size() + list.size());

        for( auto e : list )
            mCubemaps.push_back(e);
    }

    void AddTransparent(EntityHandleList list) {
        mTransparent.resize(mTransparent.size() + list.size());

        for( auto e : list ) {
            GetComponent<MaterialComponent>(e)->_IsTransparent = true;
            mTransparent.push_back(e);
        }
    }

    EntityHandleList LoadModelStatic(const char* fname) {
        std::string_view ext = path_ext(fname);

        if( ext == "obj" || ext == "dae" || ext == "fbx" ) {
            return LoadModelExternalStatic(fname);
        }

        // Other file types
        printf_s("[Scene::LoadModel]: Unsupported model format %s", ext);
        return {};
    }

    EntityHandleList LoadModelStaticOpaque(const char* fname) {
        EntityHandleList list = LoadModelStatic(fname);
        AddOpaque(list);
        return list;
    }

    EntityHandleList LoadModelStaticTransparent(const char* fname) {
        EntityHandleList list = LoadModelStatic(fname);
        AddTransparent(list);
        return list;

    }

    inline ECS* GetECS() { return &mECS; };

    void ClearRenderLists() {
        mRenderOpaqueList.Clear();
        mRenderCubemapList.Clear();
        mRenderTransparentList.Clear();

        mUpdateList.Clear();
    }

    void ClearUpdateList() {
        mUpdateList.Clear();
    }

    // Run only once to reset all system lists
    void ResetLists() {
        ClearRenderLists();
        ClearUpdateList();

        //mRenderCubemapList.;
        mRenderOpaqueList.AddSystem(*gAnimatedMeshRenderSystem);
        mRenderOpaqueList.AddSystem(*gStaticMeshRenderSystem);

        mRenderTransparentList.AddSystem(*gAnimatedMeshRenderSystem);
        mRenderTransparentList.AddSystem(*gStaticMeshRenderSystem);

        mUpdateList.AddSystem(*gVelocityIntegrationSystem);
    }

    // Use GetECS() to get ECS context & then to create camera you self
    void AddCamera(uint32_t CameraIndex, EntityHandle entity) {
        if( mCamera[CameraIndex] != NULL_HANDLE ) mECS.RemoveEntity(mCamera[CameraIndex]);
        mCamera[CameraIndex] = entity;
    }

    void MakeCameraOrtho(uint32_t CameraIndex, float _near, float _far, float width, float height) {
        if( mCamera[CameraIndex] != NULL_HANDLE ) mECS.RemoveEntity(mCamera[CameraIndex]);

        TransformComponent transf;
        transf.vPosition = {};
        transf.vRotation = {};
        transf.mWorld = DirectX::XMMatrixIdentity();

        CameraComponent cam{};
        cam.bOrtho  = true;
        cam.fNear   = _near;
        cam.fFar    = _far;
        cam.fWidth  = width;
        cam.fHeight = height;

        mCamera[CameraIndex] = mECS.MakeEntity(transf, cam);
        UpdateCameraData(CameraIndex);
        mCameraData[CameraIndex]->Build();
        bUpdatedCameraLists = false;
    }

    void MakeCameraFOVH(uint32_t CameraIndex, float _near, float _far, float width, float height, float fovx) {
        if( mCamera[CameraIndex] != NULL_HANDLE ) mECS.RemoveEntity(mCamera[CameraIndex]);

        TransformComponent transf{};
        transf.vPosition = {};
        transf.vRotation = {};
        transf.mWorld = DirectX::XMMatrixIdentity();

        CameraComponent cam{};
        cam.bOrtho  = false;
        cam.fNear   = _near;
        cam.fFar    = _far;
        cam.fAspect = width / height;
        cam.fFOV_X  = fovx;
        cam.fFOV_Y  = 0.f;

        mCamera[CameraIndex] = mECS.MakeEntity(transf, cam);
        bUpdatedCameraLists = false;
    }

    void UpdateMadeCameras() {
        for( uint32_t i = 0; i < SCENE_MAX_CAMERA_COUNT; i++ ) {
            if( mCamera[i] != NULL_HANDLE ) {
                UpdateCameraData(i);
                mCameraData[i]->Build();
            }
        }

        bUpdatedCameraLists = true;
    }

    void SetActiveCamera(uint32_t i) { mMainCamera = i; }

    /*void AddCameras(std::initializer_list<EntityHandle> list) {
        mCameras.resize(mCameras.size() + list.size());


        for( auto e : list )
            mCameras.push_back(e);
    }*/

    /*void SetActiveCamera(uint32_t index) {

    }*/

    //void RemoveOpaque();
    //void RemoveCubemap();
    //void RemoveTransparent();
    
    // Try to switch to std::array
    void UpdateCameraData(uint32_t CameraIndex) {
        if( mCamera[CameraIndex] == NULL_HANDLE ) return;
        mCameraData[CameraIndex]->cCam    = GetComponent<CameraComponent>   (mCamera[CameraIndex]);
        mCameraData[CameraIndex]->cTransf = GetComponent<TransformComponent>(mCamera[CameraIndex]);
    }
    
    void BindCamera(uint32_t CameraIndex, uint32_t types=Shader::Vertex, uint32_t slot=1) {
        // To be sure
        if( !bUpdatedCameraLists ) UpdateMadeCameras();

        // Copy component refs to mCameraData from Camera entity
        UpdateCameraData(CameraIndex);

        // Copy data to CB
        mCameraData[CameraIndex]->UpdateCB(cbCameraData[CameraIndex]);

        // Bind CB
        cbCameraData[CameraIndex]->Bind(types, slot);
    }

    inline CameraData* GetCamera(uint32_t CameraIndex) const { return mCameraData[CameraIndex]; }

    void RenderCubemap(uint32_t flags=0, Shader::ShaderType type_transf=Shader::Vertex, Shader::ShaderType type_tex=Shader::Pixel) {
        flags |= type_transf << (32 - Shader::Count);
        flags |= type_tex    << (32 - Shader::Count * 2);

        bIsTransparentPass = false;
        mECS.UpdateSystems(mRenderCubemapList, ieee_float(flags));
    }

    void RenderOpaque(uint32_t flags=0, Shader::ShaderType type_transf=Shader::Vertex, Shader::ShaderType type_tex=Shader::Pixel) {
        flags |= type_transf << (32 - Shader::Count);
        flags |= type_tex    << (32 - Shader::Count * 2);

        bIsTransparentPass = false;
        mECS.UpdateSystems(mRenderOpaqueList, ieee_float(flags));
    }

    void RenderTransparent(uint32_t flags=0, Shader::ShaderType type_transf=Shader::Vertex, Shader::ShaderType type_tex=Shader::Pixel) {
        flags |= type_transf << (32 - Shader::Count);
        flags |= type_tex    << (32 - Shader::Count * 2);

        bIsTransparentPass = true;
        mECS.UpdateSystems(mRenderTransparentList, ieee_float(flags));
    }

    void Update(float dt) {
        mECS.UpdateSystems(mUpdateList, dt);
    }
};

///////////////////////////////// 
VelocityIntegrationSystem::VelocityIntegrationSystem(): BaseECSSystem() {
    AddComponentType(TransformComponent::_ID); // 0
    AddComponentType(VelocityComponent::_ID);  // 1
}

void VelocityIntegrationSystem::UpdateComponents(float dt, BaseECSComponent** comp) {
    TransformComponent* transform = (TransformComponent*)comp[0];
    VelocityComponent* velocity = (VelocityComponent*)comp[1];

    // TODO: Fix +=
    transform->vPosition += velocity->vDirection * velocity->fVelocity * dt;
    velocity->fVelocity += velocity->fAcceleration * dt;

    velocity->fAcceleration *= .9f;
    velocity->fVelocity *= .9f;
}

///////////////////////////////// Opaque
StaticMeshRenderSystem::StaticMeshRenderSystem(): BaseECSSystem() {
    AddComponentType(TransformComponent::_ID);  // 0
    AddComponentType(MeshStaticComponent::_ID); // 1
    AddComponentType(MaterialComponent::_ID);   // 2
}

void StaticMeshRenderSystem::UpdateComponents(float dt, BaseECSComponent** comp) {
    TransformComponent* transform = (TransformComponent*)comp[0];
    MeshStaticComponent* mesh     = (MeshStaticComponent*)comp[1];
    MaterialComponent* material   = (MaterialComponent*)comp[2];

    Scene* scene = Scene::Current();

    if( material->_IsTransparent != scene->IsTransparentPass() ) return;

    uint32_t flags = ieee_uint32(dt);
    Shader::ShaderType type = Shader::ShaderType(flags >> 25);
    Shader::ShaderType mat_type = Shader::ShaderType((flags >> 18) & 0x7F);

    // Bind mesh data
    mesh->Bind();
    transform->Build();
    transform->Bind(scene->cbTransform, type, 0);
    material->Bind(scene->cbMaterial, mat_type, 0, flags);
    // TODO: Make sorting by materials

    // Draw call1
    gDirectX->gContext->DrawIndexed(mesh->mIndexBuffer->GetNumber(), 0, 0);
}

/*class VelocityIntegrationSystem: public BaseECSSystem {
public:
    VelocityIntegrationSystem(): BaseECSSystem() {
        AddComponentType(TransformComponent::_ID); // 0
        AddComponentType(VelocityComponent::_ID);  // 1
    }

    void UpdateComponents(float dt, BaseECSComponent** comp) override {
        TransformComponent* transform = (TransformComponent*)comp[0];
        VelocityComponent* velocity = (VelocityComponent*)comp[1];

        // TODO: Fix +=
        transform->vPosition += velocity->vDirection * velocity->fVelocity * dt;
        velocity->fVelocity += velocity->fAcceleration * dt;

        velocity->fAcceleration *= .9f;
        velocity->fVelocity *= .9f;
    }

};

class StaticMeshRenderSystem: public BaseECSSystem {
public:
    StaticMeshRenderSystem(): BaseECSSystem() {
        AddComponentType(TransformComponent::_ID);  // 0
        AddComponentType(MeshStaticComponent::_ID); // 1
    }

    void UpdateComponents(float dt, BaseECSComponent** comp) override {
        TransformComponent* transform = (TransformComponent*)comp[0];
        MeshStaticComponent* mesh = (MeshStaticComponent*)comp[1];

        uint32_t flags = ieee_uint32(dt);
        uint32_t low = (flags >> 24);

        uint32_t slot = low >> Shader::Count;
        Shader::ShaderType type = Shader::ShaderType(low & ((1 << Shader::Count) - 1));
        //(type | (slot << Shader::Count));

        mesh->Bind();
        //transform->Build();
        transform->Bind(Scene::Current()->cbTransform, Shader::Vertex, 0u);

        gDirectX->gContext->DrawIndexed(mesh->mIndexBuffer.GetNumber(), 0, 0);
    }
};*/
