/*
 * $Id$
 */

#include <list>
#include <map>
#include <set>

#include "location.h"

#include "context.h"
#include "combat.h"
#include "settings.h"
#include "tileset.h"
#include "xu4.h"


/**
 * Add a new location to the stack, or
 * start a new stack if 'prev' is NULL
 */
Location::Location(const Coords& coords, Map *map, int viewmode,
        LocationContext ctx, TurnController *turnCompleter, Location *prev) {

    this->coords = coords;
    this->map = map;
    this->viewMode = viewmode;
    this->context = ctx;
    this->turnCompleter = turnCompleter;

    // Push location onto the stack.
    this->prev = prev;
}

/**
 * Pop a location from the stack and free the memory
 */
void locationFree(Location **stack) {
    Location* loc = *stack;
    *stack = loc->prev;
    loc->prev = NULL;

    delete loc;
}

/**
 * Append the entire stack of objects at the given location to a vector.
 */
void Location::getTilesAt(std::vector<MapTile>& tiles,
                          const Coords& coords, bool& focus) {
    const Object *obj = map->objectAt(coords);
    const Creature *m = dynamic_cast<const Creature *>(obj);
    focus = false;

    bool avatar = this->coords == coords;

    /* Do not return objects for VIEW_GEM mode, show only the avatar and tiles */
    if (viewMode == VIEW_GEM && (!xu4.settings->enhancements || !xu4.settings->enhancementsOptions.peerShowsObjects)) {
        // When viewing a gem, always show the avatar regardless of whether or not
        // it is shown in our normal view
        if (avatar)
            tiles.push_back(c->party->getTransport());
        else
            tiles.push_back(MapTile(map->getTileFromData(coords)));
    }

    /* Add the avatar to gem view */
    if (avatar && viewMode == VIEW_GEM)
        tiles.push_back(c->party->getTransport());

    /* Add visual-only annotations to the list */
    std::list<Annotation *> annot = map->annotations.ptrsToAllAt(coords);
    std::list<Annotation *>::const_iterator i;
    for (i = annot.begin(); i != annot.end(); i++) {
        if ((*i)->visualOnly)
        {
            tiles.push_back((*i)->tile);

            /* If this is the first cover-up annotation,
             * everything underneath it will be invisible,
             * so stop here
             */
            if ((*i)->coverUp)
                return;
        }
    }

    /* then the avatar is drawn (unless on a ship) */
    if ((map->flags & SHOW_AVATAR) && (c->transportContext != TRANSPORT_SHIP) && avatar)
        //tiles.push_back(map->tileset->getByName("avatar")->id);
        tiles.push_back(c->party->getTransport());

    /* then camouflaged creatures that have a disguise */
    if (obj && (obj->objType == Object::CREATURE) &&
        ! obj->visible && m->getCamouflageTile()) {
        focus = focus || obj->focused;
        tiles.push_back(map->tileset->getByName(m->getCamouflageTile())->getId());
    }
    /* then visible creatures and objects */
    else if (obj && obj->visible) {
        focus = focus || obj->focused;
        MapTile visibleTile = obj->tile;
        //Sleeping creatures and persons have their animation frozen
        if (m && m->isAsleep())
            visibleTile.freezeAnimation = true;
        else if (obj->animId != ANIM_UNUSED)
            visibleTile.frame = anim_valueI(&xu4.eventHandler->flourishAnim,
                                            obj->animId);
        tiles.push_back(visibleTile);
    }

    /* then the party's ship (because twisters and whirlpools get displayed on top of ships) */
    if ((map->flags & SHOW_AVATAR) && (c->transportContext == TRANSPORT_SHIP) && avatar)
        tiles.push_back(c->party->getTransport());

    /* then permanent annotations */
    for (i = annot.begin(); i != annot.end(); i++) {
        if (!(*i)->visualOnly) {
            tiles.push_back((*i)->tile);

            /* If this is the first cover-up annotation,
             * everything underneath it will be invisible,
             * so stop here
             */
            if ((*i)->coverUp)
                return;
        }
    }

    /* finally the base tile */
    MapTile terrainTile(map->getTileFromData(coords));
    tiles.push_back(terrainTile);

    /* But if the base tile requires a background, we must find it */
    const Tile* tileType = terrainTile.getTileType();
    if (tileType->isLandForeground() ||
        tileType->isWaterForeground() ||
        tileType->isLivingObject()) {
        tiles.push_back(getReplacementTile(coords, tileType));
    }
}


/**
 * Finds a valid replacement tile for the given location, using surrounding tiles
 * as guidelines to choose the new tile.  The new tile will only be chosen if it
 * is marked as a valid replacement (or waterReplacement) tile in tiles.xml.  If a valid replacement
 * cannot be found, it returns a "best guess" tile.
 */
TileId Location::getReplacementTile(const Coords& atCoords, const Tile * forTile) {
    std::map<TileId, int> validMapTileCount;

    const static int dirs[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
    const static int dirs_per_step = sizeof(dirs) / sizeof(*dirs);
    int loop_count = 0;

    std::set<Coords> searched;
    std::list<Coords> searchQueue;

    //Pathfinding to closest traversable tile with appropriate replacement properties.
    //For tiles marked water-replaceable, pathfinding includes swimmables.
    searchQueue.push_back(atCoords);
    do
    {
        Coords currentStep = searchQueue.front();
        searchQueue.pop_front();

        searched.insert(currentStep);

        for (int i = 0; i < dirs_per_step; i++)
        {
            Coords newStep(currentStep);
            map_move(newStep, dirs[i][0], dirs[i][1], map);

            Tile const * tileType = map->tileTypeAt(newStep,WITHOUT_OBJECTS);

            if (!tileType->isOpaque()) {
                //if (searched.find(newStep) == searched.end()) -- the find mechanism doesn't work.
                searchQueue.push_back(newStep);
            }

            if ((tileType->isReplacement() && (forTile->isLandForeground() || forTile->isLivingObject())) ||
                (tileType->isWaterReplacement() && forTile->isWaterForeground()))
            {
                std::map<TileId, int>::iterator validCount = validMapTileCount.find(tileType->getId());

                if (validCount == validMapTileCount.end())
                {
                    validMapTileCount[tileType->getId()] = 1;
                }
                else
                {
                    validMapTileCount[tileType->getId()]++;
                }
            }
        }

        if (validMapTileCount.size() > 0)
        {
            std::map<TileId, int>::iterator itr = validMapTileCount.begin();

            TileId winner = itr->first;
            int score = itr->second;

            while (++itr != validMapTileCount.end())
            {
                if (score < itr->second)
                {
                    score = itr->second;
                    winner = itr->first;
                }
            }

            return winner;
        }
        /* loop_count is an ugly hack to temporarily fix infinite loop */
    } while (++loop_count < 128 && searchQueue.size() > 0 && searchQueue.size() < 64);

    /* couldn't find a tile, give it the classic default */
    return map->tileset->getByName(Tile::sym.brickFloor)->getId();
}

/**
 * Returns the current coordinates of the location given:
 *     If in combat - returns the coordinates of party member with focus
 *     If elsewhere - returns the coordinates of the avatar
 */
int Location::getCurrentPosition(Coords* pos) {
    if (context & CTX_COMBAT) {
        CombatController *cc = dynamic_cast<CombatController *>(xu4.eventHandler->getController());
        PartyMemberVector *party = cc->getParty();
        *pos = (*party)[cc->getFocus()]->coords;
    }
    else
        *pos = coords;

    return 1;
}

MoveResult Location::move(Direction dir, bool userEvent) {
    MoveEvent event(dir, userEvent);
    event.location = this;
    switch (map->type) {

    case Map::DUNGEON:
        moveAvatarInDungeon(event);
        break;

    case Map::COMBAT:
        movePartyMember(event);
        break;

    default:
        moveAvatar(event);
        break;
    }

    gs_emitMessage(SENDER_LOCATION, &event);
    return event.result;
}
