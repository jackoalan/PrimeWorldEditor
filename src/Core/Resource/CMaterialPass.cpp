#include "CMaterialPass.h"
#include "CMaterial.h"
#include "Core/Render/CGraphics.h"
#include <Common/CTimer.h>

CMaterialPass::CMaterialPass(CMaterial *pParent)
    : mPassType("CUST")
    , mSettings(eNoPassSettings)
    , mpTexture(nullptr)
    , mEnabled(true)
    , mpParentMat(pParent)
    , mColorOutput(ePrevReg)
    , mAlphaOutput(ePrevReg)
    , mKColorSel(eKonstOne)
    , mKAlphaSel(eKonstOne)
    , mRasSel(eRasColorNull)
    , mTexCoordSource(0xFF)
    , mAnimMode(eNoUVAnim)
{
    for (u32 iParam = 0; iParam < 4; iParam++)
    {
        mColorInputs[iParam] = eZeroRGB;
        mAlphaInputs[iParam] = eZeroAlpha;
        mAnimParams[iParam] = 0.f;
    }
}

CMaterialPass::~CMaterialPass()
{
}

CMaterialPass* CMaterialPass::Clone(CMaterial *pParent)
{
    CMaterialPass *pOut = new CMaterialPass(pParent);
    pOut->mPassType = mPassType;
    pOut->mSettings = mSettings;

    for (u32 iIn = 0; iIn < 4; iIn++)
    {
        pOut->mColorInputs[iIn] = mColorInputs[iIn];
        pOut->mAlphaInputs[iIn] = mAlphaInputs[iIn];
    }

    pOut->mColorOutput = mColorOutput;
    pOut->mAlphaOutput = mAlphaOutput;
    pOut->mKColorSel = mKColorSel;
    pOut->mKAlphaSel = mKAlphaSel;
    pOut->mRasSel = mRasSel;
    pOut->mTexCoordSource = mTexCoordSource;
    pOut->mpTexture = mpTexture;
    pOut->mAnimMode = mAnimMode;

    for (u32 iParam = 0; iParam < 4; iParam++)
        pOut->mAnimParams[iParam] = mAnimParams[iParam];

    pOut->mEnabled = mEnabled;

    return pOut;
}

void CMaterialPass::HashParameters(CHashFNV1A& rHash)
{
    if (mEnabled)
    {
        rHash.HashLong(mPassType.ToLong());
        rHash.HashLong(mSettings);
        rHash.HashData(&mColorInputs[0], sizeof(ETevColorInput) * 4);
        rHash.HashData(&mAlphaInputs[0], sizeof(ETevAlphaInput) * 4);
        rHash.HashLong(mColorOutput);
        rHash.HashLong(mAlphaOutput);
        rHash.HashLong(mKColorSel);
        rHash.HashLong(mKAlphaSel);
        rHash.HashLong(mRasSel);
        rHash.HashLong(mTexCoordSource);
        rHash.HashLong(mAnimMode);
        rHash.HashData(mAnimParams, sizeof(float) * 4);
        rHash.HashByte(mEnabled);
    }
}

void CMaterialPass::LoadTexture(u32 PassIndex)
{
    if (mpTexture)
        mpTexture->Bind(PassIndex);
}

void CMaterialPass::SetAnimCurrent(FRenderOptions Options, u32 PassIndex)
{
    if (mAnimMode == eNoUVAnim) return;

    float Seconds = CTimer::SecondsMod900();
    const CMatrix4f& ModelMtx = CGraphics::sMVPBlock.ModelMatrix;
    const CMatrix4f& ViewMtx = CGraphics::sMVPBlock.ViewMatrix;

    CMatrix4f TexMtx = CMatrix4f::skIdentity;
    CMatrix4f PostMtx = CMatrix4f::skIdentity;

    switch (mAnimMode)
    {

    case eInverseMV: // Mode 0
    case eSimpleMode: // Mode 10 - maybe not correct?
    {
        TexMtx  = (ModelMtx * ViewMtx).Transpose().Inverse();
        PostMtx = CMatrix4f(0.5f, 0.0f, 0.0f, 0.5f,
                            0.0f, 0.5f, 0.0f, 0.5f,
                            0.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, 1.0f);
        break;
    }

    case eInverseMVTranslated: // Mode 1
    {
        TexMtx = (ModelMtx * ViewMtx).Transpose().Inverse();
        PostMtx = CMatrix4f(0.5f, 0.0f, 0.0f, 0.5f,
                            0.0f, 0.5f, 0.0f, 0.5f,
                            0.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, 1.0f);
    }

    case eUVScroll: // Mode 2
    {
        if (Options & eEnableUVScroll)
        {
            TexMtx[0][3] = (Seconds * mAnimParams[2]) + mAnimParams[0];
            TexMtx[1][3] = (Seconds * mAnimParams[3]) + mAnimParams[1];
        }
        break;
    }

    case eUVRotation: // Mode 3
    {
        if (Options & eEnableUVScroll)
        {
            float Angle = (Seconds * mAnimParams[1]) + mAnimParams[0];

            float ACos = cos(Angle);
            float ASin = sin(Angle);
            float TransX = (1.f - (ACos - ASin)) * 0.5f;
            float TransY = (1.f - (ASin + ACos)) * 0.5f;

            TexMtx = CMatrix4f(ACos, -ASin, 0.0f, TransX,
                               ASin,  ACos, 0.0f, TransY,
                               0.0f,  0.0f, 1.0f,   0.0f,
                               0.0f,  0.0f, 0.0f,   1.0f);
        }
        break;
    }

    case eHFilmstrip: // Mode 4
    case eVFilmstrip: // Mode 5
    {
        if (Options & eEnableUVScroll)
        {
            float Offset = mAnimParams[2] * mAnimParams[0] * (mAnimParams[3] + Seconds);
            Offset = (float)(short)(float)(mAnimParams[1] * fmod(Offset, 1.0f)) * mAnimParams[2];
            if (mAnimMode == eHFilmstrip) TexMtx[0][3] = Offset;
            if (mAnimMode == eVFilmstrip) TexMtx[1][3] = Offset;
        }
        break;
    }

    case eModelMatrix: // Mode 6
    {
        // It looks ok, but I can't tell whether it's correct...
        TexMtx  = ModelMtx.Transpose();
        PostMtx = CMatrix4f(0.5f, 0.0f, 0.0f, TexMtx[0][3] * 0.50000001f,
                            0.0f, 0.5f, 0.0f, TexMtx[1][3] * 0.50000001f,
                            0.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, 1.0f);
        TexMtx[0][3] = 0.f;
        TexMtx[1][3] = 0.f;
        TexMtx[2][3] = 0.f;
    }

    case eConvolutedModeA: // Mode 7
    {
        CMatrix4f View = CGraphics::sMVPBlock.ViewMatrix;

        TexMtx = (ModelMtx * ViewMtx).Transpose().Inverse();
        TexMtx[0][3] = TexMtx[1][3] = TexMtx[2][3] = 0.f;

        float XY = (View[3][0] + View[3][1]) * 0.025f * mAnimParams[1];
        XY = (XY - (int) XY);

        float Z = View[3][2] * 0.05f * mAnimParams[1];
        Z = (Z - (int) Z);

        float HalfA = mAnimParams[0] * 0.5f;

        PostMtx = CMatrix4f(HalfA, 0.0f,  0.0f,   XY,
                             0.0f, 0.0f, HalfA,    Z,
                             0.0f, 0.0f,  0.0f, 1.0f,
                             0.0f, 0.0f,  0.0f, 1.0f);
        break;
    }

    case eConvolutedModeB: // Mode 8 (MP3/DKCR only)
    {
        // todo
        break;
    }

    default:
        break;
    }

    CGraphics::sVertexBlock.TexMatrices[PassIndex] = TexMtx;
    CGraphics::sVertexBlock.PostMatrices[PassIndex] = PostMtx;
}

// ************ SETTERS ************
void CMaterialPass::SetType(CFourCC Type)
{
    mPassType = Type;
    mpParentMat->Update();
}

void CMaterialPass::SetColorInputs(ETevColorInput InputA, ETevColorInput InputB, ETevColorInput InputC, ETevColorInput InputD)
{
    mColorInputs[0] = InputA;
    mColorInputs[1] = InputB;
    mColorInputs[2] = InputC;
    mColorInputs[3] = InputD;
    mpParentMat->Update();
}

void CMaterialPass::SetAlphaInputs(ETevAlphaInput InputA, ETevAlphaInput InputB, ETevAlphaInput InputC, ETevAlphaInput InputD)
{
    mAlphaInputs[0] = InputA;
    mAlphaInputs[1] = InputB;
    mAlphaInputs[2] = InputC;
    mAlphaInputs[3] = InputD;
    mpParentMat->Update();
}

void CMaterialPass::SetColorOutput(ETevOutput OutputReg)
{
    mColorOutput = OutputReg;
    mpParentMat->Update();
}

void CMaterialPass::SetAlphaOutput(ETevOutput OutputReg)
{
    mAlphaOutput = OutputReg;
    mpParentMat->Update();
}

void CMaterialPass::SetKColorSel(ETevKSel Sel)
{
    mKColorSel = Sel;
    mpParentMat->Update();
}

void CMaterialPass::SetKAlphaSel(ETevKSel Sel)
{
    // Konst RGB is invalid for alpha, so reset to One if that's the selection
    if ((Sel >= eKonst0_RGB) && (Sel <= eKonst3_RGB))
        Sel = eKonstOne;

    mKAlphaSel = Sel;
    mpParentMat->Update();
}

void CMaterialPass::SetRasSel(ETevRasSel Sel)
{
    mRasSel = Sel;
    mpParentMat->Update();
}

void CMaterialPass::SetTexCoordSource(u32 Source)
{
    mTexCoordSource = Source;
    mpParentMat->Update();
}

void CMaterialPass::SetTexture(CTexture *pTex)
{
    mpTexture = pTex;
}

void CMaterialPass::SetAnimMode(EUVAnimMode Mode)
{
    mAnimMode = Mode;
    mpParentMat->Update();
}

void CMaterialPass::SetAnimParam(u32 ParamIndex, float Value)
{
    mAnimParams[ParamIndex] = Value;
}

void CMaterialPass::SetEnabled(bool Enabled)
{
    mEnabled = Enabled;
    mpParentMat->Update();
}

// ************ STATIC ************
TString CMaterialPass::PassTypeName(CFourCC Type)
{
    if (Type == "CUST") return "Custom";
    if (Type == "DIFF") return "Light";
    if (Type == "RIML") return "Rim Light";
    if (Type == "BLOL") return "Bloom Light";
    // BLOD
    if (Type == "CLR ") return "Diffuse";
    if (Type == "TRAN") return "Opacity";
    if (Type == "INCA") return "Emissive";
    if (Type == "RFLV") return "Specular";
    if (Type == "RFLD") return "Reflection";
    // LRLD
    // LURD
    if (Type == "BLOI") return "Bloom Diffuse";
    if (Type == "XRAY") return "X-Ray";
    // TOON
    return Type.ToString();
}
