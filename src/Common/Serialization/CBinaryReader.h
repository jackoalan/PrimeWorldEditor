#ifndef CBINARYREADER
#define CBINARYREADER

#include "IArchive.h"
#include "CSerialVersion.h"
#include "Common/CFourCC.h"

class CBinaryReader : public IArchive
{
    struct SParameter
    {
        u32 Offset;
        u32 Size;
        u32 NumChildren;
        u32 ChildIndex;
        bool Abstract;
    };
    std::vector<SParameter> mParamStack;

    IInputStream *mpStream;
    bool mMagicValid;
    bool mOwnsStream;

public:
    CBinaryReader(const TString& rkFilename, u32 Magic)
        : IArchive(true, false)
        , mOwnsStream(true)
    {
        mpStream = new CFileInStream(rkFilename, IOUtil::eBigEndian);

        if (mpStream->IsValid())
        {
            mMagicValid = (mpStream->ReadLong() == Magic);
            CSerialVersion Version(*mpStream);
            SetVersion(Version);
        }

        InitParamStack();
    }

    CBinaryReader(IInputStream *pStream, const CSerialVersion& rkVersion)
        : IArchive(true, false)
        , mMagicValid(true)
        , mOwnsStream(false)
    {
        ASSERT(pStream && pStream->IsValid());
        mpStream = pStream;
        SetVersion(rkVersion);

        InitParamStack();
    }

    ~CBinaryReader()
    {
        if (mOwnsStream) delete mpStream;
    }

    inline bool IsValid() const { return mpStream->IsValid() && mMagicValid; }

private:
    void InitParamStack()
    {
        mpStream->Skip(4); // Skip root ID (which is always -1)
        u32 Size = ReadSize();
        u32 Offset = mpStream->Tell();
        u32 NumChildren = ReadSize();
        mParamStack.push_back( SParameter { Offset, Size, NumChildren, 0, false } );
        mParamStack.reserve(20);
    }

public:
    // Interface
    u32 ReadSize()
    {
        return (mArchiveVersion < eArVer_32BitBinarySize ? (u32) mpStream->ReadShort() : mpStream->ReadLong());
    }

    virtual bool ParamBegin(const char *pkName)
    {
        // If this is the parent parameter's first child, then read the child count
        if (mParamStack.back().NumChildren == 0xFFFFFFFF)
        {
            mParamStack.back().NumChildren = ReadSize();
        }

        // Save current offset
        u32 Offset = mpStream->Tell();
        u32 ParamID = TString(pkName).Hash32();

        // Check the next parameter ID first and check whether it's a match for the current parameter
        if (mParamStack.back().ChildIndex < mParamStack.back().NumChildren)
        {
            u32 NextID = mpStream->ReadLong();
            u32 NextSize = ReadSize();

            // Does the next parameter ID match the current one?
            if (NextID == ParamID)
            {
                mParamStack.push_back( SParameter { mpStream->Tell(), NextSize, 0xFFFFFFFF, 0, false } );
                return true;
            }
        }

        // It's not a match - return to the parent parameter's first child and check all children to find a match
        if (!mParamStack.empty())
        {
            bool ParentAbstract = mParamStack.back().Abstract;
            u32 ParentOffset = mParamStack.back().Offset;
            u32 NumChildren = mParamStack.back().NumChildren;
            mpStream->GoTo(ParentOffset + (ParentAbstract ? 4 : 0));

            for (u32 ChildIdx = 0; ChildIdx < NumChildren; ChildIdx++)
            {
                u32 ChildID = mpStream->ReadLong();
                u32 ChildSize = ReadSize();

                if (ChildID != ParamID)
                    mpStream->Skip(ChildSize);
                else
                {
                    mParamStack.back().ChildIndex = ChildIdx;
                    mParamStack.push_back( SParameter { mpStream->Tell(), ChildSize, 0xFFFFFFFF, 0, false } );
                    return true;
                }
            }
        }

        // None of the children were a match - this parameter isn't in the file
        mpStream->GoTo(Offset);
        return false;
    }

    virtual void ParamEnd()
    {
        // Make sure we're at the end of the parameter
        SParameter& rParam = mParamStack.back();
        u32 EndOffset = rParam.Offset + rParam.Size;
        mpStream->GoTo(EndOffset);
        mParamStack.pop_back();

        // Increment parent child index
        if (!mParamStack.empty())
            mParamStack.back().ChildIndex++;
    }

    virtual void SerializeContainerSize(u32& rSize, const TString& /*rkElemName*/)
    {
        // Mostly handled by ParamBegin, we just need to return the size correctly so the container can be resized
        rSize = (mArchiveVersion < eArVer_32BitBinarySize ? (u32) mpStream->PeekShort() : mpStream->PeekLong());
    }

    virtual void SerializeAbstractObjectType(u32& rType)
    {
        // Mark current parameter as abstract so we can account for the object type in the filestream
        rType = mpStream->ReadLong();
        mParamStack.back().Abstract = true;
    }

    virtual void SerializePrimitive(bool& rValue)           { rValue = mpStream->ReadBool(); }
    virtual void SerializePrimitive(char& rValue)           { rValue = mpStream->ReadByte(); }
    virtual void SerializePrimitive(s8& rValue)             { rValue = mpStream->ReadByte(); }
    virtual void SerializePrimitive(u8& rValue)             { rValue = mpStream->ReadByte(); }
    virtual void SerializePrimitive(s16& rValue)            { rValue = mpStream->ReadShort(); }
    virtual void SerializePrimitive(u16& rValue)            { rValue = mpStream->ReadShort(); }
    virtual void SerializePrimitive(s32& rValue)            { rValue = mpStream->ReadLong(); }
    virtual void SerializePrimitive(u32& rValue)            { rValue = mpStream->ReadLong(); }
    virtual void SerializePrimitive(s64& rValue)            { rValue = mpStream->ReadLongLong(); }
    virtual void SerializePrimitive(u64& rValue)            { rValue = mpStream->ReadLongLong(); }
    virtual void SerializePrimitive(float& rValue)          { rValue = mpStream->ReadFloat(); }
    virtual void SerializePrimitive(double& rValue)         { rValue = mpStream->ReadDouble(); }
    virtual void SerializePrimitive(TString& rValue)        { rValue = mpStream->ReadSizedString(); }
    virtual void SerializePrimitive(TWideString& rValue)    { rValue = mpStream->ReadSizedWString(); }
    virtual void SerializePrimitive(CFourCC& rValue)        { rValue = CFourCC(*mpStream); }
    virtual void SerializePrimitive(CAssetID& rValue)       { rValue = CAssetID(*mpStream, Game()); }

    virtual void SerializeHexPrimitive(u8& rValue)          { rValue = mpStream->ReadByte(); }
    virtual void SerializeHexPrimitive(u16& rValue)         { rValue = mpStream->ReadShort(); }
    virtual void SerializeHexPrimitive(u32& rValue)         { rValue = mpStream->ReadLong(); }
    virtual void SerializeHexPrimitive(u64& rValue)         { rValue = mpStream->ReadLongLong(); }

    virtual void BulkSerialize(void* pData, u32 Size)       { mpStream->ReadBytes(pData, Size); }
};

#endif // CBINARYREADER

