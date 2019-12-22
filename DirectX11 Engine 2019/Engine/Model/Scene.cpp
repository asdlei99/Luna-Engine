#include "Scene.h"

#undef _____________TYPE_FLOAT_OPERATORS22____3333
#include "Other/FloatTypeMath.h"

float fsignf(float x) { return (x < 0.f ? -1.f : 1.f); }

///////////////////////////////// 
VelocityIntegrationSystem::VelocityIntegrationSystem(): BaseECSSystem() {
    AddComponentType(TransformComponent::_ID); // 0
}

void VelocityIntegrationSystem::UpdateComponents(float dt, BaseECSComponent** comp) {
    TransformComponent* transform = (TransformComponent*)comp[0];

    // TODO: Fix +=
    transform->vPosition += transform->vDirection * transform->fVelocity * dt;
    transform->fVelocity += transform->fAcceleration * dt;

    transform->fAcceleration *= .9f;
    transform->fVelocity *= .9f;
}

///////////////////////////////// Opaque
StaticMeshRenderSystem::StaticMeshRenderSystem(): BaseECSSystem() {
    AddComponentType(TransformComponent::_ID);  // 0
    AddComponentType(MeshStaticComponent::_ID); // 1
    AddComponentType(MaterialComponent::_ID);   // 2
}

void StaticMeshRenderSystem::UpdateComponents(float dt, BaseECSComponent** comp) {
    TransformComponent* transform = (TransformComponent*)comp[0];
    MeshStaticComponent* mesh = (MeshStaticComponent*)comp[1];
    MaterialComponent* material = (MaterialComponent*)comp[2];

    Scene* scene = Scene::Current();

    // If it's transparency pass, but material is opaque, 
    // or material layer mask doesn't have at least one bit with current renderable material layer
    //      Skip rendering
    if( material->_IsTransparent != scene->IsTransparentPass()
       || (!(material->_MaterialLayer &  scene->GetEnabledMaterialLayers()) && material->_MaterialLayer) ) return;

    // Get flags
    uint32_t flags = ieee_uint32(dt);

    // If material can't cast shadows and currently we are rendering shadow map
    //      Then skip
    if( !material->_ShadowCaster && (flags & RendererFlags::ShadowPass) ) return;

    // Target shader types
    Shader::ShaderType type = Shader::ShaderType(flags >> 25);
    Shader::ShaderType mat_type = Shader::ShaderType((flags >> 18) & 0x7F);

    // Bind mesh data
    mesh->Bind();
    transform->Build();
    transform->Bind(scene->cbTransform, type, 0);
    material->Bind(scene->cbMaterial, mat_type, 0, flags);
    // TODO: Make sorting by materials

    // Draw call
    DXDrawIndexed(mesh->mIndexBuffer->GetNumber(), 0, 0);
}

MovementControlIntegrationSystem::MovementControlIntegrationSystem(): BaseECSSystem() {
    AddComponentType(TransformComponent::_ID);       // 0
    AddComponentType(MovementControlComponent::_ID); // 1
}

void MovementControlIntegrationSystem::UpdateComponents(float dt, BaseECSComponent** comp) {
    TransformComponent* Transform      = (TransformComponent*)comp[0];
    MovementControlComponent* Movement = (MovementControlComponent*)comp[1];

    using namespace LunaEngine;

    int index = 0;
    for( auto Control : Movement->mAssignedControls ) {
        float3 fValue = { 0.f, 0.f, 0.f };

        // Keyboard
        if( Control.bKeyboard ) {
            if( gKeyboard->IsDown(Control.mKeyboardKey) ) fValue = { 1.f, 1.f, 1.f };
        }

        // Gamepad
        if( Control.bGamepad ) {
            Gamepad* gp = gGamepad[Control.mGamepadIndex];

            if( gp->IsConnected() ) {
                if( Control.mGamepadButton == GamepadButtonState::ThumbstickL ) {
                    // Left shoulder
                    fValue = Math::max(fValue, gp->TriggerL());

                } else if( Control.mGamepadButton == GamepadButtonState::ThumbstickR ) {
                    // Right shoulder
                    fValue = Math::max(fValue, gp->TriggerR());

                } else if( Control.mGamepadButton < GamepadButtonState::_StickL ) {
                    // Button check
                    fValue = Math::max(fValue, gp->IsButtonDown(Control.mGamepadButton));

                } else if( Control.mGamepadButton == GamepadButtonState::_StickL ) {
                    // Left stick
                    if( !gp->IsDeadZoneL() ) {
                        float _x = gp->LeftX();
                        float _y = -gp->LeftY();

                        // If this control is in other direction
                        if( fsignf(_x) != fsignf(Control.fValue.x) ) _x = 0.f; // Reset value
                        if( fsignf(_y) != fsignf(Control.fValue.y) ) _y = 0.f;

                        // 
                        fValue = Math::max(fValue, float3(fabsf(_x), fabsf(_y), 0.f));
                    }
                } else if( Control.mGamepadButton == GamepadButtonState::_StickR ) {
                    // Right stick
                    if( !gp->IsDeadZoneR() ) {
                        float _x = gp->RightX();
                        float _y = -gp->RightY();

                        // If this control is in other direction
                        if( fsignf(_x) != fsignf(Control.fValue.x) ) _x = 0.f; // Reset value
                        if( fsignf(_y) != fsignf(Control.fValue.y) ) _y = 0.f;

                        // 
                        fValue = Math::max(fValue, float3(fabsf(_x), fabsf(_y), 0.f));
                    }
                }
            }
        }

        // Mouse
        if( Control.bMouse ) {
            if( gMouse->IsDown(Control.mMouseButton) ) fValue = { 1.f, 1.f, 1.f };
        }

        // 
        if( Control.bCallback ) {
            if( Math::length2(fValue) > 0.f ) Control.mCallback(Transform, dt);
        } else {
            if( Control.bDirectional ) {
                float3 vel = Transform->vDirection * Transform->fVelocity;
                fValue *= float3(fsignf(vel.x), fsignf(vel.y), fsignf(vel.z));
            }

            // Is camera
            if( Control.bOrientationDependent ) {
                using namespace DirectX;

                float3 r = Transform->vRotation;
                float3 p = Transform->vPosition;

                // Get angles in radians
                float pr = XMConvertToRadians(r.x);
                float yr = XMConvertToRadians(r.y);
                float qr = yr - XMConvertToRadians(90.f * fsignf(p.z));

                // Move the direction we looking at
                float3 q = float3(
                    // X
                    (p.x * sinf(yr) + p.z * cosf(yr)) * cosf(pr),

                    // Y
                    -p.x * sinf(pr),

                    // Z
                    (p.x * cosf(yr) - p.z * sinf(yr)) * cosf(pr)
                );

                p += q;
            } else {
                float3 dv = fValue * Control.fValue * dt;
                Transform->fVelocity += Math::length(dv);
                Transform->vDirection += Math::normalize(dv);
            }
        }
    }
}
