/*
 * See Licensing and Copyright notice in naev.h
 */


/**
 * @file pilot_ew.c
 *
 * @brief Pilot electronic warfare information.
 */


#include "pilot.h"

#include "naev.h"

#include <math.h>

#include "log.h"
#include "space.h"
#include "player.h"

static double sensor_curRange    = 0.; /**< Current base sensor range, used to calculate
                                         what is in range and what isn't. */


/**
 * @brief Updates the pilot's static electronic warfare properties.
 *
 *    @param p Pilot to update.
 */
void pilot_ewUpdateStatic( Pilot *p )
{
   p->ew_mass     = pilot_ewMass( p->solid->mass );
   p->ew_heat     = pilot_ewHeat( p->heat_T );
   p->ew_hide     = p->ew_base_hide * p->ew_mass * p->ew_heat;
}


/**
 * @brief Updates the pilot's dynamic electronic warfare properties.
 *
 *    @param p Pilot to update.
 */
void pilot_ewUpdateDynamic( Pilot *p )
{
   /* Update hide. */
   p->ew_heat     = pilot_ewHeat( p->heat_T );
   p->ew_hide     = p->ew_base_hide * p->ew_mass * p->ew_heat;

   /* Update evasion. */
   p->ew_movement = pilot_ewMovement( VMOD(p->solid->vel) );
   p->ew_evasion  = p->ew_hide * p->ew_movement;
}


/**
 * @brief Gets the electronic warfare movement modifier for a given velocity.
 *
 *    @param vmod Velocity to get electronic warfare movement modifier of.
 *    @return The electronic warfare movement modifier.
 */
double pilot_ewMovement( double vmod )
{
   return 1. + vmod / 100.;
}


/**
 * @brief Gets the electronic warfare heat modifier for a given temperature.
 *
 *    @param T Temperature of the ship.
 *    @return The electronic warfare heat modifier.
 */
double pilot_ewHeat( double T )
{
   return 1. - 0.001 * (T - CONST_SPACE_STAR_TEMP);
}


/**
 * @brief Gets the electronic warfare mass modifier for a given mass.
 *
 *    @param mass Mass to get the electronic warfare mass modifier of.
 *    @return The electronic warfare mass modifier.
 */
double pilot_ewMass( double mass )
{
   return 1. / (1. + pow( mass, 0.75 ) / 100. );
}


/**
 * @brief Updates the system's base sensor range.
 */
void pilot_updateSensorRange (void)
{
   /* Calculate the sensor sensor_curRange. */
   /* 0    ->   7500.0
    * 250  ->   3333.33333333
    * 500  ->   2142.85714285
    * 750  ->   1578.94736842
    * 1000 ->   1250.0 */
   sensor_curRange  = 7500;
   sensor_curRange /= ((cur_system->interference + 200) / 200.);

   /* Speeds up calculations as we compare it against vectors later on
    * and we want to avoid actually calculating the sqrt(). */
   sensor_curRange = pow2(sensor_curRange);
}


/**
 * @brief Check to see if a position is in range of the pilot.
 *
 *    @param p Pilot to check to see if position is in his sensor range.
 *    @param x X position to check.
 *    @param y Y position to check.
 *    @return 1 if the position is in range, 0 if it isn't.
 */
int pilot_inRange( const Pilot *p, double x, double y )
{
   double d, sense;

   /* Get distance. */
   d = pow2(x-p->solid->pos.x) + pow2(y-p->solid->pos.y);

   sense = sensor_curRange * p->ew_detect;
   if (d < sense)
      return 1;

   return 0;
}


/**
 * @brief Check to see if a pilot is in sensor range of another.
 *
 *    @param p Pilot who is trying to check to see if other is in sensor range.
 *    @param target Target of p to check to see if is in sensor range.
 *    @return 1 if they are in range, 0 if they aren't and -1 if they are detected fuzzily.
 */
int pilot_inRangePilot( const Pilot *p, const Pilot *target )
{
   double d, sense;

   /* Special case player or omni-visible. */
   if ((pilot_isPlayer(p) && pilot_isFlag(target, PILOT_VISPLAYER)) ||
         pilot_isFlag(target, PILOT_VISIBLE))
      return 1;

   /* Get distance. */
   d = vect_dist2( &p->solid->pos, &target->solid->pos );

   sense = sensor_curRange * p->ew_detect;
   if (d * target->ew_evasion < sense)
      return 1;
   else if  (d * target->ew_hide < sense)
      return -1;

   return 0;
}


/**
 * @brief Check to see if a planet is in sensor range of the pilot.
 *
 *    @param p Pilot who is trying to check to see if the planet is in sensor range.
 *    @param target Planet to see if is in sensor range.
 *    @return 1 if they are in range, 0 if they aren't.
 */
int pilot_inRangePlanet( const Pilot *p, int target )
{
   double d;
   Planet *pnt;
   double sense;

   /* pilot must exist */
   if ( p == NULL )
      return 0;

   /* Get the planet. */
   pnt = cur_system->planets[target];

   /* target must not be virtual */
   if ( !pnt->real )
      return 0;

   /* @TODO ew_detect should be squared upon being set. */
   sense = sensor_curRange * pow2(p->ew_detect);

   /* Get distance. */
   d = vect_dist2( &p->solid->pos, &pnt->pos );

   if (d * pnt->hide < sense )
      return 1;

   return 0;
}

/**
 * @brief Check to see if a jump point is in sensor range of the pilot.
 *
 *    @param p Pilot who is trying to check to see if the jump point is in sensor range.
 *    @param target Jump point to see if is in sensor range.
 *    @return 1 if they are in range, 0 if they aren't.
 */
int pilot_inRangeJump( const Pilot *p, int i )
{
   double d;
   JumpPoint *jp;
   double sense;
   double hide;

   /* pilot must exist */
   if ( p == NULL )
      return 0;

   /* Get the jump point. */
   jp = &cur_system->jumps[i];

   /* We don't want exit-only or unknown hidden jumps. */
   if ((jp_isFlag(jp, JP_EXITONLY)) || ((jp_isFlag(jp, JP_HIDDEN)) && (!jp_isKnown(jp)) ))
      return 0;

   sense = sensor_curRange * p->ew_jumpDetect;
   hide = jp->hide;

   /* Get distance. */
   d = vect_dist2( &p->solid->pos, &jp->pos );

   if (d * hide < sense)
      return 1;

   return 0;
}

/**
 * @brief Calculates the weapon lead (1. is 100%, 0. is 0%)..
 *
 *    @param p Pilot tracking.
 *    @param t Pilot being tracked.
 *    @param track Track limit of the weapon.
 *    @return The lead angle of the weapon.
 */
double pilot_ewWeaponTrack( const Pilot *p, const Pilot *t, double track )
{
   double limit, lead;

   limit = track * p->ew_detect;
   if (t->ew_evasion < limit)
      lead = 1.;
   else
      lead = MAX( 0., 1. - 0.5*(t->ew_evasion/limit - 1.));
   return lead;
}



