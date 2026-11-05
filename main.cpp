// ============================================================
//  FIREFLIES - A Forest Lantern Tale   (version 2)
//  CSE 402 : Computer Graphics & Image Processing Lab
//  C++ / OpenGL (FreeGLUT) / Code::Blocks
//
//  Story : You are lost in a dark forest. Catch 25 fireflies with
//          your net and keep them in your lantern jar. When the
//          jar shines bright enough, the forest lights up and you
//          run home.
//
//  Controls: A / D or Left / Right arrows = walk
//            W or Up arrow                = hop
//            SPACE                        = hop + swing net
//            Click on the traveler        = hop + swing net
//            Click anywhere else          = swing the net only
//            R = restart, ESC = quit
// ============================================================
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cctype>
#include <vector>

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

// ------------------------------------------------------------
//  Data
// ------------------------------------------------------------
struct Firefly { float x, y, ang, speed, phase, freq; };
struct Flying  { float sx, sy, t; };              // caught firefly flying to the lantern
struct Mote    { float x, y, vx, vy, ph, sz; };   // floating dust / pollen
struct Leaf    { float x, y, vx, vy, rot, vr, sz, hue; };
struct Slide   { const char *l[3]; };

Slide slides[4] = {
    {{ "Night falls over the old forest...", "", "" }},
    {{ "You wandered too far, and now the path is gone.",
       "Without any light, every way looks the same.", "" }},
    {{ "But the fireflies are still awake,",
       "dancing between the trees...", "" }},
    {{ "Catch 25 fireflies with your net and keep them in your lantern jar.",
       "When it shines bright enough, you will see the way home.", "" }}
};

Firefly ff[NUM_FF];
std::vector<Flying> flying;
Mote  motes[70];
Leaf  leaves[22];
float jfx[NEEDED], jfy[NEEDED];                   // spots of fireflies inside the jar

int   state = 0;                                  // 0 = storyboard, 1 = playing, 2 = ending
int   slide = 0;
float st = 0, fadeA = 1.0f, demoLast = 0;

float px = 200, pz = 0, vzv = 0, vx = 0, face = 1, walkPh = 0;
bool  kL = false, kR = false;
bool  swinging = false;
float swingT = 0, swingAim = 0, cooldown = 0;

int   caught = 0;
float gt = 0, ct = 0, playT = 0, hudPulse = 0;
float camX = 0, camPrev = 0, camVel = 0;
float cabinX = 0;
bool  arrived = false, hopped = false;
float lx = 0, ly = 0;                             // lantern position (world)
int   winW = VW, winH = VH, lastMs = 0;

// ------------------------------------------------------------
//  Small helpers
// ------------------------------------------------------------
float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
float wrapf(float v, float m) { return v - floorf(v / m) * m; }

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
void drawSky()
{
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
        glColor4f(0.8f, 0.92f, 1.0f, 0.25f + 0.6f * tw * hash01(i, 205));
        glVertex2f(sx, sy);
        glEnd();
    }
}

void drawMoonDisc()
{
    glColor3f(0.93f, 0.96f, 0.88f);
    circleFill(790, 590, 36, 40);
    glColor4f(0.70f, 0.78f, 0.70f, 0.35f);
    circleFill(778, 598, 8, 14);
    circleFill(802, 578, 6, 12);
    circleFill(796, 604, 4, 10);
}

// ------------------------------------------------------------
//  Fog
// ------------------------------------------------------------
void fogBand(float yLow, float yHigh, float a)
{
    vgrad(0, yLow, VW, yHigh, 0.20f, 0.34f, 0.38f, a, 0.20f, 0.34f, 0.38f, 0.0f);
}

void drawFogBlobs()
{
    normalBlend();
    for (int i = 0; i < 12; i++) {
        float sp = 6.0f + (i % 4) * 3.0f;
        float par = 0.5f + 0.1f * (i % 5);
        float x = wrapf(i * 210.0f + gt * sp - camX * par, VW + 700) - 350;
        float y = GY + 10 + ((i * 53) % 150);
        float rx = 260.0f + (i % 3) * 80.0f, ry = 34.0f + (i % 4) * 10.0f;
        glowEllipse(x, y, rx, ry, 0.42f, 0.58f, 0.62f, 0.075f);
    }
}

// slanted beams of moonlight (drawn with additive blending)
void drawGodRays(float strength)
{
    additiveBlend();
    for (int k = 0; k < 6; k++) {
        float tx = 640 + k * 60;
        float bx = tx - 430;
        float a = (0.05f + 0.03f * sinf(gt * 0.5f + k * 1.3f)) * strength;
        glBegin(GL_QUADS);
        glColor4f(0.6f, 0.82f, 0.9f, a);
        glVertex2f(tx - 12, VH); glVertex2f(tx + 12, VH);
        glColor4f(0.6f, 0.82f, 0.9f, 0.0f);
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
    vgrad(0, 0, VW, GY, 0.02f, 0.05f, 0.05f, 1, 0.09f, 0.21f, 0.16f, 1);
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
    const int N = 64;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float a = 2 * PI * i / N, c = cosf(a), s = sinf(a);
        glColor4f(0.01f, 0.02f, 0.06f, aa); glVertex2f(cx + c * ra, cy + s * ra);
        glColor4f(0.01f, 0.02f, 0.06f, ab); glVertex2f(cx + c * rb, cy + s * rb);
    }
    glEnd();
}

void drawDarkness(float cx, float cy, float R, float dark, bool hole)
{
    normalBlend();
    if (!hole) {
        glColor4f(0.01f, 0.02f, 0.06f, dark);
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
    float dir = (cosf(swingAim) >= 0) ? 1.0f : -1.0f;
    if (swinging) {
        ang = swingAim + dir * (swingT / SWING_DUR * 2.0f - 1.0f) * 1.2f;
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
    float dir = (cosf(swingAim) >= 0) ? 1.0f : -1.0f;

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
}

// ---- actions ----
void hopStart()
{
    if (pz <= 0.01f && vzv == 0) vzv = JUMP_V;
}

void netStart(float aim)
{
    if (swinging || cooldown > 0) return;
    swinging = true; swingT = 0; swingAim = aim;
    if (fabsf(cosf(aim)) > 0.2f) face = (cosf(aim) > 0) ? 1.0f : -1.0f;
}

void hopSwing()
{
    hopStart();
    netStart(face > 0 ? 0.5f : PI - 0.5f);
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
    if (cooldown > 0) cooldown -= dt;
    if (swinging) {
        swingT += dt;
        if (swingT >= SWING_DUR) { swinging = false; cooldown = 0.10f; }
    }
    lx = px - face * 13 - vx * 0.04f + sinf(gt * 2.3f) * 2;
    ly = GY + pz + 18;
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
    if (caught >= NEEDED) {                        // jar is full: the ending begins
        state = 2; ct = 0; swinging = false;
        cabinX = px + 7000; arrived = false; hopped = false;
        kL = kR = false;
    }
}

void update(float dt)
{
    gt += dt;
    if (fadeA > 0) fadeA = fmaxf(0.0f, fadeA - dt * 1.2f);
    if (hudPulse > 0) hudPulse = fmaxf(0.0f, hudPulse - dt * 2.5f);

    if (state == 0) {
        // ----- storyboard : a slow camera pan, the traveler stands and watches -----
        st += dt;
        camX = (slide == 0 ? 0 : 40) + st * 14;
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
    } else {
        // ----- ending : running home through the lit forest -----
        ct += dt;
        float rem = cabinX - 160 - px;
        float sp = fminf(700.0f, 70.0f + ct * 210.0f);
        if (rem < 1100) sp *= smooth(rem / 1100.0f) * 0.92f + 0.08f;
        if (rem <= 3) { sp = 0; if (!arrived) { arrived = true; hopStart(); } }
        vx += (sp - vx) * fminf(1.0f, dt * 6);
        if (sp == 0 && vx < 5) vx = 0;
        px += vx * dt;
        face = 1;
        walkPh += vx * dt * 0.06f;
        if (ct > 0.35f && !hopped) { hopped = true; hopStart(); }
        updateBody(dt);
        updateCamera(dt, px - 300);
    }
    updateFireflies(dt);
    updateParticles(dt);
}

// ------------------------------------------------------------
//  The whole scene (layers from far to near, then light)
// ------------------------------------------------------------
void drawScene()
{
    float frac = (state == 2) ? 1.0f : caught / (float)NEEDED;
    float dark = 0.68f, R = 130;
    bool hole = true, showPlayer = true, showFF = true;
    if (state == 0) {
        if (slide == 0) { dark = 0.34f; hole = false; showPlayer = false; showFF = false; }
        else showFF = (slide >= 2);
    } else if (state == 1) {
        dark = 0.68f - 0.016f * caught;
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
    drawLayer(1, 0.08f, 50, 296, 36, 150, 215, 22, 32, 0.10f, 0.215f, 0.27f, 0.10f, false);
    fogBand(230, 430, 0.30f);
    drawLayer(2, 0.18f, 64, 246, 26, 200, 290, 30, 44, 0.075f, 0.17f, 0.215f, 0.15f, false);
    fogBand(190, 380, 0.26f);
    drawLayer(3, 0.36f, 90, 205, 18, 270, 380, 40, 60, 0.05f, 0.125f, 0.16f, 0.25f, true);
    fogBand(160, 330, 0.22f);
    drawLayer(4, 0.62f, 140, 176, 10, 380, 540, 58, 84, 0.03f, 0.085f, 0.105f, 0.30f, true);
    fogBand(150, 260, 0.16f);

    // ---------- the path where the traveler walks ----------
    drawGround();
    drawGrass(1.0f, 26, 8, 20, GY - 40, GY - 2, 601, 0.04f, 0.14f, 0.09f, 0.13f, 0.36f, 0.24f, 1.0f);
    drawMushrooms(0);
    if (state == 2) { glPushMatrix(); glTranslatef(-camX, 0, 0); drawCabin(0); glPopMatrix(); }
    drawTrunks(1.0f, 520, 38, 56, GY - 14, 700, 0.95f);
    if (showPlayer) {
        glPushMatrix();
        glTranslatef(-camX, 0, 0);
        drawShadow();
        drawPlayer();
        drawLantern(frac);
        drawNet();
        glPopMatrix();
    }
    drawFogBlobs();
    drawLeaves();

    // ---------- things in front of the traveler ----------
    drawGrass(1.3f, 60, 55, 100, -8, 8, 801, 0.004f, 0.015f, 0.015f, 0.012f, 0.04f, 0.035f, 0.95f);
    drawTrunks(1.6f, 1100, 80, 120, -10, 900, 0.97f);

    // ---------- darkness with a hole of light around the lantern ----------
    drawDarkness(sx, ly, R, dark, hole);

    // ---------- light sources on top of the darkness ----------
    additiveBlend();
    glow(790, 590, 230, 0.70f, 0.85f, 1.0f, 0.22f);
    glow(790, 590, 430, 0.50f, 0.70f, 0.90f, 0.07f);
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
    normalBlend();

    glPushMatrix();
    glTranslatef(-camX, 0, 0);
    additiveBlend();
    if (showFF)
        for (int k = 0; k < NUM_FF; k++) {
            if (ff[k].x < camX - 70 || ff[k].x > camX + VW + 70) continue;
            drawFirefly(ff[k].x, ff[k].y, ff[k].phase, ff[k].freq);
        }
    normalBlend();
    if (showPlayer) drawFlying();
    if (state == 2) drawCabin(1);
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
    drawVignette();
}

// ------------------------------------------------------------
//  Screens : storyboard, HUD, ending
// ------------------------------------------------------------
void drawStoryUI()
{
    float ta = smooth(st / 1.5f);
    if (slide == 0) {
        fancyTitle(500, 430, 0.95f, "FIREFLIES", 1.0f, 0.92f, 0.55f, ta);
        glColor4f(0.8f, 0.95f, 0.85f, 0.85f * smooth((st - 0.8f) / 1.5f));
        drawTextCentered(500, 395, "a forest lantern tale", GLUT_BITMAP_TIMES_ROMAN_24);
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
        const char *keys[5] = { "A / D  or  arrows", "W  or  up arrow", "SPACE", "click the traveler", "click anywhere else" };
        const char *acts[5] = { "walk", "hop", "hop + swing net", "hop + swing net", "swing net only" };
        for (int i = 0; i < 5; i++) {
            glColor4f(0.85f, 0.95f, 0.90f, 1);
            drawText(582, 570 - i * 35.0f, keys[i], GLUT_BITMAP_HELVETICA_18);
            glColor4f(0.60f, 0.90f, 0.70f, 1);
            drawText(790, 570 - i * 35.0f, acts[i], GLUT_BITMAP_HELVETICA_18);
        }
        glColor4f(0.95f, 0.88f, 0.50f, 1);
        drawText(582, 385, "Goal: catch 25 fireflies,", GLUT_BITMAP_HELVETICA_18);
        drawText(582, 358, "then run home!", GLUT_BITMAP_HELVETICA_18);
    }
}

void drawPlayHUD()
{
    float s = 0.95f * (1.0f + 0.18f * hudPulse);
    drawJar(62, 640, s, caught, caught / (float)NEEDED, 1.0f);
    char buf[32];
    sprintf(buf, "%d / %d", caught, NEEDED);
    glColor4f(0.9f, 1.0f, 0.55f, 1.0f);
    drawText(100, 632, buf, GLUT_BITMAP_TIMES_ROMAN_24);
    glColor4f(0.75f, 0.9f, 0.8f, 0.8f);
    drawText(100, 612, "fireflies", GLUT_BITMAP_HELVETICA_12);

    if (caught == 0 && playT < 16) {
        float a = clampf((16 - playT) / 3.0f, 0, 1);
        glColor4f(0.9f, 0.95f, 0.9f, a * 0.9f);
        drawTextCentered(500, 62, "A / D walk   -   W hop   -   SPACE or click yourself: hop + swing   -   click anywhere: swing the net",
                         GLUT_BITMAP_HELVETICA_12);
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
    drawTextCentered(500, 335, "You found your way home.", GLUT_BITMAP_TIMES_ROMAN_24);
    glColor4f(0.80f, 0.92f, 0.82f, a * 0.9f);
    drawTextCentered(500, 305, "Your lantern turned the dark forest into a golden path.", GLUT_BITMAP_HELVETICA_18);
    if (ct > 9) {
        glColor4f(0.9f, 0.95f, 0.9f, 0.5f + 0.4f * sinf(gt * 3));
        drawTextCentered(500, 40, "Press R to play again", GLUT_BITMAP_HELVETICA_18);
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
    else drawEndUI();

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
    if (state == 0) { if (k == ' ' || k == '\r') advanceSlide(); return; }
    if (c == 'r') { startGame(); return; }
    if (state != 1) return;
    switch (c) {
        case 'a': kL = true; break;
        case 'd': kR = true; break;
        case 'w': hopStart(); break;
        case ' ': hopSwing(); break;
        case 'f': netStart(face > 0 ? 0.3f : PI - 0.3f); break;
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

void mouse(int button, int s, int x, int y)
{
    if (button != GLUT_LEFT_BUTTON || s != GLUT_DOWN) return;
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

    initWorld();
    resetGame();
    state = 0; slide = 0; st = 0; fadeA = 1.0f;

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouse);
    glutIgnoreKeyRepeat(1);

    lastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();
    return 0;
}