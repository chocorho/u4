/*
 * $Id$
 */

#include "portal.h"

#include "city.h"
#include "context.h"
#include "dungeon.h"
#include "game.h"
#include "location.h"
#include "names.h"
#include "screen.h"
#include "shrine.h"
#include "mapmgr.h"

int usePortalAt(Location *location, int x, int y, int z, PortalTriggerAction action) {
    Map *destination;
    char *msg = NULL;
    
    /* find a portal at the specified location */
    const Portal *portal = mapPortalAt(c->location->map, x, y, z);

    /* didn't find a portal there */
    if (!portal)
        return 0;
    /* action didn't work for that portal */
    else if (portal && (portal->trigger_action != action) && (portal->trigger_action != ACTION_NONE))
        return 0;
    /* conditions not met for portal to work */
    else if (portal && portal->portalConditionsMet && !(*portal->portalConditionsMet)(portal))
        return 0;
    /* must klimb or descend on foot! */
    else if (c->transportContext & ~TRANSPORT_FOOT && (action == ACTION_KLIMB || action == ACTION_DESCEND)) {
        screenMessage("%sOnly on foot!\n", action == ACTION_KLIMB ? "Klimb\n" : "");
        return 1;
    }
    
    destination = mapMgrGetById(portal->destid);

    if (!portal->message) {

        switch(action) {
        case ACTION_DESCEND:    msg = "Descend to first floor!\n"; break;
        case ACTION_KLIMB:      msg = "Klimb to second floor!\n"; break;
        case ACTION_ENTER:
            switch (destination->type) {
            case MAPTYPE_TOWN:
                screenMessage("Enter towne!\n\n%s\n\n", destination->city->name);
                break;
            case MAPTYPE_VILLAGE:
                screenMessage("Enter village!\n\n%s\n\n", destination->city->name);
                break;
            case MAPTYPE_CASTLE:
                screenMessage("Enter castle!\n\n%s\n\n", destination->city->name);
                break;
            case MAPTYPE_RUIN:
                screenMessage("Enter ruin!\n\n%s\n\n", destination->city->name);
                break;
            case MAPTYPE_SHRINE:
                screenMessage("Enter the Shrine of %s!\n\n", getVirtueName(destination->shrine->virtue));
                break;
            case MAPTYPE_DUNGEON:
                screenMessage("Enter dungeon!\n\n%s\n\n", destination->dungeon->name);
                break;
            default:
                break;
            }
            break;
        case ACTION_NONE:
        default: break;
        }
    }

    /* check the transportation requisites of the portal */
    if (c->transportContext & ~portal->portalTransportRequisites) {
        screenMessage("Only on foot!\n");
        return 1;
    }
    /* ok, we know the portal is going to work -- now display the custom message, if any */
    else if (portal->message)
        screenMessage("%s", portal->message);
    
    gameSetMap(c, destination, portal->saveLocation, portal);
    musicPlay();

    /* if the portal changes the map retroactively, do it here */
    if (portal->retroActiveDest && c->location->prev) {
        c->location->prev->x = portal->retroActiveDest->x;
        c->location->prev->y = portal->retroActiveDest->y;
        c->location->prev->z = portal->retroActiveDest->z;
        c->location->prev->map = mapMgrGetById(portal->retroActiveDest->mapid);
    }

    if (destination->type == MAPTYPE_SHRINE)        
        shrineEnter(destination->shrine);

    return 1;
}