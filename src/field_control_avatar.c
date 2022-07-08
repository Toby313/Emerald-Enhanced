#include "global.h"
#include "battle_setup.h"
#include "bike.h"
#include "coord_event_weather.h"
#include "daycare.h"
#include "dexnav.h"
#include "faraway_island.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "event_scripts.h"
#include "fieldmap.h"
#include "field_control_avatar.h"
#include "field_player_avatar.h"
#include "field_poison.h"
#include "field_screen_effect.h"
#include "field_specials.h"
#include "fldeff_misc.h"
#include "item_menu.h"
#include "link.h"
#include "match_call.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "pokemon.h"
#include "safari_zone.h"
#include "script.h"
#include "secret_base.h"
#include "sound.h"
#include "start_menu.h"
#include "trainer_see.h"
#include "trainer_hill.h"
#include "wild_encounter.h"
#include "constants/event_bg.h"
#include "constants/event_objects.h"
#include "constants/field_poison.h"
#include "constants/map_types.h"
#include "constants/maps.h"
#include "constants/songs.h"
#include "constants/trainer_hill.h"
#include "constants/event_objects.h"
#include "random.h"
#include "constants/region_map_sections.h"
#include "constants/species.h"
#include "factions.h"
#include "RyuRealEstate.h"
#include "ach_atlas.h"
#include "overworld_notif.h"
#include "RyuDynDeliveries.h"

static EWRAM_DATA u8 sWildEncounterImmunitySteps = 0;
static EWRAM_DATA u16 sPreviousPlayerMetatileBehavior = 0;
EWRAM_DATA const u8 *gOriginalNPCScript = NULL;

u8 gSelectedObjectEvent;

extern const u8 SB_SetupRandomSteppedOnEncounter[];
extern const u8 SB_SetupRandomMimikyuEncounter[];
extern const u8 SB_CheckMeloettaEncounter[];
extern const u8 Ryu_BeingWatched[];
extern const u8 Ryu_MeloettaWatchingMsg[];
extern const u8 RyuScript_CheckGivenAchievement[];
extern const u8 RyuScript_GoToLimbo[];
extern const u8 RyuScript_CompleteTravelDailyQuestType[];
extern const u8 RyuScript_NotifyPropertyDamage[];
extern const u8 RyuScript_PlayerReceivedInterest[];
extern const u8 RyuScript_NotifyRent[]; 
extern const u8 RyuScript_NotifyPickedUpItem[]; 
extern const u8 RyuGlobal_EnableNormalDexnav[];
extern const u8 RyuGlobal_CheckMagmaStatus[];
extern const u8 RyuGlobal_CheckAquaStatus[];
extern const u8 RyuScript_EncounterBuzzwole[];
extern const u8 RyuScript_EncounterPheromosa[];
extern const u8 RyuScript_EncounterKartana[];
extern const u8 RyuScript_EncounterXurkitree[];
extern const u8 RyuScript_EncounterNihilego[];
extern const u8 RyuScript_EncounterGuzzlord[];
extern const u8 RyuScript_EncounterStakataka[];
extern const u8 RyuScript_EncounterCelesteela[];
extern const u8 RyuScript_EncounterKeldeo[];
extern const u8 Ryu_FFTextSpeedWarning[];
extern const u8 RyuScript_NotifyFailedChallenge[];
extern const u8 RyuScript_NotifySucceededChallenge[];
extern const u8 RyuScript_Lv100FailMsg[];
extern const u8 RyuScript_Lv100SwitchMsg[];
extern const u8 RyuCheckForLNSUAch[];

void GetPlayerPosition(struct MapPosition *);
static void GetInFrontOfPlayerPosition(struct MapPosition *);
static u16 GetPlayerCurMetatileBehavior(int);
static bool8 TryStartInteractionScript(struct MapPosition*, u16, u8);
static const u8 *GetInteractionScript(struct MapPosition*, u8, u8);
static const u8 *GetInteractedObjectEventScript(struct MapPosition *, u8, u8);
static const u8 *GetInteractedBackgroundEventScript(struct MapPosition *, u8, u8);
static const u8 *GetInteractedMetatileScript(struct MapPosition *, u8, u8);
static const u8 *GetInteractedWaterScript(struct MapPosition *, u8, u8);
static bool32 TrySetupDiveDownScript(void);
static bool32 TrySetupDiveEmergeScript(void);
static bool8 TryStartStepBasedScript(struct MapPosition *, u16, u16);
static bool8 CheckStandardWildEncounter(u16);
static bool8 TryArrowWarp(struct MapPosition *, u16, u8);
static bool8 IsWarpMetatileBehavior(u16);
static bool8 IsArrowWarpMetatileBehavior(u16, u8);
static s8 GetWarpEventAtMapPosition(struct MapHeader *, struct MapPosition *);
static void SetupWarp(struct MapHeader *, s8, struct MapPosition *);
static bool8 TryDoorWarp(struct MapPosition *, u16, u8);
static s8 GetWarpEventAtPosition(struct MapHeader *, u16, u16, u8);
static u8 *GetCoordEventScriptAtPosition(struct MapHeader *, u16, u16, u8);
static struct BgEvent *GetBackgroundEventAtPosition(struct MapHeader *, u16, u16, u8);
static bool8 TryStartCoordEventScript(struct MapPosition *);
static bool8 TryStartWarpEventScript(struct MapPosition *, u16);
static bool8 TryStartMiscWalkingScripts(u16);
static bool8 TryStartStepCountScript(u16);
static void UpdateHappinessStepCounter(void);
static bool8 UpdatePoisonStepCounter(void);
extern void Task_MapNamePopUpWindow(u8 taskId);


void FieldClearPlayerInput(struct FieldInput *input)
{
    input->pressedAButton = FALSE;
    input->checkStandardWildEncounter = FALSE;
    input->pressedStartButton = FALSE;
    input->pressedSelectButton = FALSE;
    input->heldDirection = FALSE;
    input->heldDirection2 = FALSE;
    input->tookStep = FALSE;
    input->pressedBButton = FALSE;
    input->input_field_1_0 = FALSE;
    input->input_field_1_1 = FALSE;
    input->input_field_1_2 = FALSE;
    input->input_field_1_3 = FALSE;
    input->pressedRButton = FALSE;
    input->dpadDirection = 0;
}

void FieldGetPlayerInput(struct FieldInput *input, u16 newKeys, u16 heldKeys)
{
    u8 tileTransitionState = gPlayerAvatar.tileTransitionState;
    u8 runningState = gPlayerAvatar.runningState;
    bool8 forcedMove = MetatileBehavior_IsForcedMovementTile(GetPlayerCurMetatileBehavior(runningState));

    if ((tileTransitionState == T_TILE_CENTER && forcedMove == FALSE) || tileTransitionState == T_NOT_MOVING)
    {
        if (GetPlayerSpeed() != 4)
        {
            if (newKeys & START_BUTTON)
                input->pressedStartButton = TRUE;
            if (newKeys & SELECT_BUTTON)
                input->pressedSelectButton = TRUE;
            if (newKeys & A_BUTTON)
                input->pressedAButton = TRUE;
            if (newKeys & B_BUTTON)
                input->pressedBButton = TRUE;
            if (newKeys & R_BUTTON)
                input->pressedRButton = TRUE;
        }

        if (heldKeys & (DPAD_UP | DPAD_DOWN | DPAD_LEFT | DPAD_RIGHT))
        {
            input->heldDirection = TRUE;
            input->heldDirection2 = TRUE;
        }
    }

    if (forcedMove == FALSE)
    {
        if (tileTransitionState == T_TILE_CENTER && runningState == MOVING)
            input->tookStep = TRUE;
        if (forcedMove == FALSE && tileTransitionState == T_TILE_CENTER)
            input->checkStandardWildEncounter = TRUE;
    }

    if (heldKeys & DPAD_UP)
        input->dpadDirection = DIR_NORTH;
    else if (heldKeys & DPAD_DOWN)
        input->dpadDirection = DIR_SOUTH;
    else if (heldKeys & DPAD_LEFT)
        input->dpadDirection = DIR_WEST;
    else if (heldKeys & DPAD_RIGHT)
        input->dpadDirection = DIR_EAST;
}

bool8 RyuCheckPlayerHasPika(void)
{
    u8 i;
    bool8 hasPika = FALSE;
    for (i = 0; i < 6; i++)
    {
        if ((GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2) == SPECIES_PIKACHU))
            return TRUE;
    }

    return FALSE;
}

bool8 RyuCheckPlayerisInMtPyreAndHasPikachu(void)
{
    u16 locGroup = gSaveBlock1Ptr->location.mapGroup;
    u16 locMap = gSaveBlock1Ptr->location.mapNum;
    if (locGroup == 24)
    {
        if ((locMap > 15) && (locMap < 21))
        {
            if (RyuCheckPlayerHasPika() == TRUE)
                return TRUE;
        }
    }
    FlagSet(FLAG_TEMP_D);
    return FALSE;
}

extern int CountBadges(void);
extern void RyuCheckForFactionAchievements(void);

const u8 gRyuReachedDailyTargetLocationString[] = _("Reached target area for the {STR_VAR_1}.");

void RyuDoNotifyTasks(void)
{
    if (FlagGet(FLAG_RYU_ENTERING_OWNED_HOME) == FALSE)
        FlagSet(FLAG_RYU_HIDE_HOME_ATTENDANT);
    if ((gPlayerPartyCount == 0) && (VarGet(VAR_LITTLEROOT_INTRO_STATE) >= 10)) //check blackout for challenge/hardcore
    {
        if (!(FlagGet(FLAG_RYU_LIMBO) == 1))
        {
            if ((FlagGet(FLAG_RYU_CHALLENGEMODE) == TRUE) || (FlagGet(FLAG_RYU_HARDCORE_MODE) == TRUE))
                ScriptContext1_SetupScript(RyuScript_GoToLimbo);
        }
    }

    RyuCheckForFactionAchievements();

    if ((CheckAchievement(ACH_LEAVE_NO_STONE_UNTURNED) == FALSE) && (VarGet(VAR_TEMP_E) == 0))
    {
        VarSet(VAR_TEMP_E, 100);
        ScriptContext1_SetupScript(RyuCheckForLNSUAch);
    }

    if ((FlagGet(FLAG_RYU_FAILED_RYU_CHALLENGE) == TRUE) && (FlagGet(FLAG_RYU_NOTIFIED_CHALLENGE_FAILURE) == FALSE))
        ScriptContext1_SetupScript(RyuScript_NotifyFailedChallenge);

    if ((VarGet(VAR_RYU_SPECIAL_CHALLENGE_STATE) == 69) && (FlagGet(FLAG_RYU_NOTIFIED_CHALLENGE_SUCCESS) == FALSE))
        ScriptContext1_SetupScript(RyuScript_NotifySucceededChallenge);

    if (FlagGet(FLAG_RYU_FAILED_100_CAP_SWITCH) == TRUE)//Player attempted to switch to 100cap and failed.
        ScriptContext1_SetupScript(RyuScript_Lv100FailMsg);

    if (FlagGet(FLAG_RYU_NOTIFY_LV100_SWITCH) == TRUE)//Player switched to 100cap, warn about side effects.
        ScriptContext1_SetupScript(RyuScript_Lv100SwitchMsg);

    if (!(FlagGet(FLAG_SYS_DEXNAV_GET)) && (!(FlagGet(FLAG_TEMP_F)))) //notify and give Dexnav
        if (CountBadges() >= 6)
            ScriptContext1_SetupScript(RyuGlobal_EnableNormalDexnav);

    if ((FlagGet(FLAG_RYU_PLAYER_HELPING_MAGMA)) //Mission notifications from Magma
        && ((VarGet(VAR_RYU_QUEST_MAGMA) == 130) 
        || (VarGet(VAR_RYU_QUEST_MAGMA) == 210)
        || (VarGet(VAR_RYU_QUEST_MAGMA) == 350))
        && (!(FlagGet(FLAG_TEMP_F))))//prevents the notification from showing up as soon as the player is assigned the task.
    {
        ScriptContext1_SetupScript(RyuGlobal_CheckMagmaStatus);
    }

    if ((FlagGet(FLAG_RYU_PLAYER_HELPING_AQUA)) //Mission notifications from Aqua
        && ((VarGet(VAR_RYU_QUEST_AQUA) == 10) 
        || (VarGet(VAR_RYU_QUEST_AQUA) == 55) 
        || (VarGet(VAR_RYU_QUEST_AQUA) == 80) 
        || (VarGet(VAR_RYU_QUEST_AQUA) == 91) 
        || (VarGet(VAR_RYU_QUEST_AQUA) == 123))
        && (!(FlagGet(FLAG_TEMP_F))))
    {
        ScriptContext1_SetupScript(RyuGlobal_CheckAquaStatus);
    }
}

void RyuDoSpecialEncounterChecks(struct FieldInput *input)
{
    struct MapPosition position;
    u8 playerDirection;
    u16 metatileBehavior;
    u16 rand = (Random() % 99);
    u16 locSum = (gSaveBlock1Ptr->location.mapGroup << 8) + (gSaveBlock1Ptr->location.mapNum);
    u16 UBRotation = (VarGet(VAR_RYU_UB_EVENT_TIMER));//which UB group the player currently can encounter

    playerDirection = GetPlayerFacingDirection();
    GetPlayerPosition(&position);
    metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);

    if (MetatileBehavior_IsSandOrDeepSand(GetPlayerCurMetatileBehavior(gPlayerAvatar.runningState)))
        {
            if (!(locSum == MAP_ROUTE111))
            {
                rand = (Random() % 256);
                if ((rand == 69) || (rand == 169))
                {
                    ScriptContext1_SetupScript(SB_SetupRandomSteppedOnEncounter);
                }
            }
        }
        if (FlagGet(FLAG_TEMP_D) == 0)
        {
            if (RyuCheckPlayerisInMtPyreAndHasPikachu() == TRUE)
            {
                rand = (Random() % 256);
                if (rand == 128)
                {
                    ScriptContext1_SetupScript(SB_SetupRandomMimikyuEncounter);
                }
            }
        }

        if (input->tookStep && (FlagGet(FLAG_TEMP_E) == TRUE))
            VarSet(VAR_TEMP_E, (VarGet(VAR_TEMP_E) - 1));
        
        if (FlagGet(FLAG_TEMP_C) == 0)
            {
                if (MetatileBehavior_IsTallGrass(GetPlayerCurMetatileBehavior(gPlayerAvatar.runningState)))
                {
                    if (GetGameStat(GAME_STAT_USED_SOUND_MOVE) >= 255 && (FlagGet(FLAG_RYU_CAPTURED_MELOETTA) == 0))
                    {
                        ScriptContext1_SetupScript(SB_CheckMeloettaEncounter);
                    }
                    else if (GetGameStat(GAME_STAT_USED_SOUND_MOVE) >= 200)
                    {
                        ScriptContext1_SetupScript(Ryu_MeloettaWatchingMsg);
                    }
                    else if (GetGameStat(GAME_STAT_USED_SOUND_MOVE) >= 100)
                    {
                        ScriptContext1_SetupScript(Ryu_BeingWatched);
                    }
                    else if (GetGameStat(GAME_STAT_USED_SOUND_MOVE) < 50)
                    {
                        FlagSet(FLAG_TEMP_C);
                    }
                }
            }

    //Ultra beast encounter checks
    //I don't think this can be made much cleaner, but it shouldn't really cause lag, even if player is at the right locations
    //FLAG_TEMP_D is set after an encounter with a UB when it wasn't caught, so player has to leave map and come back for it to respawn
    //I wanted to also make it play the cries of the relevant UB once in a while in the correct area, but i'm not sure how without vastly
    //complicating this furtner.

    if ((FlagGet(FLAG_RYU_ULTRA_BEASTS_ESCAPED) == TRUE) && (FlagGet(FLAG_RYU_CAUGHT_ALL_UBS) == FALSE) && (rand < 5))//5% chance to find the UB here.
    {

        if ((FlagGet(FLAG_RYU_BUZZWOLE_CAUGHT) == FALSE) && (FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER) == FALSE) && (UBRotation == 0))
        {
            if (locSum == MAP_GRANITE_CAVE_1F ||
                locSum == MAP_GRANITE_CAVE_B1F ||
                locSum == MAP_GRANITE_CAVE_B2F)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterBuzzwole);
            }
        }

        if (!(FlagGet(FLAG_RYU_PHEROMOSA_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 0))
        {
            if (locSum == MAP_ROUTE119)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterPheromosa);
            }
        }

        if (!(FlagGet(FLAG_RYU_KARTANA_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 1))
        {
            if (gSaveBlock1Ptr->location.mapNum == MAP_ROUTE120)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterKartana);
            }
        }

        if (!(FlagGet(FLAG_RYU_XURKITREE_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 1))
        {
            if (locSum == MAP_NEW_MAUVILLE_INSIDE)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterXurkitree);
            }
        }

        if (!(FlagGet(FLAG_RYU_NIHILEGO_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 2))
        {
            if (locSum == MAP_METEOR_FALLS_2F ||
                locSum == MAP_METEOR_FALLS_1F_1R ||
                locSum == MAP_METEOR_FALLS_1F_2R ||
                locSum == MAP_METEOR_FALLS_1F_3R ||
                locSum == MAP_METEOR_FALLS_B1F_1R ||
                locSum == MAP_METEOR_FALLS_B1F_2R ||
                locSum == MAP_METEOR_FALLS_2F ||
                locSum == MAP_METEOR_FALLS_3F)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterNihilego);
            }
        }

        if (!(FlagGet(FLAG_RYU_GUZZLORD_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 2))
        {
            if (locSum == MAP_FROSTY_GROTTO)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterGuzzlord);
            }
        }

        if (!(FlagGet(FLAG_RYU_STAKATAKA_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 3))
        {
            if (locSum == MAP_MT_PYRE_2F ||
                locSum == MAP_MT_PYRE_3F ||
                locSum == MAP_MT_PYRE_4F ||
                locSum == MAP_MT_PYRE_5F ||
                locSum == MAP_MT_PYRE_6F)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterStakataka);
            }
        }

        if (!(FlagGet(FLAG_RYU_CELESTEELA_CAUGHT)) && (!(FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER))) && (UBRotation == 3))
        {
            if (locSum == MAP_ROUTE66)
            {
                FlagSet(FLAG_RYU_ENCOUNTERED_UB);
                ScriptContext1_SetupScript(RyuScript_EncounterCelesteela);
            }
        }
    

    }

    if ((VarGet(VAR_TEMP_D) == 400) &&
         FlagGet(FLAG_SYS_GAME_CLEAR) &&
         FlagGet(FLAG_RYU_CAUGHT_KELDEO) == FALSE &&
         gSaveBlock1Ptr->location.mapGroup == (MAP_GROUP(PETALBURG_WOODS)) &&
         gSaveBlock1Ptr->location.mapNum == MAP_NUM(PETALBURG_WOODS) &&
         (Random() % 100 < 5) &&
         (FlagGet(FLAG_RYU_PAUSE_UB_ENCOUNTER) == FALSE))
    {
        FlagSet(FLAG_RYU_PAUSE_UB_ENCOUNTER);
        ScriptContext1_SetupScript(RyuScript_EncounterKeldeo);
    }
}

void RyuDoDailyTravelQuestThings(void)
{
    if ((VarGet(VAR_RYU_DAILY_QUEST_DATA) > 0) && (VarGet(VAR_RYU_DAILY_QUEST_DATA) < 15) && (VarGet(VAR_RYU_DAILY_QUEST_TYPE) == 3))
            {
                VarSet(VAR_RYU_DAILY_QUEST_DATA, (VarGet(VAR_RYU_DAILY_QUEST_DATA) - 1));
            }

    if ((FlagGet(FLAG_DAILY_QUEST_ACTIVE) == TRUE) && (VarGet(VAR_RYU_DAILY_QUEST_TYPE) == 3) && (VarGet(VAR_RYU_DAILY_QUEST_DATA) == 15))
        {
            u16 locSum = (gSaveBlock1Ptr->location.mapGroup << 8) + (gSaveBlock1Ptr->location.mapNum);
            if (VarGet(VAR_RYU_DAILY_QUEST_TARGET) == locSum)
            {
                if (VarGet(VAR_RYU_DAILY_QUEST_DATA) == 15)
                {
                    VarSet(VAR_RYU_DAILY_QUEST_DATA, 14);
                }
            }
        }

    if (VarGet(VAR_RYU_DAILY_QUEST_DATA) == 0)
        {
            u8 factionId = (VarGet(VAR_RYU_DAILY_QUEST_ASSIGNEE_FACTION));
            VarSet(VAR_RYU_DAILY_QUEST_DATA, 4000);
            StringCopy(gStringVar1, gFactionNames[factionId]);
            QueueNotification(gRyuReachedDailyTargetLocationString, NOTIFY_QUEST, 180);
        }

}

int ProcessPlayerFieldInput(struct FieldInput *input)
{
    struct MapPosition position;
    u8 playerDirection;
    u16 metatileBehavior;
    

    gSpecialVar_LastTalked = 0;
    gSelectedObjectEvent = 0;

    playerDirection = GetPlayerFacingDirection();
    GetPlayerPosition(&position);
    metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);

    if (CheckForTrainersWantingBattle() == TRUE)
        return TRUE;
 
    if (TryRunOnFrameMapScript() == TRUE)
        return TRUE;
    if (!(FuncIsActiveTask(Task_MapNamePopUpWindow)))
        RyuDoNotifyTasks();

    if (input->pressedBButton && TrySetupDiveEmergeScript() == TRUE)
        return TRUE;
    if (input->tookStep)
    {
        if (FlagGet(FLAG_RYU_HAS_FOLLOWER))
            IncrementGameStat(GAME_STAT_STEPS_FOLLOWER);
        IncrementGameStat(GAME_STAT_STEPS);
        IncrementBirthIslandRockStepCount();
        if (TryStartStepBasedScript(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;

        RyuDoSpecialEncounterChecks(input);

        if ((FlagGet(FLAG_DAILY_QUEST_ACTIVE) == TRUE) && (VarGet(VAR_RYU_DAILY_QUEST_TYPE) == 3))
            RyuDoDailyTravelQuestThings();
    }

    if (input->checkStandardWildEncounter && CheckStandardWildEncounter(metatileBehavior) == TRUE)
        return TRUE;
    if (input->heldDirection && input->dpadDirection == playerDirection)
    {
        if (TryArrowWarp(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;
    }

    GetInFrontOfPlayerPosition(&position);
    metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);
    if (input->pressedAButton && TryStartInteractionScript(&position, metatileBehavior, playerDirection) == TRUE)
        return TRUE;

    if (input->heldDirection2 && input->dpadDirection == playerDirection)
    {
        if (TryDoorWarp(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;
    }
    if (input->pressedAButton && TrySetupDiveDownScript() == TRUE)
        return TRUE;
    if (input->pressedStartButton)
    {
        if (FlagGet(FLAG_SYS_DEXNAV_SEARCH))
        {
            ResetDexNavSearch();
            return FALSE;
        }
        else
        {
            PlaySE(SE_WIN_OPEN);
            ShowStartMenu();
        }
        return TRUE;
    }
    if (input->pressedSelectButton && UseRegisteredKeyItemOnField() == TRUE)
        return TRUE;
    
    //if (input->tookStep && TryFindHiddenPokemon())  //dont think this works anyway
        //return TRUE;
    
    if (input->pressedRButton && TryStartDexnavSearch())
        return TRUE;

    return FALSE;
}

void GetPlayerPosition(struct MapPosition *position)
{
    PlayerGetDestCoords(&position->x, &position->y);
    position->height = PlayerGetZCoord();
}

static void GetInFrontOfPlayerPosition(struct MapPosition *position)
{
    s16 x, y;

    GetXYCoordsOneStepInFrontOfPlayer(&position->x, &position->y);
    PlayerGetDestCoords(&x, &y);
    if (MapGridGetZCoordAt(x, y) != 0)
        position->height = PlayerGetZCoord();
    else
        position->height = 0;
}

static u16 GetPlayerCurMetatileBehavior(int runningState)
{
    s16 x, y;

    PlayerGetDestCoords(&x, &y);
    return MapGridGetMetatileBehaviorAt(x, y);
}

static bool8 TryStartInteractionScript(struct MapPosition *position, u16 metatileBehavior, u8 direction)
{
    const u8 *script = GetInteractionScript(position, metatileBehavior, direction);
    if (script == NULL)
        return FALSE;

    // Don't play interaction sound for certain scripts.
    if (script != SecretBase_EventScript_PC
     && script != SecretBase_EventScript_RecordMixingPC
     && script != SecretBase_EventScript_DollInteract
     && script != SecretBase_EventScript_CushionInteract
     && script != EventScript_PC)
        PlaySE(SE_SELECT);

    ScriptContext1_SetupScript(script);
    return TRUE;
}

static const u8 *GetInteractionScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    const u8 *script = GetInteractedObjectEventScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    script = GetInteractedBackgroundEventScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    script = GetInteractedMetatileScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    script = GetInteractedWaterScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    return NULL;
}

const u8 *GetInteractedLinkPlayerScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    u8 objectEventId;
    s32 i;

    if (!MetatileBehavior_IsCounter(MapGridGetMetatileBehaviorAt(position->x, position->y)))
        objectEventId = GetObjectEventIdByXYZ(position->x, position->y, position->height);
    else
        objectEventId = GetObjectEventIdByXYZ(position->x + gDirectionToVectors[direction].x, position->y + gDirectionToVectors[direction].y, position->height);

    if (objectEventId == OBJECT_EVENTS_COUNT || gObjectEvents[objectEventId].localId == PLAYER)
        return NULL;

    for (i = 0; i < 4; i++)
    {
        if (gLinkPlayerObjectEvents[i].active == TRUE && gLinkPlayerObjectEvents[i].objEventId == objectEventId)
            return NULL;
    }

    gSelectedObjectEvent = objectEventId;
    gSpecialVar_LastTalked = gObjectEvents[objectEventId].localId;
    gSpecialVar_Facing = direction;
    return GetObjectEventScriptPointerByObjectEventId(objectEventId);
}

static const u8 *GetInteractedObjectEventScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    u8 objectEventId;
    const u8 *script;
    bool8 isTrainer = FALSE;
    u8 currentTrainerFaction = 0;
    u16 currentTrainer = 0;
    u8 currentFaction = FACTION_OTHERS;

    objectEventId = GetObjectEventIdByXYZ(position->x, position->y, position->height);
    if (objectEventId == OBJECT_EVENTS_COUNT || gObjectEvents[objectEventId].localId == PLAYER)
    {
        if (MetatileBehavior_IsCounter(metatileBehavior) != TRUE)
            return NULL;

        // Look for an object event on the other side of the counter.
        objectEventId = GetObjectEventIdByXYZ(position->x + gDirectionToVectors[direction].x, position->y + gDirectionToVectors[direction].y, position->height);
        if (objectEventId == OBJECT_EVENTS_COUNT || gObjectEvents[objectEventId].localId == PLAYER)
            return NULL;
    }

    gSelectedObjectEvent = objectEventId;
    gSpecialVar_LastTalked = gObjectEvents[objectEventId].localId;
    gSpecialVar_Facing = direction;

    if (gObjectEvents[objectEventId].trainerType != 0)
        isTrainer = TRUE;

    if (InTrainerHill() == TRUE)
        script = GetTrainerHillTrainerScript();
    else if (gObjectEvents[objectEventId].localId == FOLLOWER)
        script = gFollowerScript;
    else
    {
        script = GetObjectEventScriptPointerByObjectEventId(objectEventId);
        if (isTrainer == TRUE)
        {
            currentTrainer = T1_READ_16(script + 2);
            currentFaction = GetFactionId(currentTrainer);
            if (FlagGet(TRAINER_FLAGS_START + currentTrainer) == TRUE)
                {
                    if (currentTrainer == TRAINER_TIANA)
                    {
                        return script;
                    }
                    else if (GetFactionStanding(currentTrainer) <= 50)
                    {
                        return script;
                    }
                    else
                    {
                        currentTrainerFaction = gTrainers[currentTrainer].trainerFaction;
                        if (currentTrainerFaction < FACTION_OTHERS)
                            {
                                gOriginalNPCScript = script;
                                script = RyuGetFactionDailyQuestScriptPtr(currentTrainerFaction);
                                return script;
                            }
                    }
                }
        }
            
    }

    script = GetRamScript(gSpecialVar_LastTalked, script);
    return script;
}

static const u8 *GetInteractedBackgroundEventScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    struct BgEvent *bgEvent = GetBackgroundEventAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);

    if (bgEvent == NULL)
        return NULL;
    if (bgEvent->bgUnion.script == NULL)
        return EventScript_TestSignpostMsg;

    switch (bgEvent->kind)
    {
    case BG_EVENT_PLAYER_FACING_ANY:
    default:
        return bgEvent->bgUnion.script;
    case BG_EVENT_PLAYER_FACING_NORTH:
        if (direction != DIR_NORTH)
            return NULL;
        break;
    case BG_EVENT_PLAYER_FACING_SOUTH:
        if (direction != DIR_SOUTH)
            return NULL;
        break;
    case BG_EVENT_PLAYER_FACING_EAST:
        if (direction != DIR_EAST)
            return NULL;
        break;
    case BG_EVENT_PLAYER_FACING_WEST:
        if (direction != DIR_WEST)
            return NULL;
        break;
    case 5:
    case 6:
    case BG_EVENT_HIDDEN_ITEM:
        gSpecialVar_0x8004 = ((u32)bgEvent->bgUnion.script >> 16) + FLAG_HIDDEN_ITEMS_START;
        gSpecialVar_0x8005 = (u32)bgEvent->bgUnion.script;
        if (FlagGet(gSpecialVar_0x8004) == TRUE)
            return NULL;
        return EventScript_HiddenItemScript;
    case BG_EVENT_SECRET_BASE:
        if (direction == DIR_NORTH)
        {
            gSpecialVar_0x8004 = bgEvent->bgUnion.secretBaseId;
            if (TrySetCurSecretBase())
                return SecretBase_EventScript_CheckEntrance;
        }
        return NULL;
    }

    return bgEvent->bgUnion.script;
}

static const u8 *GetInteractedMetatileScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    s8 height;

    if (MetatileBehavior_IsPlayerFacingTVScreen(metatileBehavior, direction) == TRUE)
        return EventScript_TV;
    if (MetatileBehavior_IsPC(metatileBehavior) == TRUE)
        return EventScript_PC;
    if (MetatileBehavior_IsClosedSootopolisDoor(metatileBehavior) == TRUE)
        return EventScript_ClosedSootopolisDoor;
    if (MetatileBehavior_IsSkyPillarClosedDoor(metatileBehavior) == TRUE)
        return SkyPillar_Outside_EventScript_ClosedDoor;
    if (MetatileBehavior_IsCableBoxResults1(metatileBehavior) == TRUE)
        return EventScript_CableBoxResults;
    if (MetatileBehavior_IsPokeblockFeeder(metatileBehavior) == TRUE)
        return EventScript_PokeBlockFeeder;
    if (MetatileBehavior_IsTrickHousePuzzleDoor(metatileBehavior) == TRUE)
        return Route110_TrickHousePuzzle_EventScript_Door;
    if (MetatileBehavior_IsRegionMap(metatileBehavior) == TRUE)
        return EventScript_RegionMap;
    if (MetatileBehavior_IsPictureBookShelf(metatileBehavior) == TRUE)
        return EventScript_PictureBookShelf;
    if (MetatileBehavior_IsBookShelf(metatileBehavior) == TRUE)
        return EventScript_BookShelf;
    if (MetatileBehavior_IsPokeCenterBookShelf(metatileBehavior) == TRUE)
        return EventScript_PokemonCenterBookShelf;
    if (MetatileBehavior_IsVase(metatileBehavior) == TRUE)
        return EventScript_Vase;
    if (MetatileBehavior_IsTrashCan(metatileBehavior) == TRUE)
        return EventScript_EmptyTrashCan;
    if (MetatileBehavior_IsShopShelf(metatileBehavior) == TRUE)
        return EventScript_ShopShelf;
    if (MetatileBehavior_IsBlueprint(metatileBehavior) == TRUE)
        return EventScript_Blueprint;
    if (MetatileBehavior_IsPlayerFacingWirelessBoxResults(metatileBehavior, direction) == TRUE)
        return EventScript_WirelessBoxResults;
    if (MetatileBehavior_IsCableBoxResults2(metatileBehavior, direction) == TRUE)
        return EventScript_CableBoxResults;
    if (MetatileBehavior_IsQuestionnaire(metatileBehavior) == TRUE)
        return ryu_end;
    if (MetatileBehavior_IsTrainerHillTimer(metatileBehavior) == TRUE)
        return EventScript_TrainerHillTimer;

    height = position->height;
    if (height == MapGridGetZCoordAt(position->x, position->y))
    {
        if (MetatileBehavior_IsSecretBasePC(metatileBehavior) == TRUE)
            return SecretBase_EventScript_PC;
        if (MetatileBehavior_IsRecordMixingSecretBasePC(metatileBehavior) == TRUE)
            return SecretBase_EventScript_RecordMixingPC;
        if (MetatileBehavior_IsSecretBaseSandOrnament(metatileBehavior) == TRUE)
            return SecretBase_EventScript_SandOrnament;
        if (MetatileBehavior_IsSecretBaseShieldOrToyTV(metatileBehavior) == TRUE)
            return SecretBase_EventScript_ShieldOrToyTV;
        if (MetatileBehavior_IsMB_C6(metatileBehavior) == TRUE)
        {
            CheckInteractedWithFriendsFurnitureBottom();
            return NULL;
        }
        if (MetatileBehavior_HoldsLargeDecoration(metatileBehavior) == TRUE)
        {
            CheckInteractedWithFriendsFurnitureMiddle();
            return NULL;
        }
        if (MetatileBehavior_HoldsSmallDecoration(metatileBehavior) == TRUE)
        {
            CheckInteractedWithFriendsFurnitureTop();
            return NULL;
        }
    }
    else if (MetatileBehavior_IsSecretBasePoster(metatileBehavior) == TRUE)
    {
        CheckInteractedWithFriendsPosterDecor();
        return NULL;
    }

    return NULL;
}

static const u8 *GetInteractedWaterScript(struct MapPosition *unused1, u8 metatileBehavior, u8 direction)
{
    if (IsPlayerFacingSurfableFishableWater() == TRUE)
        return EventScript_UseSurf;

    if (MetatileBehavior_IsWaterfall(metatileBehavior) == TRUE)
    {
        if (IsPlayerSurfingNorth() == TRUE)
            return EventScript_UseWaterfall;
    }
    return NULL;
}

static bool32 TrySetupDiveDownScript(void)
{
    if (TrySetDiveWarp() == 2)
    {
        ScriptContext1_SetupScript(EventScript_UseDive);
        return TRUE;
    }
    return FALSE;
}

static bool32 TrySetupDiveEmergeScript(void)
{
    if (gMapHeader.mapType == MAP_TYPE_UNDERWATER && TrySetDiveWarp() == 1)
    {
        ScriptContext1_SetupScript(EventScript_UseDiveUnderwater);
        return TRUE;
    }
    return FALSE;
}

static bool8 TryStartStepBasedScript(struct MapPosition *position, u16 metatileBehavior, u16 direction)
{
    if (TryStartCoordEventScript(position) == TRUE)
        return TRUE;
    if (TryStartWarpEventScript(position, metatileBehavior) == TRUE)
        return TRUE;
    if (TryStartMiscWalkingScripts(metatileBehavior) == TRUE)
        return TRUE;
    if (TryStartStepCountScript(metatileBehavior) == TRUE)
        return TRUE;
    if (UpdateRepelCounter() == TRUE)
        return TRUE;
    return FALSE;
}

static bool8 TryStartCoordEventScript(struct MapPosition *position)
{
    u8 *script = GetCoordEventScriptAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);

    if (script == NULL)
        return FALSE;
    ScriptContext1_SetupScript(script);
    return TRUE;
}

static bool8 TryStartMiscWalkingScripts(u16 metatileBehavior)
{
    s16 x, y;

    if (MetatileBehavior_IsCrackedFloorHole(metatileBehavior))
    {
        ScriptContext1_SetupScript(EventScript_FallDownHole);
        return TRUE;
    }
    else if (MetatileBehavior_IsBattlePyramidWarp(metatileBehavior))
    {
        ScriptContext1_SetupScript(BattlePyramid_WarpToNextFloor);
        return TRUE;
    }
    else if (MetatileBehavior_IsSecretBaseGlitterMat(metatileBehavior) == TRUE)
    {
        DoSecretBaseGlitterMatSparkle();
        return FALSE;
    }
    else if (MetatileBehavior_IsSecretBaseSoundMat(metatileBehavior) == TRUE)
    {
        PlayerGetDestCoords(&x, &y);
        PlaySecretBaseMusicNoteMatSound(MapGridGetMetatileIdAt(x, y));
        return FALSE;
    }
    return FALSE;
}

static bool8 TryStartStepCountScript(u16 metatileBehavior)
{
    if (InUnionRoom() == TRUE)
    {
        return FALSE;
    }

    UpdateHappinessStepCounter();
    UpdateFarawayIslandStepCounter();

    if (!(gPlayerAvatar.flags & PLAYER_AVATAR_FLAG_FORCED_MOVE) && !MetatileBehavior_IsForcedMovementTile(metatileBehavior))
    {
        if (ShouldEggHatch())
        {
            IncrementGameStat(GAME_STAT_HATCHED_EGGS);
            ScriptContext1_SetupScript(EventScript_EggHatch);
            return TRUE;
        }
    }

    if (CountSSTidalStep(1) == TRUE)
    {
        ScriptContext1_SetupScript(SSTidalCorridor_EventScript_ReachedStepCount);
        return TRUE;
    }
    return FALSE;
}

void Unref_ClearHappinessStepCounter(void)
{
    VarSet(VAR_HAPPINESS_STEP_COUNTER, 0);
}

static void UpdateHappinessStepCounter(void)
{
    u16 *ptr = GetVarPointer(VAR_HAPPINESS_STEP_COUNTER);
    int i;

    (*ptr)++;
    (*ptr) %= 128;
    if (*ptr == 0)
    {
        struct Pokemon *mon = gPlayerParty;
        for (i = 0; i < PARTY_SIZE; i++)
        {
            AdjustFriendship(mon, FRIENDSHIP_EVENT_WALKING);
            mon++;
        }
    }
}

void ClearPoisonStepCounter(void)
{
    VarSet(VAR_POISON_STEP_COUNTER, 0);
}

static bool8 UpdatePoisonStepCounter(void)
{
    u16 *ptr;

    if (gMapHeader.mapType != MAP_TYPE_SECRET_BASE)
    {
        ptr = GetVarPointer(VAR_POISON_STEP_COUNTER);
        (*ptr)++;
        (*ptr) %= 4;
        if (*ptr == 0)
        {
            switch (DoPoisonFieldEffect())
            {
            case FLDPSN_NONE:
                return FALSE;
            case FLDPSN_PSN:
                return FALSE;
            case FLDPSN_FNT:
                return TRUE;
            }
        }
    }
    return FALSE;
}

void RestartWildEncounterImmunitySteps(void)
{
    // Starts at 0 and counts up to 4 steps.
    sWildEncounterImmunitySteps = 0;
}

static bool8 CheckStandardWildEncounter(u16 metatileBehavior)
{
    if (sWildEncounterImmunitySteps < 4)
    {
        sWildEncounterImmunitySteps++;
        sPreviousPlayerMetatileBehavior = metatileBehavior;
        return FALSE;
    }

    if (StandardWildEncounter(metatileBehavior, sPreviousPlayerMetatileBehavior) == TRUE)
    {
        sWildEncounterImmunitySteps = 0;
        sPreviousPlayerMetatileBehavior = metatileBehavior;
        return TRUE;
    }

    sPreviousPlayerMetatileBehavior = metatileBehavior;
    return FALSE;
}

static bool8 TryArrowWarp(struct MapPosition *position, u16 metatileBehavior, u8 direction)
{
    s8 warpEventId = GetWarpEventAtMapPosition(&gMapHeader, position);

    if (IsArrowWarpMetatileBehavior(metatileBehavior, direction) == TRUE && warpEventId != -1)
    {
        StoreInitialPlayerAvatarState();
        SetupWarp(&gMapHeader, warpEventId, position);
        DoWarp();
        return TRUE;
    }
    return FALSE;
}

static bool8 TryStartWarpEventScript(struct MapPosition *position, u16 metatileBehavior)
{
    s8 warpEventId = GetWarpEventAtMapPosition(&gMapHeader, position);

    if (warpEventId != -1 && IsWarpMetatileBehavior(metatileBehavior) == TRUE)
    {
        StoreInitialPlayerAvatarState();
        SetupWarp(&gMapHeader, warpEventId, position);
        if (MetatileBehavior_IsEscalator(metatileBehavior) == TRUE)
        {
            DoEscalatorWarp(metatileBehavior);
            return TRUE;
        }
        if (MetatileBehavior_IsLavaridgeB1FWarp(metatileBehavior) == TRUE)
        {
            DoLavaridgeGymB1FWarp();
            return TRUE;
        }
        if (MetatileBehavior_IsLavaridge1FWarp(metatileBehavior) == TRUE)
        {
            DoLavaridgeGym1FWarp();
            return TRUE;
        }
        if (MetatileBehavior_IsAquaHideoutWarp(metatileBehavior) == TRUE)
        {
            DoTeleportTileWarp();
            return TRUE;
        }
        if (MetatileBehavior_IsWarpOrBridge(metatileBehavior) == TRUE)
        {
            sub_80B0268();
            return TRUE;
        }
        if (MetatileBehavior_IsMtPyreHole(metatileBehavior) == TRUE)
        {
            ScriptContext1_SetupScript(EventScript_FallDownHoleMtPyre);
            return TRUE;
        }
        if (MetatileBehavior_IsMossdeepGymWarp(metatileBehavior) == TRUE)
        {
            DoMossdeepGymWarp();
            return TRUE;
        }
        DoWarp();
        return TRUE;
    }
    return FALSE;
}

static bool8 IsWarpMetatileBehavior(u16 metatileBehavior)
{
    if (MetatileBehavior_IsWarpDoor(metatileBehavior) != TRUE
     && MetatileBehavior_IsLadder(metatileBehavior) != TRUE
     && MetatileBehavior_IsEscalator(metatileBehavior) != TRUE
     && MetatileBehavior_IsNonAnimDoor(metatileBehavior) != TRUE
     && MetatileBehavior_IsLavaridgeB1FWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsLavaridge1FWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsAquaHideoutWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsMtPyreHole(metatileBehavior) != TRUE
     && MetatileBehavior_IsMossdeepGymWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsWarpOrBridge(metatileBehavior) != TRUE)
        return FALSE;
    return TRUE;
}

static bool8 IsArrowWarpMetatileBehavior(u16 metatileBehavior, u8 direction)
{
    switch (direction)
    {
    case DIR_NORTH:
        return MetatileBehavior_IsNorthArrowWarp(metatileBehavior);
    case DIR_SOUTH:
        return MetatileBehavior_IsSouthArrowWarp(metatileBehavior);
    case DIR_WEST:
        return MetatileBehavior_IsWestArrowWarp(metatileBehavior);
    case DIR_EAST:
        return MetatileBehavior_IsEastArrowWarp(metatileBehavior);
    }
    return FALSE;
}

static s8 GetWarpEventAtMapPosition(struct MapHeader *mapHeader, struct MapPosition *position)
{
    return GetWarpEventAtPosition(mapHeader, position->x - 7, position->y - 7, position->height);
}

static void SetupWarp(struct MapHeader *unused, s8 warpEventId, struct MapPosition *position)
{
    const struct WarpEvent *warpEvent;

    //u8 trainerHillMapId = GetCurrentTrainerHillMapId();
    //
    //if (trainerHillMapId)
    //{
    //    if (trainerHillMapId == GetNumFloorsInTrainerHillChallenge())
    //    {
    //        if (warpEventId == 0)
    //            warpEvent = &gMapHeader.events->warps[0];
    //        else
    //            warpEvent = SetWarpDestinationTrainerHill4F();
    //    }
    //    else if (trainerHillMapId == TRAINER_HILL_ROOF)
    //    {
    //        warpEvent = SetWarpDestinationTrainerHillFinalFloor(warpEventId);
    //    }
    //    else
    //    {
    //        warpEvent = &gMapHeader.events->warps[warpEventId];
    //    }
    //}
    //else
    //{
        warpEvent = &gMapHeader.events->warps[warpEventId];
    //}

    if (warpEvent->mapNum == MAP_NUM(NONE))
    {
        SetWarpDestinationToDynamicWarp(warpEvent->warpId);
    }
    else
    {
        const struct MapHeader *mapHeader;

        SetWarpDestinationToMapWarp(warpEvent->mapGroup, warpEvent->mapNum, warpEvent->warpId);
        UpdateEscapeWarp(position->x, position->y);
        mapHeader = Overworld_GetMapHeaderByGroupAndId(warpEvent->mapGroup, warpEvent->mapNum);
        if (mapHeader->events->warps[warpEvent->warpId].mapNum == MAP_NUM(NONE))
            SetDynamicWarp(mapHeader->events->warps[warpEventId].warpId, gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum, warpEventId);
    }
}

static bool8 TryDoorWarp(struct MapPosition *position, u16 metatileBehavior, u8 direction)
{
    s8 warpEventId;

    if (direction == DIR_NORTH)
    {
        if (MetatileBehavior_IsOpenSecretBaseDoor(metatileBehavior) == TRUE)
        {
            WarpIntoSecretBase(position, gMapHeader.events);
            return TRUE;
        }

        if (MetatileBehavior_IsWarpDoor(metatileBehavior) == TRUE)
        {
            warpEventId = GetWarpEventAtMapPosition(&gMapHeader, position);
            if (warpEventId != -1 && IsWarpMetatileBehavior(metatileBehavior) == TRUE)
            {
                StoreInitialPlayerAvatarState();
                SetupWarp(&gMapHeader, warpEventId, position);
                DoDoorWarp();
                return TRUE;
            }
        }
    }
    return FALSE;
}

static s8 GetWarpEventAtPosition(struct MapHeader *mapHeader, u16 x, u16 y, u8 elevation)
{
    s32 i;
    struct WarpEvent *warpEvent = mapHeader->events->warps;
    u8 warpCount = mapHeader->events->warpCount;

    for (i = 0; i < warpCount; i++, warpEvent++)
    {
        if ((u16)warpEvent->x == x && (u16)warpEvent->y == y)
        {
            if (warpEvent->elevation == elevation || warpEvent->elevation == 0)
                return i;
        }
    }
    return -1;
}

static u8 *TryRunCoordEventScript(struct CoordEvent *coordEvent)
{
    if (coordEvent != NULL)
    {
        if (coordEvent->script == NULL)
        {
            DoCoordEventWeather(coordEvent->trigger);
            return NULL;
        }
        if (coordEvent->trigger == 0)
        {
            ScriptContext2_RunNewScript(coordEvent->script);
            return NULL;
        }
        if (VarGet(coordEvent->trigger) == (u8)coordEvent->index)
            return coordEvent->script;
    }
    return NULL;
}

static u8 *GetCoordEventScriptAtPosition(struct MapHeader *mapHeader, u16 x, u16 y, u8 elevation)
{
    s32 i;
    struct CoordEvent *coordEvents = mapHeader->events->coordEvents;
    u8 coordEventCount = mapHeader->events->coordEventCount;

    for (i = 0; i < coordEventCount; i++)
    {
        if ((u16)coordEvents[i].x == x && (u16)coordEvents[i].y == y)
        {
            if (coordEvents[i].elevation == elevation || coordEvents[i].elevation == 0)
            {
                u8 *script = TryRunCoordEventScript(&coordEvents[i]);
                if (script != NULL)
                    return script;
            }
        }
    }
    return NULL;
}

u8 *GetCoordEventScriptAtMapPosition(struct MapPosition *position)
{
    return GetCoordEventScriptAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);
}

static struct BgEvent *GetBackgroundEventAtPosition(struct MapHeader *mapHeader, u16 x, u16 y, u8 elevation)
{
    u8 i;
    struct BgEvent *bgEvents = mapHeader->events->bgEvents;
    u8 bgEventCount = mapHeader->events->bgEventCount;

    for (i = 0; i < bgEventCount; i++)
    {
        if ((u16)bgEvents[i].x == x && (u16)bgEvents[i].y == y)
        {
            if (bgEvents[i].elevation == elevation || bgEvents[i].elevation == 0)
                return &bgEvents[i];
        }
    }
    return NULL;
}

bool8 TryDoDiveWarp(struct MapPosition *position, u16 metatileBehavior)
{
    if (gMapHeader.mapType == MAP_TYPE_UNDERWATER && !MetatileBehavior_IsUnableToEmerge(metatileBehavior))
    {
        if (SetDiveWarpEmerge(position->x - 7, position->y - 7))
        {
            StoreInitialPlayerAvatarState();
            DoDiveWarp();
            PlaySE(SE_M_DIVE);
            return TRUE;
        }
    }
    else if (MetatileBehavior_IsDiveable(metatileBehavior) == TRUE)
    {
        if (SetDiveWarpDive(position->x - 7, position->y - 7))
        {
            StoreInitialPlayerAvatarState();
            DoDiveWarp();
            PlaySE(SE_M_DIVE);
            return TRUE;
        }
    }
    return FALSE;
}

u8 TrySetDiveWarp(void)
{
    s16 x, y;
    u8 metatileBehavior;

    PlayerGetDestCoords(&x, &y);
    metatileBehavior = MapGridGetMetatileBehaviorAt(x, y);
    if (gMapHeader.mapType == MAP_TYPE_UNDERWATER && !MetatileBehavior_IsUnableToEmerge(metatileBehavior))
    {
        if (SetDiveWarpEmerge(x - 7, y - 7) == TRUE)
            return 1;
    }
    else if (MetatileBehavior_IsDiveable(metatileBehavior) == TRUE)
    {
        if (SetDiveWarpDive(x - 7, y - 7) == TRUE)
            return 2;
    }
    return 0;
}

const u8 *GetObjectEventScriptPointerPlayerFacing(void)
{
    u8 direction;
    struct MapPosition position;

    direction = GetPlayerMovementDirection();
    GetInFrontOfPlayerPosition(&position);
    return GetInteractedObjectEventScript(&position, MapGridGetMetatileBehaviorAt(position.x, position.y), direction);
}

int SetCableClubWarp(void)
{
    struct MapPosition position;

    GetPlayerPosition(&position);
    SetupWarp(&gMapHeader, GetWarpEventAtMapPosition(&gMapHeader, &position), &position);
    return 0;
}
