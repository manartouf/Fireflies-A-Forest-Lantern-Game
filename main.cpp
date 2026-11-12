// ============================================================
//  FIREFLIES - A Forest Lantern Tale   (version 3)
//  CSE 402 : Computer Graphics & Image Processing Lab
//  C++ / OpenGL (FreeGLUT) / Code::Blocks
//
//  Three levels (pick one in the menu):
//    EASY   : Lost in the Late Night     - catch 25 fireflies, run home.
//    MEDIUM : Wandering in Twilight      - a vampire hunts you. Catch 25 fireflies
//                                          + collect 5 lair mushrooms, then run home.
//    HARD   : The Witch's Mansion        - a witch hunts you. Catch 25 fireflies
//                                          + collect lair mushrooms, then catch the
//                                          10 wisps on the cage locks to free 10 children.
//
//  Controls: A / D or Left / Right arrows = walk
//            W or Up arrow                = hop
//            SPACE                        = hop + swing net
//            Click on the traveler        = hop + swing net
//            Click anywhere else          = swing the net only
//            E or Right-click             = mushroom blast (medium / hard)
//            R = restart level, M = menu, ESC = quit
//  Menu    : W / S or Up / Down arrows (or mouse) to choose, ENTER / SPACE / click to start
// ============================================================
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cctype>
#include <vector>

// (Background music: see the "BACKGROUND MUSIC" comment inside main() near the bottom of this file.)

// ------------------------------------------------------------
//  Constants
// ------------------------------------------------------------
const float PI = 3.14159265f;
const int   VW = 1000, VH = 700;                  // virtual screen
const float LEVEL_W = 6500.0f;                    // length of the forest path
const float GY = 150.0f;                          // ground line (feet of the traveler)
const int   NEEDED = 25;                          // fireflies needed
const int   NUM_FF = 46;                          // fireflies alive in the forest
const float SWING_DUR = 0.32f;
const float JUMP_V = 760.0f, GRAV = 2400.0f;

// ---- new constants for the medium / hard levels ----
const float LAIR_X    = 4600.0f;                  // medium : x of the vampire's crypt
const float MANSION_X = 5950.0f;                  // hard   : x of the witch's mansion
const float CAGE_X    = 5250.0f;                  // hard   : x of the cage in front of the mansion
const int   MED_NEED  = 5;                        // medium : lair mushrooms needed
const int   KIDS      = 10;                       // hard   : children / wisps / mushrooms needed
const int   MAXLM     = 20;                       // max lair mushrooms in a level
const float PUSH_DIST = 250.0f;                   // "5 meters" = 250 pixels : how far a blast pushes the hunter back
const int   NBAT      = 40;                       // bats (medium)
const int   NCROW     = 16;                       // flying crows (hard)

// ------------------------------------------------------------
//  Data
// ------------------------------------------------------------
struct Firefly { float x, y, ang, speed, phase, freq; };
struct Flying  { float sx, sy, t; };              // caught firefly flying to the lantern
struct Mote    { float x, y, vx, vy, ph, sz; };   // floating dust / pollen
struct Leaf    { float x, y, vx, vy, rot, vr, sz, hue; };
struct Slide   { const char *l[3]; };
struct Bat     { float cx, cy, rx, ry, w, ph, sz; };          // a bat flying a figure-eight
struct Crow    { float cx, cy, rx, ry, w, ph, sz; };          // a crow circling
struct Part    { float x, y, vx, vy, life, maxLife, sz, r, g, b; int kind; };   // kind 0 = blood, 1 = spark
struct LMush   { float x, y, sz; bool got; };                 // a lair mushroom

// story slides: [difficulty][slide]
Slide slideSets[3][4] = {
    {   // ----- EASY : Lost in the Late Night -----
        {{ "Late at night, you slipped into the old forest...", "", "" }},
        {{ "Looking for enchanted mushrooms", "that glow in the dark.", "" }},
        {{ "Your basket is full,", "but the path home is gone.", "" }},
        {{ "Catch 25 fireflies with your net.", "Their light will show you the way.", "" }}
    },
    {   // ----- MEDIUM : Wandering in Twilight -----
        {{ "At twilight, you returned for more glowing mushrooms.", "", "" }},
        {{ "But the sun sank, and you crossed", "into a vampire's hunting ground.", "" }},
        {{ "His lair grows the brightest mushrooms,", "and he is awake.", "" }},
        {{ "Gather 25 fireflies and 5 lair mushrooms.", "Blast him back, then run home.", "" }}
    },
    {   // ----- HARD : The Witch's Mansion -----
        {{ "The witch had stolen the village children.", "", "" }},
        {{ "She locked them in a cage", "before her crooked mansion.", "" }},
        {{ "Each lock is guarded by a wisp", "that only fears true light.", "" }},
        {{ "Gather 25 fireflies and 10 mushrooms.", "Catch every wisp, free every child.", "" }}
    }
};
Slide *slides = slideSets[0];                     // slides of the chosen level

Firefly ff[NUM_FF];
std::vector<Flying> flying;
std::vector<Part> parts;                          // blood, sparks
Mote  motes[70];
Leaf  leaves[22];
Bat   bats[NBAT];
Crow  crows[NCROW];
LMush lm[MAXLM];
float jfx[NEEDED], jfy[NEEDED];                   // spots of fireflies inside the jar

int   state = 0;                                  // 0 = storyboard, 1 = playing, 2 = ending, 3 = menu, 4 = game over
int   slide = 0;
float st = 0, fadeA = 1.0f, demoLast = 0;

float px = 200, pz = 0, vzv = 0, vx = 0, face = 1, walkPh = 0;
bool  kL = false, kR = false;
bool  swinging = false;
float swingT = 0, swingAim = 0, cooldown = 0;
bool  arcSwing = false;                           // true = the half-circle swing used while jumping
float swingDur = SWING_DUR, swingSgn = 1;         // length of the current swing, direction of the sweep

int   caught = 0;
float gt = 0, ct = 0, playT = 0, hudPulse = 0;
float camX = 0, camPrev = 0, camVel = 0;
float cabinX = 0;
float streamCX = 0;                               // world x of the middle of the stream crossed at the end
bool  arrived = false, hopped = false;
float lx = 0, ly = 0;                             // lantern position (world)
int   winW = VW, winH = VH, lastMs = 0;

// ---- new globals ----
int   difficulty = 0;                             // 0 = easy, 1 = medium, 2 = hard
int   menuSel = 0;                                // highlighted level in the menu
float menuT = 0;
float bendT = 0;                                  // traveler bends down to pick a mushroom
float msgT = 0;                                   // timer of the on-screen message
char  msgBuf[96] = "";
float deathT = 0;
int   deathType = 0;                              // 1 = bitten by the vampire, 2 = caught by the witch
float blastT = 0, blastX = 0;                     // mushroom blast ring
int   lairGot = 0, ordCount = 0, freedCount = 0, lmN = 0;
bool  kidFree[KIDS];                              // child i is freed
float wispFlee[KIDS];                             // wisp i is fleeing (0..1)
bool  ordGot[64];                                 // ordinary mushrooms already picked
float hunterX = -5000, hunterA = 1, hunterStun = 0, hunterKB = 0, hunterWake = 0;   // vampire (medium) or witch (hard)
bool  hunterOn = false;

// ------------------------------------------------------------
//  Look of each level (colours of fog, darkness, tint ...)
// ------------------------------------------------------------
struct Look {
    float darkBase;
    float dk[3];                                  // colour of the darkness
    float fog[3];                                 // fog bands
    float blob[3];                                // drifting fog blobs
    float ray[3];                                 // moonlight beams
    float mulC[3], addC[3];                       // tint for hills, trees, ground
    float moonGlow[3], moonGlow2[3];              // glow around the moon
    float starMul;
};

Look looks[3] = {
    // EASY : the original cold blue forest night
    { 0.68f, { 0.01f, 0.02f, 0.06f }, { 0.20f, 0.34f, 0.38f }, { 0.42f, 0.58f, 0.62f }, { 0.60f, 0.82f, 0.90f },
      { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.70f, 0.85f, 1.0f }, { 0.50f, 0.70f, 0.90f }, 1.0f },
    // MEDIUM : purple / orange twilight
    { 0.50f, { 0.04f, 0.01f, 0.06f }, { 0.45f, 0.22f, 0.36f }, { 0.62f, 0.36f, 0.50f }, { 1.00f, 0.62f, 0.50f },
      { 1.5f, 0.85f, 1.2f }, { 0.03f, 0.0f, 0.035f }, { 1.0f, 0.50f, 0.35f }, { 0.90f, 0.40f, 0.50f }, 0.45f },
    // HARD : murky green / violet witchlight
    { 0.70f, { 0.02f, 0.02f, 0.04f }, { 0.22f, 0.32f, 0.24f }, { 0.40f, 0.52f, 0.40f }, { 0.65f, 0.90f, 0.55f },
      { 1.1f, 0.95f, 1.05f }, { 0.025f, 0.01f, 0.03f }, { 0.60f, 0.95f, 0.55f }, { 0.40f, 0.70f, 0.50f }, 0.9f }
};

const Look &LK() { return looks[difficulty]; }
void tintC(float &r, float &g, float &b)          // apply the level tint to a colour
{
    const Look &k = LK();
    r = r * k.mulC[0] + k.addC[0];
    g = g * k.mulC[1] + k.addC[1];
    b = b * k.mulC[2] + k.addC[2];
}

// ------------------------------------------------------------
//  Small helpers
// ------------------------------------------------------------
float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
float wrapf(float v, float m) { return v - floorf(v / m) * m; }
bool  inStreamX(float wx, float margin) { return state == 2 && fabsf(wx - streamCX) < margin; }   // is this world x over the crossing stream?

// stable pseudo random number 0..1 from an integer (so trees never "jump")
float hash01(int i, int salt)
{
    unsigned int h = (unsigned int)(i * 374761393u) + (unsigned int)(salt * 668265263u) + 1013904223u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (h & 0xFFFFu) / 65536.0f;
}

void normalBlend()   { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
void additiveBlend() { glBlendFunc(GL_SRC_ALPHA, GL_ONE); }

// ---- small helpers for the new levels ----
void  showMsg(const char *s) { strncpy(msgBuf, s, 95); msgBuf[95] = 0; msgT = 3.2f; }   // message in the middle of the screen
bool  immune() { return caught >= NEEDED; }                                              // 25 fireflies = the hunter cannot hurt you
float hunterFace() { return px >= hunterX ? 1.0f : -1.0f; }                              // hunter looks toward the traveler
int   lmLeft() { int n = 0; for (int i = 0; i < lmN; i++) if (!lm[i].got) n++; return n; }   // lair mushrooms still on the ground
bool  ordIsPicked(int i) { int k = i + 2; return (k >= 0 && k < 64) ? ordGot[k] : false; }
void  ordSetPicked(int i) { int k = i + 2; if (k >= 0 && k < 64) ordGot[k] = true; }

// ------------------------------------------------------------
//  Shape helpers
// ------------------------------------------------------------
void circleFill(float x, float y, float r, int seg = 20)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= seg; i++) {
        float a = 2 * PI * i / seg;
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
    }
    glEnd();
}

void circleLine(float x, float y, float r, int seg = 24)
{
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) {
        float a = 2 * PI * i / seg;
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
    }
    glEnd();
}

// soft glow: strong in the centre, transparent at the edge
void glow(float x, float y, float r, float R, float G, float B, float a)
{
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(R, G, B, a);
    glVertex2f(x, y);
    glColor4f(R, G, B, 0.0f);
    for (int i = 0; i <= 28; i++) {
        float ang = 2 * PI * i / 28;
        glVertex2f(x + cosf(ang) * r, y + sinf(ang) * r);
    }
    glEnd();
}

void glowEllipse(float x, float y, float rx, float ry, float R, float G, float B, float a)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(rx, ry, 1);
    glow(0, 0, 1, R, G, B, a);
    glPopMatrix();
}

// vertical gradient rectangle (bottom colour -> top colour)
void vgrad(float x0, float y0, float x1, float y1,
           float r1, float g1, float b1, float a1,
           float r2, float g2, float b2, float a2)
{
    glBegin(GL_QUADS);
    glColor4f(r1, g1, b1, a1); glVertex2f(x0, y0); glVertex2f(x1, y0);
    glColor4f(r2, g2, b2, a2); glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
}

// horizontal gradient rectangle (left colour -> right colour)
void hgrad(float x0, float y0, float x1, float y1,
           float r1, float g1, float b1, float a1,
           float r2, float g2, float b2, float a2)
{
    glBegin(GL_QUADS);
    glColor4f(r1, g1, b1, a1); glVertex2f(x0, y0); glVertex2f(x0, y1);
    glColor4f(r2, g2, b2, a2); glVertex2f(x1, y1); glVertex2f(x1, y0);
    glEnd();
}

void roundRect(float x0, float y0, float x1, float y1, float r, bool fill)
{
    float cx[4] = { x1 - r, x0 + r, x0 + r, x1 - r };
    float cy[4] = { y1 - r, y1 - r, y0 + r, y0 + r };
    glBegin(fill ? GL_POLYGON : GL_LINE_LOOP);
    for (int c = 0; c < 4; c++)
        for (int i = 0; i <= 6; i++) {
            float a = (c * 90 + i * 15) * PI / 180.0f;
            glVertex2f(cx[c] + cosf(a) * r, cy[c] + sinf(a) * r);
        }
    glEnd();
}

// ------------------------------------------------------------
//  Text helpers
// ------------------------------------------------------------
void drawText(float x, float y, const char *s, void *font)
{
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(font, *s);
}

void drawTextCentered(float cx, float y, const char *s, void *font)
{
    int w = glutBitmapLength(font, (const unsigned char *)s);
    drawText(cx - w / 2.0f, y, s, font);
}

// draws only the first n letters (typewriter effect), but keeps the line centered
void drawTextPartial(float cx, float y, const char *s, int n, void *font)
{
    int w = glutBitmapLength(font, (const unsigned char *)s);
    glRasterPos2f(cx - w / 2.0f, y);
    for (int i = 0; s[i] && i < n; i++) glutBitmapCharacter(font, s[i]);
}

float strokeLen(const char *s, float sc)
{
    return glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char *)s) * sc;
}

void strokeRaw(float x, float y, float sc, const char *s)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(sc, sc, 1);
    for (; *s; s++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *s);
    glPopMatrix();
}

// big glowing title made of thick strokes
void fancyTitle(float cx, float y, float sc, const char *s, float r, float g, float b, float a)
{
    float x = cx - strokeLen(s, sc) / 2.0f;
    additiveBlend();
    glLineWidth(8); glColor4f(r, g, b, 0.10f * a); strokeRaw(x, y, sc, s);
    glLineWidth(5); glColor4f(r, g, b, 0.18f * a); strokeRaw(x, y, sc, s);
    normalBlend();
    glLineWidth(5); glColor4f(0, 0, 0, 0.45f * a); strokeRaw(x + 3, y - 3, sc, s);
    glLineWidth(3); glColor4f(r, g, b, a);         strokeRaw(x, y, sc, s);
    glLineWidth(1);
}

// ------------------------------------------------------------
//  Leaves and vines (decoration)
// ------------------------------------------------------------
void leafShape(float x, float y, float ang, float len, float r, float g, float b, float a)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(ang * 180.0f / PI, 0, 0, 1);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r * 0.6f, g * 0.6f, b * 0.6f, a);
    glVertex2f(0, 0);
    glColor4f(r, g, b, a);
    for (int i = 0; i <= 8; i++) { float t = i / 8.0f; glVertex2f(t * len, sinf(t * PI) * len * 0.26f); }
    for (int i = 8; i >= 0; i--) { float t = i / 8.0f; glVertex2f(t * len, -sinf(t * PI) * len * 0.26f); }
    glEnd();
    glLineWidth(1);
    glColor4f(r * 0.5f, g * 0.5f, b * 0.5f, a * 0.8f);
    glBegin(GL_LINES);
    glVertex2f(0, 0); glVertex2f(len * 0.95f, 0);
    glEnd();
    glPopMatrix();
}

void drawVine(float x0, float x1, float yc, float a)
{
    glLineWidth(2.5f);
    glColor4f(0.30f, 0.55f, 0.30f, a);
    glBegin(GL_LINE_STRIP);
    for (float x = x0; x <= x1; x += 6) glVertex2f(x, yc + sinf(x * 0.035f + 0.6f) * 8);
    glEnd();
    glLineWidth(1);
    int k = 0;
    for (float x = x0 + 20; x < x1 - 10; x += 34, k++) {
        float y = yc + sinf(x * 0.035f + 0.6f) * 8;
        float side = (k % 2) ? 1.0f : -1.0f;
        if (k % 3 == 0) leafShape(x, y, side * 0.95f, 22, 0.92f, 0.78f, 0.35f, a);
        else            leafShape(x, y, side * 0.95f, 24, 0.35f, 0.65f, 0.30f, a);
    }
}

// ------------------------------------------------------------
//  Background : sky, stars, moon
// ------------------------------------------------------------
float moonY() { return difficulty == 1 ? 455.0f : 590.0f; }          // medium : big low moon
float moonR() { return difficulty == 1 ? 54.0f : (difficulty == 2 ? 40.0f : 36.0f); }

void drawSky()
{
    if (difficulty == 1) {                        // MEDIUM : purple and orange twilight
        vgrad(0, 0, VW, 140, 0.78f, 0.33f, 0.25f, 1, 0.66f, 0.26f, 0.32f, 1);
        vgrad(0, 140, VW, 300, 0.66f, 0.26f, 0.32f, 1, 0.42f, 0.17f, 0.36f, 1);
        vgrad(0, 300, VW, 480, 0.42f, 0.17f, 0.36f, 1, 0.16f, 0.07f, 0.26f, 1);
        vgrad(0, 480, VW, VH, 0.16f, 0.07f, 0.26f, 1, 0.04f, 0.02f, 0.12f, 1);
        additiveBlend();                          // last glow of the sun and thin pink clouds
        glowEllipse(200, 235, 560, 120, 1.0f, 0.55f, 0.25f, 0.34f);
        for (int i = 0; i < 6; i++) {
            float x = wrapf(i * 230.0f + gt * 4.0f, VW + 600) - 300;
            glowEllipse(x, 300 + (i * 61) % 220, 230, 12 + (i % 3) * 5, 0.95f, 0.45f, 0.55f, 0.16f);
        }
        normalBlend();
        return;
    }
    if (difficulty == 2) {                        // HARD : murky green night
        vgrad(0, 0, VW, 300, 0.13f, 0.17f, 0.15f, 1, 0.10f, 0.13f, 0.16f, 1);
        vgrad(0, 300, VW, VH, 0.10f, 0.13f, 0.16f, 1, 0.02f, 0.03f, 0.06f, 1);
        return;
    }
    vgrad(0, 0, VW, 300, 0.085f, 0.20f, 0.25f, 1, 0.085f, 0.20f, 0.25f, 1);
    vgrad(0, 300, VW, VH, 0.085f, 0.20f, 0.25f, 1, 0.012f, 0.028f, 0.085f, 1);
}

void drawStars()
{
    for (int i = 0; i < 110; i++) {
        float sx = hash01(i, 201) * VW;
        float sy = 320 + hash01(i, 202) * 380;
        float tw = 0.5f + 0.5f * sinf(gt * (0.8f + hash01(i, 203) * 2.0f) + i);
        glPointSize(1.0f + hash01(i, 204) * 1.6f);
        glBegin(GL_POINTS);
        glColor4f(0.8f, 0.92f, 1.0f, (0.25f + 0.6f * tw * hash01(i, 205)) * LK().starMul);
        glVertex2f(sx, sy);
        glEnd();
    }
}

void drawMoonDisc()
{
    float my = moonY(), mr = moonR(), s = mr / 36.0f;
    if (difficulty == 1)      glColor3f(1.0f, 0.55f, 0.38f);       // blood moon
    else if (difficulty == 2) glColor3f(0.80f, 0.93f, 0.62f);      // sickly green moon
    else                      glColor3f(0.93f, 0.96f, 0.88f);
    circleFill(790, my, mr, 40);
    if (difficulty == 1)      glColor4f(0.75f, 0.30f, 0.25f, 0.40f);
    else if (difficulty == 2) glColor4f(0.50f, 0.65f, 0.40f, 0.40f);
    else                      glColor4f(0.70f, 0.78f, 0.70f, 0.35f);
    circleFill(790 - 12 * s, my + 8 * s, 8 * s, 14);
    circleFill(790 + 12 * s, my - 12 * s, 6 * s, 12);
    circleFill(790 + 6 * s, my + 14 * s, 4 * s, 10);
}

// HARD : dark storm clouds drifting over the moon
void drawStormClouds()
{
    if (difficulty != 2) return;
    normalBlend();
    for (int i = 0; i < 8; i++) {
        float x = wrapf(i * 190.0f + gt * (5.0f + (i % 3) * 3.0f), VW + 600) - 300;
        float y = 520 + (i * 47) % 160;
        glowEllipse(x, y, 230, 36, 0.03f, 0.04f, 0.06f, 0.60f);
        glowEllipse(x + 40, y + 16, 140, 20, 0.05f, 0.07f, 0.09f, 0.45f);
    }
}

// ------------------------------------------------------------
//  Fog
// ------------------------------------------------------------
void fogBand(float yLow, float yHigh, float a)
{
    const Look &k = LK();
    vgrad(0, yLow, VW, yHigh, k.fog[0], k.fog[1], k.fog[2], a, k.fog[0], k.fog[1], k.fog[2], 0.0f);
}

void drawFogBlobs()
{
    const Look &k = LK();
    normalBlend();
    for (int i = 0; i < 12; i++) {
        float sp = 6.0f + (i % 4) * 3.0f;
        float par = 0.5f + 0.1f * (i % 5);
        float x = wrapf(i * 210.0f + gt * sp - camX * par, VW + 700) - 350;
        float y = GY + 10 + ((i * 53) % 150);
        float rx = 260.0f + (i % 3) * 80.0f, ry = 34.0f + (i % 4) * 10.0f;
        glowEllipse(x, y, rx, ry, k.blob[0], k.blob[1], k.blob[2], 0.075f);
    }
}

// slanted beams of moonlight (drawn with additive blending)
void drawGodRays(float strength)
{
    const Look &lk = LK();
    additiveBlend();
    for (int k = 0; k < 6; k++) {
        float tx = 640 + k * 60;
        float bx = tx - 430;
        float a = (0.05f + 0.03f * sinf(gt * 0.5f + k * 1.3f)) * strength;
        glBegin(GL_QUADS);
        glColor4f(lk.ray[0], lk.ray[1], lk.ray[2], a);
        glVertex2f(tx - 12, VH); glVertex2f(tx + 12, VH);
        glColor4f(lk.ray[0], lk.ray[1], lk.ray[2], 0.0f);
        glVertex2f(bx + 45, GY); glVertex2f(bx - 45, GY);
        glEnd();
    }
    normalBlend();
}

// ------------------------------------------------------------
//  Trees
// ------------------------------------------------------------
float hillOffset(float wx, float amp, float f1, float f2, float salt)
{
    return amp * sinf(wx * f1 + salt) + amp * 0.5f * sinf(wx * f2 + salt * 2.3f);
}

void drawPine(float x, float by, float h, float w, float sway, float r, float g, float b, bool rim)
{
    // trunk
    float tw = w * 0.09f;
    glBegin(GL_QUADS);
    glColor3f(r * 1.3f + 0.01f, g * 0.9f, b * 0.8f);
    glVertex2f(x - tw, by - 8); glVertex2f(x + tw, by - 8);
    glVertex2f(x + tw * 0.6f, by + h * 0.28f); glVertex2f(x - tw * 0.6f, by + h * 0.28f);
    glEnd();

    // six tiers of needles, small jagged edge at the bottom of each tier
    for (int k = 0; k < 6; k++) {
        float yb = by + h * (0.10f + 0.135f * k);
        float yt = yb + h * 0.24f;
        float hw = w * (1.0f - 0.15f * k);
        float sx = sway * (0.3f + 0.25f * k);
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(r * 1.9f, g * 1.9f, b * 1.9f);
        glVertex2f(x + sx, yt);
        glColor3f(r, g, b);
        glVertex2f(x - hw, yb);
        glVertex2f(x - hw * 0.5f, yb + h * 0.018f);
        glVertex2f(x, yb - h * 0.012f);
        glVertex2f(x + hw * 0.5f, yb + h * 0.018f);
        glVertex2f(x + hw, yb);
        glEnd();
    }
    // moonlight on the right edge
    if (rim) {
        additiveBlend();
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        for (int k = 0; k < 6; k++) {
            float yb = by + h * (0.10f + 0.135f * k);
            float yt = yb + h * 0.24f;
            float hw = w * (1.0f - 0.15f * k);
            float sx = sway * (0.3f + 0.25f * k);
            glColor4f(0.35f, 0.62f, 0.68f, 0.40f); glVertex2f(x + sx, yt);
            glColor4f(0.35f, 0.62f, 0.68f, 0.05f); glVertex2f(x + hw, yb);
        }
        glEnd();
        glLineWidth(1);
        normalBlend();
    }
}

void drawBroad(float x, float by, float h, float w, float sway, float seed,
               float r, float g, float b, bool rim)
{
    float tw = w * 0.10f;
    glBegin(GL_QUADS);
    glColor3f(r * 1.4f + 0.01f, g, b * 0.9f);
    glVertex2f(x - tw, by - 8); glVertex2f(x + tw, by - 8);
    glColor3f(r * 1.2f, g * 0.9f, b * 0.85f);
    glVertex2f(x + tw * 0.6f, by + h * 0.62f); glVertex2f(x - tw * 0.6f, by + h * 0.62f);
    glEnd();

    glLineWidth(2);
    glColor3f(r * 1.2f, g * 0.9f, b * 0.85f);
    glBegin(GL_LINES);
    glVertex2f(x, by + h * 0.35f); glVertex2f(x - w * 0.38f, by + h * 0.62f);
    glVertex2f(x, by + h * 0.42f); glVertex2f(x + w * 0.40f, by + h * 0.66f);
    glEnd();
    glLineWidth(1);

    for (int k = 0; k < 8; k++) {
        float cx = x + sinf(k * 2.3f + seed) * w * 0.52f + sway * 0.5f;
        float cy = by + h * (0.66f + 0.09f * (k % 3)) + cosf(k * 1.9f + seed) * h * 0.04f;
        float rad = w * (0.34f + 0.07f * (k % 3));
        float m = 0.85f + 0.10f * (k % 4);
        glColor3f(r * m, g * m * 1.1f, b * m);
        circleFill(cx, cy, rad, 14);
    }
    if (rim) {
        additiveBlend();
        for (int k = 0; k < 8; k++) {
            float cx = x + sinf(k * 2.3f + seed) * w * 0.52f + sway * 0.5f;
            float cy = by + h * (0.66f + 0.09f * (k % 3)) + cosf(k * 1.9f + seed) * h * 0.04f;
            float rad = w * (0.34f + 0.07f * (k % 3));
            glColor4f(0.30f, 0.55f, 0.60f, 0.10f);
            circleFill(cx + rad * 0.35f, cy + rad * 0.35f, rad * 0.55f, 10);
        }
        normalBlend();
    }
}

// one far-away layer: rolling hill + trees standing on it (parallax scrolling)
void drawLayer(int id, float par, float spacing, float hbase, float hamp,
               float hMin, float hMax, float wMin, float wMax,
               float r, float g, float b, float broad, bool rim)
{
    tintC(r, g, b);                               // level colour (no change on easy)
    float off = camX * par;
    float f1 = 0.0045f + id * 0.0008f, f2 = 0.011f + id * 0.0015f, salt = id * 1.7f;

    // hill (quad strip)
    glBegin(GL_QUAD_STRIP);
    for (float x = 0; x <= VW + 16; x += 16) {
        float y = hbase + hillOffset(x + off, hamp, f1, f2, salt);
        glColor3f(r * 0.9f, g * 0.9f, b * 0.9f); glVertex2f(x, 0);
        glColor3f(r * 1.15f, g * 1.15f, b * 1.15f); glVertex2f(x, y);
    }
    glEnd();

    // trees
    int i0 = (int)floorf((off - 250) / spacing), i1 = (int)floorf((off + VW + 250) / spacing);
    for (int i = i0; i <= i1; i++) {
        float wx = i * spacing + (hash01(i, id * 13 + 3) - 0.5f) * spacing * 0.9f;
        float x = wx - off;
        float h = hMin + (hMax - hMin) * hash01(i, id * 13 + 1);
        float w = wMin + (wMax - wMin) * hash01(i, id * 13 + 2);
        float by = hbase + hillOffset(wx, hamp, f1, f2, salt);
        float sway = sinf(gt * 0.7f + i * 1.7f) * (2.0f + par * 5.0f);
        float k = 0.9f + 0.2f * hash01(i, id * 13 + 5);
        if (hash01(i, id * 13 + 4) < broad)
            drawBroad(x, by, h * 0.8f, w * 1.5f, sway, (float)i, r * k, g * k, b * k, rim);
        else
            drawPine(x, by, h, w, sway, r * k, g * k, b * k, rim);
    }
}

// ------------------------------------------------------------
//  Ground, grass, mushrooms, trunks
// ------------------------------------------------------------
void drawGround()
{
    float gr = 0.09f, gg = 0.21f, gb = 0.16f;
    tintC(gr, gg, gb);
    vgrad(0, 0, VW, GY, 0.02f, 0.05f, 0.05f, 1, gr, gg, gb, 1);
    // moonlit edge of the path
    vgrad(0, GY - 5, VW, GY, 0.35f, 0.58f, 0.52f, 0.0f, 0.35f, 0.58f, 0.52f, 0.35f);
    // a few darker soil patches
    for (int i = (int)floorf(camX / 300) - 1; i <= (int)floorf((camX + VW) / 300) + 1; i++) {
        float x = i * 300 + hash01(i, 301) * 200 - camX;
        glowEllipse(x, GY * (0.25f + 0.5f * hash01(i, 302)), 120, 18, 0.0f, 0.02f, 0.02f, 0.35f);
    }
}

void drawGrass(float par, float spacing, float hMin, float hMax, float baseLow, float baseHigh,
               int salt, float r1, float g1, float b1, float r2, float g2, float b2, float a)
{
    float off = camX * par;
    int i0 = (int)floorf((off - 60) / spacing), i1 = (int)floorf((off + VW + 60) / spacing);
    for (int i = i0; i <= i1; i++) {
        float x0 = i * spacing + hash01(i, salt) * spacing - off;
        for (int j = 0; j < 3; j++) {
            int id = i * 3 + j;
            float bx = x0 + (j - 1) * 4.0f;
            if (par == 1.0f && inStreamX(bx + off, 270.0f)) continue;      // no grass in the stream
            float by = baseLow + (baseHigh - baseLow) * hash01(id, salt + 1);
            float h = hMin + (hMax - hMin) * hash01(id, salt + 2);
            float lean = (hash01(id, salt + 3) - 0.5f) * 10.0f + sinf(gt * 1.7f + i * 0.6f + j) * (3.0f + h * 0.05f);
            glBegin(GL_TRIANGLES);
            glColor4f(r1, g1, b1, a); glVertex2f(bx - 2.5f, by); glVertex2f(bx + 2.5f, by);
            glColor4f(r2, g2, b2, a); glVertex2f(bx + lean, by + h);
            glEnd();
        }
    }
}

// pass 0 = the little mushrooms, pass 1 = their glow (drawn on top of the darkness)
void drawMushrooms(int pass)
{
    int i0 = (int)floorf(camX / 210) - 1, i1 = (int)floorf((camX + VW) / 210) + 1;
    for (int i = i0; i <= i1; i++) {
        if (hash01(i, 401) < 0.35f) continue;
        float x = i * 210 + hash01(i, 402) * 160 - camX;
        if (inStreamX(x + camX, 270.0f)) continue;                        // none in the stream
        if (difficulty != 0 && ordIsPicked(i)) continue;                  // already picked (medium / hard)
        float y = GY - 6 - hash01(i, 403) * 34;
        float sz = 5 + hash01(i, 404) * 4;
        bool teal = hash01(i, 405) < 0.6f;
        float cr = teal ? 0.30f : 0.90f, cg = teal ? 0.80f : 0.50f, cb = teal ? 0.90f : 0.80f;
        if (pass == 0) {
            glColor3f(0.75f, 0.8f, 0.75f);
            glBegin(GL_QUADS);
            glVertex2f(x - 1.5f, y); glVertex2f(x + 1.5f, y);
            glVertex2f(x + 1.5f, y + sz); glVertex2f(x - 1.5f, y + sz);
            glEnd();
            glColor3f(cr * 0.7f, cg * 0.7f, cb * 0.7f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x, y + sz);
            for (int k = 0; k <= 10; k++) {
                float a = PI * k / 10;
                glVertex2f(x + cosf(a) * sz * 1.1f, y + sz + sinf(a) * sz * 0.8f);
            }
            glEnd();
        } else {
            float b = 0.6f + 0.4f * sinf(gt * 1.3f + i);
            glow(x, y + sz, sz * 4.5f, cr, cg, cb, 0.30f * b);
        }
    }
}

// tall tree trunks that stand in front of the scenery (parallax 1.0 behind the traveler,
// or > 1.0 in front of everything)
void drawTrunks(float par, float spacing, float minW, float maxW, float baseY, int salt, float alpha)
{
    float off = camX * par;
    int i0 = (int)floorf((off - 200) / spacing), i1 = (int)floorf((off + VW + 200) / spacing);
    for (int i = i0; i <= i1; i++) {
        if (hash01(i, salt + 2) < 0.30f) continue;
        float x = i * spacing + hash01(i, salt + 1) * spacing * 0.8f - off;
        if (par == 1.0f && inStreamX(x + off, 300.0f)) continue;          // keep the stream free of trunks
        float w = minW + (maxW - minW) * hash01(i, salt + 3);
        // trunk with a moonlit right side
        glBegin(GL_QUADS);
        glColor4f(0.006f, 0.02f, 0.022f, alpha); glVertex2f(x - w * 0.5f, baseY);
        glColor4f(0.035f, 0.075f, 0.08f, alpha);  glVertex2f(x + w * 0.5f, baseY);
        glColor4f(0.035f, 0.075f, 0.08f, alpha);  glVertex2f(x + w * 0.38f, VH + 20);
        glColor4f(0.006f, 0.02f, 0.022f, alpha); glVertex2f(x - w * 0.38f, VH + 20);
        glEnd();
        // roots
        glBegin(GL_TRIANGLES);
        glColor4f(0.006f, 0.02f, 0.022f, alpha);
        glVertex2f(x - w * 0.5f, baseY + 30); glVertex2f(x - w * 1.0f, baseY - 6); glVertex2f(x - w * 0.3f, baseY - 6);
        glVertex2f(x + w * 0.5f, baseY + 30); glVertex2f(x + w * 1.0f, baseY - 6); glVertex2f(x + w * 0.3f, baseY - 6);
        glEnd();
        // bark lines
        glLineWidth(1);
        glColor4f(0.0f, 0.0f, 0.0f, 0.35f * alpha);
        glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) {
            float bx = x - w * 0.3f + k * w * 0.2f;
            glVertex2f(bx, baseY + 20); glVertex2f(bx + hash01(i * 7 + k, salt + 4) * 6 - 3, VH);
        }
        glEnd();
    }
}

// ------------------------------------------------------------
//  Darkness with a soft hole of light around the lantern
// ------------------------------------------------------------
void ring(float cx, float cy, float ra, float aa, float rb, float ab)
{
    const Look &k = LK();
    const int N = 64;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float a = 2 * PI * i / N, c = cosf(a), s = sinf(a);
        glColor4f(k.dk[0], k.dk[1], k.dk[2], aa); glVertex2f(cx + c * ra, cy + s * ra);
        glColor4f(k.dk[0], k.dk[1], k.dk[2], ab); glVertex2f(cx + c * rb, cy + s * rb);
    }
    glEnd();
}

void drawDarkness(float cx, float cy, float R, float dark, bool hole)
{
    const Look &k = LK();
    normalBlend();
    if (!hole) {
        glColor4f(k.dk[0], k.dk[1], k.dk[2], dark);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(VW, 0); glVertex2f(VW, VH); glVertex2f(0, VH);
        glEnd();
        return;
    }
    float r0 = R * 0.28f, rm = R * 0.65f;
    ring(cx, cy, r0, 0.0f, rm, dark * 0.30f);
    ring(cx, cy, rm, dark * 0.30f, R, dark);
    ring(cx, cy, R, dark, 5000, dark);
}

void drawVignette()
{
    normalBlend();
    vgrad(0, 0, VW, 150, 0, 0, 0.02f, 0.55f, 0, 0, 0.02f, 0.0f);
    vgrad(0, VH - 150, VW, VH, 0, 0, 0.02f, 0.0f, 0, 0, 0.02f, 0.55f);
    hgrad(0, 0, 200, VH, 0, 0, 0.02f, 0.55f, 0, 0, 0.02f, 0.0f);
    hgrad(VW - 200, 0, VW, VH, 0, 0, 0.02f, 0.0f, 0, 0, 0.02f, 0.55f);
}

// ------------------------------------------------------------
//  Floating particles (dust, pollen, falling leaves)
// ------------------------------------------------------------
void drawLeaves()
{
    normalBlend();
    for (int i = 0; i < 22; i++) {
        const Leaf &l = leaves[i];
        float g = 0.25f + 0.25f * l.hue;
        leafShape(l.x, l.y, l.rot, l.sz, 0.12f + 0.3f * l.hue, g, 0.10f, 0.75f);
    }
}

void drawMotes()
{
    additiveBlend();
    for (int i = 0; i < 70; i++) {
        const Mote &m = motes[i];
        float b = 0.5f + 0.5f * sinf(gt * 1.5f + m.ph);
        glow(m.x, m.y, m.sz * 3.0f, 0.75f, 0.95f, 0.85f, 0.10f + 0.20f * b);
    }
    normalBlend();
}

// thin speed lines used while the traveler is running home
void drawSpeedLines(float speed)
{
    float s = clampf((speed - 250.0f) / 400.0f, 0, 1);
    if (s <= 0) return;
    additiveBlend();
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    for (int i = 0; i < 24; i++) {
        float y = 170 + hash01(i, 501) * 420;
        float len = (60 + hash01(i, 502) * 160) * s;
        float x = wrapf(i * 97.3f - camX * 1.8f, VW + 300) - 150;
        glColor4f(0.9f, 1.0f, 0.9f, 0.0f);  glVertex2f(x + len, y);
        glColor4f(0.9f, 1.0f, 0.9f, 0.16f * s); glVertex2f(x, y);
    }
    glEnd();
    glLineWidth(1);
    normalBlend();
}

// ------------------------------------------------------------
//  Water : a tiny creek far in the background, and the stream the
//  traveler hops across (on stepping stones) just before the cabin
// ------------------------------------------------------------
const float STREAM_HW  = 170.0f;                  // half width of the stream at the traveler's feet line
const float STREAM_TOP = 196.0f;                  // where the stream fades away into the background
const float STONE_GAP  = 85.0f;                   // distance between the stepping stones
const float STONE_TOP  = 12.0f;                   // height of a stepping stone
const float STONE_PEAK = 38.0f;                   // how high he leaps between the stones

const int   NROCK = 8;                            // 3 stepping stones + 5 rocks in the water
const float rockDX[NROCK] = { -STONE_GAP, 0.0f, STONE_GAP, -150.0f, 120.0f, -30.0f, 190.0f, -210.0f };
const float rockYY[NROCK] = { GY - 2.0f, GY - 2.0f, GY - 2.0f, 100.0f, 72.0f, 38.0f, 20.0f, 55.0f };
const float rockRX[NROCK] = { 31.0f, 31.0f, 31.0f, 20.0f, 24.0f, 30.0f, 20.0f, 16.0f };
const float rockRY[NROCK] = { 14.0f, 14.0f, 14.0f, 8.0f, 10.0f, 11.0f, 8.0f, 7.0f };

// half width of the stream at screen height y (perspective: narrow far away, wide close up)
float streamHW(float y) { return STREAM_HW * (420.0f - y) / 270.0f + 7.0f * sinf((y - GY) * 0.05f) + 4.0f * sinf((y - GY) * 0.13f); }

// while crossing the stream the traveler skips from stone to stone
void streamHop()
{
    float rel = px - streamCX;
    if (rel <= -STREAM_HW || rel >= STREAM_HW) return;
    float pts[5] = { -STREAM_HW, -STONE_GAP, 0.0f, STONE_GAP, STREAM_HW };
    float hts[5] = { 0.0f, STONE_TOP, STONE_TOP, STONE_TOP, 0.0f };
    int i = 0;
    while (i < 3 && rel >= pts[i + 1]) i++;
    float t = (rel - pts[i]) / (pts[i + 1] - pts[i]);
    pz = hts[i] + (hts[i + 1] - hts[i]) * t + STONE_PEAK * 4.0f * t * (1.0f - t);
    vzv = 0;
}

// same hop height as above but for any world x (used by the children following the traveler)
float stoneHeightAt(float wx)
{
    float rel = wx - streamCX;
    if (state != 2 || rel <= -STREAM_HW || rel >= STREAM_HW) return 0.0f;
    float pts[5] = { -STREAM_HW, -STONE_GAP, 0.0f, STONE_GAP, STREAM_HW };
    float hts[5] = { 0.0f, STONE_TOP, STONE_TOP, STONE_TOP, 0.0f };
    int i = 0;
    while (i < 3 && rel >= pts[i + 1]) i++;
    float t = (rel - pts[i]) / (pts[i + 1] - pts[i]);
    return hts[i] + (hts[i + 1] - hts[i]) * t + STONE_PEAK * 4.0f * t * (1.0f - t);
}

void rock(float x, float y, float rx, float ry)
{
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(0.22f, 0.27f, 0.28f);
    glVertex2f(x + rx * 0.25f, y + ry * 0.45f);
    glColor3f(0.04f, 0.06f, 0.07f);
    for (int i = 0; i <= 24; i++) {
        float a = 2 * PI * i / 24;
        glVertex2f(x + cosf(a) * rx, y + sinf(a) * ry);
    }
    glEnd();
}

// the little creek far in the background (it winds along the foot of the nearest hills and flows very slowly)
float creekY(float wx) { return 160.0f + 4.0f * sinf(wx * 0.011f + 1.3f) + 2.0f * sinf(wx * 0.027f); }

void drawFarCreek()
{
    float off = camX * 0.62f;
    int i0 = (int)floorf((off - 100) / 520.0f), i1 = (int)floorf((off + VW + 100) / 520.0f);
    for (int i = i0; i <= i1; i++) {
        if (hash01(i, 1101) < 0.25f) continue;                  // some stretches hide away
        float x0 = i * 520.0f + hash01(i, 1102) * 100.0f - off;
        float len = 360.0f + hash01(i, 1103) * 160.0f;
        const int NS = 24;
        normalBlend();
        glBegin(GL_QUAD_STRIP);
        for (int k = 0; k <= NS; k++) {
            float t = (float)k / NS;
            float x = x0 + t * len;
            float yc = creekY(x + off);
            float th = 2.5f + 3.0f * sinf(t * PI);
            float a = 0.85f * sinf(t * PI);
            glColor4f(0.30f, 0.52f, 0.60f, a); glVertex2f(x, yc + th);
            glColor4f(0.10f, 0.22f, 0.30f, a); glVertex2f(x, yc - th);
        }
        glEnd();
        additiveBlend();                                        // slow moonlit glints drifting along the creek
        for (int g = 0; g < 6; g++) {
            float gu = wrapf(hash01(i * 11 + g, 1104) + gt * (0.012f + 0.004f * g), 1.0f);
            float gx = x0 + gu * len;
            float gy = creekY(gx + off);
            float tw = 0.5f + 0.5f * sinf(gt * 2.0f + g * 1.9f + i);
            float ga = 0.80f * tw * sinf(gu * PI);
            glBegin(GL_LINE_STRIP);
            glColor4f(0.8f, 0.95f, 1.0f, 0.0f); glVertex2f(gx - 7, gy);
            glColor4f(0.8f, 0.95f, 1.0f, ga);   glVertex2f(gx, gy);
            glColor4f(0.8f, 0.95f, 1.0f, 0.0f); glVertex2f(gx + 7, gy);
            glEnd();
        }
        normalBlend();
    }
}

// the stream that crosses the path near the cabin (only exists in the ending)
// pass 0 = water, banks, rocks (drawn before the darkness), pass 1 = moonlight and sparkle on top of it
void drawStream(int pass)
{
    if (state != 2) return;
    float cx = streamCX - camX;
    if (cx < -420 || cx > VW + 420) return;
    const int NR = 28;

    if (pass == 0) {
        // dark soil along both banks
        for (int side = -1; side <= 1; side += 2) {
            glBegin(GL_QUAD_STRIP);
            for (int k = 0; k <= NR; k++) {
                float y = STREAM_TOP - (STREAM_TOP + 6.0f) * k / NR;
                float hw = streamHW(y);
                float f = clampf((STREAM_TOP - y) / 40.0f, 0.0f, 1.0f);
                glColor4f(0.0f, 0.02f, 0.02f, 0.55f * f); glVertex2f(cx + side * hw, y);
                glColor4f(0.0f, 0.02f, 0.02f, 0.0f);      glVertex2f(cx + side * (hw + 28.0f), y);
            }
            glEnd();
        }
        // dark water that fades into the mist far away
        glBegin(GL_QUAD_STRIP);
        for (int k = 0; k <= NR; k++) {
            float y = STREAM_TOP - (STREAM_TOP + 6.0f) * k / NR;
            float hw = streamHW(y);
            float f = clampf((STREAM_TOP - y) / 40.0f, 0.0f, 1.0f);
            float t = clampf((STREAM_TOP - y) / STREAM_TOP, 0.0f, 1.0f);       // 0 = far away, 1 = close
            glColor4f(0.12f + (0.015f - 0.12f) * t, 0.25f + (0.05f - 0.25f) * t, 0.34f + (0.11f - 0.34f) * t, f);
            glVertex2f(cx - hw, y); glVertex2f(cx + hw, y);
        }
        glEnd();
        // rocks (the first three are the stepping stones), far to near
        for (int i = 0; i < NROCK; i++) {
            float rx = cx + rockDX[i], ry = rockYY[i];
            glowEllipse(rx, ry - 6, rockRX[i] * 1.4f, rockRY[i] * 0.8f, 0.0f, 0.02f, 0.03f, 0.45f);
            rock(rx, ry, rockRX[i], rockRY[i]);
        }
        // reeds along the banks
        for (int side = -1; side <= 1; side += 2)
            for (int k = 0; k < 9; k++) {
                int id = k * 2 + (side > 0 ? 1 : 0);
                float y = 6.0f + k * 17.0f + hash01(id, 1201) * 10.0f;
                float bx = cx + side * (streamHW(y) + 2.0f + hash01(id, 1202) * 8.0f);
                float h = 16.0f + hash01(id, 1203) * 20.0f;
                for (int b = 0; b < 3; b++) {
                    float lean = (b - 1) * 5.0f + side * 3.0f + sinf(gt * 1.6f + id + b) * 3.0f;
                    glBegin(GL_TRIANGLES);
                    glColor3f(0.03f, 0.12f, 0.08f);
                    glVertex2f(bx + (b - 1) * 3.0f - 2.0f, y); glVertex2f(bx + (b - 1) * 3.0f + 2.0f, y);
                    glColor3f(0.10f, 0.34f, 0.22f);
                    glVertex2f(bx + (b - 1) * 3.0f + lean, y + h * (0.8f + 0.1f * b));
                    glEnd();
                }
            }
        return;
    }

    // ----- pass 1 : light on the water -----
    additiveBlend();

    // the moon's reflection: a soft bright column of light around the moon's x position
    const int RJ = 12, CK = 14;
    for (int j = 0; j < RJ; j++) {
        float ya = STREAM_TOP - (STREAM_TOP + 6.0f) * j / RJ, yb = STREAM_TOP - (STREAM_TOP + 6.0f) * (j + 1) / RJ;
        float fa = clampf((STREAM_TOP - ya) / 40.0f, 0.0f, 1.0f), fb = clampf((STREAM_TOP - yb) / 40.0f, 0.0f, 1.0f);
        float ha = streamHW(ya), hb = streamHW(yb);
        glBegin(GL_QUAD_STRIP);
        for (int c = 0; c <= CK; c++) {
            float s = -1.0f + 2.0f * c / CK;
            float da = (cx + s * ha - 790.0f) / 210.0f, db = (cx + s * hb - 790.0f) / 210.0f;
            glColor4f(0.65f, 0.85f, 0.95f, 0.16f * expf(-da * da) * fa);
            glVertex2f(cx + s * ha, ya);
            glColor4f(0.65f, 0.85f, 0.95f, 0.16f * expf(-db * db) * fb);
            glVertex2f(cx + s * hb, yb);
        }
        glEnd();
    }

    // soft flowing lines: the water runs from the background toward the viewer
    glLineWidth(1);
    for (int n = 0; n < 9; n++) {
        float sN = -0.8f + 0.2f * n;
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 12; k++) {
            float y = 176.0f - 176.0f * k / 12.0f;
            float f = clampf((190.0f - y) / 30.0f, 0.0f, 1.0f);
            float x = cx + sN * streamHW(y) + 5.0f * sinf(y * 0.06f + gt * 1.2f + n * 2.0f);
            glColor4f(0.55f, 0.80f, 0.90f, 0.10f * f);
            glVertex2f(x, y);
        }
        glEnd();
    }

    // moonlit glints sliding toward the viewer
    glLineWidth(1.5f);
    for (int g = 0; g < 46; g++) {
        float u = wrapf(hash01(g, 1302) + gt * 0.06f, 1.0f);              // 0 = far away, 1 = close
        float y = 190.0f - u * 186.0f;
        float xr = (hash01(g, 1301) - 0.5f) * 1.8f;
        float gx = cx + xr * streamHW(y);
        float dm = (gx - 790.0f) / 230.0f;
        float w = 0.25f + 0.75f * expf(-dm * dm);
        float tw = 0.5f + 0.5f * sinf(gt * 3.0f + hash01(g, 1303) * 20.0f);
        float a = 0.60f * w * tw * sinf(u * PI);
        float len = 5.0f + 15.0f * u;
        glBegin(GL_LINE_STRIP);
        glColor4f(0.8f, 0.95f, 1.0f, 0.0f); glVertex2f(gx - len, y);
        glColor4f(0.8f, 0.95f, 1.0f, a);    glVertex2f(gx, y);
        glColor4f(0.8f, 0.95f, 1.0f, 0.0f); glVertex2f(gx + len, y);
        glEnd();
    }

    // ripples spreading around the rocks, and moonlight on their tops
    glLineWidth(1.5f);
    for (int i = 0; i < NROCK; i++) {
        float rx = cx + rockDX[i], ry = rockYY[i];
        float ph = wrapf(gt * 0.5f + i * 0.37f, 1.0f);
        float k = 1.15f + 0.5f * ph;
        glColor4f(0.65f, 0.88f, 0.95f, 0.38f * (1.0f - ph));
        glBegin(GL_LINE_LOOP);
        for (int s = 0; s < 28; s++) {
            float a = 2 * PI * s / 28;
            glVertex2f(rx + cosf(a) * rockRX[i] * k, ry - rockRY[i] * 0.55f + sinf(a) * rockRY[i] * 0.55f * k);
        }
        glEnd();
        glowEllipse(rx + rockRX[i] * 0.25f, ry + rockRY[i] * 0.55f, rockRX[i] * 0.8f, rockRY[i] * 0.5f, 0.6f, 0.8f, 0.9f, 0.20f);
    }
    glLineWidth(1);

    // wet moonlit edge along both banks
    glLineWidth(1.5f);
    for (int side = -1; side <= 1; side += 2) {
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= NR; k++) {
            float y = STREAM_TOP - (STREAM_TOP + 6.0f) * k / NR;
            float f = clampf((STREAM_TOP - y) / 40.0f, 0.0f, 1.0f);
            float x = cx + side * streamHW(y);
            float dm = (x - 790.0f) / 300.0f;
            glColor4f(0.60f, 0.82f, 0.92f, (0.10f + 0.22f * expf(-dm * dm)) * f);
            glVertex2f(x, y);
        }
        glEnd();
    }
    glLineWidth(1);

    // mist over the water
    glowEllipse(cx, 168.0f, 250.0f, 30.0f, 0.45f, 0.62f, 0.68f, 0.08f);

    // warm reflection of the lantern when he is above the water
    float lf = clampf((STREAM_HW - fabsf(lx - streamCX)) / 40.0f, 0.0f, 1.0f);
    if (lf > 0.0f) glowEllipse(lx - camX, GY - 36.0f, 20.0f, 60.0f, 1.0f, 0.82f, 0.40f, 0.30f * lf);

    normalBlend();
}

// ------------------------------------------------------------
//  Jar / lantern
// ------------------------------------------------------------
// jar with glowing fireflies inside (used for the lantern and for the HUD)
void drawJar(float cx, float cy, float s, int count, float frac, float glowMul)
{
    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glScalef(s, s, 1);

    // glass
    normalBlend();
    glColor4f(0.72f, 0.90f, 1.0f, 0.14f);
    glBegin(GL_POLYGON);
    glVertex2f(-18, -28); glVertex2f(18, -28); glVertex2f(22, -22); glVertex2f(22, 12);
    glVertex2f(14, 18);   glVertex2f(-14, 18); glVertex2f(-22, 12); glVertex2f(-22, -22);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-12, 18); glVertex2f(12, 18); glVertex2f(12, 26); glVertex2f(-12, 26);
    glEnd();

    // light inside
    additiveBlend();
    glow(0, -6, 34 + 26 * frac, 1.0f, 0.92f, 0.40f, (0.14f + 0.55f * frac) * glowMul);
    for (int i = 0; i < count && i < NEEDED; i++) {
        float x = jfx[i] + sinf(gt * 1.5f + i) * 2.5f;
        float y = jfy[i] + cosf(gt * 1.2f + i * 1.7f) * 2.5f;
        float b = 0.5f + 0.5f * sinf(gt * 3 + i * 2.1f);
        glow(x, y, 8, 0.85f, 1.0f, 0.35f, 0.35f + 0.55f * b);
        glColor4f(1.0f, 1.0f, 0.75f, 0.9f);
        circleFill(x, y, 1.5f, 8);
    }
    normalBlend();

    // outline
    glLineWidth(1.5f);
    glColor4f(0.88f, 0.96f, 1.0f, 0.80f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-18, -28); glVertex2f(18, -28); glVertex2f(22, -22); glVertex2f(22, 12);
    glVertex2f(14, 18);   glVertex2f(12, 26);  glVertex2f(-12, 26); glVertex2f(-14, 18);
    glVertex2f(-22, 12);  glVertex2f(-22, -22);
    glEnd();
    // lid
    glBegin(GL_QUADS);
    glColor3f(0.40f, 0.26f, 0.15f); glVertex2f(-14, 26); glVertex2f(14, 26);
    glColor3f(0.60f, 0.42f, 0.25f); glVertex2f(14, 33);  glVertex2f(-14, 33);
    glEnd();
    // handle
    glColor4f(0.75f, 0.60f, 0.35f, 0.9f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 10; i++) { float a = PI * i / 10; glVertex2f(cosf(a) * 10, 33 + sinf(a) * 9); }
    glEnd();
    glLineWidth(1);
    glPopMatrix();
}

// ------------------------------------------------------------
//  The net
// ------------------------------------------------------------
float handX() { return px + face * 12; }
float handY() { return GY + pz + 46; }

void getNet(float &hx, float &hy, float &ang, float &dist)
{
    if (swinging) {
        if (arcSwing)   // jumping: the net sweeps a 180 degree half circle, from behind, over the head, to the front
            ang = (face > 0 ? PI : 0.0f) + swingSgn * PI * (swingT / swingDur);
        else
            ang = swingAim + swingSgn * (swingT / swingDur * 2.0f - 1.0f) * 1.2f;
        dist = 72;
    } else {
        ang = (face > 0 ? 1.15f : PI - 1.15f) + sinf(gt * 3) * 0.06f;
        dist = 50;
    }
    hx = handX() + cosf(ang) * dist;
    hy = handY() + sinf(ang) * dist;
}

void drawNet()
{
    float hx, hy, a, d;
    getNet(hx, hy, a, d);
    float ox = handX(), oy = handY();
    float ux = cosf(a), uy = sinf(a);
    float dir = swingSgn;

    // front arm
    glLineWidth(5);
    glColor3f(0.70f, 0.26f, 0.20f);
    glBegin(GL_LINES);
    glVertex2f(px + face * 3, GY + pz + 52); glVertex2f(ox, oy);
    glEnd();

    // swing trail
    if (swinging) {
        additiveBlend();
        for (int k = 1; k <= 7; k++) {
            float ta = a - dir * k * 0.09f;
            glow(ox + cosf(ta) * d, oy + sinf(ta) * d, 22, 0.8f, 0.95f, 1.0f, 0.24f - k * 0.027f);
        }
        normalBlend();
    }
    // handle
    glLineWidth(4);
    glColor3f(0.55f, 0.38f, 0.22f);
    glBegin(GL_LINES);
    glVertex2f(ox, oy); glVertex2f(hx - ux * 18, hy - uy * 18);
    glEnd();
    // bag + ring + mesh
    glColor4f(0.9f, 0.95f, 1.0f, 0.18f);
    circleFill(hx, hy, 18);
    glLineWidth(2);
    glColor4f(0.95f, 0.95f, 1.0f, 0.9f);
    circleLine(hx, hy, 18);
    glLineWidth(1);
    glColor4f(0.95f, 0.95f, 1.0f, 0.45f);
    glBegin(GL_LINES);
    for (int k = 0; k < 3; k++) {
        float ma = a + k * PI / 3;
        glVertex2f(hx + cosf(ma) * 18, hy + sinf(ma) * 18);
        glVertex2f(hx - cosf(ma) * 18, hy - sinf(ma) * 18);
    }
    glEnd();
}

// ------------------------------------------------------------
//  The traveler
// ------------------------------------------------------------
void drawShadow()
{
    float k = clampf(1.0f - pz / 220.0f, 0.3f, 1.0f);
    normalBlend();
    glowEllipse(px, GY - 5, 26 * k, 7 * k, 0, 0, 0, 0.55f * k);
}

// ---- mushrooms carried in the basket on the traveler's back (positions inside the basket, local coordinates) ----
const float BM_X[8] = { -21.0f, -17.0f, -13.0f, -19.0f, -15.0f, -23.0f, -11.0f, -17.0f };
const float BM_Y[8] = { 51.0f, 53.0f, 51.0f, 56.0f, 56.0f, 49.0f, 49.0f, 59.0f };

int backLair() { return difficulty == 0 ? 0 : (lairGot < 8 ? lairGot : 8); }                       // special lair mushrooms shown
int backOrd()                                                                                       // ordinary mushrooms shown
{
    if (difficulty == 0) return 5;                 // easy : the basket is already full (story)
    int n = 8 - backLair();
    return ordCount < n ? ordCount : n;
}
void backMushColor(int k, int nL, float &r, float &g, float &b)
{
    if (k < nL) {                                  // lair mushroom : crimson-pink (medium) or pale green (hard)
        r = (difficulty == 1) ? 0.95f : 0.55f; g = (difficulty == 1) ? 0.20f : 1.00f; b = (difficulty == 1) ? 0.55f : 0.80f;
    } else if ((k - nL) % 2 == 0) { r = 0.30f; g = 0.80f; b = 0.90f; }   // ordinary teal
    else                          { r = 0.90f; g = 0.50f; b = 0.80f; }   // ordinary pink
}

void drawPlayer()
{
    float speed = fabsf(vx);
    bool air = pz > 0.5f;
    float mv = clampf(speed / 230.0f, 0, 1.4f);
    float bob = air ? 0.0f : fabsf(sinf(walkPh)) * 3.0f * clampf(mv, 0, 1);
    float flutter = sinf(gt * 9.0f);

    glPushMatrix();
    glTranslatef(px, GY + pz, 0);
    glScalef(face, 1, 1);
    glRotatef(-clampf(speed / 700.0f, 0, 1) * 14.0f, 0, 0, 1);
    if (bendT > 0) {                              // bends down a little to pick up a mushroom
        float bd = sinf(PI * (1.0f - bendT / 0.4f));
        glRotatef(-bd * 28.0f, 0, 0, 1);
        glScalef(1.0f, 1.0f - 0.07f * bd, 1);
    }
    if (state == 4 && deathType == 1)             // bitten : falls down
        glRotatef(smooth(deathT / 0.5f) * 85.0f, 0, 0, 1);

    // legs (back leg darker)
    for (int s = 0; s < 2; s++) {
        float a = air ? (s == 0 ? 0.9f : -0.5f) : sinf(walkPh + s * PI) * 0.75f * mv;
        float hx = (s == 0 ? -2.5f : 2.5f), hy = 24 + bob;
        float fx = hx + sinf(a) * 22, fy = hy - cosf(a) * 22 * (air ? 0.8f : 1.0f);
        glLineWidth(5);
        if (s == 0) glColor3f(0.10f, 0.08f, 0.14f); else glColor3f(0.17f, 0.14f, 0.22f);
        glBegin(GL_LINES); glVertex2f(hx, hy); glVertex2f(fx, fy); glEnd();
        glColor3f(0.28f, 0.18f, 0.12f);
        circleFill(fx + 2, fy + 1, 4.2f, 10);
    }

    // cloak tail flying behind
    float tail = 22 + speed * 0.09f + flutter * 2;
    glBegin(GL_TRIANGLES);
    glColor3f(0.40f, 0.12f, 0.12f);
    glVertex2f(-8, 52 + bob); glVertex2f(-8, 28 + bob);
    glColor3f(0.30f, 0.09f, 0.10f);
    glVertex2f(-8 - tail, 20 + bob + flutter * 3);
    glEnd();
    // cloak body
    glBegin(GL_QUADS);
    glColor3f(0.80f, 0.32f, 0.25f); glVertex2f(-8, 56 + bob); glVertex2f(9, 56 + bob);
    glColor3f(0.46f, 0.14f, 0.14f); glVertex2f(14, 17 + bob); glVertex2f(-14, 17 + bob);
    glEnd();
    // scarf
    float st1 = 18 + speed * 0.07f;
    glColor3f(0.95f, 0.78f, 0.30f);
    glBegin(GL_QUADS);
    glVertex2f(-8, 52 + bob); glVertex2f(9, 52 + bob); glVertex2f(9, 58 + bob); glVertex2f(-8, 58 + bob);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-8, 57 + bob); glVertex2f(-8, 52 + bob);
    glVertex2f(-8 - st1, 49 + bob + flutter * 3); glVertex2f(-8 - st1, 55 + bob + sinf(gt * 9 + 1) * 3);
    glEnd();
    // wicker basket on the back with the picked mushrooms (forest-core)
    {
        int nL = backLair(), nT = nL + backOrd();
        for (int k = 0; k < nT; k++) {             // mushrooms peeking out of the basket
            float mr, mg, mb;
            backMushColor(k, nL, mr, mg, mb);
            float mx = BM_X[k], my = BM_Y[k] + bob;
            glLineWidth(1.5f);
            glColor3f(0.85f, 0.82f, 0.78f);
            glBegin(GL_LINES); glVertex2f(mx, 40 + bob); glVertex2f(mx, my); glEnd();
            glColor3f(mr, mg, mb);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(mx, my);
            for (int i = 0; i <= 8; i++) { float a = PI * i / 8; glVertex2f(mx + cosf(a) * 3.6f, my + sinf(a) * 3.0f); }
            glEnd();
            if (k < nL) {                          // little white spots on the special ones
                glColor3f(1.0f, 1.0f, 1.0f);
                circleFill(mx - 1.0f, my + 1.2f, 0.7f, 6);
                circleFill(mx + 1.3f, my + 0.8f, 0.6f, 6);
            }
        }
        glBegin(GL_QUADS);                         // basket body
        glColor3f(0.30f, 0.19f, 0.10f); glVertex2f(-24, 22 + bob); glVertex2f(-10, 22 + bob);
        glColor3f(0.50f, 0.34f, 0.18f); glVertex2f(-7, 46 + bob);  glVertex2f(-27, 46 + bob);
        glEnd();
        glLineWidth(1);                            // wicker weave
        glColor4f(0.15f, 0.09f, 0.04f, 0.55f);
        glBegin(GL_LINES);
        for (int k = 0; k < 3; k++) { glVertex2f(-25.0f - k, 28 + k * 6.0f + bob); glVertex2f(-9.0f + k, 28 + k * 6.0f + bob); }
        for (int k = 0; k < 4; k++) { glVertex2f(-22.0f + k * 4.0f, 22 + bob); glVertex2f(-24.0f + k * 5.0f, 46 + bob); }
        glEnd();
        glLineWidth(2.5f);                         // rim
        glColor3f(0.58f, 0.40f, 0.20f);
        glBegin(GL_LINES); glVertex2f(-27, 46 + bob); glVertex2f(-7, 46 + bob); glEnd();
        glLineWidth(2);                            // strap over the shoulder
        glColor3f(0.40f, 0.26f, 0.14f);
        glBegin(GL_LINES); glVertex2f(-9, 44 + bob); glVertex2f(0, 56 + bob); glEnd();
        glLineWidth(1);
        leafShape(-27, 44 + bob, 2.6f, 9, 0.30f, 0.55f, 0.25f, 0.95f);    // leaves tucked into the rim
        leafShape(-7, 46 + bob, 0.5f, 8, 0.40f, 0.62f, 0.28f, 0.95f);
        glColor3f(0.20f, 0.40f, 0.18f);            // moss
        circleFill(-22, 25 + bob, 2.2f, 8);
        circleFill(-13, 24 + bob, 1.6f, 8);
    }
    // back arm holding the lantern
    glLineWidth(5);
    glColor3f(0.55f, 0.18f, 0.16f);
    glBegin(GL_LINES); glVertex2f(-4, 52 + bob); glVertex2f(-13, 38 + bob); glEnd();
    // head
    glColor3f(0.96f, 0.80f, 0.64f);
    circleFill(1, 66 + bob, 9, 16);
    glColor3f(0.12f, 0.08f, 0.08f);
    circleFill(5, 67 + bob, 1.4f, 6);
    // hat
    glBegin(GL_QUADS);
    glColor3f(0.20f, 0.22f, 0.46f);
    glVertex2f(-15, 72 + bob); glVertex2f(15, 72 + bob);
    glVertex2f(12, 76 + bob); glVertex2f(-12, 76 + bob);
    glEnd();
    glBegin(GL_TRIANGLES);
    glColor3f(0.20f, 0.22f, 0.46f); glVertex2f(-10, 75 + bob); glVertex2f(10, 75 + bob);
    glColor3f(0.34f, 0.36f, 0.68f); glVertex2f(5 - flutter * 2 - speed * 0.02f, 99 + bob);
    glEnd();
    glColor3f(0.92f, 0.78f, 0.35f);
    glBegin(GL_QUADS);
    glVertex2f(-10, 75 + bob); glVertex2f(10, 75 + bob); glVertex2f(10, 78 + bob); glVertex2f(-10, 78 + bob);
    glEnd();
    glLineWidth(1);
    glPopMatrix();
}

// soft glow of the mushrooms carried on the back (drawn on top of the darkness, world coordinates)
void drawBackMushGlow()
{
    if (state == 4 && deathType == 1) return;
    int nL = backLair(), nT = nL + backOrd();
    additiveBlend();
    for (int k = 0; k < nT; k++) {
        float mr, mg, mb;
        backMushColor(k, nL, mr, mg, mb);
        float b = 0.7f + 0.3f * sinf(gt * 2.0f + k * 1.3f);
        glow(px + face * BM_X[k], GY + pz + BM_Y[k] + 2.0f, (k < nL) ? 13.0f : 9.0f, mr, mg, mb, ((k < nL) ? 0.40f : 0.22f) * b);
    }
    normalBlend();
}

// lantern jar hanging from the traveler's back hand
void drawLantern(float frac)
{
    float pivX = px - face * 13, pivY = GY + pz + 38;
    glLineWidth(1.5f);
    glColor4f(0.7f, 0.6f, 0.4f, 0.9f);
    glBegin(GL_LINES); glVertex2f(pivX, pivY); glVertex2f(lx, ly + 15); glEnd();
    glLineWidth(1);
    drawJar(lx, ly, 0.45f, caught, frac, 1.0f);
}

// ------------------------------------------------------------
//  Fireflies
// ------------------------------------------------------------
void drawFirefly(float x, float y, float phase, float freq)
{
    float b = 0.5f + 0.5f * sinf(gt * freq * 2.0f + phase);
    b = b * b;
    glow(x, y, 30 + 26 * b, 0.75f, 1.0f, 0.30f, 0.20f + 0.45f * b);
    glColor4f(1.0f, 1.0f, 0.75f, 0.55f + 0.45f * b);
    circleFill(x, y, 2.8f, 10);
    float flap = sinf(gt * 40 + phase) * 1.5f;
    glColor4f(0.9f, 1.0f, 1.0f, 0.25f);
    circleFill(x - 3, y + 3 + flap, 2.5f, 8);
    circleFill(x + 3, y + 3 + flap, 2.5f, 8);
}

void drawFlying()
{
    additiveBlend();
    for (size_t i = 0; i < flying.size(); i++) {
        float t = flying[i].t;
        float u = t * t * (3 - 2 * t);
        float x = flying[i].sx + (lx - flying[i].sx) * u;
        float y = flying[i].sy + (ly + 4 - flying[i].sy) * u + sinf(u * PI) * 60;
        glow(x, y, 26, 0.8f, 1.0f, 0.3f, 0.85f);
        glColor4f(1, 1, 0.8f, 1);
        circleFill(x, y, 3, 8);
    }
    normalBlend();
}

// ------------------------------------------------------------
//  The little cabin (home) shown at the end
// ------------------------------------------------------------
void drawCabin(int pass)
{
    float x = cabinX;
    if (x < camX - 400 || x > camX + VW + 400) return;
    if (pass == 0) {
        // chimney, walls, roof
        glColor3f(0.10f, 0.08f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2f(x + 50, GY + 120); glVertex2f(x + 76, GY + 120);
        glVertex2f(x + 76, GY + 215); glVertex2f(x + 50, GY + 215);
        glEnd();
        glBegin(GL_QUADS);
        glColor3f(0.17f, 0.11f, 0.08f); glVertex2f(x - 110, GY - 8); glVertex2f(x + 110, GY - 8);
        glColor3f(0.24f, 0.16f, 0.11f); glVertex2f(x + 110, GY + 125); glVertex2f(x - 110, GY + 125);
        glEnd();
        // log lines
        glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
        glLineWidth(2);
        glBegin(GL_LINES);
        for (int k = 1; k < 6; k++) { glVertex2f(x - 110, GY - 8 + k * 22); glVertex2f(x + 110, GY - 8 + k * 22); }
        glEnd();
        glLineWidth(1);
        glBegin(GL_TRIANGLES);
        glColor3f(0.10f, 0.07f, 0.06f); glVertex2f(x - 140, GY + 118); glVertex2f(x + 140, GY + 118);
        glColor3f(0.17f, 0.12f, 0.09f); glVertex2f(x, GY + 215);
        glEnd();
        // door and windows (warm)
        glBegin(GL_QUADS);
        glColor3f(0.95f, 0.65f, 0.28f);
        glVertex2f(x - 20, GY - 8); glVertex2f(x + 20, GY - 8); glVertex2f(x + 20, GY + 66); glVertex2f(x - 20, GY + 66);
        glVertex2f(x - 85, GY + 50); glVertex2f(x - 50, GY + 50); glVertex2f(x - 50, GY + 88); glVertex2f(x - 85, GY + 88);
        glVertex2f(x + 50, GY + 50); glVertex2f(x + 85, GY + 50); glVertex2f(x + 85, GY + 88); glVertex2f(x + 50, GY + 88);
        glEnd();
        glColor3f(0.14f, 0.09f, 0.06f);
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(x - 67.5f, GY + 50); glVertex2f(x - 67.5f, GY + 88);
        glVertex2f(x - 85, GY + 69);    glVertex2f(x - 50, GY + 69);
        glVertex2f(x + 67.5f, GY + 50); glVertex2f(x + 67.5f, GY + 88);
        glVertex2f(x + 50, GY + 69);    glVertex2f(x + 85, GY + 69);
        glEnd();
        glLineWidth(1);
    } else {
        additiveBlend();
        float p = 0.85f + 0.15f * sinf(gt * 4);
        glow(x, GY + 30, 200, 1.0f, 0.70f, 0.30f, 0.32f * p);
        glow(x - 67, GY + 69, 60, 1.0f, 0.75f, 0.35f, 0.40f * p);
        glow(x + 67, GY + 69, 60, 1.0f, 0.75f, 0.35f, 0.40f * p);
        glowEllipse(x, GY - 4, 190, 16, 1.0f, 0.70f, 0.30f, 0.35f * p);
        // chimney smoke
        normalBlend();
        for (int k = 0; k < 6; k++) {
            float t = wrapf(gt * 0.25f + k / 6.0f, 1.0f);
            glowEllipse(x + 63 + sinf(t * 6 + k) * 14 + t * 30, GY + 220 + t * 120, 18 + t * 30, 14 + t * 24,
                        0.7f, 0.75f, 0.8f, 0.14f * (1 - t));
        }
    }
}

// ============================================================
//  NEW : SCENERY SHARED BY THE MEDIUM AND HARD LEVELS
// ============================================================

// ---- a bare spooky tree made of recursive branches (colour is set by the caller) ----
void branch(float x, float y, float ang, float len, int depth, float w, int seed)
{
    float x2 = x + cosf(ang) * len, y2 = y + sinf(ang) * len;
    glLineWidth(w < 1.0f ? 1.0f : w);
    glBegin(GL_LINES); glVertex2f(x, y); glVertex2f(x2, y2); glEnd();
    if (depth <= 0) return;
    float a1 = 0.45f + hash01(seed, 31) * 0.35f, a2 = 0.45f + hash01(seed, 32) * 0.35f;
    float sw = sinf(gt * 0.8f + seed) * 0.03f;
    branch(x2, y2, ang + a1 + sw, len * (0.66f + hash01(seed, 33) * 0.12f), depth - 1, w * 0.62f, seed * 2 + 1);
    branch(x2, y2, ang - a2 + sw, len * (0.66f + hash01(seed, 34) * 0.12f), depth - 1, w * 0.62f, seed * 2 + 2);
}

void deadTree(float x, float by, float h, int seed)
{
    glColor4f(0.02f, 0.012f, 0.03f, 0.97f);
    branch(x, by - 4, PI / 2 + 0.08f, h * 0.30f, 4, 9.0f, seed);
    glBegin(GL_TRIANGLES);                        // roots
    glVertex2f(x - 14, by - 4); glVertex2f(x - 4, by + 26); glVertex2f(x + 2, by - 4);
    glVertex2f(x + 14, by - 4); glVertex2f(x + 4, by + 26); glVertex2f(x - 2, by - 4);
    glEnd();
    glLineWidth(1);
}

// ---- an iron fence (spiked posts + two rails) ----
void drawFence(float x0, float x1, float yb, float r, float g, float b)
{
    float sp = 28.0f;
    float from = fmaxf(x0, camX - 60.0f), to = fminf(x1, camX + VW + 60.0f);
    if (to <= from) return;
    float start = x0 + ceilf((from - x0) / sp) * sp;
    glColor3f(r, g, b);
    for (float x = start; x < to; x += sp) {
        int id = (int)floorf(x / sp);
        if (hash01(id, 1501) < 0.12f) continue;                    // a broken piece
        float h = 46.0f + hash01(id, 1502) * 10.0f;
        glBegin(GL_QUADS);
        glVertex2f(x - 2, yb); glVertex2f(x + 2, yb); glVertex2f(x + 2, yb + h); glVertex2f(x - 2, yb + h);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(x - 3.5f, yb + h); glVertex2f(x + 3.5f, yb + h); glVertex2f(x, yb + h + 11);
        glEnd();
    }
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(from, yb + 14); glVertex2f(to, yb + 14);
    glVertex2f(from, yb + 36); glVertex2f(to, yb + 36);
    glEnd();
    glLineWidth(1);
}

// ---- a crow (wings flap with fl in -1..1, dir = +1 looking right, -1 looking left) ----
void drawCrow(float x, float y, float s, float fl, float dir, float a)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(dir * s, s, 1);
    glColor4f(0.02f, 0.02f, 0.04f, a);
    // wings
    glBegin(GL_TRIANGLES);
    glVertex2f(0.6f, 0.30f); glVertex2f(-0.9f, 0.30f); glVertex2f(-1.9f, 0.30f + fl * 1.7f);
    glVertex2f(0.4f, 0.30f); glVertex2f(-0.5f, 0.20f); glVertex2f(0.9f, 0.30f + fl * 1.4f);
    glEnd();
    // tail
    glBegin(GL_TRIANGLES);
    glVertex2f(-1.2f, 0.25f); glVertex2f(-2.5f, 0.55f); glVertex2f(-2.3f, -0.15f);
    glEnd();
    // body and head
    glowEllipse(0, 0, 1.4f, 0.62f, 0.02f, 0.02f, 0.04f, a);
    circleFill(1.25f, 0.45f, 0.45f, 8);
    // beak
    glBegin(GL_TRIANGLES);
    glColor4f(0.10f, 0.09f, 0.10f, a);
    glVertex2f(1.6f, 0.62f); glVertex2f(2.5f, 0.38f); glVertex2f(1.6f, 0.25f);
    glEnd();
    glPopMatrix();
}

// ---- a bat (flap -1..1) ----
void drawBat(float x, float y, float s, float flap, float a)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(s, s, 1);
    glColor4f(0.03f, 0.01f, 0.05f, a);
    for (int side = -1; side <= 1; side += 2) {
        glBegin(GL_TRIANGLE_FAN);                 // one wing
        glVertex2f(0.0f, 0.1f);
        glVertex2f(side * 0.7f, 0.5f + flap * 0.7f);
        glVertex2f(side * 1.5f, 0.2f + flap * 1.3f);
        glVertex2f(side * 1.25f, -0.15f + flap * 0.8f);
        glVertex2f(side * 0.9f, 0.0f + flap * 0.5f);
        glVertex2f(side * 0.65f, -0.35f + flap * 0.2f);
        glVertex2f(side * 0.3f, -0.1f);
        glVertex2f(0.0f, -0.35f);
        glEnd();
        glBegin(GL_TRIANGLES);                    // ear
        glVertex2f(side * 0.08f, 0.5f); glVertex2f(side * 0.22f, 0.45f); glVertex2f(side * 0.2f, 0.78f);
        glEnd();
    }
    circleFill(0.0f, 0.0f, 0.34f, 8);
    circleFill(0.0f, 0.36f, 0.22f, 8);
    glPopMatrix();
}

void batPos(const Bat &b, float &x, float &y)
{
    float a = b.w * gt + b.ph;
    x = b.cx + cosf(a) * b.rx;
    y = b.cy + sinf(2.0f * a) * b.ry;
}

// MEDIUM : lots of bats flying around the vampire's lair (silhouettes)
void drawBats()
{
    if (difficulty != 1) return;
    normalBlend();
    for (int i = 0; i < NBAT; i++) {
        float x, y;
        batPos(bats[i], x, y);
        if (x < camX - 60 || x > camX + VW + 60) continue;
        drawBat(x, y, bats[i].sz, sinf(gt * 12.0f + bats[i].ph * 3.0f), 0.95f);
    }
}

// glowing red eyes of the bats (drawn on top of the darkness)
void drawBatEyes()
{
    if (difficulty != 1) return;
    additiveBlend();
    for (int i = 0; i < NBAT; i++) {
        float x, y;
        batPos(bats[i], x, y);
        if (x < camX - 60 || x > camX + VW + 60) continue;
        float s = bats[i].sz;
        glow(x - s * 0.1f, y + s * 0.38f, s * 0.5f, 1.0f, 0.1f, 0.15f, 0.8f);
        glow(x + s * 0.1f, y + s * 0.38f, s * 0.5f, 1.0f, 0.1f, 0.15f, 0.8f);
    }
    normalBlend();
}

// HARD : crows circling over the yard and the mansion
void drawCrows()
{
    if (difficulty != 2) return;
    normalBlend();
    for (int i = 0; i < NCROW; i++) {
        const Crow &c = crows[i];
        float a = c.w * gt + c.ph;
        float x = c.cx + cosf(a) * c.rx, y = c.cy + sinf(2.0f * a) * c.ry;
        if (x < camX - 60 || x > camX + VW + 60) continue;
        float dir = (-sinf(a) >= 0.0f) ? 1.0f : -1.0f;
        drawCrow(x, y, c.sz, sinf(gt * 10.0f + c.ph * 4.0f), dir, 0.97f);
    }
}

// ---- a gravestone ----
void tomb(float x, float by, float h, float w, int type, float sh)
{
    glColor3f(0.13f * sh, 0.12f * sh, 0.16f * sh);
    if (type == 0) {                              // rounded top
        glBegin(GL_QUADS);
        glVertex2f(x - w / 2, by); glVertex2f(x + w / 2, by); glVertex2f(x + w / 2, by + h); glVertex2f(x - w / 2, by + h);
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, by + h);
        for (int i = 0; i <= 10; i++) { float a = PI * i / 10; glVertex2f(x + cosf(a) * w * 0.5f, by + h + sinf(a) * w * 0.5f); }
        glEnd();
    } else if (type == 1) {                       // cross
        glBegin(GL_QUADS);
        glVertex2f(x - w * 0.17f, by); glVertex2f(x + w * 0.17f, by); glVertex2f(x + w * 0.17f, by + h * 1.1f); glVertex2f(x - w * 0.17f, by + h * 1.1f);
        glVertex2f(x - w * 0.5f, by + h * 0.65f); glVertex2f(x + w * 0.5f, by + h * 0.65f);
        glVertex2f(x + w * 0.5f, by + h * 0.65f + w * 0.3f); glVertex2f(x - w * 0.5f, by + h * 0.65f + w * 0.3f);
        glEnd();
    } else {                                      // obelisk
        glBegin(GL_QUADS);
        glVertex2f(x - w * 0.5f, by); glVertex2f(x + w * 0.5f, by); glVertex2f(x + w * 0.28f, by + h); glVertex2f(x - w * 0.28f, by + h);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(x - w * 0.28f, by + h); glVertex2f(x + w * 0.28f, by + h); glVertex2f(x, by + h + w * 0.5f);
        glEnd();
    }
    glLineWidth(1.5f);                            // moonlit edge
    glColor4f(0.45f, 0.50f, 0.60f, 0.25f);
    glBegin(GL_LINES); glVertex2f(x + w * 0.5f, by); glVertex2f(x + w * 0.5f, by + h); glEnd();
    glLineWidth(1);
}

// ---- a candle (pass 0 = wax, pass 1 = flame glow) ----
void candle(float x, float yb, int id, int pass)
{
    if (pass == 0) {
        glColor3f(0.85f, 0.82f, 0.70f);
        glBegin(GL_QUADS);
        glVertex2f(x - 2.5f, yb); glVertex2f(x + 2.5f, yb); glVertex2f(x + 2.5f, yb + 11); glVertex2f(x - 2.5f, yb + 11);
        glEnd();
    } else {
        float f = 0.7f + 0.3f * sinf(gt * 9.0f + id * 2.3f);
        glow(x, yb + 14, 20, 1.0f, 0.70f, 0.30f, 0.55f * f);
        glColor4f(1.0f, 0.9f, 0.5f, 0.95f);
        circleFill(x, yb + 13.5f, 1.6f, 6);
    }
}

// ---- a window of the mansion (pass 0 = frame, pass 1 = flickering light) ----
void gothicWin(float cx, float y0, float w, float h, int id, int pass)
{
    float hw = w * 0.5f;
    if (pass == 0) {
        glColor3f(0.03f, 0.03f, 0.04f);
        glBegin(GL_QUADS);
        glVertex2f(cx - hw, y0); glVertex2f(cx + hw, y0); glVertex2f(cx + hw, y0 + h); glVertex2f(cx - hw, y0 + h);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - hw, y0 + h); glVertex2f(cx + hw, y0 + h); glVertex2f(cx, y0 + h + w * 0.7f);
        glEnd();
        glLineWidth(2);
        glColor3f(0.20f, 0.17f, 0.22f);
        glBegin(GL_LINES);
        glVertex2f(cx, y0); glVertex2f(cx, y0 + h + w * 0.5f);
        glVertex2f(cx - hw, y0 + h * 0.55f); glVertex2f(cx + hw, y0 + h * 0.55f);
        glEnd();
        if (hash01(id, 1702) < 0.4f) {            // boarded up with planks
            glLineWidth(3);
            glColor3f(0.16f, 0.10f, 0.07f);
            glBegin(GL_LINES);
            glVertex2f(cx - hw - 3, y0 + 4); glVertex2f(cx + hw + 3, y0 + h - 4);
            glVertex2f(cx - hw - 3, y0 + h - 6); glVertex2f(cx + hw + 3, y0 + 10);
            glEnd();
        }
        glLineWidth(1);
    } else {
        if (hash01(id, 1701) > 0.7f) return;      // some windows stay dark
        float f = 0.65f + 0.35f * sinf(gt * (2.0f + (id % 3)) + id * 1.7f);
        glColor4f(0.95f, 0.90f, 0.45f, 0.30f * f);
        glBegin(GL_QUADS);
        glVertex2f(cx - hw, y0); glVertex2f(cx + hw, y0); glVertex2f(cx + hw, y0 + h); glVertex2f(cx - hw, y0 + h);
        glEnd();
        glow(cx, y0 + h * 0.55f, w * 1.8f, 0.85f, 0.95f, 0.40f, 0.45f * f);
    }
}

// ---- a child (used inside the cage and when following the traveler home) ----
// armUp 0 = hands on the bars (sad), 1 = arms up (happy). runAmt 0 = standing, 1 = running
void drawChild(float cx, float yb, int id, float runPh, float runAmt, float armUp)
{
    float hr = hash01(id, 1801), hg = hash01(id, 1802), hb = hash01(id, 1803);
    glLineWidth(3.5f);                            // legs
    glColor3f(0.20f, 0.16f, 0.22f);
    glBegin(GL_LINES);
    for (int s = -1; s <= 1; s += 2) {
        float sw = sinf(runPh + (s > 0 ? PI : 0.0f)) * 6.0f * runAmt;
        glVertex2f(cx + s * 3.0f, yb + 22.0f); glVertex2f(cx + s * 3.0f + sw, yb + 2.0f);
    }
    glEnd();
    glBegin(GL_QUADS);                            // clothes
    glColor3f(0.35f + 0.5f * hr, 0.35f + 0.5f * hg, 0.35f + 0.5f * hb);
    glVertex2f(cx - 10, yb + 20); glVertex2f(cx + 10, yb + 20);
    glColor3f(0.20f + 0.4f * hr, 0.20f + 0.4f * hg, 0.20f + 0.4f * hb);
    glVertex2f(cx + 7, yb + 42); glVertex2f(cx - 7, yb + 42);
    glEnd();
    glLineWidth(3);                               // arms
    glColor3f(0.93f, 0.78f, 0.65f);
    glBegin(GL_LINES);
    for (int s = -1; s <= 1; s += 2) {
        glVertex2f(cx + s * 7.0f, yb + 40.0f);
        glVertex2f(cx + s * 15.0f, yb + 40.0f + armUp * 22.0f);
    }
    glEnd();
    glColor3f(0.93f, 0.78f, 0.65f);               // head
    circleFill(cx, yb + 50, 8, 12);
    glColor3f(0.10f + 0.3f * hg, 0.06f + 0.2f * hr, 0.04f);     // hair
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, yb + 52);
    for (int i = 0; i <= 8; i++) { float a = (15.0f + 150.0f * i / 8.0f) * PI / 180.0f; glVertex2f(cx + cosf(a) * 8.6f, yb + 50 + sinf(a) * 8.6f); }
    glEnd();
    glColor3f(0.12f, 0.08f, 0.08f);               // eyes and mouth
    circleFill(cx - 3, yb + 50.5f, 0.9f, 6);
    circleFill(cx + 3, yb + 50.5f, 0.9f, 6);
    glLineWidth(1.2f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 6; i++) {
        float t = i / 6.0f - 0.5f;
        float curve = (armUp > 0.5f) ? (-t * t * 5.0f + 1.6f) : (t * t * 5.0f - 1.6f);
        glVertex2f(cx + t * 6.0f, yb + 46.5f + curve * 0.8f);
    }
    glEnd();
    glLineWidth(1);
}

// ============================================================
//  NEW : MEDIUM LEVEL SCENERY  (twilight, vampire's crypt, graveyard)
// ============================================================
// pass 0 = solid shapes (before the darkness), pass 1 = glow (on top of the darkness)
void drawLair(int pass)
{
    float x = LAIR_X, yb = GY - 8.0f;
    if (x < camX - 900 || x > camX + VW + 900) return;
    float p = 0.8f + 0.2f * sinf(gt * 3.0f);

    if (pass == 0) {
        // --- bare trees around the crypt ---
        deadTree(x - 480, yb, 170, 11);
        deadTree(x + 450, yb, 150, 23);
        deadTree(x - 780, yb, 130, 37);
        deadTree(x + 760, yb, 140, 41);
        // --- iron fences ---
        drawFence(x - 700, x - 250, GY - 4, 0.02f, 0.015f, 0.03f);
        drawFence(x + 330, x + 700, GY - 4, 0.02f, 0.015f, 0.03f);
        // --- two side spires ---
        for (int s = -1; s <= 1; s += 2) {
            float sx = x + s * 180.0f;
            glBegin(GL_QUADS);
            glColor3f(0.08f, 0.06f, 0.10f); glVertex2f(sx - 32, yb); glVertex2f(sx + 32, yb);
            glColor3f(0.15f, 0.11f, 0.17f); glVertex2f(sx + 32, yb + 215); glVertex2f(sx - 32, yb + 215);
            glEnd();
            glBegin(GL_TRIANGLES);
            glColor3f(0.05f, 0.03f, 0.07f); glVertex2f(sx - 40, yb + 212); glVertex2f(sx + 40, yb + 212);
            glColor3f(0.13f, 0.08f, 0.15f); glVertex2f(sx, yb + 310);
            glEnd();
            glColor3f(0.02f, 0.01f, 0.03f);       // narrow window
            glBegin(GL_QUADS);
            glVertex2f(sx - 5, yb + 150); glVertex2f(sx + 5, yb + 150); glVertex2f(sx + 5, yb + 185); glVertex2f(sx - 5, yb + 185);
            glEnd();
            glBegin(GL_TRIANGLES);
            glVertex2f(sx - 5, yb + 185); glVertex2f(sx + 5, yb + 185); glVertex2f(sx, yb + 197);
            glEnd();
            glLineWidth(2);
            glColor3f(0.20f, 0.15f, 0.22f);       // spire tip
            glBegin(GL_LINES); glVertex2f(sx, yb + 310); glVertex2f(sx, yb + 328); glEnd();
            glLineWidth(1);
        }
        // --- main hall ---
        glBegin(GL_QUADS);
        glColor3f(0.10f, 0.07f, 0.12f); glVertex2f(x - 190, yb); glVertex2f(x + 190, yb);
        glColor3f(0.17f, 0.12f, 0.19f); glVertex2f(x + 190, yb + 130); glVertex2f(x - 190, yb + 130);
        glEnd();
        glColor4f(0.0f, 0.0f, 0.0f, 0.30f);       // stone block lines
        glLineWidth(1);
        glBegin(GL_LINES);
        for (int k = 1; k < 6; k++) { glVertex2f(x - 190, yb + k * 22); glVertex2f(x + 190, yb + k * 22); }
        glEnd();
        glBegin(GL_TRIANGLES);                    // roof
        glColor3f(0.05f, 0.03f, 0.07f); glVertex2f(x - 205, yb + 128); glVertex2f(x + 205, yb + 128);
        glColor3f(0.12f, 0.08f, 0.14f); glVertex2f(x, yb + 205);
        glEnd();
        // --- central tower ---
        glBegin(GL_QUADS);
        glColor3f(0.09f, 0.06f, 0.11f); glVertex2f(x - 62, yb + 100); glVertex2f(x + 62, yb + 100);
        glColor3f(0.16f, 0.11f, 0.18f); glVertex2f(x + 62, yb + 255); glVertex2f(x - 62, yb + 255);
        glEnd();
        glBegin(GL_TRIANGLES);
        glColor3f(0.05f, 0.03f, 0.07f); glVertex2f(x - 74, yb + 252); glVertex2f(x + 74, yb + 252);
        glColor3f(0.14f, 0.09f, 0.16f); glVertex2f(x, yb + 345);
        glEnd();
        glLineWidth(2);
        glColor3f(0.20f, 0.15f, 0.22f);
        glBegin(GL_LINES); glVertex2f(x, yb + 345); glVertex2f(x, yb + 366); glVertex2f(x - 8, yb + 357); glVertex2f(x + 8, yb + 357); glEnd();
        glLineWidth(1);
        // --- rose window ---
        glColor3f(0.20f, 0.15f, 0.22f);
        circleFill(x, yb + 180, 24, 20);
        glColor3f(0.03f, 0.0f, 0.04f);
        circleFill(x, yb + 180, 20, 20);
        glLineWidth(2);
        glColor3f(0.20f, 0.15f, 0.22f);
        glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) { float a = k * PI / 4; glVertex2f(x + cosf(a) * 20, yb + 180 + sinf(a) * 20); glVertex2f(x - cosf(a) * 20, yb + 180 - sinf(a) * 20); }
        glEnd();
        glLineWidth(1);
        // --- pointed arch door ---
        glColor3f(0.10f, 0.0f, 0.02f);
        glBegin(GL_QUADS);
        glVertex2f(x - 28, yb); glVertex2f(x + 28, yb); glVertex2f(x + 28, yb + 66); glVertex2f(x - 28, yb + 66);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(x - 28, yb + 66); glVertex2f(x + 28, yb + 66); glVertex2f(x, yb + 100);
        glEnd();
        glColor3f(0.14f, 0.11f, 0.15f);           // steps
        glBegin(GL_QUADS);
        glVertex2f(x - 44, yb - 6); glVertex2f(x + 44, yb - 6); glVertex2f(x + 36, yb + 2); glVertex2f(x - 36, yb + 2);
        glEnd();
        // --- standing coffin with crimson velvet ---
        glPushMatrix();
        glTranslatef(x + 265, yb, 0);
        glRotatef(8, 0, 0, 1);
        glColor3f(0.18f, 0.08f, 0.07f);
        glBegin(GL_POLYGON);
        glVertex2f(-10, 95); glVertex2f(10, 95); glVertex2f(20, 60); glVertex2f(14, 0); glVertex2f(-14, 0); glVertex2f(-20, 60);
        glEnd();
        glColor3f(0.45f, 0.05f, 0.10f);
        glBegin(GL_POLYGON);
        glVertex2f(-7, 86); glVertex2f(7, 86); glVertex2f(14, 58); glVertex2f(10, 8); glVertex2f(-10, 8); glVertex2f(-14, 58);
        glEnd();
        glPopMatrix();
        // --- candles by the door ---
        candle(x - 95, yb, 1, 0); candle(x - 70, yb, 2, 0); candle(x + 70, yb, 3, 0); candle(x + 95, yb, 4, 0);
        // --- gravestones ---
        for (int i = -7; i <= 7; i++) {
            if (i >= -2 && i <= 2) continue;      // keep the front of the crypt free
            float tx = x + i * 105.0f + (hash01(i, 1601) - 0.5f) * 50.0f;
            float by = yb + 2 - hash01(i, 1602) * 22.0f;
            tomb(tx, by, 26 + hash01(i, 1603) * 32, 24 + hash01(i, 1604) * 12, (int)(hash01(i, 1605) * 3.0f) % 3, 0.8f + 0.5f * hash01(i, 1606));
        }
    } else {
        additiveBlend();
        glow(x, yb + 45, 95, 1.0f, 0.15f, 0.20f, 0.42f * p);                 // red light from the door
        glowEllipse(x, yb + 2, 130, 14, 1.0f, 0.15f, 0.25f, 0.35f * p);
        glow(x, yb + 180, 58, 1.0f, 0.20f, 0.35f, 0.50f * p);                // rose window
        glow(x - 180, yb + 167, 30, 0.80f, 0.30f, 0.90f, 0.40f * p);         // spire windows
        glow(x + 180, yb + 167, 30, 0.80f, 0.30f, 0.90f, 0.40f * p);
        glow(x + 268, yb + 45, 42, 1.0f, 0.15f, 0.30f, 0.28f * p);           // coffin
        candle(x - 95, yb, 1, 1); candle(x - 70, yb, 2, 1); candle(x + 70, yb, 3, 1); candle(x + 95, yb, 4, 1);
        glowEllipse(x, yb + 6, 360, 26, 0.90f, 0.20f, 0.30f, 0.12f);         // low red mist
        normalBlend();
    }
}

// ============================================================
//  NEW : HARD LEVEL SCENERY  (witch's mansion, yard, scarecrows, cage)
// ============================================================
// ---- a scarecrow with a crow on its shoulder (pass 0 = body, pass 1 = glowing eyes) ----
void drawScarecrow(int k, float x, int pass)
{
    float sway = sinf(gt * 0.9f + k * 1.7f) * 2.0f;
    float lean = (hash01(k, 1601) - 0.5f) * 10.0f;
    float by = GY - 12.0f - hash01(k, 1602) * 10.0f;
    glPushMatrix();
    glTranslatef(x, by, 0);
    glRotatef(lean + sway, 0, 0, 1);
    if (pass == 0) {
        glLineWidth(6);                           // pole and crossbar
        glColor3f(0.25f, 0.16f, 0.09f);
        glBegin(GL_LINES); glVertex2f(0, 0); glVertex2f(0, 150); glEnd();
        glLineWidth(5);
        glBegin(GL_LINES); glVertex2f(-50, 106); glVertex2f(50, 100); glEnd();
        glBegin(GL_QUADS);                        // ragged tunic
        glColor3f(0.20f, 0.13f, 0.12f); glVertex2f(-21, 120); glVertex2f(21, 120);
        glColor3f(0.13f, 0.09f, 0.09f); glVertex2f(27, 70); glVertex2f(-27, 70);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(-27, 70); glVertex2f(-18, 70); glVertex2f(-23, 56);
        glVertex2f(-9, 70);  glVertex2f(2, 70);   glVertex2f(-4, 52);
        glVertex2f(10, 70);  glVertex2f(20, 70);  glVertex2f(15, 58);
        glEnd();
        glBegin(GL_QUADS);                        // sleeves
        glColor3f(0.18f, 0.12f, 0.11f);
        glVertex2f(-54, 108); glVertex2f(-18, 114); glVertex2f(-18, 94); glVertex2f(-54, 98);
        glVertex2f(54, 102);  glVertex2f(18, 112);  glVertex2f(18, 92);  glVertex2f(54, 94);
        glColor3f(0.45f, 0.14f, 0.10f);           // patches
        glVertex2f(-12, 104); glVertex2f(0, 104); glVertex2f(0, 92); glVertex2f(-12, 92);
        glColor3f(0.20f, 0.30f, 0.16f);
        glVertex2f(8, 90); glVertex2f(20, 90); glVertex2f(20, 78); glVertex2f(8, 78);
        glEnd();
        glLineWidth(2);                           // straw sticking out
        glColor3f(0.78f, 0.62f, 0.22f);
        glBegin(GL_LINES);
        for (int i = 0; i < 6; i++) {
            glVertex2f(-54, 98 + i * 2.0f); glVertex2f(-66 - (i % 3) * 3.0f, 94 + i * 3.0f);
            glVertex2f(54, 94 + i * 2.0f);  glVertex2f(67 + (i % 3) * 3.0f, 90 + i * 3.0f);
            glVertex2f(-20 + i * 8.0f, 58); glVertex2f(-22 + i * 8.0f, 48 - (i % 2) * 6.0f);
        }
        glEnd();
        glColor3f(0.52f, 0.43f, 0.28f);           // sack head
        circleFill(0, 134, 15, 16);
        glLineWidth(1.5f);
        glColor3f(0.10f, 0.07f, 0.05f);
        glBegin(GL_LINES);                        // crossed eyes
        glVertex2f(-8, 140); glVertex2f(-3, 135); glVertex2f(-8, 135); glVertex2f(-3, 140);
        glVertex2f(3, 140);  glVertex2f(8, 135);  glVertex2f(3, 135);  glVertex2f(8, 140);
        glEnd();
        glBegin(GL_LINE_STRIP);                   // stitched mouth
        for (int i = 0; i <= 8; i++) glVertex2f(-8 + i * 2.0f, 126 + ((i % 2) ? 3.0f : 0.0f));
        glEnd();
        glColor3f(0.12f, 0.08f, 0.10f);           // floppy hat
        glBegin(GL_QUADS);
        glVertex2f(-26, 144); glVertex2f(26, 144); glVertex2f(22, 150); glVertex2f(-22, 150);
        glVertex2f(-14, 150); glVertex2f(14, 150); glVertex2f(10, 176); glVertex2f(-10, 176);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(-10, 176); glVertex2f(10, 176); glVertex2f(22, 168);
        glEnd();
        glColor3f(0.35f, 0.12f, 0.45f);
        glBegin(GL_QUADS);
        glVertex2f(-14, 150); glVertex2f(14, 150); glVertex2f(13, 156); glVertex2f(-13, 156);
        glEnd();
        glLineWidth(1);
        drawCrow(50, 108, 6.5f, -0.15f, 1.0f, 0.97f);                          // crow on the arm
        if (hash01(k, 1603) < 0.6f) drawCrow(0, 178, 6.0f, -0.15f, -1.0f, 0.97f);   // crow on the hat
    } else {
        float f = 0.7f + 0.3f * sinf(gt * 5.0f + k * 2.0f);                    // eerie orange eyes
        glow(-5, 138, 12, 1.0f, 0.55f, 0.10f, 0.55f * f);
        glow(5, 138, 12, 1.0f, 0.55f, 0.10f, 0.55f * f);
    }
    glPopMatrix();
}

// ---- a jack-o-lantern (pass 0 = pumpkin, pass 1 = glowing face) ----
void drawPumpkin(float x, float y, float s, int pass)
{
    if (pass == 0) {
        glColor3f(0.50f, 0.19f, 0.04f);
        glowEllipse(x, y + s * 0.9f, s * 1.25f, s * 0.95f, 0.50f, 0.19f, 0.04f, 1.0f);
        glColor3f(0.62f, 0.26f, 0.05f);
        circleFill(x - s * 0.1f, y + s * 0.9f, s * 0.7f, 14);
        glColor3f(0.15f, 0.25f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2f(x - 2, y + s * 1.8f); glVertex2f(x + 2, y + s * 1.8f); glVertex2f(x + 3, y + s * 2.2f); glVertex2f(x - 1, y + s * 2.2f);
        glEnd();
    } else {
        float f = 0.75f + 0.25f * sinf(gt * 7.0f + x * 0.1f);
        glow(x, y + s * 0.9f, s * 3.2f, 1.0f, 0.55f, 0.10f, 0.40f * f);
        glColor4f(1.0f, 0.80f, 0.25f, 0.95f * f);                              // eyes and zigzag mouth
        glBegin(GL_TRIANGLES);
        glVertex2f(x - s * 0.7f, y + s * 1.05f); glVertex2f(x - s * 0.2f, y + s * 1.05f); glVertex2f(x - s * 0.45f, y + s * 1.45f);
        glVertex2f(x + s * 0.2f, y + s * 1.05f); glVertex2f(x + s * 0.7f, y + s * 1.05f); glVertex2f(x + s * 0.45f, y + s * 1.45f);
        glEnd();
        glLineWidth(2);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 6; i++) glVertex2f(x - s * 0.7f + i * s * 0.233f, y + s * 0.55f + ((i % 2) ? s * 0.18f : 0.0f));
        glEnd();
        glLineWidth(1);
    }
}

const float SCARE_X[5] = { 3250.0f, 3800.0f, 4350.0f, 4800.0f, 5590.0f };
const float PUMP_X[7]  = { 3050.0f, 3520.0f, 4050.0f, 4450.0f, 4880.0f, 5560.0f, 5800.0f };

// yard in front of the mansion
void drawYardProps(int pass)
{
    if (difficulty != 2) return;
    if (camX + VW < 2800 || camX > MANSION_X + 700) return;
    if (pass == 0) {
        drawFence(2950, 4880, GY - 4, 0.02f, 0.02f, 0.03f);
        drawFence(5560, 5640, GY - 4, 0.02f, 0.02f, 0.03f);
        deadTree(3400, GY - 10, 160, 53);
        deadTree(4200, GY - 10, 175, 61);
        deadTree(4640, GY - 10, 140, 71);
        for (int i = 0; i < 5; i++) drawScarecrow(i, SCARE_X[i], 0);
        for (int i = 0; i < 7; i++) drawPumpkin(PUMP_X[i], GY - 8 - hash01(i, 1604) * 18.0f, 9.0f + 3.0f * hash01(i, 1605), 0);
    } else {
        additiveBlend();
        for (int i = 0; i < 5; i++) drawScarecrow(i, SCARE_X[i], 1);
        for (int i = 0; i < 7; i++) drawPumpkin(PUMP_X[i], GY - 8 - hash01(i, 1604) * 18.0f, 9.0f + 3.0f * hash01(i, 1605), 1);
        for (int i = 0; i < 16; i++) {            // pale witchlights drifting over the yard
            float wx = 2900.0f + i * 180.0f + sinf(gt * 0.5f + i) * 40.0f;
            float wy = GY + 50 + hash01(i, 1606) * 120 + sinf(gt * 0.9f + i * 1.3f) * 14.0f;
            glow(wx, wy, 14, 0.55f, 1.0f, 0.65f, 0.20f + 0.10f * sinf(gt * 2.0f + i));
        }
        normalBlend();
    }
}

void drawMansion(int pass)
{
    if (difficulty != 2) return;
    float x = MANSION_X, yb = GY - 8.0f;
    if (x < camX - 800 || x > camX + VW + 800) return;
    float p = 0.8f + 0.2f * sinf(gt * 2.5f);

    if (pass == 0) {
        deadTree(x - 380, yb, 160, 83);
        deadTree(x + 400, yb, 150, 97);
        // left tower
        glBegin(GL_QUADS);
        glColor3f(0.08f, 0.07f, 0.10f); glVertex2f(x - 300, yb); glVertex2f(x - 205, yb);
        glColor3f(0.14f, 0.12f, 0.16f); glVertex2f(x - 205, yb + 270); glVertex2f(x - 300, yb + 270);
        glEnd();
        glBegin(GL_TRIANGLES);
        glColor3f(0.04f, 0.03f, 0.06f); glVertex2f(x - 314, yb + 268); glVertex2f(x - 192, yb + 268);
        glColor3f(0.11f, 0.07f, 0.13f); glVertex2f(x - 248, yb + 378);
        glEnd();
        // right tower (shorter, crooked roof)
        glBegin(GL_QUADS);
        glColor3f(0.08f, 0.07f, 0.10f); glVertex2f(x + 205, yb); glVertex2f(x + 292, yb);
        glColor3f(0.14f, 0.12f, 0.16f); glVertex2f(x + 292, yb + 220); glVertex2f(x + 205, yb + 220);
        glEnd();
        glBegin(GL_TRIANGLES);
        glColor3f(0.04f, 0.03f, 0.06f); glVertex2f(x + 196, yb + 218); glVertex2f(x + 302, yb + 218);
        glColor3f(0.11f, 0.07f, 0.13f); glVertex2f(x + 262, yb + 318);
        glEnd();
        // main body
        glBegin(GL_QUADS);
        glColor3f(0.09f, 0.07f, 0.11f); glVertex2f(x - 215, yb); glVertex2f(x + 215, yb);
        glColor3f(0.15f, 0.12f, 0.17f); glVertex2f(x + 215, yb + 190); glVertex2f(x - 215, yb + 190);
        glEnd();
        // crooked chimney
        glColor3f(0.07f, 0.06f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2f(x + 108, yb + 240); glVertex2f(x + 138, yb + 240); glVertex2f(x + 146, yb + 336); glVertex2f(x + 118, yb + 338);
        glEnd();
        // roof of the main body
        glBegin(GL_TRIANGLES);
        glColor3f(0.04f, 0.03f, 0.06f); glVertex2f(x - 236, yb + 188); glVertex2f(x + 236, yb + 188);
        glColor3f(0.11f, 0.07f, 0.13f); glVertex2f(x + 10, yb + 300);
        glEnd();
        // centre gable
        glBegin(GL_QUADS);
        glColor3f(0.09f, 0.07f, 0.11f); glVertex2f(x - 72, yb + 188); glVertex2f(x + 72, yb + 188);
        glColor3f(0.15f, 0.12f, 0.17f); glVertex2f(x + 72, yb + 265); glVertex2f(x - 72, yb + 265);
        glEnd();
        glBegin(GL_TRIANGLES);
        glColor3f(0.04f, 0.03f, 0.06f); glVertex2f(x - 86, yb + 262); glVertex2f(x + 86, yb + 262);
        glColor3f(0.11f, 0.07f, 0.13f); glVertex2f(x + 4, yb + 348);
        glEnd();
        // wall boards
        glColor4f(0.0f, 0.0f, 0.0f, 0.30f);
        glLineWidth(1);
        glBegin(GL_LINES);
        for (int k = 1; k < 8; k++) { glVertex2f(x - 215, yb + k * 24); glVertex2f(x + 215, yb + k * 24); }
        glEnd();
        // big arched door
        glColor3f(0.02f, 0.01f, 0.02f);
        glBegin(GL_QUADS);
        glVertex2f(x - 28, yb); glVertex2f(x + 28, yb); glVertex2f(x + 28, yb + 84); glVertex2f(x - 28, yb + 84);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(x - 28, yb + 84); glVertex2f(x + 28, yb + 84); glVertex2f(x, yb + 112);
        glEnd();
        glColor3f(0.12f, 0.09f, 0.08f);
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(x, yb); glVertex2f(x, yb + 100);
        glVertex2f(x - 14, yb); glVertex2f(x - 14, yb + 90); glVertex2f(x + 14, yb); glVertex2f(x + 14, yb + 90);
        glEnd();
        glLineWidth(1);
        glColor3f(0.13f, 0.11f, 0.10f);           // broken steps
        glBegin(GL_QUADS);
        glVertex2f(x - 48, yb - 6); glVertex2f(x + 48, yb - 6); glVertex2f(x + 38, yb + 2); glVertex2f(x - 38, yb + 2);
        glEnd();
        // windows
        gothicWin(x - 152, yb + 50, 30, 46, 1, 0);  gothicWin(x + 152, yb + 50, 30, 46, 2, 0);
        gothicWin(x - 152, yb + 120, 30, 46, 3, 0); gothicWin(x + 152, yb + 120, 30, 46, 4, 0);
        gothicWin(x - 64, yb + 120, 28, 44, 5, 0);  gothicWin(x + 64, yb + 120, 28, 44, 6, 0);
        gothicWin(x, yb + 198, 26, 38, 7, 0);
        gothicWin(x - 252, yb + 110, 26, 44, 8, 0); gothicWin(x - 252, yb + 200, 24, 40, 9, 0);
        gothicWin(x + 248, yb + 100, 26, 42, 10, 0);
        // crows sitting on the roofs
        drawCrow(x - 248, yb + 382, 8.0f, -0.1f, 1.0f, 0.97f);
        drawCrow(x + 4, yb + 352, 8.0f, -0.1f, -1.0f, 0.97f);
        drawCrow(x + 262, yb + 322, 7.5f, -0.1f, 1.0f, 0.97f);
        drawCrow(x + 132, yb + 340, 7.5f, -0.1f, -1.0f, 0.97f);
        drawCrow(x - 150, yb + 230, 7.0f, -0.1f, 1.0f, 0.97f);
        drawCrow(x + 80, yb + 290, 7.0f, -0.1f, -1.0f, 0.97f);
    } else {
        additiveBlend();
        gothicWin(x - 152, yb + 50, 30, 46, 1, 1);  gothicWin(x + 152, yb + 50, 30, 46, 2, 1);
        gothicWin(x - 152, yb + 120, 30, 46, 3, 1); gothicWin(x + 152, yb + 120, 30, 46, 4, 1);
        gothicWin(x - 64, yb + 120, 28, 44, 5, 1);  gothicWin(x + 64, yb + 120, 28, 44, 6, 1);
        gothicWin(x, yb + 198, 26, 38, 7, 1);
        gothicWin(x - 252, yb + 110, 26, 44, 8, 1); gothicWin(x - 252, yb + 200, 24, 40, 9, 1);
        gothicWin(x + 248, yb + 100, 26, 42, 10, 1);
        glColor4f(0.55f, 1.0f, 0.55f, 0.55f * p);  // green light from the crack of the door
        glBegin(GL_QUADS);
        glVertex2f(x - 2.5f, yb); glVertex2f(x + 2.5f, yb); glVertex2f(x + 2.5f, yb + 92); glVertex2f(x - 2.5f, yb + 92);
        glEnd();
        glow(x, yb + 40, 90, 0.45f, 1.0f, 0.50f, 0.28f * p);
        glowEllipse(x, yb + 2, 240, 18, 0.45f, 0.95f, 0.50f, 0.25f * p);       // witchlight on the ground
        glowEllipse(x, yb + 10, 420, 40, 0.40f, 0.70f, 0.45f, 0.10f);          // low green mist
        normalBlend();
    }
}

// ---- the cage with ten children, ten locks and ten wisps ----
void wispPos(int i, float &wx, float &wy)
{
    float cx = CAGE_X - 252.0f + 56.0f * i;
    float a = gt * 2.2f + i * 0.9f;
    wx = cx + cosf(a) * 20.0f;
    wy = GY + 36.0f + sinf(a) * 14.0f + wispFlee[i] * 38.0f;
}

void drawCage(int pass)
{
    if (difficulty != 2) return;
    float x0 = CAGE_X - 280.0f, x1 = CAGE_X + 280.0f, yb = GY - 8.0f, top = GY + 128.0f;
    if (x1 < camX - 100 || x0 > camX + VW + 100) return;
    bool inCage = (state != 2);                   // after the rescue the children walk home with you

    if (pass == 0) {
        glBegin(GL_QUADS);                        // wooden floor and dark back
        glColor3f(0.14f, 0.09f, 0.06f); glVertex2f(x0 - 12, yb - 10); glVertex2f(x1 + 12, yb - 10);
        glColor3f(0.20f, 0.13f, 0.09f); glVertex2f(x1 + 12, yb + 6); glVertex2f(x0 - 12, yb + 6);
        glEnd();
        glColor4f(0.02f, 0.015f, 0.03f, 0.55f);
        glBegin(GL_QUADS);
        glVertex2f(x0, yb + 6); glVertex2f(x1, yb + 6); glVertex2f(x1, top); glVertex2f(x0, top);
        glEnd();
        if (inCage)
            for (int i = 0; i < KIDS; i++) {
                float cx = CAGE_X - 252.0f + 56.0f * i;
                float bob = kidFree[i] ? fabsf(sinf(gt * 5.0f + i)) * 6.0f : 0.0f;
                drawChild(cx, yb + 6 + bob, i, 0.0f, 0.0f, kidFree[i] ? 1.0f : 0.0f);
            }
        glColor3f(0.08f, 0.08f, 0.10f);           // iron bars with spikes
        glLineWidth(4);
        glBegin(GL_LINES);
        for (int k = 0; k <= KIDS; k++) { glVertex2f(x0 + 56.0f * k, yb + 6); glVertex2f(x0 + 56.0f * k, top); }
        glEnd();
        glLineWidth(6);
        glBegin(GL_LINES); glVertex2f(x0 - 4, top); glVertex2f(x1 + 4, top); glEnd();
        glLineWidth(3);
        glBegin(GL_LINES); glVertex2f(x0, yb + 64); glVertex2f(x1, yb + 64); glEnd();
        glBegin(GL_TRIANGLES);
        for (int k = 0; k <= KIDS; k++) { float bx = x0 + 56.0f * k; glVertex2f(bx - 4, top); glVertex2f(bx + 4, top); glVertex2f(bx, top + 14); }
        glEnd();
        glLineWidth(1.5f);                        // moonlit edge of the bars
        glColor4f(0.30f, 0.34f, 0.38f, 0.5f);
        glBegin(GL_LINES);
        for (int k = 0; k <= KIDS; k++) { glVertex2f(x0 + 56.0f * k + 2, yb + 6); glVertex2f(x0 + 56.0f * k + 2, top); }
        glEnd();
        glLineWidth(1);
        for (int i = 0; i < KIDS; i++) {          // padlocks
            float cx = CAGE_X - 252.0f + 56.0f * i;
            bool fr = kidFree[i] || !inCage;
            glColor3f(fr ? 0.30f : 0.75f, fr ? 0.75f : 0.60f, fr ? 0.35f : 0.20f);
            glBegin(GL_QUADS);
            glVertex2f(cx - 6, yb + 8); glVertex2f(cx + 6, yb + 8); glVertex2f(cx + 6, yb + 18); glVertex2f(cx - 6, yb + 18);
            glEnd();
            glLineWidth(2);
            glBegin(GL_LINE_STRIP);
            float lift = fr ? 5.0f : 0.0f;
            glVertex2f(cx - 4, yb + 18 + lift); glVertex2f(cx - 4, yb + 24 + lift);
            glVertex2f(cx + 4, yb + 24 + lift); glVertex2f(cx + 4, yb + 18 + lift);
            glEnd();
            glLineWidth(1);
        }
    } else {
        additiveBlend();
        glowEllipse(CAGE_X, yb + 4, 340, 22, 0.50f, 0.85f, 0.55f, 0.14f);          // witch mist at the cage
        for (int i = 0; i < KIDS; i++) {
            float cx = CAGE_X - 252.0f + 56.0f * i;
            bool fr = kidFree[i] || !inCage;
            if (fr) glow(cx, yb + 14, 22, 0.5f, 1.0f, 0.5f, 0.45f);
            else    glow(cx, yb + 14, 22, 0.9f, 0.25f, 0.55f, 0.35f);
            if (fr && inCage) glow(cx, yb + 40, 38, 1.0f, 0.92f, 0.55f, 0.30f);       // freed child glows
            if (!fr) {                                                              // the wisp guarding the lock
                float wx, wy;
                wispPos(i, wx, wy);
                for (int t = 1; t <= 5; t++) {
                    float a = gt * 2.2f + i * 0.9f - t * 0.22f;
                    glow(cx + cosf(a) * 20.0f, GY + 36.0f + sinf(a) * 14.0f + wispFlee[i] * 38.0f, 12 - t, 0.55f, 0.95f, 1.0f, 0.30f - t * 0.05f);
                }
                glow(wx, wy, 22, 0.55f, 0.95f, 1.0f, 0.60f);
                glColor4f(0.9f, 1.0f, 1.0f, 0.95f);
                circleFill(wx, wy, 3, 8);
            }
        }
        normalBlend();
    }
}

// cage that drops over the traveler when the witch catches him
void drawCaptureCage()
{
    float k = smooth(deathT / 0.7f);
    float yb = GY - 8.0f + (1.0f - k) * 620.0f;
    float h = 128.0f, hw = 48.0f;
    glColor3f(0.08f, 0.08f, 0.10f);
    glLineWidth(4);
    glBegin(GL_LINES);
    for (int i = 0; i <= 6; i++) { float bx = px - hw + hw * 2.0f * i / 6.0f; glVertex2f(bx, yb); glVertex2f(bx, yb + h); }
    glEnd();
    glLineWidth(6);
    glBegin(GL_LINES); glVertex2f(px - hw - 4, yb + h); glVertex2f(px + hw + 4, yb + h); glVertex2f(px - hw - 4, yb); glVertex2f(px + hw + 4, yb); glEnd();
    glLineWidth(1);
    additiveBlend();                              // magic where it lands
    float lg = clampf((deathT - 0.55f) * 3.0f, 0.0f, 1.0f) * (1.0f - clampf((deathT - 1.4f) / 1.2f, 0.0f, 1.0f));
    glow(px, GY + 50, 110, 0.55f, 1.0f, 0.50f, 0.45f * lg);
    glow(px, GY + 30, 70, 0.75f, 0.40f, 1.0f, 0.40f * lg);
    normalBlend();
}

// ============================================================
//  NEW : LAIR MUSHROOMS  (the special bright ones you walk into)
// ============================================================
void placeLair()
{
    lmN = 0;
    if (difficulty == 1) {                        // medium : 9 mushrooms around the vampire's crypt
        lmN = 9;
        for (int i = 0; i < lmN; i++) {
            lm[i].x = 3500.0f + i * 240.0f + (hash01(i, 901) - 0.5f) * 100.0f;
            lm[i].y = GY - 8.0f - hash01(i, 902) * 30.0f;
            lm[i].sz = 9.0f + hash01(i, 903) * 3.0f;
            lm[i].got = false;
        }
    } else if (difficulty == 2) {                 // hard : 18 mushrooms through the witch's yard
        lmN = 18;
        for (int i = 0; i < lmN; i++) {
            lm[i].x = 2400.0f + i * 148.0f + (hash01(i, 901) - 0.5f) * 60.0f;
            lm[i].y = GY - 8.0f - hash01(i, 902) * 30.0f;
            lm[i].sz = 9.0f + hash01(i, 903) * 3.0f;
            lm[i].got = false;
        }
    }
}

// pass 0 = the mushroom, pass 1 = its glow and the little light beam above it
void drawLairMush(int pass)
{
    if (lmN == 0) return;
    float cr = (difficulty == 1) ? 0.95f : 0.55f;       // medium : crimson-pink, hard : pure pale green
    float cg = (difficulty == 1) ? 0.20f : 1.00f;
    float cb = (difficulty == 1) ? 0.55f : 0.80f;
    for (int i = 0; i < lmN; i++) {
        if (lm[i].got) continue;
        float x = lm[i].x - camX, y = lm[i].y, sz = lm[i].sz;
        if (x < -90 || x > VW + 90) continue;
        float b = 0.65f + 0.35f * sinf(gt * 2.0f + i * 1.3f);
        if (pass == 0) {
            glColor3f(0.85f, 0.80f, 0.90f);
            glBegin(GL_QUADS);
            glVertex2f(x - 2.5f, y); glVertex2f(x + 2.5f, y); glVertex2f(x + 2.0f, y + sz); glVertex2f(x - 2.0f, y + sz);
            glEnd();
            glBegin(GL_TRIANGLE_FAN);
            glColor3f(cr, cg, cb);
            glVertex2f(x, y + sz * 1.3f);
            glColor3f(cr * 0.55f, cg * 0.55f, cb * 0.55f);
            for (int k = 0; k <= 12; k++) {
                float a = PI * k / 12;
                glVertex2f(x + cosf(a) * sz * 1.35f, y + sz + sinf(a) * sz * 0.95f);
            }
            glEnd();
            glColor4f(1.0f, 1.0f, 1.0f, 0.85f);   // spots
            circleFill(x - sz * 0.5f, y + sz * 1.45f, sz * 0.17f, 8);
            circleFill(x + sz * 0.35f, y + sz * 1.7f, sz * 0.13f, 8);
            circleFill(x + sz * 0.7f, y + sz * 1.3f, sz * 0.11f, 8);
        } else {
            additiveBlend();
            glow(x, y + sz, sz * 8.0f, cr, cg, cb, 0.45f * b);
            glow(x, y + sz * 1.2f, sz * 3.0f, 1.0f, 1.0f, 1.0f, 0.25f * b);
            glBegin(GL_QUADS);                    // thin beam of light going up
            glColor4f(cr, cg, cb, 0.22f * b); glVertex2f(x - 6, y + sz); glVertex2f(x + 6, y + sz);
            glColor4f(cr, cg, cb, 0.0f);      glVertex2f(x + 3, y + sz + 130); glVertex2f(x - 3, y + sz + 130);
            glEnd();
            for (int k = 0; k < 3; k++) {         // spores floating up
                float t = wrapf(gt * 0.35f + k / 3.0f + i * 0.17f, 1.0f);
                glow(x + sinf(t * 6 + k + i) * 10, y + sz + 10 + t * 70, 5, cr, cg, cb, 0.7f * (1 - t));
            }
            normalBlend();
        }
    }
}

// ============================================================
//  NEW : THE HUNTER  (vampire on medium, witch on hard)
// ============================================================
void drawVampire(float x, float a)
{
    float df = hunterFace();
    float fl = sinf(gt * 3.0f), hov = 5.0f + sinf(gt * 2.2f) * 3.0f;
    normalBlend();
    glowEllipse(x, GY - 5, 32, 7, 0, 0, 0, 0.5f * a);
    glPushMatrix();
    glTranslatef(x, GY + hov, 0);
    glScalef(df, 1, 1);
    // flowing cape
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.22f, 0.01f, 0.05f, a); glVertex2f(0, 50);
    glColor4f(0.50f, 0.04f, 0.10f, a); glVertex2f(-6, 90);
    glColor4f(0.30f, 0.02f, 0.07f, a); glVertex2f(-26, 70);
    glVertex2f(-58 - fl * 5, 22 + fl * 3);
    glColor4f(0.45f, 0.03f, 0.09f, a); glVertex2f(-46, -2 + fl * 3);
    glVertex2f(-26, 6); glVertex2f(-8, -1); glVertex2f(8, -1); glVertex2f(22, 6);
    glColor4f(0.22f, 0.01f, 0.05f, a); glVertex2f(24, 50); glVertex2f(14, 88); glVertex2f(-6, 90);
    glEnd();
    // coat
    glBegin(GL_QUADS);
    glColor4f(0.06f, 0.04f, 0.09f, a); glVertex2f(-14, 86); glVertex2f(14, 86);
    glColor4f(0.03f, 0.02f, 0.05f, a); glVertex2f(18, 16); glVertex2f(-18, 16);
    glEnd();
    glBegin(GL_TRIANGLES);                        // red lining
    glColor4f(0.55f, 0.04f, 0.10f, a * 0.9f); glVertex2f(-3, 86); glVertex2f(3, 86); glVertex2f(0, 26);
    glEnd();
    glLineWidth(5);                               // legs
    glColor4f(0.03f, 0.02f, 0.04f, a);
    glBegin(GL_LINES); glVertex2f(-6, 18); glVertex2f(-7, 0); glVertex2f(6, 18); glVertex2f(8, 0); glEnd();
    // tall collar
    glBegin(GL_TRIANGLES);
    glColor4f(0.04f, 0.02f, 0.06f, a);
    glVertex2f(-14, 86); glVertex2f(-24, 108); glVertex2f(-3, 92);
    glVertex2f(14, 86);  glVertex2f(24, 108);  glVertex2f(3, 92);
    glEnd();
    // face, hair, fangs
    glColor4f(0.90f, 0.88f, 0.93f, a);
    circleFill(0, 97, 9.5f, 16);
    glColor4f(0.03f, 0.02f, 0.04f, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(-1, 98);
    for (int i = 0; i <= 8; i++) { float ang = (15.0f + 150.0f * i / 8.0f) * PI / 180.0f; glVertex2f(cosf(ang) * 10.5f - 1, 97 + sinf(ang) * 10.5f); }
    glEnd();
    glColor4f(0.35f, 0.05f, 0.08f, a);            // mouth
    glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(1, 91); glVertex2f(7, 91); glEnd();
    glColor4f(1.0f, 1.0f, 1.0f, a);               // fangs
    glBegin(GL_TRIANGLES);
    glVertex2f(1.5f, 91); glVertex2f(3.5f, 91); glVertex2f(2.5f, 86);
    glVertex2f(4.5f, 91); glVertex2f(6.5f, 91); glVertex2f(5.5f, 86);
    glEnd();
    // reaching arm with claws
    float rb = sinf(gt * 4.0f) * 3.0f;
    glLineWidth(6);
    glColor4f(0.04f, 0.02f, 0.06f, a);
    glBegin(GL_LINES); glVertex2f(9, 82); glVertex2f(34, 76 + rb); glEnd();
    glColor4f(0.88f, 0.86f, 0.90f, a);
    circleFill(37, 76 + rb, 3.5f, 8);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glVertex2f(40, 78 + rb); glVertex2f(47, 80 + rb);
    glVertex2f(40, 76 + rb); glVertex2f(48, 76 + rb);
    glVertex2f(40, 74 + rb); glVertex2f(47, 72 + rb);
    glEnd();
    glLineWidth(1);
    glPopMatrix();
}

void drawWitch(float x, float a)
{
    float df = hunterFace();
    float fl = sinf(gt * 4.0f), hov = 14.0f + sinf(gt * 2.0f) * 5.0f;
    normalBlend();
    glowEllipse(x, GY - 5, 36, 7, 0, 0, 0, 0.35f * a);
    glPushMatrix();
    glTranslatef(x, GY + hov, 0);
    glScalef(df * 0.85f, 0.85f, 1);
    glRotatef(sinf(gt * 2.0f) * 3.0f, 0, 0, 1);
    // broom twigs, handle
    glLineWidth(2);
    glColor4f(0.62f, 0.45f, 0.20f, a);
    glBegin(GL_LINES);
    for (int k = 0; k < 7; k++) { glVertex2f(-44, 31); glVertex2f(-84 - (k % 3) * 3.0f, 18 + k * 3.5f + fl * 1.5f); }
    glEnd();
    glLineWidth(5);
    glColor4f(0.40f, 0.26f, 0.14f, a);
    glBegin(GL_LINES); glVertex2f(-46, 31); glVertex2f(56, 34); glEnd();
    glColor4f(0.20f, 0.12f, 0.07f, a);
    glBegin(GL_QUADS); glVertex2f(-50, 26); glVertex2f(-40, 26); glVertex2f(-40, 37); glVertex2f(-50, 37); glEnd();
    // flowing robe
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.30f, 0.12f, 0.40f, a); glVertex2f(0, 52);
    glColor4f(0.14f, 0.05f, 0.22f, a);
    glVertex2f(-12, 74); glVertex2f(-30, 60); glVertex2f(-64 - fl * 5, 36 + fl * 3); glVertex2f(-48, 28);
    glVertex2f(-30, 34 - fl * 2); glVertex2f(-14, 30); glVertex2f(10, 33); glVertex2f(14, 56); glVertex2f(8, 74); glVertex2f(-12, 74);
    glEnd();
    // legs and boots
    glLineWidth(4);
    glColor4f(0.10f, 0.06f, 0.12f, a);
    glBegin(GL_LINES); glVertex2f(-2, 34); glVertex2f(-4, 18); glVertex2f(6, 34); glVertex2f(10, 20); glEnd();
    circleFill(-4, 17, 3.5f, 8); circleFill(11, 19, 3.5f, 8);
    // arm holding the broom
    glLineWidth(5);
    glColor4f(0.22f, 0.08f, 0.30f, a);
    glBegin(GL_LINES); glVertex2f(6, 62); glVertex2f(32, 38); glEnd();
    // head: green skin, hooked nose, hair
    glColor4f(0.45f, 0.72f, 0.35f, a);
    circleFill(2, 84, 8.5f, 14);
    glBegin(GL_TRIANGLES); glVertex2f(9, 85); glVertex2f(19, 78); glVertex2f(9, 78); glEnd();
    glColor4f(0.04f, 0.03f, 0.05f, a);
    glLineWidth(2);
    glBegin(GL_LINES);
    for (int k = 0; k < 5; k++) { glVertex2f(-4, 88 - k * 3.0f); glVertex2f(-20 - fl * 2, 78 - k * 6.0f + fl * 2); }
    glEnd();
    // pointed hat with a bent tip
    glBegin(GL_QUADS);
    glColor4f(0.10f, 0.04f, 0.15f, a);
    glVertex2f(-20, 90); glVertex2f(24, 90); glVertex2f(22, 95); glVertex2f(-18, 95);
    glEnd();
    glBegin(GL_TRIANGLES);
    glColor4f(0.12f, 0.05f, 0.18f, a); glVertex2f(-10, 93); glVertex2f(14, 93);
    glColor4f(0.24f, 0.10f, 0.30f, a); glVertex2f(0, 122);
    glColor4f(0.20f, 0.08f, 0.26f, a); glVertex2f(-6, 119); glVertex2f(5, 119); glVertex2f(-16 - fl * 2, 140);
    glEnd();
    glBegin(GL_QUADS);                            // hat band and buckle
    glColor4f(0.40f, 0.15f, 0.55f, a);
    glVertex2f(-9.5f, 93); glVertex2f(13.5f, 93); glVertex2f(13, 99); glVertex2f(-9, 99);
    glColor4f(0.95f, 0.78f, 0.30f, a);
    glVertex2f(0, 94); glVertex2f(5, 94); glVertex2f(5, 98); glVertex2f(0, 98);
    glEnd();
    glLineWidth(1);
    glPopMatrix();
}

bool hunterVisible()
{
    if (difficulty == 0) return false;
    if (state == 0) return true;                  // shown in the story pictures (only if on screen)
    if (state == 1 || state == 4) return hunterOn;
    if (state == 2) return hunterOn && hunterA > 0.02f;
    return false;
}

void drawHunter()
{
    if (!hunterVisible()) return;
    if (hunterX < camX - 250 || hunterX > camX + VW + 250) return;
    float a = (state == 2) ? hunterA : 1.0f;
    if (difficulty == 1) drawVampire(hunterX, a); else drawWitch(hunterX, a);
}

// glow of the hunter (red eyes, green eyes, aura) on top of the darkness
void drawHunterGlow()
{
    if (!hunterVisible()) return;
    if (hunterX < camX - 250 || hunterX > camX + VW + 250) return;
    float a = (state == 2) ? hunterA : 1.0f;
    float df = hunterFace();
    additiveBlend();
    if (difficulty == 1) {
        float hov = 5.0f + sinf(gt * 2.2f) * 3.0f;
        glow(hunterX, GY + hov + 55, 100, 0.9f, 0.05f, 0.15f, 0.16f * a);
        glowEllipse(hunterX, GY, 90, 14, 0.8f, 0.1f, 0.2f, 0.20f * a);
        glow(hunterX + df * 2.0f, GY + hov + 98, 13, 1.0f, 0.10f, 0.12f, 0.95f * a);
        glow(hunterX + df * 7.0f, GY + hov + 98, 13, 1.0f, 0.10f, 0.12f, 0.95f * a);
    } else {
        float hov = 14.0f + sinf(gt * 2.0f) * 5.0f;
        glow(hunterX, GY + hov + 55, 100, 0.5f, 0.8f, 0.4f, 0.12f * a);
        glow(hunterX + df * 5.0f, GY + hov + 73, 11, 0.5f, 1.0f, 0.30f, 0.95f * a);
        for (int k = 0; k < 10; k++) {            // magic sparkles trailing behind the broom
            float t = k / 10.0f;
            float sx = hunterX - df * (60.0f + k * 14.0f) * 0.85f;
            float sy = GY + hov + 30 + sinf(gt * 6.0f + k) * 8.0f;
            if (k % 2) glow(sx, sy, 8 + (1 - t) * 6, 0.6f, 1.0f, 0.4f, 0.5f * (1 - t) * a);
            else       glow(sx, sy, 8 + (1 - t) * 6, 0.8f, 0.4f, 1.0f, 0.5f * (1 - t) * a);
        }
    }
    normalBlend();
}

// ---- blood, sparks, children following home, mushroom blast ring ----
void drawParts()
{
    normalBlend();
    for (size_t i = 0; i < parts.size(); i++) {
        const Part &p = parts[i];
        if (p.kind != 0) continue;
        glColor4f(0.50f, 0.02f, 0.04f, 0.92f);
        circleFill(p.x, p.y, p.sz, 8);
        glColor4f(0.85f, 0.12f, 0.12f, 0.5f);
        circleFill(p.x - p.sz * 0.3f, p.y + p.sz * 0.3f, p.sz * 0.35f, 6);
    }
    additiveBlend();
    for (size_t i = 0; i < parts.size(); i++) {
        const Part &p = parts[i];
        if (p.kind != 1) continue;
        float k = p.life / p.maxLife;
        glow(p.x, p.y, p.sz * 4.0f, p.r, p.g, p.b, 0.7f * k);
        glColor4f(1.0f, 1.0f, 0.9f, k);
        circleFill(p.x, p.y, p.sz * 0.5f, 6);
    }
    normalBlend();
}

void drawBlast()
{
    if (blastT <= 0) return;
    float k = 1.0f - blastT / 0.6f;
    float r = 40.0f + k * 260.0f;
    float cr = (difficulty == 1) ? 1.0f : 0.7f, cg = (difficulty == 1) ? 0.5f : 1.0f, cb = (difficulty == 1) ? 0.75f : 0.8f;
    additiveBlend();
    glLineWidth(4);
    glColor4f(cr, cg, cb, 0.8f * (1 - k));
    circleLine(blastX, GY + 45, r, 40);
    glow(blastX, GY + 45, r * 0.9f, cr, cg, cb, 0.25f * (1 - k));
    glLineWidth(1);
    normalBlend();
}

// the ten rescued children running home behind the traveler (hard level ending)
void drawKidsFollow(int pass)
{
    if (difficulty != 2 || state != 2) return;
    for (int k = 0; k < KIDS; k++) {
        float x = px - 75.0f - k * 44.0f;
        float y = GY + stoneHeightAt(x) + fabsf(sinf(walkPh * 1.3f - k * 0.7f)) * 3.0f;
        if (pass == 0) {
            normalBlend();
            glowEllipse(x, GY - 5, 14, 4, 0, 0, 0, 0.4f);
            drawChild(x, y, k, walkPh * 1.3f - k * 0.7f, clampf(fabsf(vx) / 230.0f, 0.0f, 1.0f), 1.0f);
        } else {
            additiveBlend();
            glow(x, y + 30, 34, 1.0f, 0.92f, 0.60f, 0.25f);
            normalBlend();
        }
    }
}

// red / purple edge glow that gets stronger when the hunter is close
void drawDanger()
{
    if (state != 1 || !hunterOn || immune() || difficulty == 0) return;
    float d = fabsf(hunterX - px);
    float k = clampf(1.0f - d / 750.0f, 0.0f, 1.0f);
    if (k <= 0.0f) return;
    float a = 0.50f * k * (0.75f + 0.25f * sinf(gt * 8.0f));
    float cr = (difficulty == 1) ? 0.70f : 0.45f, cg = (difficulty == 1) ? 0.0f : 0.10f, cb = (difficulty == 1) ? 0.05f : 0.70f;
    normalBlend();
    if (hunterX < px) hgrad(0, 0, 260, VH, cr, cg, cb, a, cr, cg, cb, 0.0f);
    else              hgrad(VW - 260, 0, VW, VH, cr, cg, cb, 0.0f, cr, cg, cb, a);
}

// ------------------------------------------------------------
//  Game logic
// ------------------------------------------------------------
void placeFirefly(Firefly &f, int mode)
{
    if (mode == 0) {                               // near the start
        f.x = frand(380, 1250);
        f.y = frand(190, 420);
    } else {                                       // somewhere far from the traveler
        for (int t = 0; t < 30; t++) {
            f.x = frand(60, LEVEL_W - 60);
            f.y = frand(175, 430);
            if (fabsf(f.x - px) > 650) break;
        }
    }
    f.ang = frand(0, 2 * PI);
    f.speed = frand(24, 52);
    f.phase = frand(0, 6.28f);
    f.freq = frand(1.2f, 2.6f);
}

void resetGame()
{
    px = 200; pz = 0; vzv = 0; vx = 0; face = 1; walkPh = 0;
    swinging = false; swingT = 0; cooldown = 0;
    caught = 0; playT = 0; ct = 0; hudPulse = 0;
    arrived = false; hopped = false;
    flying.clear();
    for (int i = 0; i < NUM_FF; i++) placeFirefly(ff[i], i < 12 ? 0 : 1);
    camX = clampf(px - VW * 0.45f, 0, LEVEL_W - VW);
    camPrev = camX;
    // ---- new : reset everything of the medium / hard levels ----
    parts.clear();
    lairGot = 0; ordCount = 0; freedCount = 0;
    memset(ordGot, 0, sizeof(ordGot));
    for (int i = 0; i < KIDS; i++) { kidFree[i] = false; wispFlee[i] = 0.0f; }
    hunterOn = false; hunterWake = 0; hunterX = -5000; hunterA = 1.0f; hunterStun = 0; hunterKB = 0;
    bendT = 0; msgT = 0; deathT = 0; blastT = 0;
    placeLair();
}

void initWorld()
{
    for (int i = 0; i < NEEDED; i++) { jfx[i] = frand(-15, 15); jfy[i] = frand(-22, 8); }
    for (int i = 0; i < 70; i++) {
        Mote m = { frand(0, VW), frand(GY, VH), frand(-6, 6), frand(2, 10), frand(0, 6.28f), frand(1.5f, 3.5f) };
        motes[i] = m;
    }
    for (int i = 0; i < 22; i++) {
        Leaf l = { frand(0, VW), frand(0, VH), frand(-10, 10), frand(-45, -20), frand(0, 6.28f),
                   frand(-2, 2), frand(7, 13), frand(0, 1) };
        leaves[i] = l;
    }
    // bats : most of them live around the vampire's crypt
    for (int i = 0; i < NBAT; i++) {
        Bat b;
        b.cx = (i < 28) ? frand(LAIR_X - 900, LAIR_X + 700) : frand(300, 6000);
        b.cy = frand(320, 640); b.rx = frand(60, 260); b.ry = frand(15, 60);
        b.w = frand(0.5f, 1.3f); b.ph = frand(0, 6.28f); b.sz = frand(9, 15);
        bats[i] = b;
    }
    // crows : most of them circle the witch's mansion
    for (int i = 0; i < NCROW; i++) {
        Crow c;
        c.cx = (i < 10) ? frand(MANSION_X - 250, MANSION_X + 250) : frand(3300, 5200);
        c.cy = frand(330, 600); c.rx = frand(100, 300); c.ry = frand(15, 50);
        c.w = frand(0.5f, 1.1f); c.ph = frand(0, 6.28f); c.sz = frand(7, 11);
        crows[i] = c;
    }
}

// ---- messages, particles ----
void burst(float x, float y, float r, float g, float b, int n, float spd)
{
    for (int i = 0; i < n; i++) {
        float a = frand(0, 2 * PI), s = spd * frand(0.3f, 1.0f);
        float ml = frand(0.5f, 1.1f);
        Part p = { x, y, cosf(a) * s, sinf(a) * s, ml, ml, frand(2.0f, 4.0f), r, g, b, 1 };
        parts.push_back(p);
    }
}

void spawnBlood(float x, float y, int n)
{
    for (int i = 0; i < n; i++) {
        Part p = { x, y, frand(-300, 300), frand(80, 520), 99.0f, 99.0f, frand(2.0f, 5.0f), 0.55f, 0.02f, 0.04f, 0 };
        parts.push_back(p);
    }
}

void updateParts(float dt)
{
    for (size_t i = 0; i < parts.size(); ) {
        Part &p = parts[i];
        if (p.kind == 0) {                         // blood : falls and stays on the ground
            if (p.y > GY - 12.0f) {
                p.vy -= 900.0f * dt;
                p.x += p.vx * dt; p.y += p.vy * dt;
                if (p.y <= GY - 12.0f) { p.y = GY - 12.0f - frand(0, 22); p.vx = 0; p.vy = 0; }
            }
            i++;
        } else {                                   // sparks : drift and fade
            p.life -= dt;
            p.x += p.vx * dt; p.y += p.vy * dt;
            p.vx *= (1.0f - 1.5f * dt); p.vy *= (1.0f - 1.5f * dt);
            if (p.life <= 0) parts.erase(parts.begin() + i); else i++;
        }
    }
}

// ---- actions ----
void hopStart()
{
    if (pz <= 0.01f && vzv == 0) {
        vzv = JUMP_V;
        if (state != 2) {                          // every jump also swings the net in a half circle
            swinging = true; arcSwing = true;
            swingT = 0; swingDur = 0.60f;
            swingAim = (face > 0) ? 0.0f : PI;
            swingSgn = (face > 0) ? -1.0f : 1.0f;
            cooldown = 0;
        }
    }
}

void netStart(float aim)
{
    if (swinging || cooldown > 0) return;
    arcSwing = false; swingDur = SWING_DUR;
    swingSgn = (cosf(aim) >= 0) ? 1.0f : -1.0f;
    swinging = true; swingT = 0; swingAim = aim;
    if (fabsf(cosf(aim)) > 0.2f) face = (cosf(aim) > 0) ? 1.0f : -1.0f;
}

void hopSwing()
{
    hopStart();                                    // the jump already swings the net
}

void startGame()
{
    state = 1;
    resetGame();
    fadeA = 1.0f;
}

void advanceSlide()
{
    int total = (int)(strlen(slides[slide].l[0]) + strlen(slides[slide].l[1]) + strlen(slides[slide].l[2]));
    if ((int)(st * 42) < total) { st = 999; return; }   // finish typing first
    if (slide < 3) { slide++; st = 0; fadeA = 1.0f; demoLast = 0; hopped = false; }
    else startGame();
}

// ---- menu and level choosing ----
float menuCam()                                    // where the camera looks in the menu (shows the chosen level's scenery)
{
    if (difficulty == 1) return LAIR_X - 560.0f;
    if (difficulty == 2) return 5050.0f;
    return 600.0f;
}

void setMenuSel(int s)
{
    if (s == menuSel && difficulty == s) return;
    menuSel = s; difficulty = s; menuT = 0;
    placeLair();
    fadeA = fmaxf(fadeA, 0.45f);
}

void startStory(int lvl)                           // level chosen : show its storyboard
{
    difficulty = lvl; slides = slideSets[lvl];
    resetGame();
    state = 0; slide = 0; st = 0; fadeA = 1.0f; demoLast = 0; hopped = false;
    hunterX = (lvl == 1) ? LAIR_X + 90.0f : (lvl == 2 ? CAGE_X + 550.0f : -5000.0f);   // hunter shown in the story pictures
    hunterA = 1.0f;
}

void goMenu()
{
    kL = kR = false;
    state = 3; menuSel = difficulty; menuT = 0; slides = slideSets[difficulty];
    resetGame();
    fadeA = 1.0f;
}

int menuHit(float wx, float wy)                    // which menu button is under the mouse
{
    for (int i = 0; i < 3; i++) {
        float y0 = 400.0f - i * 110.0f;
        if (wx >= 250 && wx <= 750 && wy >= y0 && wy <= y0 + 90) return i;
    }
    return -1;
}

// ---- win / lose ----
void triggerEnding()
{
    state = 2; ct = 0; swinging = false;
    cabinX = px + 7000; streamCX = cabinX - 900; arrived = false; hopped = false;
    kL = kR = false;
}

void checkWin()
{
    if (state != 1) return;
    if (difficulty == 0) { if (caught >= NEEDED) triggerEnding(); }
    else if (difficulty == 1) { if (caught >= NEEDED && lairGot >= MED_NEED) triggerEnding(); }
    else { if (freedCount >= KIDS) triggerEnding(); }
}

void killPlayer()                                  // vampire bites (blood) or witch catches (cage) : game over
{
    if (state != 1) return;
    state = 4; deathT = 0; deathType = difficulty;
    swinging = false; vx = 0; kL = kR = false;
    if (difficulty == 1) spawnBlood(px, GY + pz + 45, 90);
    else                 burst(px, GY + pz + 45, 0.7f, 0.5f, 1.0f, 40, 280.0f);
}

// ---- mushroom blast : pushes the hunter back 5 meters (250 px) ----
void doBlast()
{
    if (state != 1 || difficulty == 0) return;
    int cost = (difficulty == 1) ? 1 : 2;
    if (!hunterOn || fabsf(hunterX - px) > 520.0f) { showMsg("Nothing to blast right now."); return; }
    if (lairGot < cost) { showMsg(cost == 1 ? "You need a lair mushroom to blast!" : "You need 2 lair mushrooms to blast!"); return; }
    int need = (difficulty == 1) ? MED_NEED : (KIDS - freedCount);
    if (lairGot - cost + lmLeft() < need) { showMsg("Keep your lair mushrooms - you still need them!"); return; }
    lairGot -= cost;
    hunterKB = PUSH_DIST; hunterStun = 1.2f;
    blastT = 0.6f; blastX = px;
    if (difficulty == 1) burst(px, GY + 45, 1.0f, 0.5f, 0.75f, 36, 380.0f);
    else                 burst(px, GY + 45, 0.7f, 1.0f, 0.8f, 36, 380.0f);
    showMsg(difficulty == 1 ? "BLAST! The vampire is thrown back!" : "BLAST! The witch is thrown back!");
}

// ---- per-frame updates ----
void updateParticles(float dt)
{
    for (int i = 0; i < 70; i++) {
        Mote &m = motes[i];
        m.x += (m.vx - camVel * 0.5f) * dt;
        m.y += (m.vy + sinf(gt * 0.8f + m.ph) * 8) * dt;
        m.x = wrapf(m.x + 20, VW + 40) - 20;
        if (m.y > VH) m.y = GY * 0.5f;
    }
    for (int i = 0; i < 22; i++) {
        Leaf &l = leaves[i];
        l.x += (l.vx - camVel * 0.9f + sinf(gt * 1.1f + l.hue * 6) * 25) * dt;
        l.y += l.vy * dt;
        l.rot += l.vr * dt;
        l.x = wrapf(l.x + 20, VW + 40) - 20;
        if (l.y < -20) { l.y = VH + 20; l.x = frand(0, VW); }
    }
}

void updateFireflies(float dt)
{
    bool bounded = (state != 2);
    for (int i = 0; i < NUM_FF; i++) {
        Firefly &f = ff[i];
        f.ang += sinf(gt * 0.7f + f.phase * 3) * 1.6f * dt + frand(-1, 1) * 2.2f * dt;
        f.x += cosf(f.ang) * f.speed * dt;
        f.y += sinf(f.ang) * f.speed * 0.7f * dt + sinf(gt * 1.8f + f.phase) * 12 * dt;
        if (bounded && (f.x < 40 || f.x > LEVEL_W - 40)) { f.ang = PI - f.ang; f.x = clampf(f.x, 40, LEVEL_W - 40); }
        if (f.y < 175 || f.y > 430) { f.ang = -f.ang; f.y = clampf(f.y, 175, 430); }
        if (!bounded && f.x < camX - 150) f.x = camX + VW + frand(50, 500);   // keep fireflies around while running
    }
    for (size_t i = 0; i < flying.size(); ) {
        flying[i].t += dt / 0.55f;
        if (flying[i].t >= 1) flying.erase(flying.begin() + i); else i++;
    }
}

// jumping physics, swing timer, lantern position
void updateBody(float dt)
{
    if (pz > 0 || vzv > 0) {
        vzv -= GRAV * dt;
        pz += vzv * dt;
        if (pz <= 0) { pz = 0; vzv = 0; }
    }
    if (state == 2) streamHop();                   // skip across the stepping stones
    if (cooldown > 0) cooldown -= dt;
    if (swinging) {
        swingT += dt;
        if (swingT >= swingDur) { swinging = false; cooldown = 0.10f; }
    }
    lx = px - face * 13 - vx * 0.04f + sinf(gt * 2.3f) * 2;
    ly = GY + pz + 18;
    if (state == 4 && deathType == 1) ly = GY + 4;   // the lantern drops to the ground when he is bitten
}

void updateCamera(float dt, float target)
{
    camX += (target - camX) * fminf(1.0f, dt * 4.5f);
    if (dt > 0) camVel = (camX - camPrev) / dt;
    camPrev = camX;
}

void tryCatch()
{
    if (!swinging || caught >= NEEDED) return;
    float hx, hy, a, d;
    getNet(hx, hy, a, d);
    for (int i = 0; i < NUM_FF && caught < NEEDED; i++) {
        if (hypotf(ff[i].x - hx, ff[i].y - hy) < 38) {
            Flying fy = { ff[i].x, ff[i].y, 0 };
            flying.push_back(fy);
            caught++;
            hudPulse = 1.0f;
            placeFirefly(ff[i], 1);
        }
    }
    if (caught >= NEEDED) {                        // jar is full
        if (difficulty == 0) checkWin();           // easy : the ending begins
        else if (difficulty == 1) {
            if (lairGot >= MED_NEED) checkWin();
            else showMsg("25 fireflies! The vampire cannot bite you now - find lair mushrooms!");
        } else showMsg("25 fireflies! The witch's curse cannot touch you - go to the cage!");
    }
}

// HARD : catch the wisps around the cage locks with the net
void tryWisps()
{
    if (difficulty != 2 || !swinging) return;
    float hx, hy, a, d;
    getNet(hx, hy, a, d);
    for (int i = 0; i < KIDS; i++) {
        if (kidFree[i]) continue;
        float wx, wy;
        wispPos(i, wx, wy);
        if (hypotf(wx - hx, wy - hy) < 42.0f) {
            if (caught < NEEDED) { wispFlee[i] = 1.0f; showMsg("The wisps flee from weak light - catch 25 fireflies first!"); }
            else if (lairGot < 1) { wispFlee[i] = 1.0f; showMsg("Your net needs a lair mushroom's glow to hold a wisp!"); }
            else {                                  // caught : the lock opens and the child is free
                kidFree[i] = true; freedCount++; lairGot--;
                Flying fy = { wx, wy, 0 };
                flying.push_back(fy);
                hudPulse = 1.0f;
                burst(CAGE_X - 252.0f + 56.0f * i, GY + 14, 0.6f, 1.0f, 0.6f, 14, 160.0f);
                showMsg("A child is free!");
                checkWin();
                if (state != 1) return;
            }
        }
    }
}

// walking into mushrooms picks them up (the traveler bends down a little)
void updatePickups()
{
    if (difficulty == 0) return;
    int a = (int)floorf((px - 60) / 210.0f) - 1, b = (int)floorf((px + 60) / 210.0f) + 1;
    for (int i = a; i <= b; i++) {                 // ordinary little mushrooms along the path
        if (hash01(i, 401) < 0.35f || ordIsPicked(i)) continue;
        float x = i * 210 + hash01(i, 402) * 160;
        if (fabsf(px - x) < 36.0f) {
            ordSetPicked(i); ordCount++; bendT = 0.4f;
            bool teal = hash01(i, 405) < 0.6f;
            burst(x, GY - 6 - hash01(i, 403) * 34 + 8, teal ? 0.3f : 0.9f, teal ? 0.8f : 0.5f, teal ? 0.9f : 0.8f, 8, 90.0f);
        }
    }
    for (int i = 0; i < lmN; i++) {                // lair mushrooms (these count for the goal)
        if (lm[i].got || fabsf(px - lm[i].x) >= 40.0f) continue;
        lm[i].got = true; lairGot++; bendT = 0.4f; hudPulse = 1.0f;
        if (difficulty == 1) burst(lm[i].x, lm[i].y + lm[i].sz, 1.0f, 0.4f, 0.7f, 16, 140.0f);
        else                 burst(lm[i].x, lm[i].y + lm[i].sz, 0.6f, 1.0f, 0.8f, 16, 140.0f);
        showMsg("Lair mushroom collected!");
        checkWin();
        if (state != 1) return;
    }
}

// the vampire / witch chases the traveler
void updateHunter(float dt)
{
    if (difficulty == 0) return;
    if (!hunterOn) {
        hunterWake += dt;
        if (hunterWake >= 9.0f) {
            hunterOn = true; hunterX = px - 950.0f; hunterA = 1.0f;
            showMsg(difficulty == 1 ? "The vampire has awakened... keep moving!" : "The witch is watching... she is coming!");
        }
        return;
    }
    float dir = hunterFace();
    if (hunterKB > 0) { float m = fminf(hunterKB, 900.0f * dt); hunterKB -= m; hunterX -= dir * m; }   // pushed back by a blast
    if (hunterStun > 0) hunterStun -= dt;
    else {
        float sp = (difficulty == 1 ? 78.0f : 70.0f) + fminf(40.0f, fmaxf(0.0f, playT - 9.0f) * 0.35f);
        float gap = fabsf(px - hunterX);
        if (!immune() || gap > 160.0f) hunterX += dir * sp * dt;     // with 25 fireflies he only follows at a distance
    }
    if (!immune() && hunterStun <= 0 && fabsf(hunterX - px) < 38.0f && pz < 55.0f) killPlayer();   // jump over him to be safe
}

void update(float dt)
{
    gt += dt;
    if (fadeA > 0) fadeA = fmaxf(0.0f, fadeA - dt * 1.2f);
    if (hudPulse > 0) hudPulse = fmaxf(0.0f, hudPulse - dt * 2.5f);
    if (bendT > 0) bendT -= dt;
    if (blastT > 0) blastT -= dt;
    if (msgT > 0) msgT -= dt;
    for (int i = 0; i < KIDS; i++) if (wispFlee[i] > 0) wispFlee[i] = fmaxf(0.0f, wispFlee[i] - dt * 1.5f);

    if (state == 3) {
        // ----- menu : slow camera drift over the chosen level's scenery -----
        menuT += dt;
        camX = menuCam() + menuT * 8.0f; camVel = 8;
        px = camX + 430; vx = 0; face = 1;
        updateBody(dt);
    } else if (state == 0) {
        // ----- storyboard : a slow camera pan, the traveler stands and watches -----
        st += dt;
        float base = 0;
        if (difficulty == 0) base = (slide == 0 ? 0.0f : 40.0f);
        else if (difficulty == 1) base = (slide == 0 ? 0.0f : (slide == 2 ? LAIR_X - 760.0f : 40.0f));
        else base = (slide == 0 ? 0.0f : (slide == 1 ? 5050.0f : (slide == 2 ? CAGE_X - 560.0f : 40.0f)));
        camX = base + st * 14;
        camVel = 14;
        px = camX + 430; vx = 0; face = 1;
        if (slide >= 2 && st - demoLast > 2.6f) { demoLast = st; hopSwing(); }   // little demo
        updateBody(dt);
    } else if (state == 1) {
        // ----- playing -----
        playT += dt;
        float ax = (kR ? 1.0f : 0.0f) - (kL ? 1.0f : 0.0f);
        vx += (ax * 230.0f - vx) * fminf(1.0f, dt * 12);
        px = clampf(px + vx * dt, 60, LEVEL_W - 60);
        if (ax != 0 && !swinging) face = ax > 0 ? 1.0f : -1.0f;
        walkPh += fabsf(vx) * dt * 0.045f;
        updateBody(dt);
        updateCamera(dt, clampf(px - VW * 0.45f + face * 70, 0, LEVEL_W - VW));
        tryCatch();
        if (state == 1) tryWisps();
        if (state == 1) updatePickups();
        if (state == 1) updateHunter(dt);
    } else if (state == 4) {
        // ----- game over : bitten (blood) or caught (cage) -----
        deathT += dt;
        vx = 0;
        updateBody(dt);
        updateCamera(dt, camX);
    } else {
        // ----- ending : running home through the lit forest -----
        ct += dt;
        float rem = cabinX - 160 - px;
        float sp = fminf(700.0f, 70.0f + ct * 210.0f);
        if (rem < 1100) sp *= smooth(rem / 1100.0f) * 0.92f + 0.08f;
        sp = fminf(sp, 400.0f + 0.8f * fmaxf(0.0f, fabsf(px - streamCX) - STREAM_HW));   // a little slower over the stones
        if (rem <= 3) { sp = 0; if (!arrived) { arrived = true; hopStart(); } }
        vx += (sp - vx) * fminf(1.0f, dt * 6);
        if (sp == 0 && vx < 5) vx = 0;
        px += vx * dt;
        face = 1;
        walkPh += vx * dt * 0.06f;
        if (ct > 0.35f && !hopped) { hopped = true; hopStart(); }
        if (hunterOn && hunterA > 0) {              // the hunter recoils from the light and fades away
            hunterA = fmaxf(0.0f, hunterA - dt * 0.5f);
            hunterX -= hunterFace() * 350.0f * dt;
        }
        updateBody(dt);
        updateCamera(dt, px - 300);
    }
    updateFireflies(dt);
    updateParticles(dt);
    updateParts(dt);
}

// ------------------------------------------------------------
//  The whole scene (layers from far to near, then light)
// ------------------------------------------------------------
void drawScene()
{
    const Look &lk = LK();
    float frac = (state == 2) ? 1.0f : caught / (float)NEEDED;
    float dark = lk.darkBase, R = 130;
    bool hole = true, showPlayer = true, showFF = true;
    if (state == 3) {                               // menu : just the scenery of the highlighted level
        dark = 0.40f; hole = false; showPlayer = false;
    } else if (state == 0) {
        if (slide == 0) { dark = 0.34f; hole = false; showPlayer = false; showFF = false; }
        else showFF = (slide >= 2);
    } else if (state == 1 || state == 4) {
        dark = lk.darkBase - 0.016f * caught;
        R = 125 + 15 * caught + 5 * sinf(gt * 2.2f);
    } else {
        float k = smooth(ct / 3.5f);
        dark = 0.28f - 0.10f * k;
        R = 520 + k * 1300;
    }
    float sx = lx - camX;                           // lantern on the screen

    // ---------- sky and far scenery ----------
    normalBlend();
    drawSky();
    drawStars();
    drawMoonDisc();
    drawStormClouds();                              // hard only
    drawLayer(1, 0.08f, 50, 296, 36, 150, 215, 22, 32, 0.10f, 0.215f, 0.27f, 0.10f, false);
    fogBand(230, 430, 0.30f);
    drawLayer(2, 0.18f, 64, 246, 26, 200, 290, 30, 44, 0.075f, 0.17f, 0.215f, 0.15f, false);
    fogBand(190, 380, 0.26f);
    drawLayer(3, 0.36f, 90, 205, 18, 270, 380, 40, 60, 0.05f, 0.125f, 0.16f, 0.25f, true);
    fogBand(160, 330, 0.22f);
    drawLayer(4, 0.62f, 140, 176, 10, 380, 540, 58, 84, 0.03f, 0.085f, 0.105f, 0.30f, true);
    fogBand(150, 260, 0.16f);
    drawFarCreek();

    // ---------- the path where the traveler walks ----------
    drawGround();
    drawStream(0);
    drawGrass(1.0f, 26, 8, 20, GY - 40, GY - 2, 601, 0.04f, 0.14f, 0.09f, 0.13f, 0.36f, 0.24f, 1.0f);
    drawMushrooms(0);
    drawLairMush(0);                                // the bright lair mushrooms (medium / hard)
    if (state == 2) { glPushMatrix(); glTranslatef(-camX, 0, 0); drawCabin(0); glPopMatrix(); }
    // ---- level scenery : vampire's crypt (medium), witch's mansion + yard + cage (hard) ----
    glPushMatrix();
    glTranslatef(-camX, 0, 0);
    if (difficulty == 1) drawLair(0);
    if (difficulty == 2) { drawMansion(0); drawYardProps(0); drawCage(0); }
    glPopMatrix();
    drawTrunks(1.0f, 520, 38, 56, GY - 14, 700, 0.95f);
    // ---- bats (medium) and crows (hard) flying in the sky ----
    glPushMatrix();
    glTranslatef(-camX, 0, 0);
    drawBats();
    drawCrows();
    glPopMatrix();
    if (showPlayer) {
        glPushMatrix();
        glTranslatef(-camX, 0, 0);
        drawShadow();
        drawPlayer();
        drawLantern(frac);
        if (state != 4) drawNet();
        if (state == 4 && deathType == 2) drawCaptureCage();
        glPopMatrix();
    }
    // ---- the hunter and the children following home ----
    glPushMatrix();
    glTranslatef(-camX, 0, 0);
    drawKidsFollow(0);
    drawHunter();
    glPopMatrix();
    drawFogBlobs();
    drawLeaves();

    // ---------- things in front of the traveler ----------
    drawGrass(1.3f, 60, 55, 100, -8, 8, 801, 0.004f, 0.015f, 0.015f, 0.012f, 0.04f, 0.035f, 0.95f);
    drawTrunks(1.6f, 1100, 80, 120, -10, 900, 0.97f);

    // ---------- darkness with a hole of light around the lantern ----------
    drawDarkness(sx, ly, R, dark, hole);

    // ---------- light sources on top of the darkness ----------
    additiveBlend();
    glow(790, moonY(), 230, lk.moonGlow[0], lk.moonGlow[1], lk.moonGlow[2], 0.22f);
    glow(790, moonY(), 430, lk.moonGlow2[0], lk.moonGlow2[1], lk.moonGlow2[2], 0.07f);
    normalBlend();
    drawGodRays(state == 2 ? 1.6f : 1.0f);
    if (showPlayer) {
        additiveBlend();
        float gr = fminf(R * 0.9f, 700.0f);
        glow(sx, ly, gr, 1.0f, 0.80f, 0.40f, 0.08f + 0.20f * frac);
        glowEllipse(sx, GY - 6, gr * 0.8f, 22, 1.0f, 0.80f, 0.40f, 0.22f * (0.4f + 0.6f * frac));
        normalBlend();
    }
    additiveBlend();
    drawMushrooms(1);
    drawLairMush(1);
    normalBlend();
    drawStream(1);

    glPushMatrix();
    glTranslatef(-camX, 0, 0);
    // ---- glow of the level scenery ----
    if (difficulty == 1) drawLair(1);
    if (difficulty == 2) { drawMansion(1); drawYardProps(1); drawCage(1); }
    drawBatEyes();
    drawHunterGlow();
    if (showPlayer) drawBackMushGlow();
    drawKidsFollow(1);
    additiveBlend();
    if (showFF)
        for (int k = 0; k < NUM_FF; k++) {
            if (ff[k].x < camX - 70 || ff[k].x > camX + VW + 70) continue;
            drawFirefly(ff[k].x, ff[k].y, ff[k].phase, ff[k].freq);
        }
    normalBlend();
    if (showPlayer) drawFlying();
    if (state == 2) drawCabin(1);
    drawBlast();
    drawParts();
    glPopMatrix();

    if (state == 2) drawSpeedLines(fabsf(vx));
    drawMotes();

    // ---------- golden flash when the jar becomes full ----------
    if (state == 2) {
        additiveBlend();
        float k = smooth(ct / 3.5f);
        float fl = fmaxf(0.0f, 1.0f - ct / 1.4f);
        glColor4f(1.0f, 0.88f, 0.55f, 0.85f * fl + 0.05f * k);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(VW, 0); glVertex2f(VW, VH); glVertex2f(0, VH);
        glEnd();
        normalBlend();
    }
    // ---------- hard : a faint flash of lightning now and then ----------
    if (difficulty == 2 && state != 2) {
        float ph = fmodf(gt, 13.0f);
        float fl = (ph < 0.35f) ? (0.5f + 0.5f * sinf(ph * 40.0f)) * (1.0f - ph / 0.35f) : 0.0f;
        if (fl > 0.0f) {
            additiveBlend();
            glColor4f(0.70f, 0.90f, 0.70f, 0.10f * fl);
            glBegin(GL_QUADS);
            glVertex2f(0, 0); glVertex2f(VW, 0); glVertex2f(VW, VH); glVertex2f(0, VH);
            glEnd();
            normalBlend();
        }
    }
    drawVignette();
    drawDanger();
}

// ------------------------------------------------------------
//  Screens : storyboard, HUD, ending
// ------------------------------------------------------------
void drawStoryUI()
{
    float ta = smooth(st / 1.5f);
    if (slide == 0) {
        const char *names[3] = { "Lost in the Late Night", "Wandering in Twilight", "The Witch's Mansion" };
        float tr = (difficulty == 0) ? 1.0f : (difficulty == 1 ? 1.0f : 0.70f);
        float tg = (difficulty == 0) ? 0.92f : (difficulty == 1 ? 0.62f : 1.0f);
        float tb = (difficulty == 0) ? 0.55f : (difficulty == 1 ? 0.50f : 0.60f);
        fancyTitle(500, 430, 0.95f, "FIREFLIES", tr, tg, tb, ta);
        glColor4f(0.8f, 0.95f, 0.85f, 0.85f * smooth((st - 0.8f) / 1.5f));
        char sub[96];
        sprintf(sub, "a forest lantern tale  -  %s", names[difficulty]);
        drawTextCentered(500, 395, sub, GLUT_BITMAP_TIMES_ROMAN_24);
        drawVine(280, 720, 575, ta * 0.9f);
        drawVine(280, 720, 355, ta * 0.9f);
    }

    // caption box with typewriter text
    normalBlend();
    glColor4f(0.015f, 0.04f, 0.05f, 0.75f);
    roundRect(100, 36, 900, 150, 18, true);
    glLineWidth(2);
    glColor4f(0.55f, 0.85f, 0.70f, 0.45f);
    roundRect(100, 36, 900, 150, 18, false);
    glLineWidth(1);

    int nl = 0;
    for (int j = 0; j < 3; j++) if (slides[slide].l[j][0]) nl++;
    int shown = (int)(st * 42);
    int total = 0;
    for (int j = 0; j < 3; j++) total += (int)strlen(slides[slide].l[j]);
    float y0 = 96 + (nl - 1) * 17.0f;
    glColor4f(0.95f, 0.96f, 0.88f, 1.0f);
    for (int j = 0; j < nl; j++) {
        drawTextPartial(500, y0 - j * 34, slides[slide].l[j], shown, GLUT_BITMAP_TIMES_ROMAN_24);
        shown -= (int)strlen(slides[slide].l[j]);
        if (shown < 0) shown = 0;
    }
    if ((int)(st * 42) >= total) {
        glColor4f(0.85f, 0.95f, 0.9f, 0.45f + 0.4f * sinf(gt * 3));
        drawTextCentered(500, 14, slide < 3 ? "Press SPACE or click to continue" : "Press SPACE or click to begin",
                         GLUT_BITMAP_HELVETICA_12);
    }

    // controls panel on the last slide
    if (slide == 3) {
        glColor4f(0.015f, 0.04f, 0.05f, 0.78f);
        roundRect(560, 330, 975, 640, 16, true);
        glLineWidth(2);
        glColor4f(0.55f, 0.85f, 0.70f, 0.45f);
        roundRect(560, 330, 975, 640, 16, false);
        glLineWidth(1);
        glColor4f(0.95f, 0.88f, 0.50f, 1);
        drawText(582, 608, "CONTROLS", GLUT_BITMAP_HELVETICA_18);
        const char *keys[6] = { "A / D  or  arrows", "W  or  up arrow", "SPACE", "click the traveler", "click anywhere else", "E  or  right-click" };
        const char *acts[6] = { "walk", "hop", "hop + swing net", "hop + swing net", "swing net only", "mushroom blast" };
        int nrows = (difficulty == 0) ? 5 : 6;
        float step = (difficulty == 0) ? 35.0f : 31.0f;
        for (int i = 0; i < nrows; i++) {
            glColor4f(0.85f, 0.95f, 0.90f, 1);
            drawText(582, 570 - i * step, keys[i], GLUT_BITMAP_HELVETICA_18);
            glColor4f(0.60f, 0.90f, 0.70f, 1);
            drawText(790, 570 - i * step, acts[i], GLUT_BITMAP_HELVETICA_18);
        }
        glColor4f(0.95f, 0.88f, 0.50f, 1);
        if (difficulty == 0) {
            drawText(582, 385, "Goal: catch 25 fireflies,", GLUT_BITMAP_HELVETICA_18);
            drawText(582, 358, "then run home!", GLUT_BITMAP_HELVETICA_18);
        } else if (difficulty == 1) {
            drawText(582, 378, "Goal: 25 fireflies, 5 lair mushrooms,", GLUT_BITMAP_HELVETICA_18);
            drawText(582, 351, "then run home!", GLUT_BITMAP_HELVETICA_18);
        } else {
            drawText(582, 378, "Goal: 25 fireflies, 10 mushrooms,", GLUT_BITMAP_HELVETICA_18);
            drawText(582, 351, "free all 10 children, run home!", GLUT_BITMAP_HELVETICA_18);
        }
    }
}

void drawPlayHUD()
{
    float s = 0.95f * (1.0f + 0.18f * hudPulse);
    drawJar(62, 640, s, caught, caught / (float)NEEDED, 1.0f);
    char buf[64];
    sprintf(buf, "%d / %d", caught, NEEDED);
    glColor4f(0.9f, 1.0f, 0.55f, 1.0f);
    drawText(100, 632, buf, GLUT_BITMAP_TIMES_ROMAN_24);
    glColor4f(0.75f, 0.9f, 0.8f, 0.8f);
    drawText(100, 612, "fireflies", GLUT_BITMAP_HELVETICA_12);

    // ---- medium / hard : mushrooms, children, immunity ----
    if (difficulty == 1) {
        sprintf(buf, "lair mushrooms  %d / %d", lairGot, MED_NEED);
        glColor4f(1.0f, 0.55f, 0.80f, 1.0f);
        drawText(100, 586, buf, GLUT_BITMAP_HELVETICA_18);
    } else if (difficulty == 2) {
        sprintf(buf, "lair mushrooms  %d", lairGot);
        glColor4f(0.65f, 1.0f, 0.80f, 1.0f);
        drawText(100, 586, buf, GLUT_BITMAP_HELVETICA_18);
        sprintf(buf, "children freed  %d / %d", freedCount, KIDS);
        glColor4f(1.0f, 0.92f, 0.60f, 1.0f);
        drawText(100, 562, buf, GLUT_BITMAP_HELVETICA_18);
    }
    if (difficulty != 0 && immune()) {
        glColor4f(1.0f, 0.90f, 0.45f, 0.6f + 0.3f * sinf(gt * 3));
        drawText(100, difficulty == 2 ? 540 : 564, difficulty == 1 ? "immune to his bite!" : "immune to her curse!", GLUT_BITMAP_HELVETICA_12);
    }

    if (caught == 0 && playT < 16) {
        float a = clampf((16 - playT) / 3.0f, 0, 1);
        glColor4f(0.9f, 0.95f, 0.9f, a * 0.9f);
        drawTextCentered(500, 62, "A / D walk   -   W hop   -   SPACE or click yourself: hop + swing   -   click anywhere: swing the net",
                         GLUT_BITMAP_HELVETICA_12);
        if (difficulty != 0)
            drawTextCentered(500, 44, "E or right-click: mushroom blast   -   walk into mushrooms to pick them   -   M: menu",
                             GLUT_BITMAP_HELVETICA_12);
    }

    // message in the middle of the screen
    if (msgT > 0 && msgBuf[0]) {
        float a = clampf(msgT, 0.0f, 1.0f);
        glColor4f(0.0f, 0.0f, 0.0f, 0.7f * a);
        drawTextCentered(501, 538, msgBuf, GLUT_BITMAP_HELVETICA_18);
        glColor4f(1.0f, 0.95f, 0.70f, a);
        drawTextCentered(500, 540, msgBuf, GLUT_BITMAP_HELVETICA_18);
    }
}

void drawEndUI()
{
    float a = smooth((ct - 2.0f) / 1.6f);
    vgrad(0, 250, VW, 375, 0.01f, 0.03f, 0.03f, 0.0f, 0.01f, 0.03f, 0.03f, 0.62f * a);
    vgrad(0, 375, VW, 500, 0.01f, 0.03f, 0.03f, 0.62f * a, 0.01f, 0.03f, 0.03f, 0.0f);
    drawVine(170, 830, 478, a * 0.95f);
    drawVine(170, 830, 262, a * 0.95f);
    fancyTitle(500, 385, 0.5f, "Congratulations!", 1.0f, 0.93f, 0.55f, a);
    glColor4f(0.92f, 1.0f, 0.85f, a);
    if (difficulty == 2) drawTextCentered(500, 335, "You freed all ten children!", GLUT_BITMAP_TIMES_ROMAN_24);
    else                 drawTextCentered(500, 335, "You found your way home.", GLUT_BITMAP_TIMES_ROMAN_24);
    glColor4f(0.80f, 0.92f, 0.82f, a * 0.9f);
    if (difficulty == 0)      drawTextCentered(500, 305, "Your lantern turned the dark forest into a golden path.", GLUT_BITMAP_HELVETICA_18);
    else if (difficulty == 1) drawTextCentered(500, 305, "Your lantern and the glowing mushrooms drove the vampire away.", GLUT_BITMAP_HELVETICA_18);
    else                      drawTextCentered(500, 305, "Together you followed the golden light home.", GLUT_BITMAP_HELVETICA_18);
    if (ct > 9) {
        glColor4f(0.9f, 0.95f, 0.9f, 0.5f + 0.4f * sinf(gt * 3));
        drawTextCentered(500, 40, "Press R to play again   -   M for the menu", GLUT_BITMAP_HELVETICA_18);
    }
}

// ---- the difficulty menu ----
void drawMenuUI()
{
    float tr = (menuSel == 0) ? 1.0f : (menuSel == 1 ? 1.0f : 0.70f);
    float tg = (menuSel == 0) ? 0.92f : (menuSel == 1 ? 0.62f : 1.0f);
    float tb = (menuSel == 0) ? 0.55f : (menuSel == 1 ? 0.50f : 0.60f);
    fancyTitle(500, 585, 0.95f, "FIREFLIES", tr, tg, tb, 1.0f);
    glColor4f(0.85f, 0.95f, 0.90f, 0.9f);
    drawTextCentered(500, 548, "choose your path through the forest", GLUT_BITMAP_TIMES_ROMAN_24);
    drawVine(250, 750, 520, 0.8f);

    const char *labels[3] = { "EASY  -  Lost in the Late Night", "MEDIUM  -  Wandering in Twilight", "HARD  -  The Witch's Mansion" };
    const char *descs[3]  = { "Catch 25 fireflies, then run home.",
                              "Vampire! 25 fireflies + 5 lair mushrooms.",
                              "Witch! 25 fireflies + 10 mushrooms + rescue." };
    const float bc[3][3] = { { 0.55f, 0.90f, 0.70f }, { 1.0f, 0.55f, 0.65f }, { 0.65f, 1.0f, 0.60f } };
    for (int i = 0; i < 3; i++) {
        float y0 = 400.0f - i * 110.0f, y1 = y0 + 90.0f;
        bool sel = (i == menuSel);
        normalBlend();
        glColor4f(0.015f, 0.04f, 0.05f, sel ? 0.88f : 0.70f);
        roundRect(250, y0, 750, y1, 16, true);
        glLineWidth(sel ? 3.0f : 2.0f);
        glColor4f(bc[i][0], bc[i][1], bc[i][2], sel ? 0.95f : 0.45f);
        roundRect(250, y0, 750, y1, 16, false);
        glLineWidth(1);
        if (sel) {
            additiveBlend();
            glowEllipse(500, (y0 + y1) / 2, 330, 70, bc[i][0], bc[i][1], bc[i][2], 0.18f);
            normalBlend();
            glColor4f(bc[i][0], bc[i][1], bc[i][2], 1.0f);
            glBegin(GL_TRIANGLES);                  // little arrow
            glVertex2f(266, (y0 + y1) / 2 + 9); glVertex2f(266, (y0 + y1) / 2 - 9); glVertex2f(280, (y0 + y1) / 2);
            glEnd();
        }
        glColor4f(sel ? 1.0f : 0.80f, sel ? 0.95f : 0.85f, sel ? 0.70f : 0.80f, sel ? 1.0f : 0.8f);
        drawTextCentered(500, y0 + 52, labels[i], GLUT_BITMAP_TIMES_ROMAN_24);
        glColor4f(bc[i][0], bc[i][1], bc[i][2], sel ? 1.0f : 0.7f);
        drawTextCentered(500, y0 + 22, descs[i], GLUT_BITMAP_HELVETICA_18);
    }
    glColor4f(0.85f, 0.95f, 0.9f, 0.6f + 0.3f * sinf(gt * 3));
    drawTextCentered(500, 40, "W / S or arrows: choose   -   ENTER / SPACE / click: start   -   ESC: quit", GLUT_BITMAP_HELVETICA_12);
}

// a blood splat on the screen glass
void splat(float cx, float cy, float r, float a)
{
    glColor4f(0.45f, 0.0f, 0.03f, a);
    circleFill(cx, cy, r, 18);
    for (int k = 0; k < 9; k++) {
        float an = hash01(k, (int)cx) * 2 * PI, len = r * (1.2f + hash01(k, (int)cy) * 0.9f);
        float dx = cosf(an), dy = sinf(an);
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - dy * r * 0.25f, cy + dx * r * 0.25f);
        glVertex2f(cx + dy * r * 0.25f, cy - dx * r * 0.25f);
        glVertex2f(cx + dx * len, cy + dy * len);
        glEnd();
        circleFill(cx + dx * len * 1.12f, cy + dy * len * 1.12f, r * 0.10f, 8);
    }
    glBegin(GL_QUADS);                              // a drip running down
    glVertex2f(cx - 4, cy); glVertex2f(cx + 4, cy);
    glVertex2f(cx + 3, cy - r * 1.6f); glVertex2f(cx - 3, cy - r * 1.6f);
    glEnd();
}

void drawDeathUI()
{
    normalBlend();
    if (deathType == 1) {                           // vampire : red flash and blood splats
        float fl = fmaxf(0.0f, 1.0f - deathT / 0.5f);
        glColor4f(0.6f, 0.0f, 0.04f, 0.55f * fl);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(VW, 0); glVertex2f(VW, VH); glVertex2f(0, VH);
        glEnd();
        const float sp[6][3] = { { 120, 520, 70 }, { 860, 560, 90 }, { 430, 620, 55 }, { 700, 200, 75 }, { 240, 240, 60 }, { 520, 430, 50 } };
        for (int i = 0; i < 6; i++) {
            float t = smooth((deathT - 0.1f * i) / 0.25f);
            if (t > 0) splat(sp[i][0], sp[i][1], sp[i][2] * t, 0.80f * t);
        }
    } else {                                        // witch : purple flash
        float fl = fmaxf(0.0f, 1.0f - deathT / 0.8f);
        additiveBlend();
        glColor4f(0.55f, 0.30f, 0.80f, 0.35f * fl);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(VW, 0); glVertex2f(VW, VH); glVertex2f(0, VH);
        glEnd();
        normalBlend();
    }
    float a = smooth((deathT - 1.0f) / 1.2f);
    vgrad(0, 250, VW, 375, 0.02f, 0.0f, 0.02f, 0.0f, 0.02f, 0.0f, 0.02f, 0.70f * a);
    vgrad(0, 375, VW, 500, 0.02f, 0.0f, 0.02f, 0.70f * a, 0.02f, 0.0f, 0.02f, 0.0f);
    if (deathType == 1) {
        fancyTitle(500, 385, 0.5f, "YOU WERE BITTEN", 1.0f, 0.25f, 0.30f, a);
        glColor4f(0.95f, 0.85f, 0.85f, a);
        drawTextCentered(500, 335, "The vampire's lair claims another traveler.", GLUT_BITMAP_TIMES_ROMAN_24);
    } else {
        fancyTitle(500, 385, 0.5f, "CAUGHT BY THE WITCH", 0.75f, 0.50f, 1.0f, a);
        glColor4f(0.90f, 0.95f, 0.85f, a);
        drawTextCentered(500, 335, "You join the stolen children in her cage.", GLUT_BITMAP_TIMES_ROMAN_24);
    }
    if (deathT > 2.0f) {
        glColor4f(0.9f, 0.95f, 0.9f, 0.5f + 0.4f * sinf(gt * 3));
        drawTextCentered(500, 40, "Press R to try again   -   M for the menu", GLUT_BITMAP_HELVETICA_18);
    }
}

// ------------------------------------------------------------
//  Display
// ------------------------------------------------------------
void display()
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    normalBlend();

    drawScene();
    if (state == 0) drawStoryUI();
    else if (state == 1) drawPlayHUD();
    else if (state == 2) drawEndUI();
    else if (state == 3) drawMenuUI();
    else { drawPlayHUD(); drawDeathUI(); }

    if (fadeA > 0.001f) {                           // fade in from black
        normalBlend();
        glColor4f(0, 0, 0, fadeA);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(VW, 0); glVertex2f(VW, VH); glVertex2f(0, VH);
        glEnd();
    }
    glutSwapBuffers();
}

// ------------------------------------------------------------
//  GLUT callbacks
// ------------------------------------------------------------
void reshape(int w, int h)
{
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, VW, 0, VH);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void timer(int)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastMs) / 1000.0f;
    lastMs = now;
    if (dt > 0.05f) dt = 0.05f;
    if (dt > 0) update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void keyDown(unsigned char k, int, int)
{
    if (k == 27) exit(0);
    int c = tolower(k);
    if (state == 3) {                               // menu keys
        if (c == '1') setMenuSel(0);
        else if (c == '2') setMenuSel(1);
        else if (c == '3') setMenuSel(2);
        else if (c == 'w') setMenuSel((menuSel + 2) % 3);
        else if (c == 's') setMenuSel((menuSel + 1) % 3);
        else if (k == ' ' || k == '\r') startStory(menuSel);
        return;
    }
    if (c == 'm') { goMenu(); return; }             // back to the menu from anywhere
    if (state == 0) { if (k == ' ' || k == '\r') advanceSlide(); return; }
    if (c == 'r') { startGame(); return; }
    if (state != 1) return;
    switch (c) {
        case 'a': kL = true; break;
        case 'd': kR = true; break;
        case 'w': hopStart(); break;
        case ' ': hopSwing(); break;
        case 'f': netStart(face > 0 ? 0.3f : PI - 0.3f); break;
        case 'e': doBlast(); break;                 // mushroom blast
    }
}

void keyUp(unsigned char k, int, int)
{
    int c = tolower(k);
    if (c == 'a') kL = false;
    if (c == 'd') kR = false;
}

void specialDown(int k, int, int)
{
    if (state == 3) {                               // menu : arrows move the highlight
        if (k == GLUT_KEY_UP) setMenuSel((menuSel + 2) % 3);
        if (k == GLUT_KEY_DOWN) setMenuSel((menuSel + 1) % 3);
        return;
    }
    if (state != 1) return;
    if (k == GLUT_KEY_LEFT) kL = true;
    if (k == GLUT_KEY_RIGHT) kR = true;
    if (k == GLUT_KEY_UP) hopStart();
}

void specialUp(int k, int, int)
{
    if (k == GLUT_KEY_LEFT) kL = false;
    if (k == GLUT_KEY_RIGHT) kR = false;
}

void passive(int x, int y)                          // mouse moving over the menu
{
    if (state != 3) return;
    float wxm = x * (VW / (float)winW), wym = (winH - y) * (VH / (float)winH);
    int b = menuHit(wxm, wym);
    if (b >= 0) setMenuSel(b);
}

void mouse(int button, int s, int x, int y)
{
    if (s != GLUT_DOWN) return;
    if (state == 3) {                               // menu click
        if (button == GLUT_LEFT_BUTTON) {
            float mx = x * (VW / (float)winW), my = (winH - y) * (VH / (float)winH);
            int b = menuHit(mx, my);
            if (b >= 0) startStory(b);
        }
        return;
    }
    if (button == GLUT_RIGHT_BUTTON) { if (state == 1) doBlast(); return; }
    if (button != GLUT_LEFT_BUTTON) return;
    if (state == 0) { advanceSlide(); return; }
    if (state != 1) return;
    float wx = camX + x * (VW / (float)winW);
    float wy = (winH - y) * (VH / (float)winH);
    if (hypotf(wx - px, wy - (GY + pz + 40)) < 48) hopSwing();      // click on the traveler
    else netStart(atan2f(wy - handY(), wx - handX()));             // click elsewhere: net only
}

// ------------------------------------------------------------
//  main
// ------------------------------------------------------------
int main(int argc, char **argv)
{
    srand((unsigned)time(NULL));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(VW, VH);
    glutInitWindowPosition(100, 30);
    glutCreateWindow("Fireflies - A Forest Lantern Tale");

    glEnable(GL_BLEND);
    normalBlend();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // ================= BACKGROUND MUSIC (optional) =================
    // To add your own music:
    //  1) At the very top of this file, under the other #include lines, add these two lines:
    //         #include <windows.h>
    //         #include <mmsystem.h>
    //     (-lwinmm is already in your linker options, so nothing else is needed.)
    //  2) Remove the // at the start of the two lines below and put the path of YOUR music file
    //     in place of  C:\\Users\\user\\Downloads\\YOUR_MUSIC_FILE.mp3  (keep the double backslashes).
    //     "repeat" makes the music loop forever.
    //
    // mciSendStringA("open \"C:\\Users\\user\\Downloads\\YOUR_MUSIC_FILE.mp3\" type mpegvideo alias bgm", NULL, 0, NULL);
    // mciSendStringA("play bgm repeat", NULL, 0, NULL);
    //
    //  If your file is a .wav file, use this single line instead of the two lines above:
    // PlaySound(TEXT("C:\\Users\\user\\Downloads\\YOUR_MUSIC_FILE.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    // ================================================================

    initWorld();
    difficulty = 0; slides = slideSets[0];
    resetGame();
    state = 3; menuSel = 0; menuT = 0; fadeA = 1.0f;     // the game starts at the level menu

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(passive);
    glutIgnoreKeyRepeat(1);

    lastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();
    return 0;
}
