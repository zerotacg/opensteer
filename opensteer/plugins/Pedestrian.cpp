// ----------------------------------------------------------------------------
//
//
// OpenSteer -- Steering Behaviors for Autonomous Characters
//
// Copyright (c) 2002-2003, Sony Computer Entertainment America
// Original author: Craig Reynolds <craig_reynolds@playstation.sony.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
//
// ----------------------------------------------------------------------------
//
//
// An autonomous "pedestrian":
// follows paths, avoids collisions with obstacles and other pedestrians
//
// 10-29-01 cwr: created
//
//
// ----------------------------------------------------------------------------


#include <iomanip>
#include <sstream>
#include "OpenSteer/Pathway.h"
#include "OpenSteer/SimpleVehicle.h"
#include "OpenSteer/SteerTest.h"
#include "OpenSteer/Proximity.h"


// ----------------------------------------------------------------------------


typedef AbstractProximityDatabase<AbstractVehicle*> ProximityDatabase;
typedef AbstractTokenForProximityDatabase<AbstractVehicle*> ProximityToken;


// ----------------------------------------------------------------------------


// creates a path for the PlugIn
PolylinePathway* getTestPath (void);
PolylinePathway* gTestPath = NULL;
SphericalObstacle gObstacle1;
SphericalObstacle gObstacle2;
ObstacleGroup gObstacles;
Vec3 gEndpoint0;
Vec3 gEndpoint1;
bool gUseDirectedPathFollowing = true;

// this was added for debugging tool, but I might as well leave it in
bool gWanderSwitch = true;


// ----------------------------------------------------------------------------


class Pedestrian : public SimpleVehicle
{
public:

    // type for a group of Pedestrians
    typedef std::vector<Pedestrian*> groupType;

    // constructor
    Pedestrian (ProximityDatabase& pd)
    {
        // allocate a token for this boid in the proximity database
        proximityToken = NULL;
        newPD (pd);

        // reset Pedestrian state
        reset ();
    }

    // destructor
    virtual ~Pedestrian ()
    {
        // delete this boid's token in the proximity database
        delete proximityToken;
    }

    // reset all instance state
    void reset (void)
    {
        // reset the vehicle 
        SimpleVehicle::reset ();

        // max speed and max steering force (maneuverability) 
        setMaxSpeed (2.0);
        setMaxForce (8.0);

        // initially stopped
        setSpeed (0);

        // size of bounding sphere, for obstacle avoidance, etc.
        setRadius (0.5); // width = 0.7, add 0.3 margin, take half

        // set the path for this Pedestrian to follow
        path = getTestPath ();

        // set initial position
        // (random point on path + random horizontal offset)
        const float d = path->getTotalPathLength() * frandom01();
        const float r = path->radius;
        const Vec3 randomOffset = randomVectorOnUnitRadiusXZDisk () * r;
        setPosition (path->mapPathDistanceToPoint (d) + randomOffset);

        // randomize 2D heading
        randomizeHeadingOnXZPlane ();

        // pick a random direction for path following (upstream or downstream)
        pathDirection = (frandom01() > 0.5) ? -1 : +1;

        // trail parameters: 3 seconds with 60 points along the trail
        setTrailParameters (3, 60);

        // notify proximity database that our position has changed
        proximityToken->updateForNewPosition (position());
    }

    // per frame simulation update
    void update (const float currentTime, const float elapsedTime)
    {
        // apply steering force to our momentum
        applySteeringForce (determineCombinedSteering (elapsedTime),
                            elapsedTime);

        // reverse direction when we reach an endpoint
        if (gUseDirectedPathFollowing)
        {
            const Vec3 darkRed (0.7f, 0, 0);

            if (Vec3::distance (position(), gEndpoint0) < path->radius)
            {
                pathDirection = +1;
                annotationXZCircle (path->radius, gEndpoint0, darkRed, 20);
            }
            if (Vec3::distance (position(), gEndpoint1) < path->radius)
            {
                pathDirection = -1;
                annotationXZCircle (path->radius, gEndpoint1, darkRed, 20);
            }
        }

        // annotation
        annotationVelocityAcceleration (5, 0);
        recordTrailVertex (currentTime, position());

        // notify proximity database that our position has changed
        proximityToken->updateForNewPosition (position());
    }

    // compute combined steering force: move forward, avoid obstacles
    // or neighbors if needed, otherwise follow the path and wander
    Vec3 determineCombinedSteering (const float elapsedTime)
    {
        // move forward
        Vec3 steeringForce = forward();

        // probability that a lower priority behavior will be given a
        // chance to "drive" even if a higher priority behavior might
        // otherwise be triggered.
        const float leakThrough = 0.1f;

        // determine if obstacle avoidance is required
        Vec3 obstacleAvoidance;
        if (leakThrough < frandom01())
        {
            const float oTime = 6; // minTimeToCollision = 6 seconds
            obstacleAvoidance = steerToAvoidObstacles (oTime, gObstacles);
        }

        // if obstacle avoidance is needed, do it
        if (obstacleAvoidance != Vec3::zero)
        {
            steeringForce += obstacleAvoidance;
        }
        else
        {
            // otherwise consider avoiding collisions with others
            Vec3 collisionAvoidance;
            const float caLeadTime = 3;

            // find all neighbors within maxRadius using proximity database
            // (radius is largest distance between vehicles traveling head-on
            // where a collision is possible within caLeadTime seconds.)
            const float maxRadius = caLeadTime * maxSpeed() * 2;
            neighbors.clear();
            proximityToken->findNeighbors (position(), maxRadius, neighbors);

            if (leakThrough < frandom01())
                collisionAvoidance =
                    steerToAvoidNeighbors (caLeadTime, neighbors) * 10;

            // if collision avoidance is needed, do it
            if (collisionAvoidance != Vec3::zero)
            {
                steeringForce += collisionAvoidance;
            }
            else
            {
                // add in wander component (according to user switch)
                if (gWanderSwitch)
                    steeringForce += steerForWander (elapsedTime);

                // do (interactively) selected type of path following
                const float pfLeadTime = 3;
                const Vec3 pathFollow =
                    (gUseDirectedPathFollowing ?
                     steerToFollowPath (pathDirection, pfLeadTime, *path) :
                     steerToStayOnPath (pfLeadTime, *path));

                // add in to steeringForce
                steeringForce += pathFollow * 0.5;
            }
        }

        // return steering constrained to global XZ "ground" plane
        return steeringForce.setYtoZero ();
    }


    // draw this pedestrian into scene
    void draw (void)
    {
        drawBasic2dCircularVehicle (*this, gGray50);
        drawTrail ();
    }


    // called when steerToFollowPath decides steering is required
    void annotatePathFollowing (const Vec3& future,
                                const Vec3& onPath,
                                const Vec3& target,
                                const float outside)
    {
        const Vec3 yellow (1, 1, 0);
        const Vec3 lightOrange (1.0f, 0.5f, 0.0f);
        const Vec3 darkOrange  (0.6f, 0.3f, 0.0f);
        const Vec3 yellowOrange (1.0f, 0.75f, 0.0f);

        // draw line from our position to our predicted future position
        annotationLine (position(), future, yellow);

        // draw line from our position to our steering target on the path
        annotationLine (position(), target, yellowOrange);

        // draw a two-toned line between the future test point and its
        // projection onto the path, the change from dark to light color
        // indicates the boundary of the tube.
        const Vec3 boundaryOffset = (onPath - future).normalize() * outside;
        const Vec3 onPathBoundary = future + boundaryOffset;
        annotationLine (onPath, onPathBoundary, darkOrange);
        annotationLine (onPathBoundary, future, lightOrange);
    }

    // called when steerToAvoidCloseNeighbors decides steering is required
    // (parameter names commented out to prevent compiler warning from "-W")
    void annotateAvoidCloseNeighbor (const AbstractVehicle& other,
                                     const float /*additionalDistance*/)
    {
        // draw the word "Ouch!" above colliding vehicles
        const float headOn = forward().dot(other.forward()) < 0;
        const Vec3 green (0.4f, 0.8f, 0.1f);
        const Vec3 red (1, 0.1f, 0);
        const Vec3 color = headOn ? red : green;
        const char* string = headOn ? "OUCH!" : "pardon me";
        const Vec3 location = position() + Vec3 (0, 0.5f, 0);
        if (SteerTest::annotationIsOn())
            draw2dTextAt3dLocation (*string, location, color);
    }


    // (parameter names commented out to prevent compiler warning from "-W")
    void annotateAvoidNeighbor (const AbstractVehicle& threat,
                                const float /*steer*/,
                                const Vec3& ourFuture,
                                const Vec3& threatFuture)
    {
        const Vec3 green (0.15f, 0.6f, 0.0f);

        annotationLine (position(), ourFuture, green);
        annotationLine (threat.position(), threatFuture, green);
        annotationLine (ourFuture, threatFuture, gRed);
        annotationXZCircle (radius(), ourFuture,    green, 12);
        annotationXZCircle (radius(), threatFuture, green, 12);
    }

    // xxx perhaps this should be a call to a general purpose annotation for
    // xxx "local xxx axis aligned box in XZ plane" -- same code in in
    // xxx CaptureTheFlag.cpp
    void annotateAvoidObstacle (const float minDistanceToCollision)
    {
        const Vec3 boxSide = side() * radius();
        const Vec3 boxFront = forward() * minDistanceToCollision;
        const Vec3 FR = position() + boxFront - boxSide;
        const Vec3 FL = position() + boxFront + boxSide;
        const Vec3 BR = position()            - boxSide;
        const Vec3 BL = position()            + boxSide;
        const Vec3 white (1,1,1);
        annotationLine (FR, FL, white);
        annotationLine (FL, BL, white);
        annotationLine (BL, BR, white);
        annotationLine (BR, FR, white);
    }

    // switch to new proximity database -- just for demo purposes
    void newPD (ProximityDatabase& pd)
    {
        // delete this boid's token in the old proximity database
        delete proximityToken;

        // allocate a token for this boid in the proximity database
        proximityToken = pd.allocateToken (this);
    }

    // a pointer to this boid's interface object for the proximity database
    ProximityToken* proximityToken;

    // allocate one and share amoung instances just to save memory usage
    // (change to per-instance allocation to be more MP-safe)
    static AVGroup neighbors;

    // path to be followed by this pedestrian
    // XXX Ideally this should be a generic Pathway, but we use the
    // XXX getTotalPathLength and radius methods (currently defined only
    // XXX on PolylinePathway) to set random initial positions.  Could
    // XXX there be a "random position inside path" method on Pathway?
    PolylinePathway* path;

    // direction for path following (upstream or downstream)
    int pathDirection;
};


AVGroup Pedestrian::neighbors;


// ----------------------------------------------------------------------------
// create path for PlugIn 
//
//
//        | gap |
//
//        f      b
//        |\    /\        -
//        | \  /  \       ^
//        |  \/    \      |
//        |  /\     \     |
//        | /  \     c   top
//        |/    \g  /     |
//        /        /      |
//       /|       /       V      z     y=0
//      / |______/        -      ^
//     /  e      d               |
//   a/                          |
//    |<---out-->|               o----> x
//


PolylinePathway* getTestPath (void)
{
    if (gTestPath == NULL)
    {
        const float pathRadius = 2;

        const int pathPointCount = 7;
        const float size = 30;
        const float top = 2 * size;
        const float gap = 1.2f * size;
        const float out = 2 * size;
        const float h = 0.5;
        const Vec3 pathPoints[pathPointCount] =
            {Vec3 (h+gap-out,     0,  h+top-out),  // 0 a
             Vec3 (h+gap,         0,  h+top),      // 1 b
             Vec3 (h+gap+(top/2), 0,  h+top/2),    // 2 c
             Vec3 (h+gap,         0,  h),          // 3 d
             Vec3 (h,             0,  h),          // 4 e
             Vec3 (h,             0,  h+top),      // 5 f
             Vec3 (h+gap,         0,  h+top/2)};   // 6 g

        gObstacle1.center = interpolate (0.2f, pathPoints[0], pathPoints[1]);
        gObstacle2.center = interpolate (0.5f, pathPoints[2], pathPoints[3]);
        gObstacle1.radius = 3;
        gObstacle2.radius = 5;
        gObstacles.push_back (&gObstacle1);
        gObstacles.push_back (&gObstacle2);

        gEndpoint0 = pathPoints[0];
        gEndpoint1 = pathPoints[pathPointCount-1];

        gTestPath = new PolylinePathway (pathPointCount,
                                         pathPoints,
                                         pathRadius,
                                         false);
    }
    return gTestPath;
}


// ----------------------------------------------------------------------------
// SteerTest PlugIn


class PedestrianPlugIn : public PlugIn
{
public:

    const char* name (void) {return "Pedestrians";}

    float selectionOrderSortKey (void) {return 0.02f;}

    virtual ~PedestrianPlugIn() {}// be more "nice" to avoid a compiler warning

    void open (void)
    {
        // make the database used to accelerate proximity queries
        cyclePD = -1;
        nextPD ();

        // create the specified number of Pedestrians
        population = 0;
        for (int i = 0; i < 100; i++) addPedestrianToCrowd ();

        // initialize camera and selectedVehicle
        Pedestrian& firstPedestrian = **crowd.begin();
        SteerTest::init3dCamera (firstPedestrian);
        SteerTest::camera.mode = Camera::cmFixedDistanceOffset;
        SteerTest::camera.fixedTarget.set (15, 0, 30);
        SteerTest::camera.fixedPosition.set (15, 70, -70);
    }

    void update (const float currentTime, const float elapsedTime)
    {
        // update each Pedestrian
        for (iterator i = crowd.begin(); i != crowd.end(); i++)
        {
            (**i).update (currentTime, elapsedTime);
        }
    }

    void redraw (const float currentTime, const float elapsedTime)
    {
        // selected Pedestrian (user can mouse click to select another)
        AbstractVehicle& selected = *SteerTest::selectedVehicle;

        // Pedestrian nearest mouse (to be highlighted)
        AbstractVehicle& nearMouse = *SteerTest::vehicleNearestToMouse ();

        // update camera
        SteerTest::updateCamera (currentTime, elapsedTime, selected);

        // draw "ground plane"
//         SteerTest::gridUtility (selected.position());
//         SteerTest::gridUtility (SteerTest::selectedVehicle ?
//                                 selected.position() :
//                                 Vec3::zero);
        if (SteerTest::selectedVehicle) gridCenter = selected.position();
        SteerTest::gridUtility (gridCenter);

        // draw and annotate each Pedestrian
        for (iterator i = crowd.begin(); i != crowd.end(); i++) (**i).draw (); 

        // draw the path they follow and obstacles they avoid
        drawPathAndObstacles ();

        // highlight Pedestrian nearest mouse
        SteerTest::highlightVehicleUtility (nearMouse);

        // textual annotation (at the vehicle's screen position)
        serialNumberAnnotationUtility (selected, nearMouse);

        // textual annotation for selected Pedestrian
        if (SteerTest::selectedVehicle && SteerTest::annotationIsOn())
        {
            const Vec3 color (0.8f, 0.8f, 1.0f);
            const Vec3 textOffset (0, 0.25f, 0);
            const Vec3 textPosition = selected.position() + textOffset;
            const Vec3 camPosition = SteerTest::camera.position();
            const float camDistance = Vec3::distance (selected.position(),
                                                      camPosition);
            const char* spacer = "      ";
            std::ostringstream annote;
            annote << std::setprecision (2);
            annote << std::setiosflags (std::ios::fixed);
            annote << spacer << "1: speed: " << selected.speed() << std::endl;
            annote << std::setprecision (1);
            annote << spacer << "2: cam dist: " << camDistance << std::endl;
            annote << spacer << "3: no third thing" << std::ends;
            draw2dTextAt3dLocation (annote, textPosition, color);
        }

        // display status in the upper left corner of the window
        std::ostringstream status;
        status << "[F1/F2] Crowd size: " << population;
        status << "\n[F3] PD type: ";
        switch (cyclePD)
        {
        case 0: status << "LQ bin lattice"; break;
        case 1: status << "brute force";    break;
        }
        status << "\n[F4] ";
        if (gUseDirectedPathFollowing)
            status << "Directed path following.";
        else
            status << "Stay on the path.";
        status << "\n[F5] Wander: ";
        if (gWanderSwitch) status << "yes"; else status << "no";
        status << std::endl;
        const float h = drawGetWindowHeight ();
        const Vec3 screenLocation (10, h-50, 0);
        draw2dTextAt2dLocation (status, screenLocation, gGray80);
    }


    void serialNumberAnnotationUtility (const AbstractVehicle& selected,
                                        const AbstractVehicle& nearMouse)
    {
        // display a Pedestrian's serial number as a text label near its
        // screen position when it is near the selected vehicle or mouse.
        if (&selected && &nearMouse && SteerTest::annotationIsOn())
        {
            for (iterator i = crowd.begin(); i != crowd.end(); i++)
            {
                AbstractVehicle* vehicle = *i;
                const float nearDistance = 6;
                const Vec3& vp = vehicle->position();
                const Vec3& np = nearMouse.position();
                if ((Vec3::distance (vp, selected.position()) < nearDistance)
                    ||
                    (&nearMouse && (Vec3::distance (vp, np) < nearDistance)))
                {
                    std::ostringstream sn;
                    sn << "#"
                       << ((Pedestrian*)vehicle)->serialNumber
                       << std::ends;
                    const Vec3 textColor (0.8f, 1, 0.8f);
                    const Vec3 textOffset (0, 0.25f, 0);
                    const Vec3 textPos = vehicle->position() + textOffset;
                    draw2dTextAt3dLocation (sn, textPos, textColor);
                }
            }
        }
    }

    void drawPathAndObstacles (void)
    {
        // draw a line along each segment of path
        const PolylinePathway& path = *getTestPath ();
        for (int i = 0; i < path.pointCount; i++)
            if (i > 0) drawLine (path.points[i], path.points[i-1], gRed);

        // draw obstacles
        drawXZCircle (gObstacle1.radius, gObstacle1.center, gWhite, 40);
        drawXZCircle (gObstacle2.radius, gObstacle2.center, gWhite, 40);
    }

    void close (void)
    {
        // delete all Pedestrians
       while (population > 0) removePedestrianFromCrowd ();
    }

    void reset (void)
    {
        // reset each Pedestrian
        for (iterator i = crowd.begin(); i != crowd.end(); i++) (**i).reset ();

        // reset camera position
        SteerTest::position2dCamera (*SteerTest::selectedVehicle);

        // make camera jump immediately to new position
        SteerTest::camera.doNotSmoothNextMove ();
    }

    void handleFunctionKeys (int keyNumber)
    {
        switch (keyNumber)
        {
        case 1:  addPedestrianToCrowd ();                               break;
        case 2:  removePedestrianFromCrowd ();                          break;
        case 3:  nextPD ();                                             break;
        case 4: gUseDirectedPathFollowing = !gUseDirectedPathFollowing; break;
        case 5: gWanderSwitch = !gWanderSwitch;                         break;
        }
    }

    void printMiniHelpForFunctionKeys (void)
    {
        std::ostringstream message;
        message << "Function keys handled by ";
        message << '"' << name() << '"' << ':' << std::ends;
        SteerTest::printMessage (message);
        SteerTest::printMessage (message);
        SteerTest::printMessage ("  F1     add a pedestrian to the crowd.");
        SteerTest::printMessage ("  F2     remove a pedestrian from crowd.");
        SteerTest::printMessage ("  F3     use next proximity database.");
        SteerTest::printMessage ("  F4     toggle directed path follow.");
        SteerTest::printMessage ("  F5     toggle wander component on/off.");
        SteerTest::printMessage ("");
    }


    void addPedestrianToCrowd (void)
    {
        population++;
        Pedestrian* pedestrian = new Pedestrian (*pd);
        crowd.push_back (pedestrian);
        if (population == 1) SteerTest::selectedVehicle = pedestrian;
    }


    void removePedestrianFromCrowd (void)
    {
        if (population > 0)
        {
            // save pointer to last pedestrian, then remove it from the crowd
            const Pedestrian* pedestrian = crowd.back();
            crowd.pop_back();
            population--;

            // if it is SteerTest's selected vehicle, unselect it
            if (pedestrian == SteerTest::selectedVehicle)
                SteerTest::selectedVehicle = NULL;

            // delete the Pedestrian
            delete pedestrian;
        }
    }


    // for purposes of demonstration, allow cycling through various
    // types of proximity databases.  this routine is called when the
    // SteerTest user pushes a function key.
    void nextPD (void)
    {
        // save pointer to old PD
        ProximityDatabase* oldPD = pd;

        // allocate new PD
        const int totalPD = 2;
        switch (cyclePD = (cyclePD + 1) % totalPD)
        {
        case 0:
            {
                const Vec3 center;
                const float div = 20.0f;
                const Vec3 divisions (div, 1.0f, div);
                const float diameter = 80.0f; //XXX need better way to get this
                const Vec3 dimensions (diameter, diameter, diameter);
                typedef LQProximityDatabase<AbstractVehicle*> LQPDAV;
                pd = new LQPDAV (center, dimensions, divisions);
                break;
            }
        case 1:
            {
                pd = new BruteForceProximityDatabase<AbstractVehicle*> ();
                break;
            }
        }

        // switch each boid to new PD
        for (iterator i=crowd.begin(); i!=crowd.end(); i++) (**i).newPD(*pd);

        // delete old PD (if any)
        delete oldPD;
    }


    const AVGroup& allVehicles (void) {return (const AVGroup&) crowd;}

    // crowd: a group (STL vector) of all Pedestrians
    Pedestrian::groupType crowd;
    typedef Pedestrian::groupType::const_iterator iterator;

    Vec3 gridCenter;

    // pointer to database used to accelerate proximity queries
    ProximityDatabase* pd;

    // keep track of current flock size
    int population;

    // which of the various proximity databases is currently in use
    int cyclePD;
};


PedestrianPlugIn gPedestrianPlugIn;


// ----------------------------------------------------------------------------
