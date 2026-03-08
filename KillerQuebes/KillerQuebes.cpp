// StudentName StudentID
// KillerQuebes — A marble-and-block bowling game using TL-Engine TLX

#include <TL-Engine.h>
#include <cmath>

using namespace tle;

//=============================================================================
// FORWARD DECLARATION — SphereBoxCollision
// Checks if a sphere overlaps an axis-aligned box
// hitXFace: sphere hit the left or right face (reverse VX)
// hitZFace: sphere hit the front or back face (reverse VZ)
//=============================================================================
bool SphereBoxCollision(float sx, float sz, float sr,
                        float bx, float bz, float bHalfW, float bHalfD,
                        bool& hitXFace, bool& hitZFace);

//=============================================================================
// NAMED CONSTANTS — every numeric value used in gameplay is defined here
// No magic numbers are used anywhere else in the code
//=============================================================================
const float kSpeed            = 0.1f;         // Base speed multiplier for marble movement
const float kMarbleRadius     = 2.0f;         // Radius of the marble for collision
const float kBlockHalfWidth   = 5.0f;         // Half-width of each block on X axis
const float kBlockHalfDepth   = 5.0f;         // Half-depth of each block on Z axis
const float kBarrierHalfWidth = 5.0f;         // Half-width of each barrier on X axis
const float kBarrierHalfDepth = 2.5f;         // Half-depth of each barrier on Z axis
const float kArrowSwingLimit  = 40.0f;        // Maximum degrees the arrow can rotate
const float kArrowOffsetZ     = -18.0f;       // Arrow offset behind the dummy on Z
const float kArrowRotSpeed    = 25.0f;        // Degrees per second for arrow rotation
const int   kNumBlockCols     = 9;            // Number of block columns
const int   kNumBlockRows     = 2;            // Number of block rows
const float kRow1Z            = 130.0f;       // Z position for the first row of blocks
const float kRow2Z            = 142.0f;       // Z position for the second row of blocks
const float kBlockSpacing     = 12.0f;        // Horizontal spacing between block centres
const int   kNumBarriers      = 10;           // Number of barriers per side
const float kBarrierSpacing   = 18.0f;        // Z spacing between consecutive barriers
const float kBarrierStartZ    = 10.0f;        // Z position of the first barrier
const float kBarrierLeftX     = -60.0f;       // X position of left barrier column
const float kBarrierRightX    =  60.0f;       // X position of right barrier column
const float kMarbleStartX     = 0.0f;         // Marble starting X position
const float kMarbleStartY     = 2.0f;         // Marble starting Y position (above floor)
const float kMarbleStartZ     = 0.0f;         // Marble starting Z position
const float kSkyboxY          = -850.0f;      // Y position for the skybox model
const float kCameraX          = 0.0f;         // Camera X position
const float kCameraY          = 30.0f;        // Camera Y position
const float kCameraZ          = -70.0f;       // Camera Z position
const float kCameraXRotation  = 12.0f;        // Camera downward tilt in degrees
const float kBehindBlocksZ    = 155.0f;       // Z threshold behind the blocks for reset
const float kPI               = 3.14159265f;  // Pi constant for trigonometric calculations
const int   kWaspStripeStart  = 6;            // Barrier index where wasp stripes begin
const float kDeadPosition     = 9999.0f;      // Off-screen position for dead blocks
const int   kBlockCentreCol   = 4;            // Centre column index for block X calculation
const float kDummyScale       = 0.01f;        // Near-zero scale for the dummy model
const float kZeroVelocity     = 0.0f;         // Zero velocity constant for resets
const float kFloorX           = 0.0f;         // Floor X position
const float kFloorY           = 0.0f;         // Floor Y position
const float kFloorZ           = 0.0f;         // Floor Z position
const float kSkyboxX          = 0.0f;         // Skybox X position
const float kDegreesToRadians = kPI / 180.0f; // Conversion factor from degrees to radians

//=============================================================================
// ENUMS — game states and block states drive the state machine
//=============================================================================
enum GameState  { Ready, Firing, Contact, Over };
enum BlockState { Alive, Hit, Dead };

//=============================================================================
// MAIN FUNCTION — entry point for the KillerQuebes game
//=============================================================================
void main()
{
    //=========================================================================
    // ENGINE INITIALISATION — create the 3D engine in windowed mode
    //=========================================================================
    I3DEngine* myEngine = New3DEngine(kTLX);
    myEngine->StartWindowed();

    //=========================================================================
    // MESH LOADING — load all mesh files used in the scene
    //=========================================================================
    IMesh* floorMesh   = myEngine->LoadMesh("floor.x");
    IMesh* skyboxMesh  = myEngine->LoadMesh("skybox.x");
    IMesh* cubeMesh    = myEngine->LoadMesh("cube.x");
    IMesh* marbleMesh  = myEngine->LoadMesh("marble.x");
    IMesh* arrowMesh   = myEngine->LoadMesh("arrow.x");
    IMesh* barrierMesh = myEngine->LoadMesh("Barrier.x");

    //=========================================================================
    // FLOOR AND SKYBOX — create the ground plane and sky environment
    //=========================================================================
    IModel* floor  = floorMesh->CreateModel(kFloorX, kFloorY, kFloorZ);
    IModel* skybox = skyboxMesh->CreateModel(kSkyboxX, kSkyboxY, kFloorZ);

    //=========================================================================
    // CAMERA — positioned above and behind the marble, tilted downward
    //=========================================================================
    ICamera* cam = myEngine->CreateCamera(kManual, kCameraX, kCameraY, kCameraZ);
    cam->RotateLocalX(kCameraXRotation);

    //=========================================================================
    // BLOCK ARRAY — 2 rows x 9 columns of destructible blocks
    // Blocks are centred on X=0 with kBlockSpacing between each column
    // Row 0 at Z=kRow1Z, Row 1 at Z=kRow2Z
    //=========================================================================
    IModel*    blocks[kNumBlockRows][kNumBlockCols];
    BlockState blockState[kNumBlockRows][kNumBlockCols];

    // Z positions for each row stored in an array for clean loop access
    float rowZPositions[kNumBlockRows] = { kRow1Z, kRow2Z };

    for (int row = 0; row < kNumBlockRows; row++)
    {
        for (int col = 0; col < kNumBlockCols; col++)
        {
            // Calculate X position: centre column (index 4) is at X=0
            float blockX = (col - kBlockCentreCol) * kBlockSpacing;
            float blockZ = rowZPositions[row];

            blocks[row][col] = cubeMesh->CreateModel(blockX, kFloorY, blockZ);
            blockState[row][col] = Alive;
        }
    }

    //=========================================================================
    // MARBLE — the player-controlled projectile
    //=========================================================================
    IModel* marble = marbleMesh->CreateModel(kMarbleStartX, kMarbleStartY, kMarbleStartZ);

    //=========================================================================
    // DUMMY AND ARROW — the dummy sits at marble start and the arrow attaches
    // to it. Rotating the dummy swings the arrow to aim the shot direction.
    // The dummy is scaled to near-zero so it is invisible in the scene.
    //=========================================================================
    IModel* dummy = cubeMesh->CreateModel(kMarbleStartX, kMarbleStartY, kMarbleStartZ);
    dummy->Scale(kDummyScale);

    IModel* arrow = arrowMesh->CreateModel();
    arrow->AttachToParent(dummy);
    arrow->MoveLocalZ(kArrowOffsetZ);

    // Track the current rotation of the dummy as an explicit variable
    float dummyRotation = kZeroVelocity;

    //=========================================================================
    // BARRIERS — two columns of crash barriers lining the play field
    // Left column at X=kBarrierLeftX, right column at X=kBarrierRightX
    // Barriers from index kWaspStripeStart onward receive wasp stripe skin
    //=========================================================================
    IModel* leftBarriers[kNumBarriers];
    IModel* rightBarriers[kNumBarriers];

    for (int i = 0; i < kNumBarriers; i++)
    {
        float barrierZ = kBarrierStartZ + i * kBarrierSpacing;

        leftBarriers[i]  = barrierMesh->CreateModel(kBarrierLeftX, kFloorY, barrierZ);
        rightBarriers[i] = barrierMesh->CreateModel(kBarrierRightX, kFloorY, barrierZ);

        // Apply wasp stripe skin to barriers from kWaspStripeStart onward
        if (i >= kWaspStripeStart)
        {
            leftBarriers[i]->SetSkin("barrier_stripe.png");
            rightBarriers[i]->SetSkin("barrier_stripe.png");
        }
    }

    //=========================================================================
    // GAME STATE VARIABLES — initialise the state machine and velocity
    //=========================================================================
    GameState gameState = Ready;
    float marbleVX = kZeroVelocity;
    float marbleVZ = kZeroVelocity;

    //=========================================================================
    // MAIN GAME LOOP — runs every frame until the engine stops
    //=========================================================================
    while (myEngine->IsRunning())
    {
        // Get the time elapsed since the last frame for smooth movement
        float frameTime = myEngine->Timer();

        //=====================================================================
        // ESCAPE KEY — available in all states, immediately stops the engine
        //=====================================================================
        if (myEngine->KeyHit(Key_Escape))
        {
            myEngine->Stop();
        }

        //=====================================================================
        // ARROW ROTATION — Z and X keys swing the aiming arrow left and right
        // This control is available in both Ready and Firing states
        //=====================================================================
        if (gameState == Ready || gameState == Firing)
        {
            // Z key rotates the arrow to the left (negative Y rotation)
            if (myEngine->KeyHeld(Key_Z))
            {
                dummyRotation -= kArrowRotSpeed * frameTime;

                // Clamp rotation so it never goes below the negative limit
                if (dummyRotation < -kArrowSwingLimit)
                {
                    dummyRotation = -kArrowSwingLimit;
                }
            }

            // X key rotates the arrow to the right (positive Y rotation)
            if (myEngine->KeyHeld(Key_X))
            {
                dummyRotation += kArrowRotSpeed * frameTime;

                // Clamp rotation so it never goes above the positive limit
                if (dummyRotation > kArrowSwingLimit)
                {
                    dummyRotation = kArrowSwingLimit;
                }
            }

            // Apply the tracked rotation to the dummy model every frame
            dummy->SetRotationY(dummyRotation);
        }

        //=====================================================================
        // STATE MACHINE — handle game logic based on the current state
        //=====================================================================

        //---------------------------------------------------------------------
        // READY STATE — marble is stationary, player aims with arrow
        // Pressing Space fires the marble in the direction the arrow points
        //---------------------------------------------------------------------
        if (gameState == Ready)
        {
            if (myEngine->KeyHit(Key_Space))
            {
                // Calculate velocity components from the aiming angle
                // cosf gives the Z component, sinf gives the X component
                marbleVZ = cosf(dummyRotation * kDegreesToRadians) * kSpeed;
                marbleVX = sinf(dummyRotation * kDegreesToRadians) * kSpeed;

                // Detach the arrow from the dummy so it stays behind
                arrow->DetachFromParent();

                // Transition to Firing state
                gameState = Firing;
            }
        }

        //---------------------------------------------------------------------
        // FIRING STATE — marble moves along its velocity vector
        // Collisions with blocks change block states and reverse velocity
        // Collisions with barriers reverse the appropriate velocity component
        // If the marble goes behind blocks or back past start, reset it
        //---------------------------------------------------------------------
        else if (gameState == Firing)
        {
            // Move the marble using velocity multiplied by frameTime
            marble->Move(marbleVX * frameTime, kZeroVelocity, marbleVZ * frameTime);

            // Get current marble position for collision checks
            float marblePosX = marble->GetX();
            float marblePosZ = marble->GetZ();

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // BLOCK COLLISION — loop through all blocks in the 2D array
            // Skip dead blocks. On collision, change block state and reverse
            // the appropriate velocity component based on which face was hit.
            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            for (int row = 0; row < kNumBlockRows; row++)
            {
                for (int col = 0; col < kNumBlockCols; col++)
                {
                    // Skip any block that has already been destroyed
                    if (blockState[row][col] == Dead)
                    {
                        continue;
                    }

                    // Collision detection variables
                    bool hitXFace = false;
                    bool hitZFace = false;

                    float blockPosX = blocks[row][col]->GetX();
                    float blockPosZ = blocks[row][col]->GetZ();

                    // Check for sphere-box collision between marble and block
                    if (SphereBoxCollision(marblePosX, marblePosZ, kMarbleRadius,
                                          blockPosX, blockPosZ,
                                          kBlockHalfWidth, kBlockHalfDepth,
                                          hitXFace, hitZFace))
                    {
                        if (blockState[row][col] == Alive)
                        {
                            // First hit: change to red skin and mark as Hit
                            blocks[row][col]->SetSkin("block_red.png");
                            blockState[row][col] = Hit;
                        }
                        else if (blockState[row][col] == Hit)
                        {
                            // Second hit: move block off-screen and mark as Dead
                            blocks[row][col]->SetPosition(kDeadPosition,
                                                          kDeadPosition,
                                                          kDeadPosition);
                            blockState[row][col] = Dead;
                        }

                        // Reverse velocity based on which face was hit
                        if (hitXFace)
                        {
                            marbleVX = -marbleVX;
                        }
                        if (hitZFace)
                        {
                            marbleVZ = -marbleVZ;
                        }

                        // Transition to Contact state for dead-block check
                        gameState = Contact;
                    }
                }
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // BARRIER COLLISION — loop through left and right barrier arrays
            // On collision, reverse the appropriate velocity component
            // Barriers do not change state or appearance
            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            for (int i = 0; i < kNumBarriers; i++)
            {
                bool hitXFace = false;
                bool hitZFace = false;

                // Check left barrier collision
                if (SphereBoxCollision(marblePosX, marblePosZ, kMarbleRadius,
                                      leftBarriers[i]->GetX(),
                                      leftBarriers[i]->GetZ(),
                                      kBarrierHalfWidth, kBarrierHalfDepth,
                                      hitXFace, hitZFace))
                {
                    if (hitXFace)
                    {
                        marbleVX = -marbleVX;
                    }
                    if (hitZFace)
                    {
                        marbleVZ = -marbleVZ;
                    }
                }

                // Reset flags for right barrier check
                hitXFace = false;
                hitZFace = false;

                // Check right barrier collision
                if (SphereBoxCollision(marblePosX, marblePosZ, kMarbleRadius,
                                      rightBarriers[i]->GetX(),
                                      rightBarriers[i]->GetZ(),
                                      kBarrierHalfWidth, kBarrierHalfDepth,
                                      hitXFace, hitZFace))
                {
                    if (hitXFace)
                    {
                        marbleVX = -marbleVX;
                    }
                    if (hitZFace)
                    {
                        marbleVZ = -marbleVZ;
                    }
                }
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            // BOUNDARY CHECK — if marble goes behind blocks or past start line
            // Reset marble to starting position and return to Ready state
            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            if (marble->GetZ() < kMarbleStartZ)
            {
                marble->SetPosition(kMarbleStartX, kMarbleStartY, kMarbleStartZ);
                marbleVX = kZeroVelocity;
                marbleVZ = kZeroVelocity;
                gameState = Ready;
            }

            if (marble->GetZ() > kBehindBlocksZ)
            {
                marble->SetPosition(kMarbleStartX, kMarbleStartY, kMarbleStartZ);
                marbleVX = kZeroVelocity;
                marbleVZ = kZeroVelocity;
                gameState = Ready;
            }
        }

        //---------------------------------------------------------------------
        // CONTACT STATE — resolves immediately in the same frame
        // Counts how many blocks are dead. If all are dead, game is won.
        // Otherwise, return to Firing state to continue play.
        //---------------------------------------------------------------------
        else if (gameState == Contact)
        {
            // Count the number of dead blocks across all rows and columns
            int deadBlockCount = 0;

            for (int row = 0; row < kNumBlockRows; row++)
            {
                for (int col = 0; col < kNumBlockCols; col++)
                {
                    if (blockState[row][col] == Dead)
                    {
                        deadBlockCount++;
                    }
                }
            }

            // Total number of blocks in the grid
            int totalBlocks = kNumBlockRows * kNumBlockCols;

            // Check if all blocks have been destroyed
            if (deadBlockCount == totalBlocks)
            {
                // Victory: change marble to green skin and enter Over state
                marble->SetSkin("marble_green.png");
                gameState = Over;
            }
            else
            {
                // Blocks remain: return to Firing state to continue play
                gameState = Firing;
            }
        }

        //---------------------------------------------------------------------
        // OVER STATE — game is complete, marble is green
        // No gameplay actions occur. Only Escape key works (handled above).
        //---------------------------------------------------------------------
        // Over state: do nothing, the Escape key check above handles exit

        //=====================================================================
        // DRAW SCENE — render the current frame to the screen
        //=====================================================================
        myEngine->DrawScene();
    }

    //=========================================================================
    // ENGINE CLEANUP — release engine resources before exiting
    //=========================================================================
    myEngine->Delete();
}

//=============================================================================
// SPHERE-BOX COLLISION FUNCTION — full definition placed after main()
//
// Determines if a sphere (centre sx,sz with radius sr) overlaps an
// axis-aligned box (centre bx,bz with half-extents bHalfW and bHalfD).
//
// Sets hitXFace to true if the sphere hits the left or right face of the box
// (indicating marbleVX should be reversed).
// Sets hitZFace to true if the sphere hits the front or back face of the box
// (indicating marbleVZ should be reversed).
// If the sphere hits a corner, both flags are set to true.
//
// Returns true if any collision occurred, false otherwise.
//=============================================================================
bool SphereBoxCollision(float sx, float sz, float sr,
                        float bx, float bz, float bHalfW, float bHalfD,
                        bool& hitXFace, bool& hitZFace)
{
    // Initialise output flags to no collision on either face
    hitXFace = false;
    hitZFace = false;

    // Find the closest point on the box to the sphere centre
    // Clamp the sphere centre to the box boundaries on each axis
    float closestX = sx;
    float closestZ = sz;

    // Clamp X to the box boundaries
    float boxMinX = bx - bHalfW;
    float boxMaxX = bx + bHalfW;

    if (closestX < boxMinX)
    {
        closestX = boxMinX;
    }
    else if (closestX > boxMaxX)
    {
        closestX = boxMaxX;
    }

    // Clamp Z to the box boundaries
    float boxMinZ = bz - bHalfD;
    float boxMaxZ = bz + bHalfD;

    if (closestZ < boxMinZ)
    {
        closestZ = boxMinZ;
    }
    else if (closestZ > boxMaxZ)
    {
        closestZ = boxMaxZ;
    }

    // Calculate the squared distance from the sphere centre to the closest point
    float distanceX = sx - closestX;
    float distanceZ = sz - closestZ;
    float distanceSquared = (distanceX * distanceX) + (distanceZ * distanceZ);

    // Compare squared distance against squared radius to avoid sqrt
    float radiusSquared = sr * sr;

    if (distanceSquared < radiusSquared)
    {
        // Collision detected — determine which face(s) were hit
        // If the sphere centre is outside the box on the X axis, it hit an X face
        bool outsideX = (sx < boxMinX || sx > boxMaxX);

        // If the sphere centre is outside the box on the Z axis, it hit a Z face
        bool outsideZ = (sz < boxMinZ || sz > boxMaxZ);

        if (outsideX && outsideZ)
        {
            // Corner hit: both faces are involved
            hitXFace = true;
            hitZFace = true;
        }
        else if (outsideX)
        {
            // Hit left or right face only
            hitXFace = true;
        }
        else if (outsideZ)
        {
            // Hit front or back face only
            hitZFace = true;
        }
        else
        {
            // Sphere centre is inside the box — treat as a Z-face hit
            // This handles the edge case where the sphere has fully penetrated
            hitZFace = true;
        }

        return true;
    }

    // No collision
    return false;
}
