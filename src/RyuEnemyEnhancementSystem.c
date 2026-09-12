#ifndef GUARD_RYU_ENEMY_ENHANCEMENT_H
#define GUARD_RYU_ENEMY_ENHANCEMENT_H

#include "global.h"
#include "RyuEnemyEnhancementSystem.h"
#include "constants/species.h"
#include "constants/trainers.h"
#include "constants/general.h"
#include "data.h"
#include "event_data.h"
#include "random.h"
#include "ach_atlas.h"

//autoscale data
const u16 sRange[9][2] = {//Trainer level ranges
    {9,13},
    {17,22},
    {23,28},
    {29,34},
    {35,40},
    {42,49},
    {50,57}, 
    {58,65},
    {66,72},
};

const u16 sGymRange[9][2] = {//Gym Leader level ranges
    {11,18},
    {20,24},
    {25,33},
    {34,39},
    {40,45},
    {50,54},
    {55,62}, 
    {69,76},
    {77,84},
};

const u16 sWildRange[9][2] = {//Wild level ranges
    {6,11},
    {10,20},
    {19,29},
    {28,35},
    {36,41},
    {40,45},
    {44,49},
    {48,53},
    {52,60},
};

// Final levels will be player party average + the adjustment here + Random() % 5
static const s16 sAutoscalingAdjustments[3] = {
    [SCALING_TYPE_WILD] = -8,
    [SCALING_TYPE_TRAINER] = -5,
    [SCALING_TYPE_BOSS] = 0, // E4, gym leaders, etc.
};

//functions

/*
 * Calculate the player’s relative party strength for enemy autoscaling.
 * This is defined as the average level of the three strongest Pokemon.
 */
s16 CalculatePlayerPartyStrength(void) {
    u8 partyCount = CalculatePlayerPartyCount();
    s16 highest = 0, second = 0, third = 0, average;
    u8 level, i;

    for (i = 0; i < partyCount; i++) {
        level = GetMonData(&gPlayerParty[i], MON_DATA_LEVEL);
        // shift the previous highest down by one
        if (level > highest) {
            third = second;
            second = highest;
            highest = level;
        } else if (level > second) {
            third = second;
            second = level;
        } else if (level > third) {
            third = level;
        }
    }
    average = (highest + second + third) / min(partyCount, 3);
    // in case people bring very weak mons to drag down the average
    return max(highest - 20, average);
}

u32 RyuChooseLevel(u8 badges, bool8 maxScale, u8 scalingType, s16 playerPartyStrength)
{
    u8 level = 0;
    // Allows overriding the autoscaling from scripts.
    // Usually, this will just be 2 to make sure we don’t generate level 0 or 1 mons.
    u8 minLevel = VarGet(VAR_RYU_AUTOSCALE_MIN_LEVEL);
    u8 maxLevel = MAX_LEVEL;

    if (maxScale)
        return maxLevel;

    // While we are in Ryu’s special challenge, all trainers should be scaled to 95% of the strongest player party member.
    // We therefore subtract at most 5% from the maxLevel (rounded down),
    // so a player with a level 10 party will encounter enemies at level 10,
    // level 20 party encounters level 19-20 enemies, level 40 party encounters level 38-40 enemies, etc.
    // Scaling of wild Pokemon is not affected by the challenge and handled further down with the usual logic.
    if (VarGet(VAR_RYU_SPECIAL_CHALLENGE_STATE) == 100 && scalingType != SCALING_TYPE_WILD) {
        u8 highest = 0, i, level;
        for (i = 0; i < CalculatePlayerPartyCount(); i++) {
            level = GetMonData(&gPlayerParty[i], MON_DATA_LEVEL);
            if (level > highest) highest = level;
        }
        return max(minLevel, highest - Random() % (highest / 20 + 1));
    }

    // Wild pokemon should always use badge scaling, unless the AP is enabled,
    // in which case they always scale to slightly below team level, even before NG+.
    if ((FlagGet(FLAG_RYU_ISNGPLUS) && scalingType != SCALING_TYPE_WILD)
        || (CheckIfAutolevelWilds() && scalingType == SCALING_TYPE_WILD)) {
        // Vars are usually u16, but we need a signed number here.
        // Scripts might write negative numbers which wrap to 65535, but the cast to s16 converts that back to -1.
        s16 autolevelModifier = (s16) VarGet(VAR_RYU_AUTOLEVEL_MODIFIER);
        s16 level = playerPartyStrength
            + (Random() % 5)
            + sAutoscalingAdjustments[scalingType]
            + autolevelModifier;
        return min(max(minLevel, level), maxLevel);
    }

    switch (scalingType) {
        case SCALING_TYPE_BOSS: 
            level = Random() % (sGymRange[badges][1] - sGymRange[badges][0]) + sGymRange[badges][0];
            break;
        case SCALING_TYPE_TRAINER: 
            level = Random() % (sRange[badges][1] - sRange[badges][0]) + sRange[badges][0];
            break;
        case SCALING_TYPE_WILD:
            level = Random() % (sWildRange[badges][1] - sWildRange[badges][0]) + sWildRange[badges][0];
            break;
    }
    gSpecialVar_0x800A = __abs(sAutoscalingAdjustments[scalingType]);
    gSpecialVar_0x800B = MAX_LEVEL;
    return max(level, minLevel);
}

// If either the required level is reached or the mon is level 30
// (at which point we can assume that any reasonable trainer would have found the evolution stone,
// done the necessary trade, etc. for the evolution), evolve the Pokemon.
// This is done so our autoscaled enemies don’t end up with level 200 Zigzagoons.
u16 Autoevolve(u16 species, u16 level) {
    if ((
            (gEvolutionTable[species][0].method == EVO_LEVEL && gEvolutionTable[species][0].param <= level)
            || (gEvolutionTable[species][0].method != EVO_MEGA_EVOLUTION && level >= 30)
        )
        // while there is an evolution available at all (the method is 0 if there isn’t)
        && gEvolutionTable[species][0].method != 0
        // and we’re not in a circular evolution (shouldn’t happen, but just in case)
        && species != gEvolutionTable[species][0].targetSpecies
    ) {
        return Autoevolve(gEvolutionTable[species][0].targetSpecies, level);
    }
    return species;
}
extern void RyuSetPartyFrontierMon(bool32 opponent, u16 id, u8 slot);

u16 RyuChooseEnemyProceduralMons(u16 trainerClass)
{
    u16 species = gRyuProceduralTrainerMonLists[trainerClass][(Random() % 20)];
    if ((species == SPECIES_NONE) || (species > SPECIES_MELMETAL))
    {
        species = SPECIES_BIDOOF;
        VarSet(VAR_RYU_AUTOFILL_ERROR_COUNT, (VarGet(VAR_RYU_AUTOFILL_ERROR_COUNT) + 1));
    }

    return species;
}

u16 RyuChooseAutoscaleIV(void)
{
    u16 diff = VarGet(VAR_RYU_DIFFICULTY);
    u16 prestige = VarGet(VAR_RYU_NGPLUS_COUNT);
    u16 iv = 0;
    if (prestige == 0)
        prestige = 1;

    switch (diff)
    {
    case DIFF_NORMAL:
        iv = (5 * prestige);
        if (iv > 31)
            iv = 31;
        return iv;
    case DIFF_HARD:
        iv = (7 * prestige);
        if (iv > 31)
            iv = 31;
        return iv;
    case DIFF_EASY:
        iv = (3 * prestige);
        if (iv > 31)
            iv = 31;
        return iv;
    case DIFF_HARDCORE:
        iv = (10 * prestige);
        if (iv > 31)
            iv = 31;
        return iv;
    }
    return 31;
}

u8 RyuChooseAutoscaleEv(void)
{
    u16 diff = (VarGet(VAR_RYU_DIFFICULTY));
    u16 prestige = (VarGet(VAR_RYU_NGPLUS_COUNT));
    u8 badges = (CountBadges());
    u8 ev = 16;
    badges *= 10;
    if (prestige == 0)
    prestige = 1;

    switch (diff)
    {
    case 1://autoscale
    case 10://autoscale NGP
        ev = (25 * prestige);
        if (ev > 252)
            ev = 252;
        return (ev + badges);
    case 2000://challenge mode
        ev = (50 * prestige);
        if (ev > 252)
            ev = 252;
        return (ev + badges);
    case 4000://easy mode
    case 8000://god mode
        ev = (10 * prestige);
        if (ev > 252)
            ev = 252;
        return (ev + badges);
    case 1000://hard/hardcore
        ev = (75 * prestige);
        if (ev > 252)
            ev = 252;
        return (ev + badges);
    }
    return 255;

}

u8 RyuChoosePartyCount(u16 trainer)
{
    u8 count = ((Random() % 4) + 1);

    if (VarGet(VAR_RYU_QUEST_MAGMA) == 315)
        count += 2;

    switch (VarGet(VAR_RYU_DIFFICULTY))
    {
        case DIFF_NORMAL:
            count += 1;
            break;
        case DIFF_HARD:
            count += 2;
            break;
        case DIFF_HARDCORE:
            count = 6;
            break;
    }
    if (count > 6)
        count = 6;
    if (count < 1)
        count = 2;
    if (gTrainers[trainer].doubleBattle == TRUE)
        if (count < 2)
            count = 2;
    if (FlagGet(FLAG_RYU_FORCE_FULL_AUTOFILL_PARTY) == TRUE) //used for autofilled bosses
            count = PARTY_SIZE;
    return count;
}

//procedural trainer mon table
const u16 gRyuProceduralTrainerMonLists[72][20] = {
[TRAINER_CLASS_AETHER_WORKER] =
{
        SPECIES_ABRA,
        SPECIES_PSYDUCK,
        SPECIES_DROWZEE,
        SPECIES_GIRAFARIG,
        SPECIES_NATU,
        SPECIES_RALTS,
        SPECIES_NATU,
        SPECIES_SIGILYPH,
        SPECIES_SLOWPOKE,
        SPECIES_EXEGGCUTE,
        SPECIES_ZORUA,
        SPECIES_DARUMAKA,
        SPECIES_BELDUM,
        SPECIES_PANCHAM,
        SPECIES_TRAPINCH,
        SPECIES_SNUBBULL,
        SPECIES_SOLOSIS,
        SPECIES_WYNAUT,
        SPECIES_SPOINK,
        SPECIES_SCYTHER
},
[TRAINER_CLASS_AETHER_ADMIN] =
{
        SPECIES_ALAKAZAM,
        SPECIES_SOLROCK,
        SPECIES_HYPNO,
        SPECIES_GIRAFARIG,
        SPECIES_TURTONATOR,
        SPECIES_GARDEVOIR,
        SPECIES_XATU,
        SPECIES_SIGILYPH,
        SPECIES_PROBOPASS,
        SPECIES_EXEGGUTOR,
        SPECIES_GALLADE,
        SPECIES_DARMANITAN,
        SPECIES_METAGROSS,
        SPECIES_FLYGON,
        SPECIES_CLAWITZER,
        SPECIES_GRANBULL,
        SPECIES_DHELMISE,
        SPECIES_WOBBUFFET,
        SPECIES_SCIZOR,
        SPECIES_AERODACTYL
},
[TRAINER_CLASS_TEAM_AQUA] = 
    {
        SPECIES_CARVANHA, 
        SPECIES_POOCHYENA, 
        SPECIES_FRILLISH,
        SPECIES_LAPRAS,
        SPECIES_POPPLIO,
        SPECIES_TENTACOOL,
        SPECIES_CLAMPERL,
        SPECIES_KRABBY,
        SPECIES_CORPHISH,
        SPECIES_MAGIKARP,
        SPECIES_WIMPOD,
        SPECIES_EEVEE,
        SPECIES_FINNEON,
        SPECIES_FEEBAS,
        SPECIES_OSHAWOTT,
        SPECIES_BASCULIN,
        SPECIES_QWILFISH,
        SPECIES_HORSEA,
        SPECIES_SNEASEL,
        SPECIES_MUDKIP
    },
[TRAINER_CLASS_AQUA_ADMIN] = 
    {
        SPECIES_SHARPEDO, 
        SPECIES_JELLICENT, 
        SPECIES_LAPRAS,
        SPECIES_BRIONNE,
        SPECIES_GOREBYSS,
        SPECIES_KINGLER,
        SPECIES_CRAWDAUNT,
        SPECIES_GYARADOS,
        SPECIES_GOLISOPOD,
        SPECIES_DEWOTT,
        SPECIES_BASCULIN,
        SPECIES_QWILFISH,
        SPECIES_SEADRA,
        SPECIES_SEALEO,
        SPECIES_HARIYAMA,
        SPECIES_TENTACRUEL,
        SPECIES_SWALOT,
        SPECIES_ZANGOOSE,
        SPECIES_CASTFORM,
        SPECIES_BANETTE
    },
[TRAINER_CLASS_AQUA_LEADER] = 
    {
        SPECIES_SHARPEDO, 
        SPECIES_HARIYAMA, 
        SPECIES_JELLICENT,
        SPECIES_LAPRAS,
        SPECIES_PRIMARINA,
        SPECIES_GOREBYSS,
        SPECIES_KINGLER,
        SPECIES_CRAWDAUNT,
        SPECIES_GYARADOS,
        SPECIES_GOLISOPOD,
        SPECIES_SAMUROTT,
        SPECIES_BASCULIN,
        SPECIES_QWILFISH,
        SPECIES_KINGDRA,
        SPECIES_SWAMPERT,
        SPECIES_DUSKNOIR,
        SPECIES_GLALIE,
        SPECIES_WALREIN,
        SPECIES_RELICANTH,
        SPECIES_METAGROSS
    },
[TRAINER_CLASS_AROMA_LADY] = 
    {
        SPECIES_PARAS,
        SPECIES_CHIKORITA,
        SPECIES_AIPOM,
        SPECIES_VENONAT,
        SPECIES_MUNCHLAX,
        SPECIES_WURMPLE,
        SPECIES_WURMPLE,
        SPECIES_CHERUBI,
        SPECIES_HERACROSS,
        SPECIES_DURANT,
        SPECIES_CARNIVINE,
        SPECIES_LEDYBA,
        SPECIES_HOPPIP,
        SPECIES_SMOOCHUM,
        SPECIES_MILTANK,
        SPECIES_SUNKERN,
        SPECIES_PETILIL,
        SPECIES_TEDDIURSA,
        SPECIES_EXEGGCUTE,
        SPECIES_PICHU,
    },
[TRAINER_CLASS_RUIN_MANIAC] = 
    {
        SPECIES_SANDSHREW,
        SPECIES_NIDORAN_M,
        SPECIES_DIGLETT,
        SPECIES_GEODUDE,
        SPECIES_OMANYTE,
        SPECIES_KABUTO,
        SPECIES_AERODACTYL,
        SPECIES_DUNSPARCE,
        SPECIES_SWINUB,
        SPECIES_CORSOLA,
        SPECIES_BALTOY,
        SPECIES_LILEEP,
        SPECIES_ANORITH,
        SPECIES_RELICANTH,
        SPECIES_CRANIDOS,
        SPECIES_SHIELDON,
        SPECIES_TIRTOUGA,
        SPECIES_ARCHEN,
        SPECIES_TYRUNT,
        SPECIES_AERODACTYL,
    },
[TRAINER_CLASS_INTERVIEWER] = 
    {
        SPECIES_CHARMANDER,
        SPECIES_SQUIRTLE,
        SPECIES_PICHU,
        SPECIES_GROWLITHE,
        SPECIES_VULPIX,
        SPECIES_MEOWTH,
        SPECIES_MAGNEMITE,
        SPECIES_WHISMUR,
        SPECIES_EEVEE,
        SPECIES_MUNCHLAX,
        SPECIES_MUDKIP,
        SPECIES_TRAPINCH,
        SPECIES_TURTWIG,
        SPECIES_CHIMCHAR,
        SPECIES_SHINX,
        SPECIES_RALTS,
        SPECIES_GIBLE,
        SPECIES_RIOLU,
        SPECIES_MIMIKYU,
        SPECIES_FROAKIE,
    },
[TRAINER_CLASS_COOLTRAINER_2] = 
    {
        SPECIES_ELECTRIKE,
        SPECIES_EEVEE,
        SPECIES_AXEW,
        SPECIES_LAPRAS,
        SPECIES_MAGIKARP,
        SPECIES_DRATINI,
        SPECIES_GOOMY,
        SPECIES_ABSOL,
        SPECIES_HONEDGE,
        SPECIES_VULPIX,
        SPECIES_VULPIX,
        SPECIES_GASTLY,
        SPECIES_RUFFLET,
        SPECIES_RHYHORN,
        SPECIES_SNORUNT,
        SPECIES_EEVEE,
        SPECIES_BUDEW,
        SPECIES_SEVIPER,
        SPECIES_ZANGOOSE,
        SPECIES_ONIX
    },
[TRAINER_CLASS_TUBER_F] = 
    {
        SPECIES_TENTACOOL,
        SPECIES_SEEL,
        SPECIES_SHELLDER,
        SPECIES_GOLDEEN,
        SPECIES_CLEFFA,
        SPECIES_LAPRAS,
        SPECIES_MAGIKARP,
        SPECIES_CHINCHOU,
        SPECIES_WAILMER,
        SPECIES_SURSKIT,
        SPECIES_BUNEARY,
        SPECIES_SPHEAL,
        SPECIES_BOUNSWEET,
        SPECIES_PIPLUP,
        SPECIES_DELIBIRD,
        SPECIES_LUVDISC,
        SPECIES_MANTYKE,
        SPECIES_FRILLISH,
        SPECIES_POPPLIO,
        SPECIES_MUNCHLAX,
    },
[TRAINER_CLASS_TUBER_M] = 
    {
        SPECIES_HORSEA,
        SPECIES_TOTODILE,
        SPECIES_CHINCHOU,
        SPECIES_WOOPER,
        SPECIES_REMORAID,
        SPECIES_MUDKIP,
        SPECIES_WINGULL,
        SPECIES_MAGIKARP,
        SPECIES_CARVANHA,
        SPECIES_CLAMPERL,
        SPECIES_PIPLUP,
        SPECIES_SHELLOS,
        SPECIES_FINNEON,
        SPECIES_MANTYKE,
        SPECIES_TYMPOLE,
        SPECIES_TIRTOUGA,
        SPECIES_TYNAMO,
        SPECIES_FROAKIE,
        SPECIES_CLAUNCHER,
        SPECIES_WIMPOD,
    },
[TRAINER_CLASS_SIS_AND_BRO] = 
    {
        SPECIES_NIDORAN_M,
        SPECIES_NIDORAN_F,
        SPECIES_PSYDUCK,
        SPECIES_POLIWAG,
        SPECIES_TENTACOOL,
        SPECIES_SLOWPOKE,
        SPECIES_SEEL,
        SPECIES_SHELLDER,
        SPECIES_HORSEA,
        SPECIES_GOLDEEN,
        SPECIES_MAGIKARP,
        SPECIES_DRATINI,
        SPECIES_NATU,
        SPECIES_WINGULL,
        SPECIES_MUDKIP,
        SPECIES_SPINDA,
        SPECIES_LILEEP,
        SPECIES_OSHAWOTT,
        SPECIES_TIRTOUGA,
    },
[TRAINER_CLASS_COOLTRAINER] = 
    {
        SPECIES_HONEDGE,
        SPECIES_GROWLITHE,
        SPECIES_GASTLY,
        SPECIES_ELEKID,
        SPECIES_MAGBY,
        SPECIES_MAGIKARP,
        SPECIES_MUNCHLAX,
        SPECIES_AERODACTYL,
        SPECIES_HAPPINY,
        SPECIES_DRATINI,
        SPECIES_LARVITAR,
        SPECIES_SLAKOTH,
        SPECIES_SPHEAL,
        SPECIES_BAGON,
        SPECIES_BELDUM,
        SPECIES_GIBLE,
        SPECIES_CHIMCHAR,
        SPECIES_RIOLU,
        SPECIES_GOOMY,
        SPECIES_TERRAKION,
    },
[TRAINER_CLASS_HEX_MANIAC] = 
    {
        SPECIES_PARAS,
        SPECIES_GASTLY,
        SPECIES_SLOWPOKE,
        SPECIES_SLOWPOKE,
        SPECIES_DROWZEE,
        SPECIES_CUBONE,
        SPECIES_PORYGON,
        SPECIES_MISDREAVUS,
        SPECIES_WYNAUT,
        SPECIES_SABLEYE,
        SPECIES_SPINDA,
        SPECIES_NINCADA,
        SPECIES_LUNATONE,
        SPECIES_DUSKULL,
        SPECIES_SHUPPET,
        SPECIES_ROTOM,
        SPECIES_YAMASK,
        SPECIES_LITWICK,
        SPECIES_PHANTUMP,
        SPECIES_MIMIKYU,
    },
[TRAINER_CLASS_LADY] = 
    {
        SPECIES_CHARMANDER,
        SPECIES_CLEFFA,
        SPECIES_MEOWTH,
        SPECIES_KANGASKHAN,
        SPECIES_POLIWAG,
        SPECIES_CHINCHOU,
        SPECIES_MISDREAVUS,
        SPECIES_ZIGZAGOON,
        SPECIES_BUDEW,
        SPECIES_SABLEYE,
        SPECIES_BUNEARY,
        SPECIES_SCYTHER,
        SPECIES_SNEASEL,
        SPECIES_TORKOAL,
        SPECIES_ZORUA,
        SPECIES_LITWICK,
        SPECIES_AXEW,
        SPECIES_DEINO,
        SPECIES_FENNEKIN,
        SPECIES_AMAURA,
    },
[TRAINER_CLASS_BEAUTY] = 
    {
        SPECIES_CLEFFA,
        SPECIES_VULPIX,
        SPECIES_MAREEP,
        SPECIES_ODDISH,
        SPECIES_SNUBBULL,
        SPECIES_EEVEE,
        SPECIES_AZURILL,
        SPECIES_WURMPLE,
        SPECIES_RALTS,
        SPECIES_LUVDISC,
        SPECIES_FEEBAS,
        SPECIES_BUNEARY,
        SPECIES_AUDINO,
        SPECIES_VENIPEDE,
        SPECIES_PETILIL,
        SPECIES_COTTONEE,
        SPECIES_FENNEKIN,
        SPECIES_EEVEE,
        SPECIES_EEVEE,
        SPECIES_POPPLIO,
    },
[TRAINER_CLASS_RICH_BOY] = 
    {
        SPECIES_SQUIRTLE,
        SPECIES_MEOWTH,
        SPECIES_SNEASEL,
        SPECIES_DURANT,
        SPECIES_BONSLY,
        SPECIES_LITWICK,
        SPECIES_DRATINI,
        SPECIES_EEVEE,
        SPECIES_CHESPIN,
        SPECIES_ROCKRUFF,
        SPECIES_SIGILYPH,
        SPECIES_LILLIPUP,
        SPECIES_OSHAWOTT,
        SPECIES_ORANGURU,
        SPECIES_HORSEA,
        SPECIES_PHANPY,
        SPECIES_GIBLE,
        SPECIES_SHINX,
        SPECIES_STARLY,
        SPECIES_RUFFLET,
    },
[TRAINER_CLASS_POKEMANIAC] = 
    {
        SPECIES_POPPLIO,
        SPECIES_DURANT,
        SPECIES_BELDUM,
        SPECIES_PICHU,
        SPECIES_DARUMAKA,
        SPECIES_ZORUA,
        SPECIES_SOLOSIS,
        SPECIES_PAWNIARD,
        SPECIES_CATERPIE,
        SPECIES_GROWLITHE,
        SPECIES_SHIELDON,
        SPECIES_COTTONEE,
        SPECIES_CRANIDOS,
        SPECIES_LUNATONE,
        SPECIES_BINACLE,
        SPECIES_BELDUM,
        SPECIES_BALTOY,
        SPECIES_CARBINK,
        SPECIES_STARLY,
    },
[TRAINER_CLASS_SWIMMER_M] = 
    {
        SPECIES_PSYDUCK,
        SPECIES_POLIWAG,
        SPECIES_TENTACOOL,
        SPECIES_SLOWPOKE,
        SPECIES_SEEL,
        SPECIES_HORSEA,
        SPECIES_LOTAD,
        SPECIES_WINGULL,
        SPECIES_SEEDOT,
        SPECIES_AZURILL,
        SPECIES_SABLEYE,
        SPECIES_SKRELP,
        SPECIES_CATERPIE,
        SPECIES_SOLROCK,
        SPECIES_AMAURA,
        SPECIES_MANKEY,
        SPECIES_ZORUA,
        SPECIES_SWABLU,
        SPECIES_STUNFISK,
        SPECIES_DEINO,
    },
[TRAINER_CLASS_BLACK_BELT] = 
    {
        SPECIES_HERACROSS,
        SPECIES_CROAGUNK,
        SPECIES_GIBLE,
        SPECIES_RALTS,
        SPECIES_SHROOMISH,
        SPECIES_LARVITAR,
        SPECIES_SLAKOTH,
        SPECIES_TORCHIC,
        SPECIES_PINSIR,
        SPECIES_MACHOP,
        SPECIES_SCYTHER,
        SPECIES_AXEW,
        SPECIES_RIOLU,
        SPECIES_CARVANHA,
        SPECIES_TIMBURR,
        SPECIES_MACHOP,
        SPECIES_SCRAGGY,
        SPECIES_EEVEE,
        SPECIES_EEVEE,
        SPECIES_PASSIMIAN,
    },
[TRAINER_CLASS_GUITARIST] = 
    {
        SPECIES_NINCADA,
        SPECIES_VOLTORB,
        SPECIES_PINECO,
        SPECIES_AERODACTYL,
        SPECIES_TEPIG,
        SPECIES_TREECKO,
        SPECIES_ELECTRIKE,
        SPECIES_EEVEE,
        SPECIES_ZUBAT,
        SPECIES_NOIBAT,
        SPECIES_ELEKID,
        SPECIES_NOIBAT,
        SPECIES_WHISMUR,
        SPECIES_PICHU,
        SPECIES_HOUNDOUR,
        SPECIES_NATU,
        SPECIES_EEVEE,
        SPECIES_KOFFING,
        SPECIES_GRIMER,
        SPECIES_MAGNEMITE,
    },
[TRAINER_CLASS_KINDLER] = 
    {
        SPECIES_SLUGMA,
        SPECIES_NUMEL,
        SPECIES_LILLIPUP,
        SPECIES_DRIFLOON,
        SPECIES_CHARMANDER,
        SPECIES_FENNEKIN,
        SPECIES_CATERPIE,
        SPECIES_FLETCHLING,
        SPECIES_DUSKULL,
        SPECIES_TEPIG,
        SPECIES_CHESPIN,
        SPECIES_PHANTUMP,
        SPECIES_SNOVER,
        SPECIES_TROPIUS,
        SPECIES_PUMPKABOO,
        SPECIES_LITTEN,
        SPECIES_TURTONATOR,
        SPECIES_TYNAMO,
        SPECIES_MANKEY,
        SPECIES_PHANTUMP,
    },
[TRAINER_CLASS_CAMPER] = 
    {
        SPECIES_CACNEA,
        SPECIES_GULPIN,
        SPECIES_ODDISH,
        SPECIES_SANDSHREW,
        SPECIES_SWABLU,
        SPECIES_IGGLYBUFF,
        SPECIES_SHINX,
        SPECIES_NIDORAN_F,
        SPECIES_NIDORAN_M,
        SPECIES_PONYTA,
        SPECIES_SUNKERN,
        SPECIES_HOPPIP,
        SPECIES_MAREEP,
        SPECIES_CUBONE,
        SPECIES_VULPIX,
        SPECIES_WHISMUR,
        SPECIES_BELLSPROUT,
        SPECIES_MUDBRAY,
        SPECIES_SPOINK,
        SPECIES_CLEFFA
    },
[TRAINER_CLASS_OLD_COUPLE] = 
    {
        SPECIES_TYROGUE,
        SPECIES_TYROGUE,
        SPECIES_LARVESTA,
        SPECIES_MAKUHITA,
        SPECIES_TANGELA,
        SPECIES_RIOLU,
        SPECIES_SNIVY,
        SPECIES_MAGNEMITE,
        SPECIES_MAWILE,
        SPECIES_HAWLUCHA,
        SPECIES_FROAKIE,
        SPECIES_GLIGAR,
        SPECIES_TYROGUE,
        SPECIES_GIBLE,
        SPECIES_DRILBUR,
        SPECIES_SHROOMISH,
        SPECIES_ARON,
        SPECIES_VIRIZION,
        SPECIES_TERRAKION,
        SPECIES_COBALION,
    },
[TRAINER_CLASS_BUG_MANIAC] = 
    {
        SPECIES_KABUTO,
        SPECIES_GRUBBIN,
        SPECIES_CARNIVINE,
        SPECIES_CATERPIE,
        SPECIES_WEEDLE,
        SPECIES_GRUBBIN,
        SPECIES_PINSIR,
        SPECIES_DUSKULL,
        SPECIES_YANMA,
        SPECIES_NOIBAT,
        SPECIES_SCYTHER,
        SPECIES_DEWPIDER,
        SPECIES_MIMIKYU,
        SPECIES_MUNCHLAX,
        SPECIES_TRAPINCH,
        SPECIES_VENIPEDE,
        SPECIES_DWEBBLE,
        SPECIES_LARVESTA,
        SPECIES_SCRAGGY,
        SPECIES_HERACROSS
    },
[TRAINER_CLASS_PSYCHIC] = 
    {
        SPECIES_NATU,
        SPECIES_SIGILYPH,
        SPECIES_BELDUM,
        SPECIES_ABRA,
        SPECIES_TOGEPI,
        SPECIES_PORYGON,
        SPECIES_RALTS,
        SPECIES_NATU,
        SPECIES_EEVEE,
        SPECIES_SLOWPOKE,
        SPECIES_AUDINO,
        SPECIES_UNOWN,
        SPECIES_GOLETT,
        SPECIES_PSYDUCK,
        SPECIES_ELGYEM,
        SPECIES_EXEGGCUTE,
        SPECIES_COTTONEE,
        SPECIES_SANDYGAST,
        SPECIES_MAGBY,
        SPECIES_YANMA
    },
[TRAINER_CLASS_GENTLEMAN] = 
    {
        SPECIES_SABLEYE,
        SPECIES_ZIGZAGOON,
        SPECIES_DUNSPARCE,
        SPECIES_MARACTUS,
        SPECIES_TYMPOLE,
        SPECIES_MURKROW,
        SPECIES_PIKIPEK,
        SPECIES_MINCCINO,
        SPECIES_HONEDGE,
        SPECIES_EEVEE,
        SPECIES_EEVEE,
        SPECIES_BERGMITE,
        SPECIES_ELECTRIKE,
        SPECIES_RUFFLET,
        SPECIES_TRAPINCH,
        SPECIES_MAREEP,
        SPECIES_SLOWPOKE,
        SPECIES_MAREANIE,
        SPECIES_TOGEDEMARU,
        SPECIES_LILLIPUP
    },
[TRAINER_CLASS_SCHOOL_KID] = 
    {
        SPECIES_RALTS,
        SPECIES_LITWICK,
        SPECIES_WURMPLE,
        SPECIES_CATERPIE,
        SPECIES_WINGULL,
        SPECIES_DEERLING,
        SPECIES_IGGLYBUFF,
        SPECIES_EEVEE,
        SPECIES_BELLSPROUT,
        SPECIES_LUVDISC,
        SPECIES_DRIFLOON,
        SPECIES_BLITZLE,
        SPECIES_HOOTHOOT,
        SPECIES_CATERPIE,
        SPECIES_STARLY,
        SPECIES_PIDGEY,
        SPECIES_MAGIKARP,
        SPECIES_SHROOMISH,
        SPECIES_PARAS,
        SPECIES_VANILLITE
    },
[TRAINER_CLASS_SR_AND_JR] = 
    {
        SPECIES_ZIGZAGOON,
        SPECIES_MANKEY,
        SPECIES_RATTATA,
        SPECIES_NIDORAN_F,
        SPECIES_NIDORAN_M,
        SPECIES_POOCHYENA,
        SPECIES_RATTATA,
        SPECIES_ZORUA,
        SPECIES_SANDILE,
        SPECIES_SLAKOTH,
        SPECIES_NUMEL,
        SPECIES_BUDEW,
        SPECIES_SHINX,
        SPECIES_PONYTA,
        SPECIES_RHYHORN,
        SPECIES_SPINDA,
        SPECIES_AZURILL,
        SPECIES_PICHU,
        SPECIES_EEVEE,   
        SPECIES_CHIKORITA,   
    },
[TRAINER_CLASS_POKEFAN] = 
    {
        SPECIES_EMOLGA,
        SPECIES_EMOLGA,
        SPECIES_TEDDIURSA,
        SPECIES_STUFFUL,
        SPECIES_SNUBBULL,
        SPECIES_SNORUNT,
        SPECIES_EEVEE,
        SPECIES_BUNEARY,
        SPECIES_PICHU,
        SPECIES_CUBCHOO,
        SPECIES_CLEFFA,
        SPECIES_LILLIPUP,
        SPECIES_PURRLOIN,
        SPECIES_HOUNDOUR,
        SPECIES_SKITTY,
        SPECIES_POPPLIO,
        SPECIES_TURTWIG,
        SPECIES_SKITTY,
        SPECIES_EEVEE,
        SPECIES_IGGLYBUFF,

    },
[TRAINER_CLASS_EXPERT] = 
    {
        SPECIES_TIMBURR,
        SPECIES_MAKUHITA,
        SPECIES_TIMBURR,
        SPECIES_MAKUHITA,
        SPECIES_HAWLUCHA,
        SPECIES_RIOLU,
        SPECIES_HERACROSS,
        SPECIES_ABSOL,
        SPECIES_MAWILE,
        SPECIES_BELDUM,
        SPECIES_TORCHIC,
        SPECIES_TAILLOW,
        SPECIES_TYROGUE,
        SPECIES_TREECKO,
        SPECIES_MUDKIP,
        SPECIES_SHROOMISH,
        SPECIES_RALTS,
        SPECIES_VIRIZION,
        SPECIES_TERRAKION,
        SPECIES_COBALION,
    },
[TRAINER_CLASS_YOUNGSTER] = 
    {
        SPECIES_GRUBBIN,
        SPECIES_CATERPIE,
        SPECIES_WEEDLE,
        SPECIES_ROCKRUFF,
        SPECIES_ZORUA,
        SPECIES_EMOLGA,
        SPECIES_TAUROS,
        SPECIES_RATTATA,
        SPECIES_PICHU,
        SPECIES_SLAKOTH,
        SPECIES_EKANS,
        SPECIES_TRAPINCH,
        SPECIES_MACHOP,
        SPECIES_SENTRET,
        SPECIES_HOOTHOOT,
        SPECIES_BUNEARY,
        SPECIES_PIDGEY,
        SPECIES_SPEAROW,
        SPECIES_NIDORAN_M,
        SPECIES_AZURILL
    },
[TRAINER_CLASS_FISHERMAN] = 
    {
        SPECIES_BINACLE,
        SPECIES_TIRTOUGA,
        SPECIES_DRATINI,
        SPECIES_MAGIKARP,
        SPECIES_FEEBAS,
        SPECIES_TENTACOOL,
        SPECIES_CHINCHOU,
        SPECIES_POLIWAG,
        SPECIES_GOLDEEN,
        SPECIES_TYNAMO,
        SPECIES_TYNAMO,
        SPECIES_WAILMER,
        SPECIES_REMORAID,
        SPECIES_CARVANHA,
        SPECIES_BARBOACH,
        SPECIES_LUVDISC,
        SPECIES_SHINX,
        SPECIES_BERGMITE,
        SPECIES_WIMPOD,
        SPECIES_DRATINI
    },
[TRAINER_CLASS_TRIATHLETE] = 
    {
        SPECIES_VOLTORB,
        SPECIES_MAGNEMITE,
        SPECIES_BELDUM,
        SPECIES_WAILMER,
        SPECIES_TRAPINCH,
        SPECIES_AZURILL,
        SPECIES_DODUO,
        SPECIES_ZUBAT,
        SPECIES_GROWLITHE,
        SPECIES_MACHOP,
        SPECIES_RIOLU,
        SPECIES_TAILLOW,
        SPECIES_KABUTO,
        SPECIES_SENTRET,
        SPECIES_SNEASEL,
        SPECIES_ZIGZAGOON,
        SPECIES_SURSKIT,
        SPECIES_ZANGOOSE,
        SPECIES_AZURILL,
        SPECIES_OSHAWOTT
    },
[TRAINER_CLASS_DRAGON_TAMER] = 
    {
        SPECIES_GIBLE,
        SPECIES_MAGIKARP,
        SPECIES_TRAPINCH,
        SPECIES_DRATINI,
        SPECIES_HORSEA,
        SPECIES_BAGON,
        SPECIES_NOIBAT,
        SPECIES_AXEW,
        SPECIES_GOOMY,
        SPECIES_DEINO,
        SPECIES_TYRUNT,
        SPECIES_DRATINI,
        SPECIES_TURTONATOR,
        SPECIES_CHARMANDER,
        SPECIES_SKARMORY,
        SPECIES_SWABLU,
        SPECIES_AERODACTYL,
        SPECIES_LARVITAR,
        SPECIES_TREECKO,
        SPECIES_FEEBAS
    },
[TRAINER_CLASS_BIRD_KEEPER] = 
    {
        SPECIES_NATU,
        SPECIES_SIGILYPH,
        SPECIES_PIDGEY,
        SPECIES_SPEAROW,
        SPECIES_TAILLOW,
        SPECIES_SWABLU,
        SPECIES_TROPIUS,
        SPECIES_TOGEPI,
        SPECIES_MURKROW,
        SPECIES_HOOTHOOT,
        SPECIES_STARLY,
        SPECIES_SKARMORY,
        SPECIES_FLETCHLING,
        SPECIES_FARFETCHD,
        SPECIES_DELIBIRD,
        SPECIES_FARFETCHD,
        SPECIES_TAILLOW,
        SPECIES_STARLY,
        SPECIES_MANTYKE,
        SPECIES_STARLY
    },
[TRAINER_CLASS_NINJA_BOY] = 
    {
        SPECIES_KOFFING,
        SPECIES_NINCADA,
        SPECIES_ZUBAT,
        SPECIES_SHINX,
        SPECIES_VENONAT,
        SPECIES_SKORUPI,
        SPECIES_SKORUPI,
        SPECIES_ZUBAT,
        SPECIES_SEVIPER,
        SPECIES_EKANS,
        SPECIES_ARCHEN,
        SPECIES_WURMPLE,
        SPECIES_EMOLGA,
        SPECIES_SCYTHER,
        SPECIES_FROAKIE,
        SPECIES_CROAGUNK,
        SPECIES_BUNNELBY,
        SPECIES_DELIBIRD,
        SPECIES_TRAPINCH,
        SPECIES_GASTLY
    },
[TRAINER_CLASS_BATTLE_GIRL] = 
    {
        SPECIES_MAKUHITA,
        SPECIES_TIMBURR,
        SPECIES_CACNEA,
        SPECIES_MIENFOO,
        SPECIES_HAWLUCHA,
        SPECIES_TIMBURR,
        SPECIES_SCRAGGY,
        SPECIES_RIOLU,
        SPECIES_SENTRET,
        SPECIES_MACHOP,
        SPECIES_POLIWAG,
        SPECIES_MANKEY,
        SPECIES_TYROGUE,
        SPECIES_GROWLITHE,
        SPECIES_GOLDEEN,
        SPECIES_TAILLOW,
        SPECIES_NINCADA,
        SPECIES_MEDITITE,
        SPECIES_GIBLE,
        SPECIES_DURANT
    },
[TRAINER_CLASS_PARASOL_LADY] = 
    {
        SPECIES_CASTFORM,
        SPECIES_VULPIX,
        SPECIES_SPHEAL,
        SPECIES_SEEL,
        SPECIES_NUMEL,
        SPECIES_DRIFLOON,
        SPECIES_SURSKIT,
        SPECIES_PIPLUP,
        SPECIES_HIPPOPOTAS,
        SPECIES_SHELLOS,
        SPECIES_MARACTUS,
        SPECIES_CLAUNCHER,
        SPECIES_SNOVER,
        SPECIES_SUNKERN,
        SPECIES_CHERUBI,
        SPECIES_FOMANTIS,
        SPECIES_ZIGZAGOON,
        SPECIES_SNORUNT,
        SPECIES_BULBASAUR,
        SPECIES_SMOOCHUM
    },
[TRAINER_CLASS_SWIMMER_F] = 
    {
        SPECIES_FARFETCHD,
        SPECIES_SURSKIT,
        SPECIES_WINGULL,
        SPECIES_BASCULIN,
        SPECIES_PSYDUCK,
        SPECIES_CLAMPERL,
        SPECIES_TENTACOOL,
        SPECIES_REMORAID,
        SPECIES_MAREANIE,
        SPECIES_MANTYKE,
        SPECIES_PYUKUMUKU,
        SPECIES_SLOWPOKE,
        SPECIES_GOLDEEN,
        SPECIES_MAGIKARP,
        SPECIES_SHELLDER,
        SPECIES_CORSOLA,
        SPECIES_RELICANTH,
        SPECIES_CRABRAWLER,
        SPECIES_BINACLE,
        SPECIES_BARBOACH
    },
[TRAINER_CLASS_PICNICKER] = 
    {
        SPECIES_CACNEA,
        SPECIES_GULPIN,
        SPECIES_ODDISH,
        SPECIES_SANDSHREW,
        SPECIES_MUNCHLAX,
        SPECIES_IGGLYBUFF,
        SPECIES_SHINX,
        SPECIES_NIDORAN_F,
        SPECIES_NIDORAN_M,
        SPECIES_PONYTA,
        SPECIES_SNUBBULL,
        SPECIES_LEDYBA,
        SPECIES_MAREEP,
        SPECIES_EEVEE,
        SPECIES_TOGEPI,
        SPECIES_AZURILL,
        SPECIES_SWABLU,
        SPECIES_CHERUBI,
        SPECIES_HAPPINY,
        SPECIES_LICKITUNG
    },
[TRAINER_CLASS_TWINS] = 
    {
        SPECIES_COTTONEE,
        SPECIES_PETILIL,
        SPECIES_CORSOLA,
        SPECIES_LUVDISC,
        SPECIES_IGGLYBUFF,
        SPECIES_MINUN,
        SPECIES_PLUSLE,
        SPECIES_LOTAD,
        SPECIES_SEEDOT,
        SPECIES_WURMPLE,
        SPECIES_CLEFFA,
        SPECIES_IGGLYBUFF,
        SPECIES_SUNKERN,
        SPECIES_SWABLU,
        SPECIES_ELEKID,
        SPECIES_AZURILL,
        SPECIES_PURRLOIN,
        SPECIES_COTTONEE,
        SPECIES_PHANPY,
        SPECIES_MIMIKYU
    },
[TRAINER_CLASS_SAILOR] = 
    {
        SPECIES_WOOPER,
        SPECIES_WAILMER,
        SPECIES_WINGULL,
        SPECIES_MACHOP,
        SPECIES_MAKUHITA,
        SPECIES_MIENFOO,
        SPECIES_MAKUHITA,
        SPECIES_TIMBURR,
        SPECIES_TYROGUE,
        SPECIES_TENTACOOL,
        SPECIES_FRILLISH,
        SPECIES_LAPRAS,
        SPECIES_KRABBY,
        SPECIES_HORSEA,
        SPECIES_SLOWPOKE,
        SPECIES_MAGIKARP,
        SPECIES_POLIWAG,
        SPECIES_MANKEY,
        SPECIES_PSYDUCK,
        SPECIES_SEEL
    },
[TRAINER_CLASS_COLLECTOR] = 
    {
        SPECIES_LARVESTA,
        SPECIES_LARVITAR,
        SPECIES_COTTONEE,
        SPECIES_PASSIMIAN,
        SPECIES_DRATINI,
        SPECIES_MUNCHLAX,
        SPECIES_BAGON,
        SPECIES_SEVIPER,
        SPECIES_EKANS,
        SPECIES_ZANGOOSE,
        SPECIES_DURANT,
        SPECIES_HERACROSS,
        SPECIES_PORYGON,
        SPECIES_GIBLE,
        SPECIES_EEVEE,
        SPECIES_EEVEE,
        SPECIES_EEVEE,
        SPECIES_SKORUPI,
        SPECIES_TROPIUS,
        SPECIES_PARAS
    },
[TRAINER_CLASS_PKMN_BREEDER] = 
    {
        SPECIES_WHISMUR,
        SPECIES_ZIGZAGOON,
        SPECIES_MAKUHITA,
        SPECIES_TAILLOW,
        SPECIES_BUDEW,
        SPECIES_WINGULL,
        SPECIES_SHROOMISH,
        SPECIES_SHROOMISH,
        SPECIES_ARON,
        SPECIES_SKITTY,
        SPECIES_TOGEPI,
        SPECIES_MIMEJR,
        SPECIES_NIDORAN_F,
        SPECIES_NIDORAN_M,
        SPECIES_PICHU,
        SPECIES_SKORUPI,
        SPECIES_MAGBY,
        SPECIES_ELEKID,
        SPECIES_SWABLU,
        SPECIES_FARFETCHD
    },
[TRAINER_CLASS_PKMN_RANGER] = 
    {
        SPECIES_SEEDOT,
        SPECIES_LOTAD,
        SPECIES_ABSOL,
        SPECIES_TROPIUS,
        SPECIES_ODDISH,
        SPECIES_SHROOMISH,
        SPECIES_KECLEON,
        SPECIES_FEEBAS,
        SPECIES_WHISMUR,
        SPECIES_KRABBY,
        SPECIES_MAWILE,
        SPECIES_TANGELA,
        SPECIES_EXEGGCUTE,
        SPECIES_FERROSEED,
        SPECIES_SKARMORY,
        SPECIES_PHANPY,
        SPECIES_PHANTUMP,
        SPECIES_LICKITUNG,
        SPECIES_SLAKOTH,
        SPECIES_TRAPINCH
    },
[TRAINER_CLASS_TEAM_MAGMA] = 
    {
        SPECIES_POOCHYENA,
        SPECIES_NUMEL,
        SPECIES_ONIX,
        SPECIES_ZUBAT,
        SPECIES_NOIBAT,
        SPECIES_SEEDOT,
        SPECIES_BALTOY,
        SPECIES_GASTLY,
        SPECIES_KOFFING,
        SPECIES_RATTATA,
        SPECIES_CHARMANDER,
        SPECIES_MAGBY,
        SPECIES_EEVEE,
        SPECIES_CYNDAQUIL,
        SPECIES_TORKOAL,
        SPECIES_LITTEN,
        SPECIES_GEODUDE,
        SPECIES_SKORUPI,
        SPECIES_WHISMUR,
        SPECIES_SLUGMA
    },
[TRAINER_CLASS_MAGMA_ADMIN] = 
    {
        SPECIES_MIGHTYENA,
        SPECIES_PRIMEAPE,
        SPECIES_MAGCARGO,
        SPECIES_GOLBAT,
        SPECIES_NOIBAT,
        SPECIES_BRAIXEN,
        SPECIES_RHYDON,
        SPECIES_HAUNTER,
        SPECIES_WEEZING,
        SPECIES_RATICATE,
        SPECIES_DUSCLOPS,
        SPECIES_MAGMAR,
        SPECIES_FLAREON,
        SPECIES_TURTONATOR,
        SPECIES_PAWNIARD,
        SPECIES_TORRACAT,
        SPECIES_MONFERNO,
        SPECIES_HAWLUCHA,
        SPECIES_BEWEAR,
        SPECIES_CHARIZARD
    },
[TRAINER_CLASS_MAGMA_LEADER] = 
    {
        SPECIES_MIGHTYENA,
        SPECIES_INCINEROAR,
        SPECIES_MAGCARGO,
        SPECIES_CROBAT,
        SPECIES_DELPHOX,
        SPECIES_MEDICHAM,
        SPECIES_LUCARIO,
        SPECIES_GENGAR,
        SPECIES_WEEZING,
        SPECIES_RATICATE,
        SPECIES_CHARIZARD,
        SPECIES_MAGMORTAR,
        SPECIES_FLAREON,
        SPECIES_QUILAVA,
        SPECIES_TORKOAL,
        SPECIES_INCINEROAR,
        SPECIES_INFERNAPE,
        SPECIES_TALONFLAME,
        SPECIES_TURTONATOR,
        SPECIES_LUCARIO
    },
[TRAINER_CLASS_LASS] = 
    {
        SPECIES_LUVDISC,
        SPECIES_GOLDEEN,
        SPECIES_WAILMER,
        SPECIES_LOTAD,
        SPECIES_SHROOMISH,
        SPECIES_TAILLOW,
        SPECIES_SKITTY,
        SPECIES_AZURILL,
        SPECIES_ODDISH,
        SPECIES_ZIGZAGOON,
        SPECIES_HOPPIP,
        SPECIES_RATTATA,
        SPECIES_CLEFFA,
        SPECIES_PIDGEY,
        SPECIES_BELLSPROUT,
        SPECIES_NIDORAN_F,
        SPECIES_IGGLYBUFF,
        SPECIES_CHERUBI,
        SPECIES_PLUSLE,
        SPECIES_MISDREAVUS
    },
[TRAINER_CLASS_BUG_CATCHER] = 
    {
        SPECIES_WEEDLE,
        SPECIES_WURMPLE,
        SPECIES_SURSKIT,
        SPECIES_CATERPIE,
        SPECIES_PINSIR,
        SPECIES_PINECO,
        SPECIES_VOLBEAT,
        SPECIES_ILLUMISE,
        SPECIES_LEDYBA,
        SPECIES_PINECO,
        SPECIES_SPINARAK,
        SPECIES_NINCADA,
        SPECIES_BAGON,
        SPECIES_NINCADA,
        SPECIES_WURMPLE,
        SPECIES_PARAS,
        SPECIES_SCYTHER,
        SPECIES_LEDYBA,
        SPECIES_SPINARAK,
        SPECIES_YANMA
    },
[TRAINER_CLASS_HIKER] = 
    {
        SPECIES_GEODUDE,
        SPECIES_ROGGENROLA,
        SPECIES_NOSEPASS,
        SPECIES_BONSLY,
        SPECIES_OMANYTE,
        SPECIES_PHANPY,
        SPECIES_MUDBRAY,
        SPECIES_LARVITAR,
        SPECIES_GLIGAR,
        SPECIES_MAKUHITA,
        SPECIES_SWINUB,
        SPECIES_TERRAKION,
        SPECIES_RHYHORN,
        SPECIES_ARON,
        SPECIES_RELICANTH,
        SPECIES_AXEW,
        SPECIES_ONIX,
        SPECIES_TAUROS,
        SPECIES_DRILBUR,
        SPECIES_TURTWIG
    },
[TRAINER_CLASS_YOUNG_COUPLE] = 
    {
        SPECIES_PLUSLE,
        SPECIES_MINUN,
        SPECIES_SKITTY,
        SPECIES_ROCKRUFF,
        SPECIES_AZURILL,
        SPECIES_PICHU,
        SPECIES_VOLBEAT,
        SPECIES_ILLUMISE,
        SPECIES_LUVDISC,
        SPECIES_WEEDLE,
        SPECIES_WURMPLE,
        SPECIES_ELECTRIKE,
        SPECIES_MAREEP,
        SPECIES_HOOTHOOT,
        SPECIES_HOUNDOUR,
        SPECIES_BUNEARY,
        SPECIES_WOOPER,
        SPECIES_MUDKIP,
        SPECIES_ROWLET,
        SPECIES_DIGLETT
    },
[TRAINER_CLASS_DEVON_ENFORCER] = 
    { 
        SPECIES_LILLIPUP,
        SPECIES_MEOWTH, 
        SPECIES_AIPOM, 
        SPECIES_GROWLITHE, 
        SPECIES_SNUBBULL, 
        SPECIES_CHINCHOU, 
        SPECIES_PURRLOIN,
        SPECIES_WAILMER, 
        SPECIES_TAILLOW, 
        SPECIES_RATTATA, 
        SPECIES_CROAGUNK, 
        SPECIES_SENTRET, 
        SPECIES_TEDDIURSA, 
        SPECIES_MILTANK, 
        SPECIES_ZIGZAGOON, 
        SPECIES_SKITTY,
        SPECIES_MINCCINO,
        SPECIES_CHIKORITA,
        SPECIES_ROCKRUFF,
        SPECIES_SWINUB
    },
};

#endif
