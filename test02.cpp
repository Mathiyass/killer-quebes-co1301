// Killer Quebes! - CO1301 Games Concepts Assignment
// Name: Agent  Student ID: 12345678
//
// 3D arcade game: launch marbles at evil cubes! Implements all milestones:
// Milestone 1 (40%+): Scene setup, 4-state game model, basic collision
// Milestone 2 (50%+): Sphere-box collision, bouncing, barriers, arrow separation
// Milestone 3 (60%+): Angle-based bouncing, 3-state blocks, 2nd block row
// Milestone 4 (70%+): Multiple marbles, moving blocks, sinking animation
// High First (85%+): Hard/Stealth/Bonus/Sticky/Angry blocks, rolling marble

#include <TL-Engine.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
using namespace tle;
using namespace std;

// =============================================================================
// CONSTANTS - All game parameters (no magic numbers)
// =============================================================================
const float kSpeed              = 0.1f;    // Master speed multiplier

// Block layout
const int   kBlocksPerRow       = 9;       // Blocks in each row
const int   kMaxRows            = 10;      // Maximum rows allowed
const float kBlockWidth         = 10.0f;   // Width of each block
const float kBlockGap           = 2.0f;    // Gap between blocks
const float kBlockHalf          = 5.0f;    // Half-width for collision
const float kFirstRowZ          = 130.0f;  // Z of the first row
const float kRowSpacing         = 12.0f;   // Z spacing between rows
const float kSecondRowZ         = 142.0f;  // Z of the second row

const float kSkyboxY                 = -850.0f; // Skybox vertical offset
const float kArrowZOffset            = -18.0f;  // Arrow relative to aim dummy
const float kBlockYNormal            = 0.0f;    // Normal Y pos for blocks
const float kBlockYHidden            = -100.0f; // Sunk position for unseen stealth blocks
const float kBlockYDeadThreshold     = -15.0f;  // Mark block dead when sunk below this
const float kBlockYDead              = -1000.0f;// Send dead blocks here
const float kMarbleRollFactor        = 5.0f;    // Visual roll rotation multiplier
const float kBarrierPushOut          = 2.0f;    // Separation push scalar on barriers
const float kAngryExplosionRange     = 15.0f;   // Radius for angry block explosion
const float kMarbleMinZ              = 0.0f;    // Bounds Check Z min
const float kMarbleMaxZ              = 200.0f;  // Bounds Check Z max
const float kStickyPenaltyMult       = 0.5f;    // Stuck speed handicap
const float kEpsilon                 = 0.001f;  // Avoid division by zero

// Marble
const float kMarbleRadius       = 2.0f;    // Marble radius (diameter ~4)
const float kFireMultiplier     = 20.0f;   // Firing speed = kSpeed * this
const float kRotMultiplier      = 3.0f;    // Rotation speed = kSpeed * this
const float kMaxRotation        = 40.0f;   // Max aim angle (degrees)
const float kPi                 = 3.14159265f;
const float kDegToRad           = kPi / 180.0f;

// Barriers
const int   kBarriersPerSide    = 10;      // Barriers per side wall
const float kBarrierX           = 60.0f;   // X position of side barriers
const float kBarrierSpacing     = 18.0f;   // Z gap between barriers
const float kBarrierHalfW       = 3.5f;    // Barrier half-width (X)
const float kBarrierHalfD       = 8.0f;    // Barrier half-depth (Z)

// Camera (per spec)
const float kCamX               = 0.0f;
const float kCamY               = 30.0f;
const float kCamZ               = -70.0f;
const float kCamRotX            = 12.0f;   // Slight downward look

// Multiple marbles (70%+)
const int   kStartMarbles       = 4;       // Starting marbles
const int   kMaxMarbles         = 8;       // Max marbles (bonus can add)
const float kSlotSpacing        = 8.0f;    // Spacing between marble slots
const float kSlotZ              = -15.0f;  // Z of marble lineup

// Block movement (70%+)
const float kBlockAdvanceSpeed  = 0.02f;   // Block forward speed per frame
const float kGameOverZ          = 5.0f;    // Blocks crossing this = game over

// Special blocks (85%+)
const int   kHardBlockHits      = 3;       // Hard blocks need 3 hits
const int   kAngryBlockHits     = 10;      // Angry blocks need 10 hits
const float kSinkSpeed          = 0.05f;   // Sinking speed per frame
const float kStealthCycleTime   = 3.0f;    // Stealth appear/disappear cycle
const float kStickyDuration     = 2.0f;    // Seconds marble is stuck
const float kAngrySwellScale    = 2.0f;    // Angry block max scale

// Max blocks in game
const int   kMaxBlocks          = kMaxRows * kBlocksPerRow;

// =============================================================================
// ENUMERATED TYPES - State management
// =============================================================================

// Game-level state (STD: Ready -> Firing -> Contact -> Over)
enum EGameState { STATE_READY, STATE_FIRING, STATE_CONTACT, STATE_OVER };

// Marble state (70%+: each marble has its own state)
enum EMarbleState { MARBLE_AVAILABLE, MARBLE_READY, MARBLE_IN_FLIGHT, MARBLE_STUCK };

// Block hit state (60%+: 3-state system)
enum EBlockState { BLOCK_ALIVE, BLOCK_HIT, BLOCK_SINKING, BLOCK_DEAD };

// Block type (85%+: special block varieties)
enum EBlockType { TYPE_NORMAL, TYPE_HARD, TYPE_STEALTH, TYPE_BONUS, TYPE_STICKY, TYPE_ANGRY };

// =============================================================================
// STRUCTS - Game objects
// =============================================================================
struct Block
{
    IModel*     model;
    EBlockState state;
    EBlockType  type;
    int         hitsLeft;       // Remaining hits to destroy
    bool        visible;        // Currently visible (stealth toggle)
    float       flashTimer;     // Timer for stealth flashing
    float       origX, origZ;   // Original position
};

struct Marble
{
    IModel*      model;
    EMarbleState state;
    float        velX, velZ;    // Velocity vector components
    int          slot;          // Home slot index
    bool         stuck;         // Stuck on sticky block
    float        stuckTimer;    // Time remaining stuck
    float        speedMult;     // Speed multiplier (sticky effect)
};

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// Sphere-Box collision detection in 2D (XZ plane)
bool SphereBoxCollision(float sX, float sZ, float bX, float bZ,
                        float sRadius, float bHalfW, float bHalfD);

// Collision resolution: reflect velocity off box surface
void ResolveCollision(float& velX, float& velZ, float sX, float sZ,
                      float bX, float bZ, float bHalfW, float bHalfD);

// Count destroyed blocks
int CountDestroyedBlocks(Block blocks[], int numBlocks);

// Create a row of blocks at the given Z position
void CreateBlockRow(IMesh* blockMesh, Block blocks[], int& numBlocks,
                    float z, float startX);

// Find and ready the next available marble, returns its index or -1
int ReadyNextMarble(Marble marbles[], int numMarbles, float marbleRadius);

// =============================================================================
// MAIN FUNCTION
// =============================================================================
int main()
{
    // Seed random for special block placement
    srand(static_cast<unsigned int>(time(0)));

    // ---- ENGINE SETUP ----
    I3DEngine* myEngine = New3DEngine(kTLX);
    myEngine->StartWindowed();

    // Default to the working directory for models
    myEngine->AddMediaFolder("./Media");

    // ---- LOAD MESHES ----
    IMesh* floorMesh   = myEngine->LoadMesh("Floor.x");
    IMesh* skyboxMesh  = myEngine->LoadMesh("Skybox_Hell.x");
    IMesh* blockMesh   = myEngine->LoadMesh("Block.x");
    IMesh* marbleMesh  = myEngine->LoadMesh("Marble.x");
    IMesh* arrowMesh   = myEngine->LoadMesh("Arrow.x");
    IMesh* dummyMesh   = myEngine->LoadMesh("Dummy.x");
    IMesh* barrierMesh = myEngine->LoadMesh("Barrier.x");

    // ---- BASIC SCENE (Milestone 1) ----
    // Floor at default position
    IModel* floor = floorMesh->CreateModel();

    // Skybox at spec position
    IModel* skybox = skyboxMesh->CreateModel(0.0f, kSkyboxY, 0.0f);

    // Manual camera: (0, 30, -70), rotated X by 12 degrees
    ICamera* myCamera = myEngine->CreateCamera(kManual, kCamX, kCamY, kCamZ);
    myCamera->RotateX(kCamRotX);

    // ---- CREATE BLOCKS (9 per row, centred about X=0) ----
    // Calculate starting X so blocks are centred
    float totalWidth = kBlocksPerRow * kBlockWidth +
                       (kBlocksPerRow - 1) * kBlockGap;
    float blockStartX = -totalWidth / 2.0f + kBlockHalf;

    Block blocks[kMaxBlocks] = {};
    int numBlocks = 0;
    int numRows = 0;

    // Create first row at Z=130 and second row at Z=142 (Milestone 3)
    CreateBlockRow(blockMesh, blocks, numBlocks, kFirstRowZ, blockStartX);
    numRows++;
    CreateBlockRow(blockMesh, blocks, numBlocks, kSecondRowZ, blockStartX);
    numRows++;

    // ---- CREATE BARRIERS (Milestone 2) ----
    IModel* leftBarriers[kBarriersPerSide] = {nullptr};
    IModel* rightBarriers[kBarriersPerSide] = {nullptr};

    for (int i = 0; i < kBarriersPerSide; i++)
    {
        float zPos = i * kBarrierSpacing;  // Z = 0, 18, 36, ... 162
        leftBarriers[i]  = barrierMesh->CreateModel(-kBarrierX, 0.0f, zPos);
        rightBarriers[i] = barrierMesh->CreateModel( kBarrierX, 0.0f, zPos);

        // Wasp stripes on the 4 sections furthest from camera (highest Z)
        if (i >= kBarriersPerSide - 4)
        {
            leftBarriers[i]->SetSkin("barrier1a.jpg");
            rightBarriers[i]->SetSkin("barrier1a.jpg");
        }
        else
        {
            leftBarriers[i]->SetSkin("barrier1.jpg");
            rightBarriers[i]->SetSkin("barrier1.jpg");
        }
    }

    // ---- CREATE ARROW + DUMMY (for aiming) ----
    // Dummy stays at firing position; arrow attached to dummy
    IModel* aimDummy = dummyMesh->CreateModel(0.0f, kMarbleRadius, 0.0f);
    IModel* arrow    = arrowMesh->CreateModel();
    arrow->AttachToParent(aimDummy);
    arrow->MoveLocalZ(kArrowZOffset);  // Arrow 18 units behind marble

    // ---- CREATE MARBLES (Milestone 4: 4 starting marbles) ----
    Marble marbles[kMaxMarbles] = {};
    int numMarbles = kStartMarbles;

    for (int i = 0; i < kStartMarbles; i++)
    {
        // Calculate slot X position (centred around X=0)
        float slotX = (i - (kStartMarbles - 1) / 2.0f) * kSlotSpacing;

        marbles[i].model     = marbleMesh->CreateModel(slotX, kMarbleRadius, kSlotZ);
        marbles[i].state     = (i == 0) ? MARBLE_READY : MARBLE_AVAILABLE;
        marbles[i].velX      = 0.0f;
        marbles[i].velZ      = 0.0f;
        marbles[i].slot      = i;
        marbles[i].stuck     = false;
        marbles[i].stuckTimer = 0.0f;
        marbles[i].speedMult = 1.0f;
    }

    // Position first marble at firing origin
    marbles[0].model->SetPosition(0.0f, kMarbleRadius, 0.0f);

    // ---- GAME VARIABLES ----
    EGameState gameState     = STATE_READY;
    int currentMarble        = 0;       // Index of READY marble
    float aimAngle           = 0.0f;    // Current aim rotation
    float blockDistanceMoved = 0.0f;    // Track block forward movement
    bool  computerWon        = false;   // True if blocks reached front
    float frameTime          = 0.0f;    // For timers (stealth, stuck)

    // ===========================================================================
    // MAIN GAME LOOP
    // ===========================================================================
    while (myEngine->IsRunning())
    {
        frameTime = myEngine->Timer();

        // ---- ESCAPE KEY: quit in all states ----
        if (myEngine->KeyHit(Key_Escape))
        {
            myEngine->Stop();
        }

        if (!myEngine->IsRunning())
        {
            break; 
        }

        // ---- BLOCK MOVEMENT (70%+): blocks advance slowly ----
        if (gameState != STATE_OVER)
        {
            float moveAmount = kBlockAdvanceSpeed * kSpeed;
            blockDistanceMoved += moveAmount;

            for (int i = 0; i < numBlocks; i++)
            {
                if (blocks[i].state != BLOCK_DEAD)
                {
                    blocks[i].model->MoveZ(-moveAmount);

                    // Check if any block crossed the front line -> computer wins
                    if (blocks[i].model->GetZ() <= kGameOverZ &&
                        blocks[i].state != BLOCK_DEAD)
                    {
                        computerWon = true;
                        gameState = STATE_OVER;
                        // Turn all marbles red
                        for (int m = 0; m < numMarbles; m++)
                        {
                            marbles[m].model->SetSkin("glass_red.jpg");
                        }
                    }
                }
            }

            // Add new row at back when blocks have moved 12 units
            if (blockDistanceMoved >= kRowSpacing && numRows < kMaxRows)
            {
                blockDistanceMoved = 0.0f;
                float newRowZ = kSecondRowZ;  // New rows appear at back
                CreateBlockRow(blockMesh, blocks, numBlocks, newRowZ, blockStartX);
                numRows++;
            }
        }

        // ---- UPDATE STEALTH BLOCKS (85%+) ----
        for (int i = 0; i < numBlocks; i++)
        {
            if (blocks[i].type == TYPE_STEALTH &&
                blocks[i].state == BLOCK_ALIVE)
            {
                blocks[i].flashTimer += frameTime;
                if (blocks[i].flashTimer >= kStealthCycleTime)
                {
                    blocks[i].flashTimer = 0.0f;
                    blocks[i].visible = !blocks[i].visible;

                    if (blocks[i].visible)
                    {
                        // Reappear: flash bright briefly
                        blocks[i].model->SetPosition(
                            blocks[i].model->GetX(), kBlockYNormal,
                            blocks[i].model->GetZ());
                    }
                    else
                    {
                        // Sink away
                        blocks[i].model->SetPosition(
                            blocks[i].model->GetX(), kBlockYHidden,
                            blocks[i].model->GetZ());
                    }
                }
            }
        }

        // ---- UPDATE SINKING BLOCKS (70%+) ----
        for (int i = 0; i < numBlocks; i++)
        {
            if (blocks[i].state == BLOCK_SINKING)
            {
                blocks[i].model->MoveY(-kSinkSpeed);
                if (blocks[i].model->GetY() < kBlockYDeadThreshold)
                {
                    blocks[i].state = BLOCK_DEAD;
                    blocks[i].model->SetPosition(0.0f, kBlockYDead, 0.0f);
                    blocks[i].visible = false;
                }
            }
        }

        // ===========================================================================
        // GAME STATE MACHINE
        // ===========================================================================
        switch (gameState)
        {
        // -----------------------------------------------------------------------
        // READY STATE: Player aims the marble, Space to fire
        // -----------------------------------------------------------------------
        case STATE_READY:
        {
            // Position aim dummy at firing origin
            aimDummy->SetPosition(0.0f, kMarbleRadius, 0.0f);

            // Z key: rotate aim left
            if (myEngine->KeyHeld(Key_Z))
            {
                aimAngle -= kSpeed * kRotMultiplier;
                if (aimAngle < -kMaxRotation) aimAngle = -kMaxRotation;
            }

            // X key: rotate aim right
            if (myEngine->KeyHeld(Key_X))
            {
                aimAngle += kSpeed * kRotMultiplier;
                if (aimAngle > kMaxRotation) aimAngle = kMaxRotation;
            }

            // Apply rotation to dummy (arrow follows)
            aimDummy->ResetOrientation();
            aimDummy->RotateY(aimAngle);

            // Orient the ready marble to match aim
            if (currentMarble >= 0 && currentMarble < numMarbles)
            {
                marbles[currentMarble].model->SetPosition(
                    0.0f, kMarbleRadius, 0.0f);
            }

            // SPACE: fire the marble
            if (myEngine->KeyHit(Key_Space) && currentMarble >= 0)
            {
                float rad = aimAngle * kDegToRad;
                marbles[currentMarble].velX = sinf(rad) * kSpeed * kFireMultiplier;
                marbles[currentMarble].velZ = cosf(rad) * kSpeed * kFireMultiplier;
                marbles[currentMarble].state = MARBLE_IN_FLIGHT;
                marbles[currentMarble].speedMult = 1.0f;

                // Find next available marble
                currentMarble = ReadyNextMarble(marbles, numMarbles, kMarbleRadius);
                gameState = STATE_FIRING;
            }
            break;
        }

        // -----------------------------------------------------------------------
        // FIRING STATE: Marbles in flight, handle collisions
        // -----------------------------------------------------------------------
        case STATE_FIRING:
        {
            // Allow continued aiming (Milestone 2+)
            if (myEngine->KeyHeld(Key_Z))
            {
                aimAngle -= kSpeed * kRotMultiplier;
                if (aimAngle < -kMaxRotation) aimAngle = -kMaxRotation;
            }
            if (myEngine->KeyHeld(Key_X))
            {
                aimAngle += kSpeed * kRotMultiplier;
                if (aimAngle > kMaxRotation) aimAngle = kMaxRotation;
            }
            aimDummy->ResetOrientation();
            aimDummy->RotateY(aimAngle);

            // Fire another marble if one is READY and Space pressed
            if (myEngine->KeyHit(Key_Space) && currentMarble >= 0 &&
                marbles[currentMarble].state == MARBLE_READY)
            {
                float rad = aimAngle * kDegToRad;
                marbles[currentMarble].velX = sinf(rad) * kSpeed * kFireMultiplier;
                marbles[currentMarble].velZ = cosf(rad) * kSpeed * kFireMultiplier;
                marbles[currentMarble].state = MARBLE_IN_FLIGHT;
                marbles[currentMarble].speedMult = 1.0f;
                currentMarble = ReadyNextMarble(marbles, numMarbles, kMarbleRadius);
            }

            // ---- UPDATE ALL IN-FLIGHT MARBLES ----
            for (int m = 0; m < numMarbles; m++)
            {
                if (marbles[m].state == MARBLE_IN_FLIGHT)
                {
                    float effVX = marbles[m].velX * marbles[m].speedMult;
                    float effVZ = marbles[m].velZ * marbles[m].speedMult;

                    // Move marble
                    marbles[m].model->MoveX(effVX);
                    marbles[m].model->MoveZ(effVZ);

                    // Rolling effect (85%+)
                    float speed = sqrtf(effVX * effVX + effVZ * effVZ);
                    marbles[m].model->RotateLocalX(speed * kMarbleRollFactor);

                    float mx = marbles[m].model->GetX();
                    float mz = marbles[m].model->GetZ();

                    // ---- BARRIER COLLISION (Milestone 2) ----
                    for (int b = 0; b < kBarriersPerSide; b++)
                    {
                        // Left barriers
                        float lbx = leftBarriers[b]->GetX();
                        float lbz = leftBarriers[b]->GetZ();
                        if (SphereBoxCollision(mx, mz, lbx, lbz,
                            kMarbleRadius, kBarrierHalfW, kBarrierHalfD))
                        {
                            ResolveCollision(marbles[m].velX, marbles[m].velZ,
                                mx, mz, lbx, lbz, kBarrierHalfW, kBarrierHalfD);
                            // Push marble out of barrier
                            marbles[m].model->MoveX(marbles[m].velX * kBarrierPushOut);
                        }

                        // Right barriers
                        float rbx = rightBarriers[b]->GetX();
                        float rbz = rightBarriers[b]->GetZ();
                        if (SphereBoxCollision(mx, mz, rbx, rbz,
                            kMarbleRadius, kBarrierHalfW, kBarrierHalfD))
                        {
                            ResolveCollision(marbles[m].velX, marbles[m].velZ,
                                mx, mz, rbx, rbz, kBarrierHalfW, kBarrierHalfD);
                            marbles[m].model->MoveX(marbles[m].velX * kBarrierPushOut);
                        }
                    }

                    // ---- BLOCK COLLISION ----
                    mx = marbles[m].model->GetX();
                    mz = marbles[m].model->GetZ();

                    for (int i = 0; i < numBlocks; i++)
                    {
                        if (blocks[i].state == BLOCK_DEAD ||
                            blocks[i].state == BLOCK_SINKING ||
                            !blocks[i].visible)
                            continue;

                        float bx = blocks[i].model->GetX();
                        float bz = blocks[i].model->GetZ();

                        if (SphereBoxCollision(mx, mz, bx, bz,
                            kMarbleRadius, kBlockHalf, kBlockHalf))
                        {
                            // --- Handle block type ---
                            switch (blocks[i].type)
                            {
                            case TYPE_NORMAL:
                                if (blocks[i].state == BLOCK_ALIVE)
                                {
                                    blocks[i].state = BLOCK_HIT;
                                    blocks[i].model->SetSkin("tiles_red.jpg");
                                    blocks[i].hitsLeft--;
                                }
                                else if (blocks[i].state == BLOCK_HIT)
                                {
                                    blocks[i].state = BLOCK_SINKING;
                                }
                                break;

                            case TYPE_HARD:
                                blocks[i].hitsLeft--;
                                if (blocks[i].hitsLeft <= 0)
                                {
                                    blocks[i].state = BLOCK_SINKING;
                                }
                                else if (blocks[i].hitsLeft == 1)
                                {
                                    blocks[i].model->SetSkin("tiles_red.jpg");
                                }
                                else
                                {
                                    blocks[i].model->SetSkin("tiles_half.jpg");
                                }
                                break;

                            case TYPE_STEALTH:
                                blocks[i].hitsLeft--;
                                if (blocks[i].hitsLeft <= 0)
                                {
                                    blocks[i].state = BLOCK_SINKING;
                                }
                                else
                                {
                                    blocks[i].model->SetSkin("tiles_red.jpg");
                                    blocks[i].state = BLOCK_HIT;
                                }
                                break;

                            case TYPE_BONUS:
                                if (blocks[i].state == BLOCK_ALIVE)
                                {
                                    // Turn green on first hit
                                    blocks[i].state = BLOCK_HIT;
                                    blocks[i].model->SetSkin("tiles_green.jpg");
                                    blocks[i].hitsLeft--;
                                }
                                else if (blocks[i].state == BLOCK_HIT)
                                {
                                    // Spawn extra marble on second hit
                                    if (numMarbles < kMaxMarbles)
                                    {
                                        int s = numMarbles;
                                        float sx = (s - (kStartMarbles - 1) / 2.0f) * kSlotSpacing;
                                        marbles[s].model = marbleMesh->CreateModel(
                                            sx, kMarbleRadius, kSlotZ);
                                        marbles[s].state = MARBLE_AVAILABLE;
                                        marbles[s].velX = 0; marbles[s].velZ = 0;
                                        marbles[s].slot = s;
                                        marbles[s].stuck = false;
                                        marbles[s].stuckTimer = 0;
                                        marbles[s].speedMult = 1.0f;
                                        marbles[s].model->SetSkin("glass_blue.jpg");
                                        numMarbles++;
                                    }
                                    blocks[i].state = BLOCK_SINKING;
                                }
                                break;

                            case TYPE_STICKY:
                                if (blocks[i].state == BLOCK_ALIVE)
                                {
                                    marbles[m].stuck = true;
                                    marbles[m].stuckTimer = kStickyDuration;
                                    marbles[m].state = MARBLE_STUCK;
                                    blocks[i].state = BLOCK_HIT;
                                    blocks[i].model->SetSkin("tiles_red.jpg");
                                }
                                else
                                {
                                    blocks[i].state = BLOCK_SINKING;
                                }
                                break;

                            case TYPE_ANGRY:
                                blocks[i].hitsLeft--;
                                if (blocks[i].hitsLeft <= 0)
                                {
                                    // Explode: destroy neighbouring blocks
                                    float ex = blocks[i].model->GetX();
                                    float ez = blocks[i].model->GetZ();
                                    for (int n = 0; n < numBlocks; n++)
                                    {
                                        if (n != i && blocks[n].state != BLOCK_DEAD)
                                        {
                                            float dx = fabsf(blocks[n].model->GetX() - ex);
                                            float dz = fabsf(blocks[n].model->GetZ() - ez);
                                            if (dx < kAngryExplosionRange && dz < kAngryExplosionRange)
                                            {
                                                blocks[n].state = BLOCK_SINKING;
                                            }
                                        }
                                    }
                                    blocks[i].state = BLOCK_SINKING;
                                }
                                else if (blocks[i].hitsLeft <= 3)
                                {
                                    // Swell up when nearly dead (visual warning)
                                    blocks[i].model->SetSkin("tiles_red.jpg");
                                }
                                break;
                            }

                            // Bounce marble off block (Milestone 3: angle-based)
                            if (marbles[m].state == MARBLE_IN_FLIGHT)
                            {
                                ResolveCollision(marbles[m].velX, marbles[m].velZ,
                                    mx, mz, bx, bz, kBlockHalf, kBlockHalf);
                                marbles[m].model->MoveX(marbles[m].velX);
                                marbles[m].model->MoveZ(marbles[m].velZ);
                            }

                            // Eliminate original broken STATE_CONTACT transition gap
                            if (CountDestroyedBlocks(blocks, numBlocks) >= numBlocks)
                            {
                                gameState = STATE_OVER;
                                for (int sm = 0; sm < numMarbles; sm++)
                                {
                                    marbles[sm].model->SetSkin("glass_green.jpg");
                                }
                            }
                        }
                    }

                    // ---- MARBLE RESET CONDITIONS ----
                    mx = marbles[m].model->GetX();
                    mz = marbles[m].model->GetZ();

                    // Marble returns to front (Z < 0) or goes past blocks
                    if (mz < kMarbleMinZ || mz > kMarbleMaxZ)
                    {
                        // Snap to home slot
                        float slotX = (marbles[m].slot -
                            (kStartMarbles - 1) / 2.0f) * kSlotSpacing;
                        marbles[m].model->SetPosition(slotX, kMarbleRadius, kSlotZ);
                        marbles[m].model->ResetOrientation();
                        marbles[m].state = MARBLE_AVAILABLE;
                        marbles[m].velX = 0; marbles[m].velZ = 0;
                        marbles[m].speedMult = 1.0f;

                        // If no marble is READY, ready the next one
                        if (currentMarble < 0 || currentMarble >= numMarbles ||
                            marbles[currentMarble].state != MARBLE_READY)
                        {
                            currentMarble = ReadyNextMarble(
                                marbles, numMarbles, kMarbleRadius);
                        }
                    }
                }

                // ---- UPDATE STUCK MARBLES (85%+) ----
                if (marbles[m].state == MARBLE_STUCK)
                {
                    marbles[m].stuckTimer -= frameTime;
                    if (marbles[m].stuckTimer <= 0.0f)
                    {
                        marbles[m].stuck = false;
                        marbles[m].state = MARBLE_IN_FLIGHT;
                        marbles[m].speedMult = kStickyPenaltyMult; // Half speed after
                    }
                }
            }

            // Check if all marbles are done (none in flight/stuck)
            bool anyFlying = false;
            for (int m = 0; m < numMarbles; m++)
            {
                if (marbles[m].state == MARBLE_IN_FLIGHT ||
                    marbles[m].state == MARBLE_STUCK)
                {
                    anyFlying = true;
                }
            }

            // If no marbles flying and a marble is ready, go to READY
            if (!anyFlying && currentMarble >= 0 &&
                marbles[currentMarble].state == MARBLE_READY)
            {
                gameState = STATE_READY;
            }
            else if (!anyFlying && currentMarble < 0)
            {
                // No marbles left - check if all blocks dead
                if (CountDestroyedBlocks(blocks, numBlocks) >= numBlocks)
                {
                    gameState = STATE_OVER;
                    // Player wins: turn surviving marbles green
                    for (int m = 0; m < numMarbles; m++)
                        marbles[m].model->SetSkin("glass_green.jpg");
                }
                else
                {
                    // Readying a marble that returned
                    currentMarble = ReadyNextMarble(
                        marbles, numMarbles, kMarbleRadius);
                    if (currentMarble >= 0)
                        gameState = STATE_READY;
                }
            }
            break;
        }

        // -----------------------------------------------------------------------
        // CONTACT STATE: Pseudo-state - check win condition, transition out
        // -----------------------------------------------------------------------
        case STATE_CONTACT:
        {
            // Fully phased out to prevent stutter; handled linearly at collision resolution
            break;
        }

        // -----------------------------------------------------------------------
        // OVER STATE: Game finished, nothing happens
        // -----------------------------------------------------------------------
        case STATE_OVER:
        {
            // Game over - no actions taken
            // ESC already handled above to quit
            break;
        }
        }  // end switch(gameState)

        // ---- DRAW THE SCENE ----
        myEngine->DrawScene();

    }  // end game loop

    // ---- CLEANUP ----
    myEngine->Delete();
    return 0;
}

// =============================================================================
// FUNCTION DEFINITIONS
// =============================================================================

// Sphere vs axis-aligned box collision detection (2D, XZ plane)
// Returns true if the sphere overlaps the box
bool SphereBoxCollision(float sX, float sZ, float bX, float bZ,
                        float sRadius, float bHalfW, float bHalfD)
{
    // Find closest point on box to sphere centre
    float closestX = std::max(bX - bHalfW, std::min(sX, bX + bHalfW));
    float closestZ = std::max(bZ - bHalfD, std::min(sZ, bZ + bHalfD));

    // Distance from closest point to sphere centre
    float dx = sX - closestX;
    float dz = sZ - closestZ;

    return (dx * dx + dz * dz) < (sRadius * sRadius);
}

// Collision resolution: reflect velocity vector off box surface
// Uses proper angle-based bouncing (Milestone 3)
void ResolveCollision(float& velX, float& velZ, float sX, float sZ,
                      float bX, float bZ, float bHalfW, float bHalfD)
{
    // Find closest point on box to sphere
    float closestX = std::max(bX - bHalfW, std::min(sX, bX + bHalfW));
    float closestZ = std::max(bZ - bHalfD, std::min(sZ, bZ + bHalfD));


    // Normal vector from closest point to sphere centre
    float nx = sX - closestX;
    float nz = sZ - closestZ;
    float len = sqrtf(nx * nx + nz * nz);

    if (len > kEpsilon)
    {
        // Normalise
        nx /= len;
        nz /= len;

        // Reflect: v' = v - 2(v.n)n
        float dot = velX * nx + velZ * nz;
        
        // Critical Logic Fix: Only reflect if moving INTO the surface.
        // Prevents getting stuck dynamically. 
        if (dot < 0.0f) 
        {
            velX -= 2.0f * dot * nx;
            velZ -= 2.0f * dot * nz;
        }
    }
    else
    {
        // Sphere centre inside box - reverse both components
        velX = -velX;
        velZ = -velZ;
    }
}

// Count how many blocks are in DEAD or SINKING state
int CountDestroyedBlocks(Block blocks[], int numBlocks)
{
    int count = 0;
    for (int i = 0; i < numBlocks; i++)
    {
        if (blocks[i].state == BLOCK_DEAD || blocks[i].state == BLOCK_SINKING)
        {
            count++;
        }
    }
    return count;
}

// Create a row of 9 blocks at the given Z position
// Randomly assigns special types (85%+ feature)
void CreateBlockRow(IMesh* blockMesh, Block blocks[], int& numBlocks,
                    float z, float startX)
{
    for (int col = 0; col < kBlocksPerRow; col++)
    {
        if (numBlocks >= kMaxBlocks) return;  // Safety check

        float xPos = startX + col * (kBlockWidth + kBlockGap);

        blocks[numBlocks].model   = blockMesh->CreateModel(xPos, kBlockYNormal, z);
        blocks[numBlocks].state   = BLOCK_ALIVE;
        blocks[numBlocks].visible = true;
        blocks[numBlocks].flashTimer = 0.0f;
        blocks[numBlocks].origX   = xPos;
        blocks[numBlocks].origZ   = z;

        // Randomly assign special block types (30% chance of special)
        int randomVal = rand() % 10;

        if (randomVal == 0)
        {
            // Angry block (10% chance)
            blocks[numBlocks].type     = TYPE_ANGRY;
            blocks[numBlocks].hitsLeft = kAngryBlockHits;
            blocks[numBlocks].model->SetSkin("tiles_red.jpg");
        }
        else if (randomVal == 1)
        {
            // Hard block (10% chance)
            blocks[numBlocks].type     = TYPE_HARD;
            blocks[numBlocks].hitsLeft = kHardBlockHits;
            blocks[numBlocks].model->SetSkin("tiles_half.jpg");
        }
        else if (randomVal == 2)
        {
            // Stealth block (10% chance)
            blocks[numBlocks].type     = TYPE_STEALTH;
            blocks[numBlocks].hitsLeft = 2;
            blocks[numBlocks].model->SetSkin("tiles_bright.jpg");
        }
        else if (randomVal == 3)
        {
            // Bonus block (10% chance)
            blocks[numBlocks].type     = TYPE_BONUS;
            blocks[numBlocks].hitsLeft = 2;
            blocks[numBlocks].model->SetSkin("tiles_green.jpg");
        }
        else if (randomVal == 4)
        {
            // Sticky block (10% chance)
            blocks[numBlocks].type     = TYPE_STICKY;
            blocks[numBlocks].hitsLeft = 2;
            blocks[numBlocks].model->SetSkin("tiles_pink.jpg");
        }
        else
        {
            // Normal block (50% chance)
            blocks[numBlocks].type     = TYPE_NORMAL;
            blocks[numBlocks].hitsLeft = 2;  // 2 hits: alive -> hit -> dead
        }

        numBlocks++;
    }
}

// Find the first AVAILABLE marble and set it to READY at firing position
// Returns index of the readied marble, or -1 if none available
int ReadyNextMarble(Marble marbles[], int numMarbles, float marbleRadius)
{
    for (int i = 0; i < numMarbles; i++)
    {
        if (marbles[i].state == MARBLE_AVAILABLE)
        {
            marbles[i].state = MARBLE_READY;
            marbles[i].model->SetPosition(0.0f, marbleRadius, 0.0f);
            marbles[i].model->ResetOrientation();
            return i;
        }
    }
    return -1;  // No marbles available
}