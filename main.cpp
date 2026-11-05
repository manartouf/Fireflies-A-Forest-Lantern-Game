// ============================================================
//  FIREFLIES - A Forest Lantern Game
//  CSE 402 : Computer Graphics & Image Processing Lab
//  C++ / OpenGL (FreeGLUT) / Code::Blocks
//
//  Story : You are lost in a dark forest. Catch 25 fireflies with
//          your net and put them in the jar. Once the jar is full
//          it shines bright enough to show the way out.
//
//  Controls: W A S D / Arrow keys = walk
//            SPACE or Left Mouse  = swing the net
//            R = restart, ESC = quit
// ============================================================
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cctype>
#include <vector>
#include <algorithm>

// ------------------------------------------------------------
//  Constants
// ------------------------------------------------------------
const float PI = 3.14159265f;
const int   VW = 1000, VH = 700;                 // virtual screen size
const float WORLD_W = 3200.0f, WORLD_H = 1800.0f;
const int   NEEDED = 25;                         // fireflies needed
const int   NUM_FIREFLIES = 40;                  // fireflies alive in the forest
const float EXIT_X = 3000.0f, EXIT_Y = 1450.0f;  // forest exit
const float JAR_X = 95.0f, JAR_Y = 125.0f;       // jar centre on screen

// ------------------------------------------------------------
//  Data structures
// ------------------------------------------------------------
struct Tree     { float x, y, s, phase, g; bool solid; };
struct Firefly  { float x, y, ang, speed, phase, freq; };
struct Flying   { float sx, sy, t; };            // caught firefly flying to jar
struct Tuft     { float x, y, s, phase; };
struct Shroom   { float x, y, r, phase; };

// ------------------------------------------------------------
//  Globals
// ------------------------------------------------------------
std::vector<Tree>    trees;
std::vector<Tuft>    tufts;
std::vector<Shroom>  shrooms;
std::vector<Flying>  flying;
Firefly fl[NUM_FIREFLIES];

float px = 250, py = 250, face = 0;              // player (feet position)
bool  moving = false;
bool  kL = false, kR = false, kU = false, kD = false;

bool  swinging = false;
float swingT = 0, swingBase = 0, cooldown = 0;
const float SWING_DUR = 0.32f;

int   caught = 0;
int   state = 0;                                 // 0 = playing, 1 = won
float winT = 0, msgT = 0, runT = 0, gt = 0;
float camx = 0, camy = 0;
int   winW = VW, winH = VH;
int   lastMs = 0;
float jfx[NEEDED], jfy[NEEDED];                  // spots of fireflies inside the jar

// ------------------------------------------------------------
//  Small helpers
// ------------------------------------------------------------
float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
bool  treeCmp(const Tree &a, const Tree &b) { return a.y > b.y; }   // far trees first

void normalBlend()   { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
void additiveBlend() { glBlendFunc(GL_SRC_ALPHA, GL_ONE); }

void circleFill(float x, float y, float r, int seg = 24)
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

// soft glow: opaque in the centre, transparent at the edge (use with additive blend)
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

// ------------------------------------------------------------
//  World setup
// ------------------------------------------------------------
void initWorld()
{
    trees.clear(); tufts.clear(); shrooms.clear();

    // border trees (a wall of forest around the map)
    for (float x = 0; x <= WORLD_W; x += 75) {
        Tree a = { x + frand(-20, 20), frand(0, 35), frand(1.3f, 1.7f), frand(0, 6), frand(0, 0.1f), false };
        Tree b = { x + frand(-20, 20), WORLD_H - frand(0, 35), frand(1.3f, 1.7f), frand(0, 6), frand(0, 0.1f), false };
        trees.push_back(a); trees.push_back(b);
    }
    for (float y = 0; y <= WORLD_H; y += 90) {
        Tree a = { frand(0, 35), y, frand(1.3f, 1.7f), frand(0, 6), frand(0, 0.1f), false };
        Tree b = { WORLD_W - frand(0, 35), y, frand(1.3f, 1.7f), frand(0, 6), frand(0, 0.1f), false };
        trees.push_back(a); trees.push_back(b);
    }
    // inner trees (solid, the player cannot walk through them)
    for (int i = 0; i < 180; i++) {
        float x = frand(120, WORLD_W - 120), y = frand(120, WORLD_H - 120);
        if (hypotf(x - 250, y - 250) < 220) continue;
        if (hypotf(x - EXIT_X, y - EXIT_Y) < 300) continue;
        Tree t = { x, y, frand(0.8f, 1.5f), frand(0, 6), frand(0, 0.12f), true };
        trees.push_back(t);
    }
    std::sort(trees.begin(), trees.end(), treeCmp);

    for (int i = 0; i < 550; i++) {
        Tuft t = { frand(0, WORLD_W), frand(0, WORLD_H), frand(0.7f, 1.4f), frand(0, 6) };
        tufts.push_back(t);
    }
    for (int i = 0; i < 55; i++) {
        Shroom s = { frand(60, WORLD_W - 60), frand(60, WORLD_H - 60), frand(5, 9), frand(0, 6) };
        shrooms.push_back(s);
    }
    // positions of the little fireflies inside the jar
    for (int i = 0; i < NEEDED; i++) {
        jfx[i] = frand(62, 128);
        jfy[i] = frand(66, 172);
    }
}

void placeFirefly(Firefly &f, bool nearStart)
{
    for (int tries = 0; tries < 50; tries++) {
        if (nearStart) {
            float a = frand(0, 2 * PI), d = frand(130, 430);
            f.x = 250 + cosf(a) * d; f.y = 250 + sinf(a) * d;
        } else {
            f.x = frand(60, WORLD_W - 60); f.y = frand(60, WORLD_H - 60);
        }
        f.x = clampf(f.x, 60, WORLD_W - 60);
        f.y = clampf(f.y, 60, WORLD_H - 60);
        if (nearStart || hypotf(f.x - px, f.y - py) > 380) break;
    }
    f.ang = frand(0, 2 * PI);
    f.speed = frand(30, 65);
    f.phase = frand(0, 6.28f);
    f.freq = frand(1.2f, 2.6f);
}

void resetGame()
{
    px = 250; py = 250; face = 0; moving = false;
    swinging = false; swingT = 0; cooldown = 0;
    caught = 0; state = 0; winT = 0; msgT = 0; runT = 0;
    flying.clear();
    for (int i = 0; i < NUM_FIREFLIES; i++) placeFirefly(fl[i], i < 9);
}

// ------------------------------------------------------------
//  Net
// ------------------------------------------------------------
void startSwing(float angle)
{
    if (swinging || cooldown > 0 || state != 0) return;
    swinging = true; swingT = 0; swingBase = angle; face = angle;
}

// position of the net head; hand is at (px, py+22)
void getNet(float &hx, float &hy, float &ang, float &dist)
{
    if (swinging) {
        ang = swingBase + (swingT / SWING_DUR * 2.0f - 1.0f) * 1.15f;
        dist = 66;
    } else {
        ang = face - 1.3f;
        dist = 44;
    }
    hx = px + cosf(ang) * dist;
    hy = py + 22 + sinf(ang) * dist;
}

// ------------------------------------------------------------
//  Update (called every ~16 ms)
// ------------------------------------------------------------
void update(float dt)
{
    gt += dt; runT += dt;
    if (msgT > 0) msgT -= dt;
    if (cooldown > 0) cooldown -= dt;

    // ----- fireflies wander -----
    for (int i = 0; i < NUM_FIREFLIES; i++) {
        Firefly &f = fl[i];
        f.ang += sinf(gt * 0.7f + f.phase * 3) * 1.5f * dt + frand(-1, 1) * 2.0f * dt;
        f.x += cosf(f.ang) * f.speed * dt;
        f.y += sinf(f.ang) * f.speed * dt + sinf(gt * 2 + f.phase) * 10 * dt;
        if (f.x < 40 || f.x > WORLD_W - 40) { f.ang = PI - f.ang; f.x = clampf(f.x, 40, WORLD_W - 40); }
        if (f.y < 40 || f.y > WORLD_H - 40) { f.ang = -f.ang;     f.y = clampf(f.y, 40, WORLD_H - 40); }
    }

    // ----- fireflies flying into the jar -----
    for (size_t i = 0; i < flying.size(); ) {
        flying[i].t += dt / 0.7f;
        if (flying[i].t >= 1) flying.erase(flying.begin() + i);
        else i++;
    }

    if (state == 1) { winT += dt; return; }

    // ----- player movement -----
    float dx = (kR ? 1.f : 0.f) - (kL ? 1.f : 0.f);
    float dy = (kU ? 1.f : 0.f) - (kD ? 1.f : 0.f);
    moving = (dx != 0 || dy != 0);
    if (moving) {
        float len = sqrtf(dx * dx + dy * dy);
        dx /= len; dy /= len;
        px += dx * 210 * dt;
        py += dy * 210 * dt;
        if (!swinging) face = atan2f(dy, dx);
    }
    // collide with trees (push the player out of the trunk)
    for (size_t i = 0; i < trees.size(); i++) {
        if (!trees[i].solid) continue;
        float ddx = px - trees[i].x, ddy = py - trees[i].y;
        float minD = 14 * trees[i].s + 12;
        float d = sqrtf(ddx * ddx + ddy * ddy);
        if (d < minD && d > 0.001f) { px = trees[i].x + ddx / d * minD; py = trees[i].y + ddy / d * minD; }
    }
    px = clampf(px, 50, WORLD_W - 50);
    py = clampf(py, 50, WORLD_H - 50);

    // camera follows the player
    camx = clampf(px - VW / 2.0f, 0, WORLD_W - VW);
    camy = clampf(py - VH / 2.0f, 0, WORLD_H - VH);

    // ----- net swing + catching -----
    if (swinging) {
        swingT += dt;
        if (swingT >= SWING_DUR) { swinging = false; cooldown = 0.12f; }
        else if (caught < NEEDED) {
            float hx, hy, a, d;
            getNet(hx, hy, a, d);
            for (int i = 0; i < NUM_FIREFLIES && caught < NEEDED; i++) {
                if (hypotf(fl[i].x - hx, fl[i].y - hy) < 36) {
                    Flying fy = { fl[i].x - camx, fl[i].y - camy, 0 };
                    flying.push_back(fy);
                    caught++;
                    placeFirefly(fl[i], false);          // a new firefly appears elsewhere
                    if (caught == NEEDED) msgT = 7;
                }
            }
        }
    }

    // ----- reached the exit? -----
    if (caught >= NEEDED && hypotf(px - EXIT_X, py - EXIT_Y) < 70) { state = 1; winT = 0; }
}

// ------------------------------------------------------------
//  Drawing : world objects
// ------------------------------------------------------------
void drawGround()
{
    glBegin(GL_QUADS);
    glColor3f(0.035f, 0.12f, 0.10f); glVertex2f(0, 0);        glVertex2f(WORLD_W, 0);
    glColor3f(0.07f, 0.19f, 0.15f);  glVertex2f(WORLD_W, WORLD_H); glVertex2f(0, WORLD_H);
    glEnd();

    for (size_t i = 0; i < tufts.size(); i++) {
        const Tuft &t = tufts[i];
        if (t.x < camx - 20 || t.x > camx + VW + 20 || t.y < camy - 20 || t.y > camy + VH + 20) continue;
        float sw = sinf(gt * 1.5f + t.phase) * 3;
        glBegin(GL_TRIANGLES);
        glColor3f(0.05f, 0.22f, 0.13f);
        glVertex2f(t.x - 3, t.y); glVertex2f(t.x + 3, t.y);
        glColor3f(0.10f, 0.38f, 0.22f);
        glVertex2f(t.x - 4 + sw, t.y + 16 * t.s);
        glColor3f(0.05f, 0.22f, 0.13f);
        glVertex2f(t.x - 1, t.y); glVertex2f(t.x + 5, t.y);
        glColor3f(0.10f, 0.38f, 0.22f);
        glVertex2f(t.x + 5 + sw, t.y + 12 * t.s);
        glEnd();
    }
}

void drawTree(const Tree &t)
{
    if (t.y > camy + VH + 20 || t.y + 300 < camy || t.x < camx - 120 || t.x > camx + VW + 120) return;
    float s = t.s;
    float sw = sinf(gt * 0.8f + t.phase) * 4 * s;

    // trunk
    glBegin(GL_QUADS);
    glColor3f(0.14f, 0.08f, 0.06f);
    glVertex2f(t.x - 8 * s, t.y); glVertex2f(t.x + 8 * s, t.y);
    glColor3f(0.24f, 0.14f, 0.10f);
    glVertex2f(t.x + 6 * s, t.y + 65 * s); glVertex2f(t.x - 6 * s, t.y + 65 * s);
    glEnd();

    // three layers of leaves
    float baseY[3] = { 30, 62, 94 };
    float halfW[3] = { 50, 41, 31 };
    float apexY[3] = { 100, 134, 170 };
    for (int k = 0; k < 3; k++) {
        float lift = 0.035f * k + t.g;
        glBegin(GL_TRIANGLES);
        glColor3f(0.03f, 0.15f + lift, 0.14f + lift * 0.5f);
        glVertex2f(t.x - halfW[k] * s, t.y + baseY[k] * s);
        glVertex2f(t.x + halfW[k] * s, t.y + baseY[k] * s);
        glColor3f(0.07f, 0.30f + lift, 0.24f + lift * 0.5f);
        glVertex2f(t.x + sw * (0.5f + k * 0.4f), t.y + apexY[k] * s);
        glEnd();
    }
}

void drawPlayer()
{
    float bob = moving ? sinf(gt * 14) * 2.0f : 0.0f;

    // shadow
    glPushMatrix();
    glTranslatef(px, py - 2, 0);
    glScalef(1.0f, 0.35f, 1.0f);
    glColor4f(0, 0, 0, 0.35f);
    circleFill(0, 0, 16);
    glPopMatrix();

    // cloak
    glBegin(GL_TRIANGLES);
    glColor3f(0.55f, 0.18f, 0.16f);
    glVertex2f(px - 14, py + bob); glVertex2f(px + 14, py + bob);
    glColor3f(0.85f, 0.36f, 0.26f);
    glVertex2f(px, py + 38 + bob);
    glEnd();
    // head
    glColor3f(0.96f, 0.80f, 0.64f);
    circleFill(px, py + 46 + bob, 8.5f);
    // hat
    glBegin(GL_TRIANGLES);
    glColor3f(0.22f, 0.24f, 0.50f);
    glVertex2f(px - 11, py + 50 + bob); glVertex2f(px + 11, py + 50 + bob);
    glColor3f(0.34f, 0.36f, 0.68f);
    glVertex2f(px + 3, py + 70 + bob);
    glEnd();
    // scarf
    glColor3f(0.95f, 0.78f, 0.30f);
    glBegin(GL_QUADS);
    glVertex2f(px - 7, py + 36 + bob); glVertex2f(px + 7, py + 36 + bob);
    glVertex2f(px + 7, py + 40 + bob); glVertex2f(px - 7, py + 40 + bob);
    glEnd();
}

void drawNet()
{
    float hx, hy, a, d;
    getNet(hx, hy, a, d);
    float ox = px, oy = py + 22;                       // hand
    float ux = cosf(a), uy = sinf(a);

    // swing trail
    if (swinging) {
        additiveBlend();
        for (int k = 1; k <= 7; k++) {
            float ta = a - k * 0.09f;
            glow(ox + cosf(ta) * d, oy + sinf(ta) * d, 20, 0.8f, 0.95f, 1.0f, 0.22f - k * 0.025f);
        }
        normalBlend();
    }
    // handle
    glLineWidth(4);
    glColor3f(0.55f, 0.38f, 0.22f);
    glBegin(GL_LINES);
    glVertex2f(ox, oy); glVertex2f(hx - ux * 18, hy - uy * 18);
    glEnd();
    // net bag (soft white) + ring + mesh
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
//  Drawing : darkness (a black layer with a soft hole of light)
// ------------------------------------------------------------
void ring(float cx, float cy, float ra, float aa, float rb, float ab)
{
    const int N = 72;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float a = 2 * PI * i / N, c = cosf(a), s = sinf(a);
        glColor4f(0.01f, 0.02f, 0.05f, aa); glVertex2f(cx + c * ra, cy + s * ra);
        glColor4f(0.01f, 0.02f, 0.05f, ab); glVertex2f(cx + c * rb, cy + s * rb);
    }
    glEnd();
}

void drawDarkness(float cx, float cy, float R, float dark)
{
    float r0 = R * 0.30f, rm = R * 0.65f, r1 = R;
    ring(cx, cy, r0, 0.0f,        rm, dark * 0.30f);
    ring(cx, cy, rm, dark * 0.30f, r1, dark);
    ring(cx, cy, r1, dark,        4000, dark);
}

// ------------------------------------------------------------
//  Drawing : glowing things (drawn on top of the darkness)
// ------------------------------------------------------------
void drawFirefly(float x, float y, float phase, float freq)
{
    float b = 0.5f + 0.5f * sinf(gt * freq * 2.0f + phase);
    b = b * b;
    glow(x, y, 26 + 22 * b, 0.75f, 1.0f, 0.30f, 0.20f + 0.45f * b);
    glColor4f(1.0f, 1.0f, 0.75f, 0.55f + 0.45f * b);
    circleFill(x, y, 2.8f, 10);
    // tiny wings
    float flap = sinf(gt * 40 + phase) * 1.5f;
    glColor4f(0.9f, 1.0f, 1.0f, 0.25f);
    circleFill(x - 3, y + 3 + flap, 2.5f, 8);
    circleFill(x + 3, y + 3 + flap, 2.5f, 8);
}

void drawExitGate()
{
    float pulse = 0.8f + 0.2f * sinf(gt * 3);
    float x = EXIT_X, y = EXIT_Y;

    glow(x, y + 90, 260, 1.0f, 0.82f, 0.38f, 0.45f * pulse);

    // light beam into the sky
    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.85f, 0.45f, 0.45f * pulse);
    glVertex2f(x - 30, y); glVertex2f(x + 30, y);
    glColor4f(1.0f, 0.85f, 0.45f, 0.0f);
    glVertex2f(x + 12, y + 1000); glVertex2f(x - 12, y + 1000);
    glEnd();

    // two pillars of light
    for (int side = -1; side <= 1; side += 2) {
        float cx = x + side * 70;
        glBegin(GL_QUADS);
        glColor4f(1.0f, 0.9f, 0.55f, 0.85f * pulse);
        glVertex2f(cx - 6, y); glVertex2f(cx + 6, y);
        glColor4f(1.0f, 0.9f, 0.55f, 0.55f * pulse);
        glVertex2f(cx + 6, y + 170); glVertex2f(cx - 6, y + 170);
        glEnd();
    }
    // arch
    glLineWidth(6);
    glColor4f(1.0f, 0.9f, 0.55f, 0.85f * pulse);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 24; i++) {
        float a = PI * i / 24;
        glVertex2f(x + cosf(a) * 70, y + 170 + sinf(a) * 60);
    }
    glEnd();
    glLineWidth(1);
    // glowing doorway
    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.95f, 0.7f, 0.55f * pulse);
    glVertex2f(x - 64, y); glVertex2f(x + 64, y);
    glColor4f(1.0f, 0.95f, 0.7f, 0.15f * pulse);
    glVertex2f(x + 64, y + 170); glVertex2f(x - 64, y + 170);
    glEnd();
}

// ------------------------------------------------------------
//  Drawing : HUD (jar, counter, messages)
// ------------------------------------------------------------
void drawJar()
{
    float frac = caught / (float)NEEDED;
    float pulse = (caught >= NEEDED) ? 0.85f + 0.15f * sinf(gt * 3) : 1.0f;

    // glass
    normalBlend();
    glColor4f(0.70f, 0.90f, 1.0f, 0.12f);
    glBegin(GL_POLYGON);
    glVertex2f(55, 55); glVertex2f(135, 55); glVertex2f(145, 65);
    glVertex2f(145, 180); glVertex2f(45, 180); glVertex2f(45, 65);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(60, 180); glVertex2f(130, 180); glVertex2f(130, 205); glVertex2f(60, 205);
    glEnd();

    // light inside + around the jar (grows with every firefly)
    additiveBlend();
    glow(JAR_X, JAR_Y, 70 + 210 * frac, 0.85f, 1.0f, 0.35f, (0.10f + 0.50f * frac) * pulse);
    for (int i = 0; i < caught; i++) {
        float x = jfx[i] + sinf(gt * 1.5f + i) * 4;
        float y = jfy[i] + cosf(gt * 1.2f + i * 1.7f) * 4;
        float b = 0.5f + 0.5f * sinf(gt * 3 + i * 2.1f);
        glow(x, y, 14, 0.8f, 1.0f, 0.3f, 0.35f + 0.55f * b);
        glColor4f(1, 1, 0.7f, 0.8f);
        circleFill(x, y, 2.4f, 8);
    }
    normalBlend();

    // outline + lid
    glLineWidth(2);
    glColor4f(0.85f, 0.95f, 1.0f, 0.75f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(55, 55); glVertex2f(135, 55); glVertex2f(145, 65); glVertex2f(145, 180);
    glVertex2f(130, 192); glVertex2f(130, 205); glVertex2f(60, 205); glVertex2f(60, 192);
    glVertex2f(45, 180); glVertex2f(45, 65);
    glEnd();
    glLineWidth(1);
    glBegin(GL_QUADS);
    glColor3f(0.40f, 0.26f, 0.15f); glVertex2f(52, 205); glVertex2f(138, 205);
    glColor3f(0.58f, 0.40f, 0.24f); glVertex2f(138, 222); glVertex2f(52, 222);
    glEnd();
}

void drawFlying()
{
    additiveBlend();
    for (size_t i = 0; i < flying.size(); i++) {
        float t = flying[i].t;
        float u = t * t * (3 - 2 * t);                          // smoothstep
        float x = flying[i].sx + (JAR_X - flying[i].sx) * u;
        float y = flying[i].sy + (JAR_Y + 40 - flying[i].sy) * u + sinf(u * PI) * 80;
        glow(x, y, 24, 0.8f, 1.0f, 0.3f, 0.85f);
        glColor4f(1, 1, 0.8f, 1);
        circleFill(x, y, 3, 8);
    }
    normalBlend();
}

void drawExitArrow()
{
    float ex = EXIT_X - px, ey = EXIT_Y - py;
    float d = sqrtf(ex * ex + ey * ey);
    if (d < 450) return;
    float dx = ex / d, dy = ey / d;
    float sx = px - camx + dx * 175, sy = py + 25 - camy + dy * 175;
    float pulse = 0.7f + 0.3f * sinf(gt * 5);
    additiveBlend();
    glow(sx, sy, 34, 1.0f, 0.85f, 0.4f, 0.5f * pulse);
    glBegin(GL_TRIANGLES);
    glColor4f(1.0f, 0.92f, 0.55f, 0.95f * pulse);
    glVertex2f(sx + dx * 17, sy + dy * 17);
    glVertex2f(sx - dx * 10 - dy * 11, sy - dy * 10 + dx * 11);
    glVertex2f(sx - dx * 10 + dy * 11, sy - dy * 10 - dx * 11);
    glEnd();
    normalBlend();
}

void drawHUD()
{
    drawJar();
    drawFlying();

    // counter
    char buf[64];
    sprintf(buf, "%d / %d", caught, NEEDED);
    glColor4f(0.9f, 1.0f, 0.55f, 1.0f);
    drawText(165, 122, buf, GLUT_BITMAP_TIMES_ROMAN_24);
    glColor4f(0.75f, 0.9f, 0.8f, 0.8f);
    drawText(165, 98, "fireflies", GLUT_BITMAP_HELVETICA_12);

    // messages
    if (state == 0) {
        if (caught == 0 && runT < 14) {
            float a = clampf((14 - runT) / 3.0f, 0, 1);
            glColor4f(0.95f, 0.95f, 0.85f, a);
            drawTextCentered(VW / 2.0f, VH - 60, "You are lost in a dark forest...", GLUT_BITMAP_TIMES_ROMAN_24);
            glColor4f(0.75f, 0.9f, 0.85f, a);
            drawTextCentered(VW / 2.0f, VH - 88, "Catch 25 fireflies to light your way.", GLUT_BITMAP_HELVETICA_18);
            drawTextCentered(VW / 2.0f, VH - 112, "WASD / Arrows = walk     SPACE / Click = swing net", GLUT_BITMAP_HELVETICA_12);
        }
        if (caught >= NEEDED && msgT > 0) {
            glColor4f(1.0f, 0.92f, 0.55f, clampf(msgT, 0, 1));
            drawTextCentered(VW / 2.0f, VH - 60, "The jar shines bright! Follow the golden light to the exit.", GLUT_BITMAP_TIMES_ROMAN_24);
        }
        if (caught < NEEDED && hypotf(px - EXIT_X, py - EXIT_Y) < 380) {
            glColor4f(0.9f, 0.9f, 1.0f, 0.9f);
            drawTextCentered(VW / 2.0f, VH - 60, "You sense a way out, but it is too dark to see...", GLUT_BITMAP_TIMES_ROMAN_24);
        }
        if (caught >= NEEDED) drawExitArrow();
    } else if (winT > 1.0f) {
        float a = clampf((winT - 1.0f) / 1.5f, 0, 1);
        glColor4f(0.0f, 0.0f, 0.0f, 0.35f * a);
        glBegin(GL_QUADS);
        glVertex2f(0, VH / 2.0f - 70); glVertex2f(VW, VH / 2.0f - 70);
        glVertex2f(VW, VH / 2.0f + 70); glVertex2f(0, VH / 2.0f + 70);
        glEnd();
        glColor4f(1.0f, 0.95f, 0.65f, a);
        drawTextCentered(VW / 2.0f, VH / 2.0f + 10, "You found your way home!", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor4f(0.95f, 0.95f, 0.95f, a);
        drawTextCentered(VW / 2.0f, VH / 2.0f - 25, "Press R to play again, ESC to quit", GLUT_BITMAP_HELVETICA_18);
    }
}

// ------------------------------------------------------------
//  Display
// ------------------------------------------------------------
void display()
{
    glClearColor(0.01f, 0.02f, 0.04f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    // ---------- world (ground, trees, player) ----------
    glPushMatrix();
    glTranslatef(-camx, -camy, 0);
    drawGround();
    size_t i = 0;
    while (i < trees.size() && trees[i].y > py) drawTree(trees[i++]);   // trees behind the player
    drawPlayer();
    drawNet();
    while (i < trees.size()) drawTree(trees[i++]);                      // trees in front
    glPopMatrix();

    // ---------- darkness with a hole of light around the player ----------
    float R = 85 + caught * 14 + 5 * sinf(gt * 2.2f);
    float dark = 0.97f - 0.014f * caught;
    if (state == 1) {                          // sunrise: light spreads over the forest
        R += winT * 700;
        dark *= clampf(1 - winT * 0.45f, 0, 1);
    }
    if (R > 3000) R = 3000;
    drawDarkness(px - camx, py + 25 - camy, R, dark);

    // ---------- glowing things on top of the darkness ----------
    glPushMatrix();
    glTranslatef(-camx, -camy, 0);
    additiveBlend();
    for (size_t k = 0; k < shrooms.size(); k++) {
        const Shroom &s = shrooms[k];
        if (s.x < camx - 40 || s.x > camx + VW + 40 || s.y < camy - 40 || s.y > camy + VH + 40) continue;
        float b = 0.6f + 0.4f * sinf(gt * 1.3f + s.phase);
        glow(s.x, s.y, s.r * 3.2f, 0.35f, 0.65f, 1.0f, 0.22f * b);
        glColor4f(0.6f, 0.85f, 1.0f, 0.55f * b);
        circleFill(s.x, s.y, s.r * 0.35f, 8);
    }
    for (int k = 0; k < NUM_FIREFLIES; k++) {
        if (fl[k].x < camx - 60 || fl[k].x > camx + VW + 60 || fl[k].y < camy - 60 || fl[k].y > camy + VH + 60) continue;
        drawFirefly(fl[k].x, fl[k].y, fl[k].phase, fl[k].freq);
    }
    if (caught >= NEEDED) drawExitGate();
    normalBlend();
    glPopMatrix();

    // ---------- HUD ----------
    drawHUD();

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
    update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void keyDown(unsigned char k, int, int)
{
    switch (tolower(k)) {
        case 'w': kU = true; break;
        case 's': kD = true; break;
        case 'a': kL = true; break;
        case 'd': kR = true; break;
        case ' ': startSwing(face); break;
        case 'r': resetGame(); break;
        case 27:  exit(0);
    }
}

void keyUp(unsigned char k, int, int)
{
    switch (tolower(k)) {
        case 'w': kU = false; break;
        case 's': kD = false; break;
        case 'a': kL = false; break;
        case 'd': kR = false; break;
    }
}

void specialDown(int k, int, int)
{
    if (k == GLUT_KEY_UP) kU = true;
    if (k == GLUT_KEY_DOWN) kD = true;
    if (k == GLUT_KEY_LEFT) kL = true;
    if (k == GLUT_KEY_RIGHT) kR = true;
}

void specialUp(int k, int, int)
{
    if (k == GLUT_KEY_UP) kU = false;
    if (k == GLUT_KEY_DOWN) kD = false;
    if (k == GLUT_KEY_LEFT) kL = false;
    if (k == GLUT_KEY_RIGHT) kR = false;
}

void mouse(int button, int st, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && st == GLUT_DOWN && state == 0) {
        float wx = camx + x * (VW / (float)winW);
        float wy = camy + (winH - y) * (VH / (float)winH);
        startSwing(atan2f(wy - (py + 22), wx - px));
    }
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
    glutInitWindowPosition(100, 40);
    glutCreateWindow("Fireflies - Lost in the Forest");

    glEnable(GL_BLEND);
    normalBlend();
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    initWorld();
    resetGame();
    camx = clampf(px - VW / 2.0f, 0, WORLD_W - VW);
    camy = clampf(py - VH / 2.0f, 0, WORLD_H - VH);

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