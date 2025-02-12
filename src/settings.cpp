/*
 * $Id$
 */

#include <cctype>
#include <cstring>

#include "settings.h"

#include "error.h"
#include "filesystem.h"
#include "screen.h"
#include "xu4.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#include <shlobj.h>
#elif defined(MACOSX)
#include <CoreServices/CoreServices.h>
#elif defined(IOS)
#include <CoreFoundation/CoreFoundation.h>
#include "U4CFHelper.h"
#include "ios_helpers.h"
#endif

using namespace std;

#if defined(_WIN32) || defined(__CYGWIN__)
#define SETTINGS_BASE_FILENAME "xu4.cfg"
#else
#define SETTINGS_BASE_FILENAME "xu4rc"
#endif

bool SettingsData::operator==(const SettingsData &s) const {
    intptr_t offset = (intptr_t)&end_of_bitwise_comparators - (intptr_t)this;
    if (memcmp(this, &s, offset) != 0)
        return false;

    if (gemLayout != s.gemLayout)
        return false;
    if (videoType != s.videoType)
        return false;
    if (logging != s.logging)
        return false;
    if (game != s.game)
        return false;

    return true;
}

bool SettingsData::operator!=(const SettingsData &s) const {
    return !operator==(s);
}


/**
 * Initialize the settings.
 */
void Settings::init(const char* profileName) {
    if (profileName && profileName[0]) {
        userPath = "./profiles/";
        userPath += profileName;
        userPath += "/";

        profile = profileName;
        if (profile.length() > 20)
            errorFatal("Profile name must be no more than 20 characters.");
    } else {
        profile.clear();

#if defined(MACOSX)
            FSRef folder;
            OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &folder);
            if (err == noErr) {
                UInt8 path[2048];
                if (FSRefMakePath(&folder, path, 2048) == noErr) {
                    userPath.append(reinterpret_cast<const char *>(path));
                    userPath.append("/xu4/");
                }
            }
            if (userPath.empty()) {
                char *home = getenv("HOME");
                if (home && home[0]) {
                    if (userPath.size() == 0) {
                        userPath += home;
                        userPath += "/.xu4";
                        userPath += "/";
                    }
                } else {
                    userPath = "./";
                }
            }
#elif defined(IOS)
        boost::intrusive_ptr<CFURL> urlForLocation = cftypeFromCreateOrCopy(U4IOS::copyAppSupportDirectoryLocation());
        if (urlForLocation != 0) {
            char path[2048];
            boost::intrusive_ptr<CFString> tmpStr = cftypeFromCreateOrCopy(CFURLCopyFileSystemPath(urlForLocation.get(), kCFURLPOSIXPathStyle));
            if (CFStringGetFileSystemRepresentation(tmpStr.get(), path, 2048) != false) {
                userPath.append(path);
                userPath += "/";
            }
        }
#elif defined(__unix__)
        char *home = getenv("HOME");
        if (home && home[0]) {
            userPath += home;
#ifdef __linux__
            userPath += "/.config/xu4/";
#else
            userPath += "/.xu4/";
#endif
        } else
            userPath = "./";
#elif defined(_WIN32) || defined(__CYGWIN__)
        char* appdata = getenv("APPDATA");
        if (appdata) {
            userPath = appdata;
            userPath += "\\xu4\\";
        } else
            userPath = ".\\";
#else
        userPath = "./";
#endif

    }
    FileSystem::createDirectory(userPath);

    filename = userPath + SETTINGS_BASE_FILENAME;

    read();
}

void Settings::setData(const SettingsData &data) {
    // bitwise copy is safe
    *(SettingsData *)this = data;
}

/*
 * Return index of value in the names string list, or zero (the first name) if
 * there is no matching name.
 */
uint8_t Settings::settingEnum(const char** names, const char* value) {
    const char** it = names;
    while (*it) {
        if (strcasecmp(*it, value) == 0)
            return it - names;
        ++it;
    }
    return 0;
}

/**
 * Read settings in from the settings file.
 */
bool Settings::read() {
    char buffer[256];
    FILE *settingsFile;

    /* default settings */
    scale                 = DEFAULT_SCALE;
    fullscreen            = DEFAULT_FULLSCREEN;
    filter                = DEFAULT_FILTER;
    videoType             = DEFAULT_VIDEO_TYPE;
    gemLayout             = DEFAULT_GEM_LAYOUT;
    lineOfSight           = DEFAULT_LINEOFSIGHT;
    screenShakes          = DEFAULT_SCREEN_SHAKES;
    gamma                 = DEFAULT_GAMMA;
    musicVol              = DEFAULT_MUSIC_VOLUME;
    soundVol              = DEFAULT_SOUND_VOLUME;
    volumeFades           = DEFAULT_VOLUME_FADES;
    shortcutCommands      = DEFAULT_SHORTCUT_COMMANDS;
    keydelay              = DEFAULT_KEY_DELAY;
    keyinterval           = DEFAULT_KEY_INTERVAL;
    filterMoveMessages    = DEFAULT_FILTER_MOVE_MESSAGES;
    battleSpeed           = DEFAULT_BATTLE_SPEED;
    enhancements          = DEFAULT_ENHANCEMENTS;
    gameCyclesPerSecond   = DEFAULT_CYCLES_PER_SECOND;
    screenAnimationFramesPerSecond = DEFAULT_ANIMATION_FRAMES_PER_SECOND;
    debug                 = DEFAULT_DEBUG;
    battleDiff            = DEFAULT_BATTLE_DIFFICULTY;
#ifndef USE_BORON
    validateXml           = DEFAULT_VALIDATE_XML;
#endif
    spellEffectSpeed      = DEFAULT_SPELL_EFFECT_SPEED;
    campTime              = DEFAULT_CAMP_TIME;
    innTime               = DEFAULT_INN_TIME;
    shrineTime            = DEFAULT_SHRINE_TIME;
    shakeInterval         = DEFAULT_SHAKE_INTERVAL;
    titleSpeedRandom      = DEFAULT_TITLE_SPEED_RANDOM;
    titleSpeedOther       = DEFAULT_TITLE_SPEED_OTHER;

#if 0
    pauseForEachMovement  = DEFAULT_PAUSE_FOR_EACH_MOVEMENT;
    pauseForEachTurn      = DEFAULT_PAUSE_FOR_EACH_TURN;
#endif

    /* all specific minor enhancements default to "on", any major enhancements default to "off" */
    enhancementsOptions.activePlayer     = true;
    enhancementsOptions.u5spellMixing    = true;
    enhancementsOptions.u5shrines        = true;
    enhancementsOptions.slimeDivides     = true;
    enhancementsOptions.gazerSpawnsInsects = true;
    enhancementsOptions.textColorization = false;
    enhancementsOptions.c64chestTraps    = true;
    enhancementsOptions.smartEnterKey    = true;
    enhancementsOptions.peerShowsObjects = false;
    enhancementsOptions.u5combat         = false;
    enhancementsOptions.u4TileTransparencyHack = true;
    enhancementsOptions.u4TileTransparencyHackPixelShadowOpacity = DEFAULT_SHADOW_PIXEL_OPACITY;
    enhancementsOptions.u4TrileTransparencyHackShadowBreadth = DEFAULT_SHADOW_PIXEL_SIZE;

    innAlwaysCombat = 0;
    campingAlwaysCombat = 0;

    /* mouse defaults to on */
    mouseOptions.enabled = 1;

    logging = DEFAULT_LOGGING;
    game = "Ultima-IV";

    settingsFile = fopen(filename.c_str(), "rt");
    if (!settingsFile)
        return false;

    while(fgets(buffer, sizeof(buffer), settingsFile) != NULL) {
        while (isspace(buffer[strlen(buffer) - 1]))
            buffer[strlen(buffer) - 1] = '\0';

        if (strstr(buffer, "scale=") == buffer)
            scale = (unsigned int) strtoul(buffer + strlen("scale="), NULL, 0);
        else if (strstr(buffer, "fullscreen=") == buffer)
            fullscreen = (int) strtoul(buffer + strlen("fullscreen="), NULL, 0);
        else if (strstr(buffer, "filter=") == buffer)
            filter = settingEnum(screenGetFilterNames(),
                               buffer + strlen("filter="));
        else if (strstr(buffer, "lineOfSight=") == buffer)
            lineOfSight = settingEnum(screenGetLineOfSightStyles(),
                                      buffer + strlen("lineOfSight="));
        else if (strstr(buffer, "video=") == buffer)
            videoType = buffer + strlen("video=");
        else if (strstr(buffer, "gemLayout=") == buffer)
            gemLayout = buffer + strlen("gemLayout=");
        else if (strstr(buffer, "screenShakes=") == buffer)
            screenShakes = (int) strtoul(buffer + strlen("screenShakes="), NULL, 0);
        else if (strstr(buffer, "gamma=") == buffer)
            gamma = (int) strtoul(buffer + strlen("gamma="), NULL, 0);
        else if (strstr(buffer, "musicVol=") == buffer)
            musicVol = (int) strtoul(buffer + strlen("musicVol="), NULL, 0);
        else if (strstr(buffer, "soundVol=") == buffer)
            soundVol = (int) strtoul(buffer + strlen("soundVol="), NULL, 0);
        else if (strstr(buffer, "volumeFades=") == buffer)
            volumeFades = (int) strtoul(buffer + strlen("volumeFades="), NULL, 0);
        else if (strstr(buffer, "shortcutCommands=") == buffer)
            shortcutCommands = (int) strtoul(buffer + strlen("shortcutCommands="), NULL, 0);
        else if (strstr(buffer, "keydelay=") == buffer)
            keydelay = (int) strtoul(buffer + strlen("keydelay="), NULL, 0);
        else if (strstr(buffer, "keyinterval=") == buffer)
            keyinterval = (int) strtoul(buffer + strlen("keyinterval="), NULL, 0);
        else if (strstr(buffer, "filterMoveMessages=") == buffer)
            filterMoveMessages = (int) strtoul(buffer + strlen("filterMoveMessages="), NULL, 0);
        else if (strstr(buffer, "battlespeed=") == buffer)
            battleSpeed = (int) strtoul(buffer + strlen("battlespeed="), NULL, 0);
        else if (strstr(buffer, "enhancements=") == buffer)
            enhancements = (int) strtoul(buffer + strlen("enhancements="), NULL, 0);
        else if (strstr(buffer, "gameCyclesPerSecond=") == buffer)
            gameCyclesPerSecond = (int) strtoul(buffer + strlen("gameCyclesPerSecond="), NULL, 0);
        else if (strstr(buffer, "debug=") == buffer)
            debug = (int) strtoul(buffer + strlen("debug="), NULL, 0);
        else if (strstr(buffer, "battleDiff=") == buffer)
            battleDiff = settingEnum(battleDiffStrings(),
                                     buffer + strlen("battleDiff="));
#ifndef USE_BORON
        else if (strstr(buffer, "validateXml=") == buffer)
            validateXml = (int) strtoul(buffer + strlen("validateXml="), NULL, 0);
#endif
        else if (strstr(buffer, "spellEffectSpeed=") == buffer)
            spellEffectSpeed = (int) strtoul(buffer + strlen("spellEffectSpeed="), NULL, 0);
        else if (strstr(buffer, "campTime=") == buffer)
            campTime = (int) strtoul(buffer + strlen("campTime="), NULL, 0);
        else if (strstr(buffer, "innTime=") == buffer)
            innTime = (int) strtoul(buffer + strlen("innTime="), NULL, 0);
        else if (strstr(buffer, "shrineTime=") == buffer)
            shrineTime = (int) strtoul(buffer + strlen("shrineTime="), NULL, 0);
        else if (strstr(buffer, "shakeInterval=") == buffer)
            shakeInterval = (int) strtoul(buffer + strlen("shakeInterval="), NULL, 0);
        else if (strstr(buffer, "titleSpeedRandom=") == buffer)
            titleSpeedRandom = (int) strtoul(buffer + strlen("titleSpeedRandom="), NULL, 0);
        else if (strstr(buffer, "titleSpeedOther=") == buffer)
            titleSpeedOther = (int) strtoul(buffer + strlen("titleSpeedOther="), NULL, 0);

        /* minor enhancement options */
        else if (strstr(buffer, "activePlayer=") == buffer)
            enhancementsOptions.activePlayer = (int) strtoul(buffer + strlen("activePlayer="), NULL, 0);
        else if (strstr(buffer, "u5spellMixing=") == buffer)
            enhancementsOptions.u5spellMixing = (int) strtoul(buffer + strlen("u5spellMixing="), NULL, 0);
        else if (strstr(buffer, "u5shrines=") == buffer)
            enhancementsOptions.u5shrines = (int) strtoul(buffer + strlen("u5shrines="), NULL, 0);
        else if (strstr(buffer, "slimeDivides=") == buffer)
            enhancementsOptions.slimeDivides = (int) strtoul(buffer + strlen("slimeDivides="), NULL, 0);
        else if (strstr(buffer, "gazerSpawnsInsects=") == buffer)
            enhancementsOptions.gazerSpawnsInsects = (int) strtoul(buffer + strlen("gazerSpawnsInsects="), NULL, 0);
        else if (strstr(buffer, "textColorization=") == buffer)
            enhancementsOptions.textColorization = (int) strtoul(buffer + strlen("textColorization="), NULL, 0);
        else if (strstr(buffer, "c64chestTraps=") == buffer)
            enhancementsOptions.c64chestTraps = (int) strtoul(buffer + strlen("c64chestTraps="), NULL, 0);
        else if (strstr(buffer, "smartEnterKey=") == buffer)
            enhancementsOptions.smartEnterKey = (int) strtoul(buffer + strlen("smartEnterKey="), NULL, 0);

        /* major enhancement options */
        else if (strstr(buffer, "peerShowsObjects=") == buffer)
            enhancementsOptions.peerShowsObjects = (int) strtoul(buffer + strlen("peerShowsObjects="), NULL, 0);
        else if (strstr(buffer, "u5combat=") == buffer)
            enhancementsOptions.u5combat = (int) strtoul(buffer + strlen("u5combat="), NULL, 0);
        else if (strstr(buffer, "innAlwaysCombat=") == buffer)
            innAlwaysCombat = (int) strtoul(buffer + strlen("innAlwaysCombat="), NULL, 0);
        else if (strstr(buffer, "campingAlwaysCombat=") == buffer)
            campingAlwaysCombat = (int) strtoul(buffer + strlen("campingAlwaysCombat="), NULL, 0);

        /* mouse options */
        else if (strstr(buffer, "mouseEnabled=") == buffer)
            mouseOptions.enabled = (int) strtoul(buffer + strlen("mouseEnabled="), NULL, 0);
        else if (strstr(buffer, "logging=") == buffer)
            logging = buffer + strlen("logging=");
        else if (strstr(buffer, "game=") == buffer)
            game = buffer + strlen("game=");

        /* graphics enhancements options */
        else if (strstr(buffer, "renderTileTransparency=") == buffer)
            enhancementsOptions.u4TileTransparencyHack = (int) strtoul(buffer + strlen("renderTileTransparency="), NULL, 0);
        else if (strstr(buffer, "transparentTilePixelShadowOpacity=") == buffer)
            enhancementsOptions.u4TileTransparencyHackPixelShadowOpacity = (int) strtoul(buffer + strlen("transparentTilePixelShadowOpacity="), NULL, 0);
        else if (strstr(buffer, "transparentTileShadowSize=") == buffer)
            enhancementsOptions.u4TrileTransparencyHackShadowBreadth = (int) strtoul(buffer + strlen("transparentTileShadowSize="), NULL, 0);



        /**
         * FIXME: this is just to avoid an error for those who have not written
         * a new xu4.cfg file since these items were removed.  Remove them after a reasonable
         * amount of time
         *
         * remove:  attackspeed, minorEnhancements, majorEnhancements, vol
         */

        else if (strstr(buffer, "attackspeed=") == buffer);
        else if (strstr(buffer, "minorEnhancements=") == buffer)
            enhancements = (int)strtoul(buffer + strlen("minorEnhancements="), NULL, 0);
        else if (strstr(buffer, "majorEnhancements=") == buffer);
        else if (strstr(buffer, "vol=") == buffer)
            musicVol = soundVol = (int) strtoul(buffer + strlen("vol="), NULL, 0);

        /***/

        else
            errorWarning("invalid line in settings file %s", buffer);
    }

    fclose(settingsFile);
    return true;
}

/**
 * Write the settings out into a human readable file.  This also
 * notifies observers that changes have been commited.
 */
bool Settings::write() {
    FILE *settingsFile;

    settingsFile = fopen(filename.c_str(), "wt");
    if (!settingsFile) {
        errorWarning("can't write settings file");
        return false;
    }

    fprintf(settingsFile,
            "scale=%d\n"
            "fullscreen=%d\n"
            "filter=%s\n"
            "video=%s\n"
            "gemLayout=%s\n"
            "lineOfSight=%s\n"
            "screenShakes=%d\n"
            "gamma=%d\n"
            "musicVol=%d\n"
            "soundVol=%d\n"
            "volumeFades=%d\n"
            "shortcutCommands=%d\n"
            "keydelay=%d\n"
            "keyinterval=%d\n"
            "filterMoveMessages=%d\n"
            "battlespeed=%d\n"
            "enhancements=%d\n"
            "gameCyclesPerSecond=%d\n"
            "debug=%d\n"
            "battleDiff=%s\n"
            "spellEffectSpeed=%d\n"
            "campTime=%d\n"
            "innTime=%d\n"
            "shrineTime=%d\n"
            "shakeInterval=%d\n"
            "titleSpeedRandom=%d\n"
            "titleSpeedOther=%d\n",
            scale,
            fullscreen,
            screenGetFilterNames()[ filter ],
            videoType.c_str(),
            gemLayout.c_str(),
            screenGetLineOfSightStyles()[ lineOfSight ],
            screenShakes,
            gamma,
            musicVol,
            soundVol,
            volumeFades,
            shortcutCommands,
            keydelay,
            keyinterval,
            filterMoveMessages,
            battleSpeed,
            enhancements,
            gameCyclesPerSecond,
            debug,
            battleDiffStrings()[ battleDiff ],
            spellEffectSpeed,
            campTime,
            innTime,
            shrineTime,
            shakeInterval,
            titleSpeedRandom,
            titleSpeedOther);

#ifndef USE_BORON
    fprintf(settingsFile, "validateXml=%d\n", validateXml);
#endif

    // Enhancements Options
    fprintf(settingsFile,
            "activePlayer=%d\n"
            "u5spellMixing=%d\n"
            "u5shrines=%d\n"
            "slimeDivides=%d\n"
            "gazerSpawnsInsects=%d\n"
            "textColorization=%d\n"
            "c64chestTraps=%d\n"
            "smartEnterKey=%d\n"
            "peerShowsObjects=%d\n"
            "u5combat=%d\n"
            "innAlwaysCombat=%d\n"
            "campingAlwaysCombat=%d\n"
            "mouseEnabled=%d\n"
            "logging=%s\n"
            "game=%s\n"
            "renderTileTransparency=%d\n"
            "transparentTilePixelShadowOpacity=%d\n"
            "transparentTileShadowSize=%d\n",
            enhancementsOptions.activePlayer,
            enhancementsOptions.u5spellMixing,
            enhancementsOptions.u5shrines,
            enhancementsOptions.slimeDivides,
            enhancementsOptions.gazerSpawnsInsects,
            enhancementsOptions.textColorization,
            enhancementsOptions.c64chestTraps,
            enhancementsOptions.smartEnterKey,
            enhancementsOptions.peerShowsObjects,
            enhancementsOptions.u5combat,
            innAlwaysCombat,
            campingAlwaysCombat,
            mouseOptions.enabled,
            logging.c_str(),
            game.c_str(),
            enhancementsOptions.u4TileTransparencyHack,
            enhancementsOptions.u4TileTransparencyHackPixelShadowOpacity,
            enhancementsOptions.u4TrileTransparencyHackShadowBreadth);

    fclose(settingsFile);

    gs_emitMessage(SENDER_SETTINGS, this);

    return true;
}

const char** Settings::battleDiffStrings() {
    static const char* difficulty[] = {"Normal", "Hard", "Expert", NULL};
    return difficulty;
}
