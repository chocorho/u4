/*
 * object.h
 */

#ifndef OBJECT_H
#define OBJECT_H

#include "anim.h"
#include "coords.h"
#include "tile.h"

typedef enum {
    MOVEMENT_FIXED,
    MOVEMENT_WANDER,
    MOVEMENT_FOLLOW_AVATAR,
    MOVEMENT_ATTACK_AVATAR
} ObjectMovementBehavior;

class Map;

class Object {
public:
    enum Type {
        UNKNOWN,
        CREATURE,
        PERSON
    };

    Object(Type type = UNKNOWN);
    virtual ~Object();

    // Methods
    MapTile& getTile()                      { return tile; }
    const MapTile& getTile() const          { return tile; }
    const MapTile& getPrevTile() const      { return prevTile; }
    const Coords& getCoords() const         { return coords; }
    const Coords& getPrevCoords() const     { return prevCoords; }
    ObjectMovementBehavior getMovementBehavior() const    { return movement_behavior; }
    Type getType() const                    { return objType; }
    bool hasFocus() const                   { return focused; }
    bool isVisible() const                  { return visible; }
    bool isAnimated() const                 { return animated; }

    void setTile(MapTile t)                 { tile = t; }
    void setTile(const Tile *t)             { tile = t->getId(); }
    void setPrevTile(MapTile t)             { prevTile = t; }
    void setCoords(const Coords& c)         { prevCoords = coords; coords = c; }
    void setPrevCoords(const Coords& c)     { prevCoords = c; }
    void setMovementBehavior(ObjectMovementBehavior b)          { movement_behavior = b; }
    void setType(Type t)                    { objType = t; }
    void setFocus(bool f = true)            { focused = f; }
    void setVisible(bool v = true)          { visible = v; }
    void setAnimated(bool a = true)         { animated = a; }

    void placeOnMap(Map*, const Coords&);
    void removeFromMaps();

    bool setDirection(Direction d);

    void animateMovement();

    // Properties
protected:
    MapTile tile, prevTile;
    Coords coords, prevCoords;
    ObjectMovementBehavior movement_behavior;
    Type objType;
    AnimId animId;

    bool focused;
    bool visible;
    bool animated;
    uint8_t onMaps;

    friend class Map;
};

#endif
