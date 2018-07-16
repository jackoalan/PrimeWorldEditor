#ifndef CENUMPROPERTY_H
#define CENUMPROPERTY_H

#include "../IPropertyNew.h"

/** There are two types of enum properties in the game data: enum and choice.
 *
 *  In the game, the difference is that choice properties are index-based, while
 *  enum properties are stored as a hash of the name of the enum value.
 *
 *  In PWE, however, they are both implemented the same way under the hood.
 */
template<EPropertyTypeNew TypeEnum>
class TEnumPropertyBase : public TTypedPropertyNew<int, TypeEnum>
{
    friend class CTemplateLoader;
    struct SEnumValue
    {
        TString Name;
        u32 ID;

        SEnumValue(const TString& rkInName, u32 InID)
            : Name(rkInName), ID(InID) {}

        inline bool operator==(const SEnumValue& rkOther) const
        {
            return( Name == rkOther.Name && ID == rkOther.ID );
        }
    };
    std::vector<SEnumValue> mValues;

    /** XML template file that this enum originated from; for archetypes */
    TString mSourceFile;

public:
    virtual const char* GetHashableTypeName() const
    {
        if (TypeEnum == EPropertyTypeNew::Enum)
            return "enum";
        else
            return "choice";
    }

    virtual void SerializeValue(void* pData, IArchive& Arc) const
    {
        Arc.SerializePrimitive( (u32&) ValueRef(pData) );
    }

    virtual void InitFromArchetype(IPropertyNew* pOther)
    {
        TTypedPropertyNew::InitFromArchetype(pOther);
        TEnumPropertyBase* pOtherEnum = static_cast<TEnumPropertyBase*>(pOther);
        mValues = pOtherEnum->mValues;
    }

    virtual TString GetTemplateFileName()
    {
        ASSERT(IsArchetype() || mpArchetype);
        return IsArchetype() ? mSourceFile : mpArchetype->GetTemplateFileName();
    }

    void AddValue(TString ValueName, u32 ValueID)
    {
        mValues.push_back( SEnumValue(ValueName, ValueID) );
    }

    inline u32 NumPossibleValues() const { return mValues.size(); }

    u32 ValueIndex(u32 ID) const
    {
        for (u32 ValueIdx = 0; ValueIdx < mValues.size(); ValueIdx++)
        {
            if (mValues[ValueIdx].ID == ID)
            {
                return ValueIdx;
            }
        }
        return -1;
    }

    u32 ValueID(u32 Index) const
    {
        ASSERT(Index >= 0 && Index < mValues.size());
        return mValues[Index].ID;
    }

    TString ValueName(u32 Index) const
    {
        ASSERT(Index >= 0 && Index < mValues.size());
        return mValues[Index].Name;
    }

    bool HasValidValue(void* pPropertyData)
    {
        int ID = ValueRef(pPropertyData);
        u32 Index = ValueIndex(ID);
        return Index >= 0 && Index < mValues.size();
    }
};

typedef TEnumPropertyBase<EPropertyTypeNew::Choice> CChoiceProperty;
typedef TEnumPropertyBase<EPropertyTypeNew::Enum>   CEnumProperty;

// Specialization of TPropCast to allow interchangeable casting, as both types are the same thing
template<>
inline CEnumProperty* TPropCast(IPropertyNew* pProperty)
{
    EPropertyTypeNew InType = pProperty->Type();

    if (InType == EPropertyTypeNew::Enum || InType == EPropertyTypeNew::Choice)
    {
        return static_cast<CEnumProperty*>(pProperty);
    }
    else
    {
        return nullptr;
    }
}

template<>
inline CChoiceProperty* TPropCast(IPropertyNew* pProperty)
{
    if (pProperty)
    {
        EPropertyTypeNew InType = pProperty->Type();

        if (InType == EPropertyTypeNew::Enum || InType == EPropertyTypeNew::Choice)
        {
            return static_cast<CChoiceProperty*>(pProperty);
        }
    }

    return nullptr;
}

#endif // CENUMPROPERTY_H
