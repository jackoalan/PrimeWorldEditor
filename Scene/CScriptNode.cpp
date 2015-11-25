#include "CScriptNode.h"
#include <gtx/quaternion.hpp>
#include <Common/AnimUtil.h>
#include <Common/Math.h>
#include <Core/CDrawUtil.h>
#include <Core/CGraphics.h>
#include <Core/CRenderer.h>
#include <Core/CResCache.h>
#include <Core/CSceneManager.h>
#include <Resource/script/CMasterTemplate.h>

CScriptNode::CScriptNode(CSceneManager *pScene, CSceneNode *pParent, CScriptObject *pObject)
    : CSceneNode(pScene, pParent)
{
    mpVolumePreviewNode = nullptr;

    // Evaluate instance
    mpInstance = pObject;
    mpActiveModel = nullptr;
    mpBillboard = nullptr;
    mpCollisionNode = new CCollisionNode(pScene, this);
    mpCollisionNode->SetInheritance(true, true, false);

    if (mpInstance)
    {
        CScriptTemplate *pTemp = mpInstance->Template();

        // Determine transform
        mPosition = mpInstance->Position();
        mRotation = CQuaternion::FromEuler(mpInstance->Rotation());
        SetName("[" + pTemp->TemplateName(mpInstance->NumProperties()) + "] " + mpInstance->InstanceName());

        if (pTemp->ScaleType() == CScriptTemplate::eScaleEnabled)
            mScale = mpInstance->Scale();

        MarkTransformChanged();

        // Determine display assets
        mpActiveModel = mpInstance->GetDisplayModel();
        mModelToken = CToken(mpActiveModel);

        mpBillboard = mpInstance->GetBillboard();
        mBillboardToken = CToken(mpBillboard);

        mpCollisionNode->SetCollision(mpInstance->GetCollision());

        // Create preview volume node
        mHasValidPosition = pTemp->HasPosition();
        mHasVolumePreview = (pTemp->ScaleType() == CScriptTemplate::eScaleVolume);

        if (mHasVolumePreview)
        {
            EVolumeShape shape = mpInstance->VolumeShape();
            CModel *pVolumeModel = nullptr;

            if ((shape == eAxisAlignedBoxShape) || (shape == eBoxShape))
                pVolumeModel = (CModel*) gResCache.GetResource("../resources/VolumeBox.cmdl");

            else if (shape == eEllipsoidShape)
                pVolumeModel = (CModel*) gResCache.GetResource("../resources/VolumeSphere.cmdl");

            else if (shape == eCylinderShape)
                pVolumeModel = (CModel*) gResCache.GetResource("../resources/VolumeCylinder.cmdl");

            else if (shape == eCylinderLargeShape)
                pVolumeModel = (CModel*) gResCache.GetResource("../resources/VolumeCylinderLarge.cmdl");

            if (pVolumeModel)
            {
                mpVolumePreviewNode = new CModelNode(pScene, this, pVolumeModel);
                mpVolumePreviewNode->SetInheritance(true, (shape != eAxisAlignedBoxShape), false);
                mpVolumePreviewNode->Scale(mpInstance->Scale());
                mpVolumePreviewNode->ForceAlphaEnabled(true);
            }
        }

        // Fetch LightParameters
        mpLightParameters = new CLightParameters(mpInstance->LightParameters(), mpInstance->MasterTemplate()->GetGame());
        SetLightLayerIndex(mpLightParameters->LightLayerIndex());
    }

    else
    {
        // Shouldn't ever happen
        SetName("ScriptNode - NO INSTANCE");
    }

    if (mpActiveModel)
        mLocalAABox = mpActiveModel->AABox();
    else
        mLocalAABox = CAABox::skOne;
}

ENodeType CScriptNode::NodeType()
{
    return eScriptNode;
}

TString CScriptNode::PrefixedName() const
{
    return "[" + mpInstance->Template()->TemplateName() + "] " + mpInstance->InstanceName();
}

void CScriptNode::AddToRenderer(CRenderer *pRenderer, const SViewInfo& ViewInfo)
{
    if (!mpInstance) return;

    ERenderOptions options = pRenderer->RenderOptions();

    if (options & eDrawObjectCollision)
        mpCollisionNode->AddToRenderer(pRenderer, ViewInfo);

    if (options & eDrawObjects)
    {
        if (ViewInfo.ViewFrustum.BoxInFrustum(AABox()))
        {
            if (!mpActiveModel)
                pRenderer->AddOpaqueMesh(this, 0, AABox(), eDrawMesh);

            else
            {
                if (!mpActiveModel->IsBuffered())
                    mpActiveModel->BufferGL();

                if (!mpActiveModel->HasTransparency(0))
                    pRenderer->AddOpaqueMesh(this, 0, AABox(), eDrawMesh);

                else
                {
                    u32 SubmeshCount = mpActiveModel->GetSurfaceCount();

                    for (u32 s = 0; s < SubmeshCount; s++)
                    {
                        if (ViewInfo.ViewFrustum.BoxInFrustum(mpActiveModel->GetSurfaceAABox(s).Transformed(Transform())))
                        {
                            if (!mpActiveModel->IsSurfaceTransparent(s, 0))
                                pRenderer->AddOpaqueMesh(this, s, mpActiveModel->GetSurfaceAABox(s).Transformed(Transform()), eDrawAsset);
                            else
                                pRenderer->AddTransparentMesh(this, s, mpActiveModel->GetSurfaceAABox(s).Transformed(Transform()), eDrawAsset);
                        }
                    }
                }
            }

        }
    }

    if (IsSelected())
    {
        // Script nodes always draw their selections regardless of frustum planes
        // in order to ensure that script connection lines don't get improperly culled.
        pRenderer->AddOpaqueMesh(this, 0, AABox(), eDrawSelection);

        if (mHasVolumePreview)
            mpVolumePreviewNode->AddToRenderer(pRenderer, ViewInfo);
    }
}

void CScriptNode::Draw(ERenderOptions Options)
{
    if (!mpInstance) return;

    // Draw model
    if (mpActiveModel)
    {
        LoadModelMatrix();
        LoadLights();
        mpActiveModel->Draw(Options, 0);
    }

    // Draw billboard
    else if (mpBillboard)
    {
        CDrawUtil::DrawBillboard(mpBillboard, mPosition, BillboardScale(), TintColor());
    }

    // If no model or billboard, default to drawing a purple box
    else
    {
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ZERO);
        glDepthMask(GL_TRUE);

        LoadModelMatrix();
        LoadLights();
        CGraphics::UpdateVertexBlock();
        CGraphics::UpdateLightBlock();
        CDrawUtil::DrawShadedCube(CColor::skTransparentPurple);
        return;
    }
}

void CScriptNode::DrawAsset(ERenderOptions Options, u32 Asset)
{
    if (!mpInstance) return;
    if (!mpActiveModel) return;

    if (CGraphics::sLightMode == CGraphics::WorldLighting)
        CGraphics::sVertexBlock.COLOR0_Amb = CGraphics::sAreaAmbientColor.ToVector4f() * CGraphics::sWorldLightMultiplier;
    else
        CGraphics::sVertexBlock.COLOR0_Amb = CGraphics::skDefaultAmbientColor.ToVector4f();

    LoadModelMatrix();
    LoadLights();

    mpActiveModel->DrawSurface(Options, Asset, 0);
}

void CScriptNode::DrawSelection()
{
    glBlendFunc(GL_ONE, GL_ZERO);

    // Only draw bounding box for models; billboards get a tint color
    if (mpActiveModel || !mpBillboard)
    {
        LoadModelMatrix();
        CDrawUtil::DrawWireCube(AABox(), CColor::skTransparentWhite);
    }

    if (mpInstance)
    {
        CGraphics::sMVPBlock.ModelMatrix = CMatrix4f::skIdentity;
        CGraphics::UpdateMVPBlock();

        for (u32 iIn = 0; iIn < mpInstance->NumInLinks(); iIn++)
        {
            const SLink& con = mpInstance->InLink(iIn);
            CScriptNode *pLinkNode = mpScene->ScriptNodeByID(con.ObjectID);
            if (pLinkNode) CDrawUtil::DrawLine(CenterPoint(), pLinkNode->CenterPoint(), CColor::skTransparentRed);
        }

        for (u32 iOut = 0; iOut < mpInstance->NumOutLinks(); iOut++)
        {
            const SLink& con = mpInstance->OutLink(iOut);
            CScriptNode *pLinkNode = mpScene->ScriptNodeByID(con.ObjectID);
            if (pLinkNode) CDrawUtil::DrawLine(CenterPoint(), pLinkNode->CenterPoint(), CColor::skTransparentGreen);
        }
    }
}

void CScriptNode::RayAABoxIntersectTest(CRayCollisionTester &Tester)
{
    if (!mpInstance)
        return;

    const CRay& Ray = Tester.Ray();

    if (mpActiveModel || !mpBillboard)
    {
        std::pair<bool,float> BoxResult = AABox().IntersectsRay(Ray);

        if (BoxResult.first)
        {
            if (mpActiveModel)
            {
                for (u32 iSurf = 0; iSurf < mpActiveModel->GetSurfaceCount(); iSurf++)
                {
                    std::pair<bool,float> SurfResult = mpActiveModel->GetSurfaceAABox(iSurf).Transformed(Transform()).IntersectsRay(Ray);

                    if (SurfResult.first)
                        Tester.AddNode(this, iSurf, SurfResult.second);
                }
            }
            else Tester.AddNode(this, 0, BoxResult.second);
        }
    }

    else
    {
        // Because the billboard rotates a lot, expand the AABox on the X/Y axes to cover any possible orientation
        CVector2f BillScale = BillboardScale();
        float ScaleXY = (BillScale.x > BillScale.y ? BillScale.x : BillScale.y);

        CAABox BillBox = CAABox(mPosition + CVector3f(-ScaleXY, -ScaleXY, -BillScale.y),
                                mPosition + CVector3f( ScaleXY,  ScaleXY,  BillScale.y));

        std::pair<bool,float> BoxResult = BillBox.IntersectsRay(Ray);
        if (BoxResult.first) Tester.AddNode(this, 0, BoxResult.second);
    }
}

SRayIntersection CScriptNode::RayNodeIntersectTest(const CRay& Ray, u32 AssetID, const SViewInfo& ViewInfo)
{
    ERenderOptions options = ViewInfo.pRenderer->RenderOptions();

    SRayIntersection out;
    out.pNode = this;
    out.AssetIndex = AssetID;

    if (options & eDrawObjects)
    {
        // Model test
        if (mpActiveModel || !mpBillboard)
        {
            CModel *pModel = (mpActiveModel ? mpActiveModel : CDrawUtil::GetCubeModel());

            CRay TransformedRay = Ray.Transformed(Transform().Inverse());
            std::pair<bool,float> Result = pModel->GetSurface(AssetID)->IntersectsRay(TransformedRay, ((options & eEnableBackfaceCull) == 0));

            if (Result.first)
            {
                out.Hit = true;

                CVector3f HitPoint = TransformedRay.PointOnRay(Result.second);
                CVector3f WorldHitPoint = Transform() * HitPoint;
                out.Distance = Math::Distance(Ray.Origin(), WorldHitPoint);
            }

            else
                out.Hit = false;
        }

        // Billboard test
        // todo: come up with a better way to share this code between CScriptNode and CLightNode
        else
        {
            // Step 1: check whether the ray intersects with the plane the billboard is on
            CPlane BillboardPlane(-ViewInfo.pCamera->Direction(), mPosition);
            std::pair<bool,float> PlaneTest = Math::RayPlaneIntersecton(Ray, BillboardPlane);

            if (PlaneTest.first)
            {
                // Step 2: transform the hit point into the plane's local space
                CVector3f PlaneHitPoint = Ray.PointOnRay(PlaneTest.second);
                CVector3f RelHitPoint = PlaneHitPoint - mPosition;

                CVector3f PlaneForward = -ViewInfo.pCamera->Direction();
                CVector3f PlaneRight = -ViewInfo.pCamera->RightVector();
                CVector3f PlaneUp = ViewInfo.pCamera->UpVector();
                CQuaternion PlaneRot = CQuaternion::FromAxes(PlaneRight, PlaneForward, PlaneUp);

                CVector3f RotatedHitPoint = PlaneRot.Inverse() * RelHitPoint;
                CVector2f LocalHitPoint = RotatedHitPoint.xz() / BillboardScale();

                // Step 3: check whether the transformed hit point is in the -1 to 1 range
                if ((LocalHitPoint.x >= -1.f) && (LocalHitPoint.x <= 1.f) && (LocalHitPoint.y >= -1.f) && (LocalHitPoint.y <= 1.f))
                {
                    // Step 4: look up the hit texel and check whether it's transparent or opaque
                    CVector2f TexCoord = (LocalHitPoint + CVector2f(1.f)) * 0.5f;
                    TexCoord.x = -TexCoord.x + 1.f;
                    float TexelAlpha = mpBillboard->ReadTexelAlpha(TexCoord);

                    if (TexelAlpha < 0.25f)
                        out.Hit = false;

                    else
                    {
                        // It's opaque... we have a hit!
                        out.Hit = true;
                        out.Distance = PlaneTest.second;
                    }
                }

                else
                    out.Hit = false;
            }

            else
                out.Hit = false;
        }
    }
    else out.Hit = false;

    return out;
}

bool CScriptNode::IsVisible() const
{
    // Reimplementation of CSceneNode::IsHidden() to allow for layer and template visiblity to be taken into account
    return (mVisible && mpInstance->Layer()->IsVisible() && mpInstance->Template()->IsVisible());
}

CScriptObject* CScriptNode::Object()
{
    return mpInstance;
}

CModel* CScriptNode::ActiveModel()
{
    return mpActiveModel;
}

void CScriptNode::GeneratePosition()
{
    if  (!mHasValidPosition)
    {
        // Default to center of the active area; this is to preven recursion issues
        CTransform4f& AreaTransform = mpScene->GetActiveArea()->GetTransform();
        mPosition = CVector3f(AreaTransform[0][3], AreaTransform[1][3], AreaTransform[2][3]);
        mHasValidPosition = true;
        MarkTransformChanged();

        // Ideal way to generate the position is to find a spot close to where it's being used.
        // To do this I check the location of the objects that this one is linked to.
        u32 NumLinks = mpInstance->NumInLinks() + mpInstance->NumOutLinks();

        // In the case of one link, apply an offset so the new position isn't the same place as the object it's linked to
        if (NumLinks == 1)
        {
            const SLink& link = (mpInstance->NumInLinks() > 0 ? mpInstance->InLink(0) : mpInstance->OutLink(0));
            CScriptNode *pNode = mpScene->ScriptNodeByID(link.ObjectID);
            pNode->GeneratePosition();
            mPosition = pNode->AbsolutePosition();
            mPosition.z += (pNode->AABox().Size().z / 2.f);
            mPosition.z += (AABox().Size().z / 2.f);
            mPosition.z += 2.f;
        }

        // For two or more links, average out the position of the connected objects.
        else if (NumLinks >= 2)
        {
            CVector3f NewPos = CVector3f::skZero;

            for (u32 iIn = 0; iIn < mpInstance->NumInLinks(); iIn++)
            {
                CScriptNode *pNode = mpScene->ScriptNodeByID(mpInstance->InLink(iIn).ObjectID);

                if (pNode)
                {
                    pNode->GeneratePosition();
                    NewPos += pNode->AABox().Center();
                }
            }

            for (u32 iOut = 0; iOut < mpInstance->NumOutLinks(); iOut++)
            {
                CScriptNode *pNode = mpScene->ScriptNodeByID(mpInstance->OutLink(iOut).ObjectID);

                if (pNode)
                {
                    pNode->GeneratePosition();
                    NewPos += pNode->AABox().Center();
                }
            }

            mPosition = NewPos / (float) NumLinks;
            mPosition.x += 2.f;
        }

        MarkTransformChanged();
    }
}

bool CScriptNode::HasPreviewVolume()
{
    return mHasVolumePreview;
}

CAABox CScriptNode::PreviewVolumeAABox()
{
    if (!mHasVolumePreview)
        return CAABox::skZero;
    else
        return mpVolumePreviewNode->AABox();
}

CVector2f CScriptNode::BillboardScale()
{
    CVector2f out = (mpInstance->Template()->ScaleType() == CScriptTemplate::eScaleEnabled ? mScale.xz() : CVector2f(1.f));
    return out * 0.5f;
}
