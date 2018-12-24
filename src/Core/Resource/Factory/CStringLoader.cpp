#include "CStringLoader.h"
#include <Common/Log.h>
#include <Common/Math/MathUtil.h>

void CStringLoader::LoadPrimeDemoSTRG(IInputStream& STRG)
{
    // This function starts at 0x4 in the file - right after the size
    // This STRG version only supports one language per file
    mpStringTable->mLanguages.resize(1);
    CStringTable::SLanguageData& Language = mpStringTable->mLanguages[0];
    Language.Language = ELanguage::English;
    uint TableStart = STRG.Tell();

    // Header
    uint NumStrings = STRG.ReadLong();
    Language.Strings.resize(NumStrings);

    // String offsets (yeah, that wasn't much of a header)
    std::vector<uint> StringOffsets(NumStrings);
    for (uint32 OffsetIdx = 0; OffsetIdx < NumStrings; OffsetIdx++)
        StringOffsets[OffsetIdx] = STRG.ReadLong();

    // Strings
    for (uint StringIdx = 0; StringIdx < NumStrings; StringIdx++)
    {
        STRG.GoTo( TableStart + StringOffsets[StringIdx] );
        Language.Strings[StringIdx] = STRG.Read16String().ToUTF8();
    }
}

void CStringLoader::LoadPrimeSTRG(IInputStream& STRG)
{
    // This function starts at 0x8 in the file, after magic/version
    // Header
    uint NumLanguages = STRG.ReadLong();
    uint NumStrings = STRG.ReadLong();

    // Language definitions
    mpStringTable->mLanguages.resize(NumLanguages);
    std::vector<uint> LanguageOffsets(NumLanguages);

    for (uint LanguageIdx = 0; LanguageIdx < NumLanguages; LanguageIdx++)
    {
        mpStringTable->mLanguages[LanguageIdx].Language = (ELanguage) STRG.ReadFourCC();
        LanguageOffsets[LanguageIdx] = STRG.ReadLong();

        // Skip strings size in MP2
        if (mVersion >= EGame::EchoesDemo)
        {
            STRG.Skip(4);
        }
    }

    // String names
    if (mVersion >= EGame::EchoesDemo)
    {
        LoadNameTable(STRG);
    }

    // Strings
    uint StringsStart = STRG.Tell();
    for (uint32 LanguageIdx = 0; LanguageIdx < NumLanguages; LanguageIdx++)
    {
        STRG.GoTo( StringsStart + LanguageOffsets[LanguageIdx] );

        // Skip strings size in MP1
        if (mVersion == EGame::Prime)
        {
            STRG.Skip(4);
        }

        CStringTable::SLanguageData& Language = mpStringTable->mLanguages[LanguageIdx];
        Language.Strings.resize(NumStrings);

        // Offsets
        uint LanguageStart = STRG.Tell();
        std::vector<uint> StringOffsets(NumStrings);

        for (uint StringIdx = 0; StringIdx < NumStrings; StringIdx++)
        {
            StringOffsets[StringIdx] = LanguageStart + STRG.ReadLong();
        }

        // The actual strings
        for (uint StringIdx = 0; StringIdx < NumStrings; StringIdx++)
        {
            STRG.GoTo( StringOffsets[StringIdx] );
            Language.Strings[StringIdx] = STRG.Read16String().ToUTF8();
        }
    }
}

void CStringLoader::LoadCorruptionSTRG(IInputStream& STRG)
{
    // This function starts at 0x8 in the file, after magic/version
    // Header
    uint NumLanguages = STRG.ReadLong();
    uint NumStrings = STRG.ReadLong();

    // String names
    LoadNameTable(STRG);

    // Language definitions
    mpStringTable->mLanguages.resize(NumLanguages);
    std::vector< std::vector<uint> > LanguageOffsets(NumLanguages);

    for (uint LanguageIdx = 0; LanguageIdx < NumLanguages; LanguageIdx++)
    {
        mpStringTable->mLanguages[LanguageIdx].Language = (ELanguage) STRG.ReadFourCC();
    }

    for (uint LanguageIdx = 0; LanguageIdx < NumLanguages; LanguageIdx++)
    {
        LanguageOffsets[LanguageIdx].resize(NumStrings);
        STRG.Skip(4); // Skipping total string size

        for (uint StringIdx = 0; StringIdx < NumStrings; StringIdx++)
        {
            LanguageOffsets[LanguageIdx][StringIdx] = STRG.ReadLong();
        }
    }

    // Strings
    uint StringsStart = STRG.Tell();

    for (uint LanguageIdx = 0; LanguageIdx < NumLanguages; LanguageIdx++)
    {
        CStringTable::SLanguageData& Language = mpStringTable->mLanguages[LanguageIdx];
        Language.Strings.resize(NumStrings);

        for (uint StringIdx = 0; StringIdx < NumStrings; StringIdx++)
        {
            STRG.GoTo( StringsStart + LanguageOffsets[LanguageIdx][StringIdx] );
            STRG.Skip(4); // Skipping string size
            Language.Strings[StringIdx] = STRG.ReadString();
        }
    }
}

void CStringLoader::LoadNameTable(IInputStream& STRG)
{
    // Name table header
    uint NameCount = STRG.ReadLong();
    uint NameTableSize = STRG.ReadLong();
    uint NameTableStart = STRG.Tell();
    uint NameTableEnd = NameTableStart + NameTableSize;

    // Name definitions
    struct SNameDef {
        uint NameOffset, StringIndex;
    };
    std::vector<SNameDef> NameDefs(NameCount);

    // Keep track of max string index so we can size the names array appropriately.
    // Note that it is possible that not every string in the table has a name.
    int MaxIndex = -1;

    for (uint NameIdx = 0; NameIdx < NameCount; NameIdx++)
    {
        NameDefs[NameIdx].NameOffset = STRG.ReadLong() + NameTableStart;
        NameDefs[NameIdx].StringIndex = STRG.ReadLong();
        MaxIndex = Math::Max(MaxIndex, (int) NameDefs[NameIdx].StringIndex);
    }

    // Name strings
    mpStringTable->mStringNames.resize(MaxIndex + 1);

    for (uint NameIdx = 0; NameIdx < NameCount; NameIdx++)
    {
        SNameDef& NameDef = NameDefs[NameIdx];
        STRG.GoTo(NameDef.NameOffset);
        mpStringTable->mStringNames[NameDef.StringIndex] = STRG.ReadString();
    }
    STRG.GoTo(NameTableEnd);
}

// ************ STATIC ************
CStringTable* CStringLoader::LoadSTRG(IInputStream& STRG, CResourceEntry* pEntry)
{
    if (!STRG.IsValid()) return nullptr;

    // Verify that this is a valid STRG
    uint Magic = STRG.ReadLong();
    EGame Version = EGame::Invalid;

    if (Magic != 0x87654321)
    {
        // Check for MP1 Demo STRG format - no magic/version; the first value is actually the filesize
        // so the best I can do is verify the first value actually points to the end of the file.
        // The file can have up to 31 padding bytes at the end so we account for that
        if (Magic <= (uint) STRG.Size() && Magic > STRG.Size() - 32)
        {
            Version = EGame::PrimeDemo;
        }

        // If not, then we seem to have an invalid file...
        if (Version != EGame::PrimeDemo)
        {
            errorf("%s: Invalid STRG magic: 0x%08X", *STRG.GetSourceString(), Magic);
            return nullptr;
        }
    }

    else
    {
        uint FileVersion = STRG.ReadLong();
        Version = GetFormatVersion(FileVersion);

        if (Version == EGame::Invalid)
        {
            errorf("%s: Unrecognized STRG version: 0x%X", *STRG.GetSourceString(), FileVersion);
            return nullptr;
        }
    }

    // Valid; now we create the loader and call the function that reads the rest of the file
    CStringLoader Loader;
    Loader.mpStringTable = new CStringTable(pEntry);
    Loader.mVersion = Version;

    if (Version == EGame::PrimeDemo)        Loader.LoadPrimeDemoSTRG(STRG);
    else if (Version < EGame::Corruption)   Loader.LoadPrimeSTRG(STRG);
    else                                    Loader.LoadCorruptionSTRG(STRG);

    return Loader.mpStringTable;
}

EGame CStringLoader::GetFormatVersion(uint32 Version)
{
    switch (Version)
    {
    case 0x0: return EGame::Prime;
    case 0x1: return EGame::Echoes;
    case 0x3: return EGame::Corruption;
    default: return EGame::Invalid;
    }
}
