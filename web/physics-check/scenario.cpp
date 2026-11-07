/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native vs wasm physics comparison, using the vendored ODE only.
 *
 * Builds a bike-shaped world with the solver settings from bin/Physics/original.xml
 * and the same features BikePlayer.cpp uses (dDOUBLE, dWorldQuickStep, finite
 * rotation about Z, stopped hinges, soft contact joints, spring forces), drives it
 * with a scripted input sequence, and prints a hash of the raw bits of every
 * body's state after every step. Identical output means identical trajectories.
 *
 * This checks the solver and the math library. It does not run BikePlayer.cpp or
 * Collision.cpp, which need the whole game.
 *
 * Usage: scenario physics | scenario libm
 */
#include <math.h>
#include <ode/ode.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double WORLD_ERP = 0.3, WORLD_CFM = 0.000001, GRAVITY = 9.81;
static const double STEP = 0.01 * 0.6; /* PHYS_STEP_SIZE * simulation_speed_factor */
static const int ITERATIONS = 10;
static const double WHEEL_RADIUS = 0.35, WHEEL_BASE = 1.4, WHEEL_MASS = 10.0;
static const double WHEEL_ERP = 0.8, WHEEL_CFM = 0.00001, GRIP = 20.0;
static const double STOP_ERP = 0.8, STOP_CFM = 0.00001;

/* Own generator, so the script does not disturb ODE's dRand stream. */
static uint32_t scriptSeed;
static double scriptRand() { /* in [0, 1) */
  scriptSeed = scriptSeed * 1664525u + 1013904223u;
  return (scriptSeed >> 8) / 16777216.0;
}

static void hashBytes(uint64_t &h, const void *data, size_t n) {
  const unsigned char *p = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
}

static double terrain(double x) {
  return 0.4 * sin(0.7 * x) + 0.15 * cos(2.3 * x) + 0.02 * x;
}
static double terrainSlope(double x) {
  return 0.28 * cos(0.7 * x) - 0.345 * sin(2.3 * x) + 0.02;
}

static dBodyID makeBody(dWorldID world, double x, double y, double mass, double radius) {
  dBodyID b = dBodyCreate(world);
  dMass m;
  dBodySetPosition(b, x, y, 0);
  dMassSetSphereTotal(&m, mass, radius);
  dBodySetMass(b, &m);
  dBodySetFiniteRotationMode(b, 1);
  dBodySetFiniteRotationAxis(b, 0, 0, 1);
  return b;
}

static void makeStoppedHinge(dWorldID world, dBodyID a, dBodyID b, double x, double y) {
  dJointID j = dJointCreateHinge(world, 0);
  dJointAttach(j, a, b);
  dJointSetHingeAnchor(j, x, y, 0);
  dJointSetHingeAxis(j, 0, 0, 1);
  dJointSetHingeParam(j, dParamLoStop, 0);
  dJointSetHingeParam(j, dParamHiStop, 0);
  dJointSetHingeParam(j, dParamStopERP, STOP_ERP);
  dJointSetHingeParam(j, dParamStopCFM, STOP_CFM);
}

static void addWheelContact(dWorldID world, dJointGroupID group, dBodyID wheel) {
  const dReal *p = dBodyGetPosition(wheel);
  double slope = terrainSlope(p[0]);
  double len = sqrt(1.0 + slope * slope);
  double nx = -slope / len, ny = 1.0 / len;
  double depth = (terrain(p[0]) - p[1]) * ny + WHEEL_RADIUS;
  if (depth <= 0) {
    return;
  }
  dContact c;
  memset(&c, 0, sizeof(c));
  c.surface.mode = dContactSoftERP | dContactSoftCFM | dContactApprox1;
  c.surface.mu = GRIP;
  c.surface.soft_erp = WHEEL_ERP;
  c.surface.soft_cfm = WHEEL_CFM;
  c.geom.pos[0] = p[0] - nx * WHEEL_RADIUS;
  c.geom.pos[1] = p[1] - ny * WHEEL_RADIUS;
  c.geom.normal[0] = nx;
  c.geom.normal[1] = ny;
  c.geom.depth = depth;
  dJointAttach(dJointCreateContact(world, group, &c), wheel, 0);
}

static void addSpring(dBodyID a, dBodyID b, double rest, double k, double damp) {
  const dReal *pa = dBodyGetPosition(a), *pb = dBodyGetPosition(b);
  const dReal *va = dBodyGetLinearVel(a), *vb = dBodyGetLinearVel(b);
  double dx = pb[0] - pa[0], dy = pb[1] - pa[1];
  double len = sqrt(dx * dx + dy * dy);
  if (len < 1e-9) {
    return;
  }
  double ux = dx / len, uy = dy / len;
  double closing = (vb[0] - va[0]) * ux + (vb[1] - va[1]) * uy;
  double f = k * (len - rest) + damp * closing;
  dBodyAddForce(a, f * ux, f * uy, 0);
  dBodyAddForce(b, -f * ux, -f * uy, 0);
}

static bool dumpState = false; /* third argument "dump": print raw state lines */

static void runPhysics(uint32_t seed, int steps, unsigned long odeSeed) {
  scriptSeed = seed;
  dRandSetSeed(odeSeed);

  dWorldID world = dWorldCreate();
  dWorldSetERP(world, WORLD_ERP);
  dWorldSetCFM(world, WORLD_CFM);
  dWorldSetGravity(world, 0, -GRAVITY, 0);
  dWorldSetQuickStepNumIterations(world, ITERATIONS);
  dJointGroupID contacts = dJointGroupCreate(0);

  double x0 = 0, y0 = terrain(0) + WHEEL_RADIUS + 0.2;
  enum { FRAME, REAR, FRONT, TORSO, ULEG, LLEG, UARM, LARM, N };
  dBodyID b[N];
  b[FRAME] = makeBody(world, x0, y0 + 0.5, 25, 0.5);
  b[REAR] = makeBody(world, x0 - WHEEL_BASE / 2, y0, WHEEL_MASS, WHEEL_RADIUS);
  b[FRONT] = makeBody(world, x0 + WHEEL_BASE / 2, y0, WHEEL_MASS, WHEEL_RADIUS);
  b[TORSO] = makeBody(world, x0 - 0.2, y0 + 1.5, 20, 0.2);
  b[ULEG] = makeBody(world, x0 - 0.1, y0 + 0.95, 10, 0.2);
  b[LLEG] = makeBody(world, x0, y0 + 0.75, 5, 0.2);
  b[UARM] = makeBody(world, x0 - 0.1, y0 + 1.3, 3, 0.1);
  b[LARM] = makeBody(world, x0 + 0.1, y0 + 1.15, 3, 0.1);

  makeStoppedHinge(world, b[LLEG], b[ULEG], x0 - 0.05, y0 + 0.85);
  makeStoppedHinge(world, b[ULEG], b[TORSO], x0 - 0.15, y0 + 1.15);
  makeStoppedHinge(world, b[TORSO], b[UARM], x0 - 0.15, y0 + 1.4);
  makeStoppedHinge(world, b[UARM], b[LARM], x0, y0 + 1.22);

  for (int step = 0; step < steps; step++) {
    /* a few seconds of throttle, brake or lean at a time, like a player */
    static double throttle, lean;
    if (step % 150 == 0) {
      throttle = scriptRand() * 2.0 - 0.6;
      lean = scriptRand() * 2.0 - 1.0;
    }

    addWheelContact(world, contacts, b[REAR]);
    addWheelContact(world, contacts, b[FRONT]);
    addSpring(b[FRAME], b[REAR], 0.85, 5000, 250);
    addSpring(b[FRAME], b[FRONT], 0.85, 5000, 250);
    addSpring(b[REAR], b[FRONT], WHEEL_BASE, 20000, 500);
    addSpring(b[FRAME], b[LLEG], 0.3, 2000, 100);
    addSpring(b[FRAME], b[LARM], 0.75, 2000, 100);

    dBodyAddTorque(b[REAR], 0, 0, -throttle * 60.0);
    dBodyAddTorque(b[FRAME], 0, 0, lean * 40.0);

    dWorldQuickStep(world, STEP);
    dJointGroupEmpty(contacts);

    /* Keep it in range if it escapes, using only deterministic inputs. */
    const dReal *p = dBodyGetPosition(b[FRAME]);
    double kick = (p[1] < terrain(p[0]) - 5.0) ? 200.0 : 0.0;
    if (kick > 0) {
      for (int i = 0; i < N; i++) {
        dBodyAddForce(b[i], 0, kick * 10.0, 0);
      }
    }

    if (dumpState && seed == 1 && step < 400) {
      for (int i = 0; i < N; i++) {
        const dReal *parts[4] = {dBodyGetPosition(b[i]), dBodyGetQuaternion(b[i]),
                                 dBodyGetLinearVel(b[i]), dBodyGetAngularVel(b[i])};
        const int sizes[4] = {3, 4, 3, 3};
        for (int part = 0; part < 4; part++) {
          for (int k = 0; k < sizes[part]; k++) {
            uint64_t bits;
            memcpy(&bits, &parts[part][k], 8);
            printf("D %d %d %d %d %016llx %.17g\n", step, i, part, k,
                   (unsigned long long)bits, parts[part][k]);
          }
        }
      }
    }

    uint64_t h = 14695981039346656037ULL;
    for (int i = 0; i < N; i++) {
      hashBytes(h, dBodyGetPosition(b[i]), sizeof(dReal) * 3);
      hashBytes(h, dBodyGetQuaternion(b[i]), sizeof(dReal) * 4);
      hashBytes(h, dBodyGetLinearVel(b[i]), sizeof(dReal) * 3);
      hashBytes(h, dBodyGetAngularVel(b[i]), sizeof(dReal) * 3);
    }
    uint64_t x, y;
    memcpy(&x, &p[0], 8);
    memcpy(&y, &p[1], 8);
    printf("%u %d %016llx %016llx %016llx %.6f %.6f\n", seed, step,
           (unsigned long long)h, (unsigned long long)x, (unsigned long long)y,
           p[0], p[1]);
  }

  dJointGroupDestroy(contacts);
  dWorldDestroy(world);
}

static void bitsLine(const char *name, double a, double b, double r) {
  uint64_t ra, rb, rr;
  memcpy(&ra, &a, 8);
  memcpy(&rb, &b, 8);
  memcpy(&rr, &r, 8);
  printf("%s %016llx %016llx %016llx\n", name, (unsigned long long)ra,
         (unsigned long long)rb, (unsigned long long)rr);
}

static void runLibm(int n) {
  scriptSeed = 12345;
  for (int i = 0; i < n; i++) {
    /* the magnitudes physics actually sees: small angles and level coordinates */
    double a = (scriptRand() - 0.5) * 6.4 + scriptRand() * 1e-7;
    double b = (scriptRand() - 0.5) * 200.0 + scriptRand() * 1e-7;
    double u = scriptRand() * 2.0 - 1.0;
    double p = scriptRand() * 10.0 + 1e-3;
    bitsLine("sin", a, 0, sin(a));
    bitsLine("cos", a, 0, cos(a));
    bitsLine("sin_large", b, 0, sin(b));
    bitsLine("cos_large", b, 0, cos(b));
    bitsLine("tan", a, 0, tan(a));
    bitsLine("acos", u, 0, acos(u));
    bitsLine("asin", u, 0, asin(u));
    bitsLine("atan2", a, b, atan2(a, b));
    bitsLine("pow", p, u, pow(p, u));
    bitsLine("exp", u, 0, exp(u));
    bitsLine("log", p, 0, log(p));
    bitsLine("sqrt", p, 0, sqrt(p));
    bitsLine("sinf", a, 0, (double)sinf((float)a));
    bitsLine("cosf", a, 0, (double)cosf((float)a));
    bitsLine("atan2f", a, b, (double)atan2f((float)a, (float)b));
  }
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "physics";
  dInitODE();
  if (strcmp(mode, "libm") == 0) {
    runLibm(20000);
  } else {
    /* 20 different rides of 16667 steps (about 167 seconds at 100 steps/s). The
       optional second argument sets ODE's starting random seed: the game never
       resets it, so a retry in one session starts from a different value. */
    unsigned long odeSeed = argc > 2 ? strtoul(argv[2], NULL, 10) : 0;
    dumpState = argc > 3 && strcmp(argv[3], "dump") == 0;
    for (uint32_t ride = 1; ride <= 20; ride++) {
      runPhysics(ride, 16667, odeSeed);
    }
  }
  dCloseODE();
  return 0;
}
