#include "CStringTable.h"
#include <algorithm>
#include <iterator>

/**
 * Listing of supported languages for different engine versions. Note we ignore the "unused" languages.
 * This is also the order that languages appear in game STRG assets.
 */
// Supported languages in the original NTSC release of Metroid Prime
const ELanguage gkSupportedLanguagesMP1[] =
{
    ELanguage::English
};

// Supported languages in the PAL version of Metroid Prime, and also Metroid Prime 2
const ELanguage gkSupportedLanguagesMP1PAL[] =
{
    ELanguage::English, ELanguage::French, ELanguage::German,
    ELanguage::Spanish, ELanguage::Italian, ELanguage::Japanese
};

// Supported languages in Metroid Prime 3
const ELanguage gkSupportedLanguagesMP3[] =
{
    ELanguage::English, ELanguage::German, ELanguage::French,
    ELanguage::Spanish, ELanguage::Italian, ELanguage::Japanese
};

// Supported languages in DKCR
const ELanguage gkSupportedLanguagesDKCR[] =
{
    ELanguage::English, ELanguage::Japanese, ELanguage::German,
    ELanguage::French, ELanguage::Spanish, ELanguage::Italian,
    ELanguage::UKEnglish, ELanguage::Korean,
    ELanguage::NAFrench, ELanguage::NASpanish
};

// Utility function - retrieve the index of a given language
static int FindLanguageIndex(const CStringTable* pkInTable, ELanguage InLanguage)
{
    for (uint LanguageIdx = 0; LanguageIdx < pkInTable->NumLanguages(); LanguageIdx++)
    {
        if (pkInTable->LanguageByIndex(LanguageIdx) == InLanguage)
        {
            return LanguageIdx;
        }
    }

    return -1;
}

/** Returns a string given a language/index pair */
TString CStringTable::GetString(ELanguage Language, uint StringIndex) const
{
    int LanguageIdx = FindLanguageIndex(this, Language);

    if (LanguageIdx >= 0 && mLanguages[LanguageIdx].Strings.size() > StringIndex)
    {
        return mLanguages[LanguageIdx].Strings[StringIndex];
    }
    else
    {
        return "";
    }
}

/** Updates a string for a given language */
void CStringTable::SetString(ELanguage Language, uint StringIndex, const TString& kNewString)
{
    int LanguageIdx = FindLanguageIndex(this, Language);

    if (LanguageIdx >= 0 && mLanguages[LanguageIdx].Strings.size() > StringIndex)
    {
        mLanguages[LanguageIdx].Strings[StringIndex] = kNewString;
    }
}

/** Updates a string name */
void CStringTable::SetStringName(uint StringIndex, const TString& kNewName)
{
    // Sanity check - make sure the string index is valid
    ASSERT( NumStrings() > StringIndex );

    // Expand the name listing if needed and assign the name
    if (mStringNames.size() <= StringIndex)
    {
        mStringNames.resize( StringIndex + 1 );
    }

    mStringNames[StringIndex] = kNewName;
}

/** Configures the string table with default languages for the game/region pairing of the resource */
void CStringTable::ConfigureDefaultLanguages()
{
    //@todo; this should be called on all newly created string tables
}

/** Serialize resource data */
void CStringTable::Serialize(IArchive& Arc)
{
    Arc << SerialParameter("StringNames", mStringNames, SH_Optional)
        << SerialParameter("Languages", mLanguages);
}

/** Build the dependency tree for this resource */
CDependencyTree* CStringTable::BuildDependencyTree() const
{
    // STRGs can reference FONTs with the &font=; formatting tag and TXTRs with the &image=; tag
    CDependencyTree* pTree = new CDependencyTree();
    EIDLength IDLength = CAssetID::GameIDLength( Game() );

    for (uint LanguageIdx = 0; LanguageIdx < mLanguages.size(); LanguageIdx++)
    {
        const SLanguageData& kLanguage = mLanguages[LanguageIdx];

        for (uint StringIdx = 0; StringIdx < kLanguage.Strings.size(); StringIdx++)
        {
            const TString& kString = kLanguage.Strings[StringIdx];

            for (int TagIdx = kString.IndexOf('&'); TagIdx != -1; TagIdx = kString.IndexOf('&', TagIdx + 1))
            {
                // Check for double ampersand (escape character in DKCR, not sure about other games)
                if (kString.At(TagIdx + 1) == '&')
                {
                    TagIdx++;
                    continue;
                }

                // Get tag name and parameters
                int NameEnd = kString.IndexOf('=', TagIdx);
                int TagEnd = kString.IndexOf(';', TagIdx);
                if (NameEnd == -1 || TagEnd == -1) continue;

                TString TagName = kString.SubString(TagIdx + 1, NameEnd - TagIdx - 1);
                TString ParamString = kString.SubString(NameEnd + 1, TagEnd - NameEnd - 1);
                if (ParamString.IsEmpty()) continue;

                // Font
                if (TagName == "font")
                {
                    if (Game() >= EGame::CorruptionProto)
                    {
                        ASSERT(ParamString.StartsWith("0x"));
                        ParamString = ParamString.ChopFront(2);
                    }

                    ASSERT(ParamString.Size() == IDLength * 2);
                    pTree->AddDependency( CAssetID::FromString(ParamString) );
                }

                // Image
                else if (TagName == "image")
                {
                    // Determine which params are textures based on image type
                    TStringList Params = ParamString.Split(",");
                    TString ImageType = Params.front();
                    uint TexturesStart = 0;

                    if (ImageType == "A")
                        TexturesStart = 2;

                    else if (ImageType == "SI")
                        TexturesStart = 3;

                    else if (ImageType == "SA")
                        TexturesStart = 4;

                    else if (ImageType == "B")
                        TexturesStart = 2;

                    else if (ImageType.IsHexString(false, IDLength * 2))
                        TexturesStart = 0;

                    else
                    {
                        errorf("Unrecognized image type: %s", *ImageType);
                        continue;
                    }

                    // Load texture IDs
                    TStringList::iterator Iter = Params.begin();

                    for (uint ParamIdx = 0; ParamIdx < Params.size(); ParamIdx++, Iter++)
                    {
                        if (ParamIdx >= TexturesStart)
                        {
                            TString Param = *Iter;

                            if (Game() >= EGame::CorruptionProto)
                            {
                                ASSERT(Param.StartsWith("0x"));
                                Param = Param.ChopFront(2);
                            }

                            ASSERT(Param.Size() == IDLength * 2);
                            pTree->AddDependency( CAssetID::FromString(Param) );
                        }
                    }
                }
            }
        }
    }

    return pTree;
}

/** Static - Strip all formatting tags for a given string */
TString CStringTable::StripFormatting(const TString& kInString)
{
    TString Out = kInString;
    int TagStart = -1;

    for (uint CharIdx = 0; CharIdx < Out.Size(); CharIdx++)
    {
        if (Out[CharIdx] == '&')
        {
            if (TagStart == -1)
                TagStart = CharIdx;

            else
            {
                Out.Remove(TagStart, 1);
                TagStart = -1;
                CharIdx--;
            }
        }

        else if (TagStart != -1 && Out[CharIdx] == ';')
        {
            int TagEnd = CharIdx + 1;
            int TagLen = TagEnd - TagStart;
            Out.Remove(TagStart, TagLen);
            CharIdx = TagStart - 1;
            TagStart = -1;
        }
    }

    return Out;
}

/** Static - Returns whether a given language is supported by the given game/region combination */
bool CStringTable::IsLanguageSupported(ELanguage Language, EGame Game, ERegion Region)
{
    // Pick the correct array to iterate based on which game/region was requested.
    const ELanguage* pkSupportedLanguages = nullptr;
    uint NumLanguages = 0;

    if (Game <= EGame::Prime && Region == ERegion::NTSC)
    {
        return (Language == ELanguage::English);
    }
    else if (Game <= EGame::CorruptionProto)
    {
        pkSupportedLanguages = &gkSupportedLanguagesMP1PAL[0];
        NumLanguages = ARRAY_SIZE( gkSupportedLanguagesMP1PAL );
    }
    else if (Game <= EGame::Corruption)
    {
        pkSupportedLanguages = &gkSupportedLanguagesMP3[0];
        NumLanguages = ARRAY_SIZE( gkSupportedLanguagesMP3 );
    }
    else if (Game <= EGame::DKCReturns)
    {
        pkSupportedLanguages = &gkSupportedLanguagesDKCR[0];
        NumLanguages = ARRAY_SIZE( gkSupportedLanguagesDKCR );
    }
    ASSERT(pkSupportedLanguages);
    ASSERT(NumLanguages > 0);

    // Check if the requested language is in the array.
    for (uint LanguageIdx = 0; LanguageIdx < NumLanguages; LanguageIdx++)
    {
        if (pkSupportedLanguages[LanguageIdx] == Language)
        {
            return true;
        }
    }

    // Unsupported
    return false;
}
