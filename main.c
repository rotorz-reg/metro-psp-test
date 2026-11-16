#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "render_models.h"

PSP_MODULE_INFO("Metro_PSP_D6", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
	unsigned int color;
	float x, y, z;
} Vertex;

#define TUNNEL_SECTION_COUNT 24
#define TUNNEL_SECTION_VERTEX_COUNT 132
#define GRID_CELL_SIZE 8.0f
#define MAX_TUNNEL_VERTS 16384
#define ARCH_VERTEX_COUNT (9 * 9 * 36)
#define SLEEPER_VERTEX_COUNT (36 * 36)
#define TUNNEL_END_STRUCTURE_VERTEX_COUNT (3 * 36)
#define HALL_SECTION_COUNT 17
#define HALL_SECTION_VERTEX_COUNT 36
#define HALL_COLUMN_VERTEX_COUNT (4 * 36)
#define INTRO_SECTION_COUNT 14
#define INTRO_SECTION_VERTEX_COUNT 24
#define INTRO_PARTITION_COUNT 6
#define BARREL_SEGMENTS 12
#define BARREL_VERTEX_COUNT (BARREL_SEGMENTS * 5 * 6)
#define MUTANT_VERTEX_CAPACITY 8192
#define PSP_FRAMEBUFFER_WIDTH 512
#define PSP_FRAMEBUFFER_HEIGHT 272
#define PSP_FRAMEBUFFER_BYTES (PSP_FRAMEBUFFER_WIDTH * PSP_FRAMEBUFFER_HEIGHT * 4)
#define PSP_DEPTH_BUFFER_BYTES (PSP_FRAMEBUFFER_WIDTH * PSP_FRAMEBUFFER_HEIGHT * 2)
#define BARREL_WORLD_X 1.0f
#define BARREL_WORLD_Z -68.5f
#define KHAN_WORLD_X 2.2f
#define KHAN_WORLD_Z -67.0f
#define AMMO_CRATE_WORLD_X -3.75f
#define AMMO_CRATE_WORLD_Z -73.5f
#define EXIT_GATE_WORLD_Z -108.0f
#define ROOM_DOOR_Z 45.0f
#define RANGE_GATE_Z 12.0f

static unsigned int __attribute__((aligned(16)))
	texture_staging[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE];
static unsigned int (*textures)[TEXTURE_SIZE * TEXTURE_SIZE];
static TexturedVertex __attribute__((aligned(16))) tunnel_track_grid[MAX_TUNNEL_VERTS];
static TexturedVertex __attribute__((aligned(16))) tunnel_concrete_grid[MAX_TUNNEL_VERTS];
static unsigned int tunnel_track_base_colors[MAX_TUNNEL_VERTS];
static unsigned int tunnel_concrete_base_colors[MAX_TUNNEL_VERTS];
static unsigned int tunnel_track_grid_count;
static unsigned int tunnel_concrete_grid_count;
static TexturedVertex __attribute__((aligned(16))) gate_panel_grid[4096];
static TexturedVertex __attribute__((aligned(16))) gate_frame_grid[4096];
static unsigned int gate_panel_base_colors[4096];
static unsigned int gate_frame_base_colors[4096];
static unsigned int gate_panel_grid_count;
static unsigned int gate_frame_grid_count;
static Vertex __attribute__((aligned(16))) glow_verts[16 * 9];
static unsigned int glow_vertex_count;

#define V(c, x, y, z) {c, x, y, z}
#define TRI(c, ax, ay, az, bx, by, bz, cx, cy, cz) \
	V(c, ax, ay, az), V(c, bx, by, bz), V(c, cx, cy, cz)
#define QUAD(c, ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz) \
	TRI(c, ax, ay, az, bx, by, bz, cx, cy, cz), \
	TRI(c, ax, ay, az, cx, cy, cz, dx, dy, dz)
#define BOX(x0, x1, y0, y1, z0, z1, c) \
	QUAD(c, x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0), \
	QUAD(c, x1, y0, z1, x0, y0, z1, x0, y1, z1, x1, y1, z1), \
	QUAD(c, x0, y0, z1, x0, y0, z0, x0, y1, z0, x0, y1, z1), \
	QUAD(c, x1, y0, z0, x1, y0, z1, x1, y1, z1, x1, y1, z0), \
	QUAD(c, x0, y0, z1, x1, y0, z1, x1, y0, z0, x0, y0, z0), \
	QUAD(c, x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1)
#define TUNNEL_SECTION(z0, z1, floor_color, left_color, right_color) \
	QUAD(floor_color, -2.5f, -1.5f, z0, 2.5f, -1.5f, z0, \
		2.5f, -1.5f, z1, -2.5f, -1.5f, z1), \
	BOX(-0.82f, -0.66f, -1.27f, -1.08f, z0, z1, 0xff9ba5ac), \
	BOX(0.66f, 0.82f, -1.27f, -1.08f, z0, z1, 0xff9ba5ac), \
	QUAD(left_color, -2.5f, -1.5f, z0, -2.5f, -1.5f, z1, \
		-2.5f, 0.65f, z1, -2.5f, 0.65f, z0), \
	QUAD(right_color, 2.5f, -1.5f, z1, 2.5f, -1.5f, z0, \
		2.5f, 0.65f, z0, 2.5f, 0.65f, z1), \
	QUAD(0xff303239, -2.5f, 0.65f, z0, -2.0f, 1.30f, z0, \
		-2.0f, 1.30f, z1, -2.5f, 0.65f, z1), \
	QUAD(0xff35383e, -2.0f, 1.30f, z0, -1.25f, 1.85f, z0, \
		-1.25f, 1.85f, z1, -2.0f, 1.30f, z1), \
	QUAD(0xff3b3e44, -1.25f, 1.85f, z0, -0.5f, 2.14f, z0, \
		-0.5f, 2.14f, z1, -1.25f, 1.85f, z1), \
	QUAD(0xff41444a, -0.5f, 2.14f, z0, 0.5f, 2.14f, z0, \
		0.5f, 2.14f, z1, -0.5f, 2.14f, z1), \
	QUAD(0xff3b3e44, 0.5f, 2.14f, z0, 1.25f, 1.85f, z0, \
		1.25f, 1.85f, z1, 0.5f, 2.14f, z1), \
	QUAD(0xff35383e, 1.25f, 1.85f, z0, 2.0f, 1.30f, z0, \
		2.0f, 1.30f, z1, 1.25f, 1.85f, z1), \
	QUAD(0xff303239, 2.0f, 1.30f, z0, 2.5f, 0.65f, z0, \
		2.5f, 0.65f, z1, 2.0f, 1.30f, z1)
#define HALL_SECTION(z0, z1, c) \
	QUAD(c, -5.5f, -1.5f, z0, 5.5f, -1.5f, z0, \
		5.5f, -1.5f, z1, -5.5f, -1.5f, z1), \
	QUAD(c, -5.5f, -1.5f, z0, -5.5f, -1.5f, z1, \
		-5.5f, 2.2f, z1, -5.5f, 2.2f, z0), \
	QUAD(c, 5.5f, -1.5f, z1, 5.5f, -1.5f, z0, \
		5.5f, 2.2f, z0, 5.5f, 2.2f, z1), \
	QUAD(c, -5.5f, 2.2f, z0, -2.7f, 3.2f, z0, \
		-2.7f, 3.2f, z1, -5.5f, 2.2f, z1), \
	QUAD(c, -2.7f, 3.2f, z0, 2.7f, 3.2f, z0, \
		2.7f, 3.2f, z1, -2.7f, 3.2f, z1), \
	QUAD(c, 2.7f, 3.2f, z0, 5.5f, 2.2f, z0, \
		5.5f, 2.2f, z1, 2.7f, 3.2f, z1)
#define INTRO_SECTION(z0, z1, width) \
	QUAD(0xff777777, -(width), -1.5f, z0, (width), -1.5f, z0, \
		(width), -1.5f, z1, -(width), -1.5f, z1), \
	QUAD(0xff686868, -(width), -1.5f, z0, -(width), -1.5f, z1, \
		-(width), 2.4f, z1, -(width), 2.4f, z0), \
	QUAD(0xff686868, (width), -1.5f, z1, (width), -1.5f, z0, \
		(width), 2.4f, z0, (width), 2.4f, z1), \
	QUAD(0xff454545, -(width), 2.4f, z0, (width), 2.4f, z0, \
		(width), 2.4f, z1, -(width), 2.4f, z1)
#define ARCH(z) \
	BOX(-2.50f, -2.26f, -1.50f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff777777), \
	BOX(2.26f, 2.50f, -1.50f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff777777), \
	BOX(-2.50f, -2.00f, 0.48f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff888888), \
	BOX(-2.00f, -1.25f, 1.12f, 1.32f, (z) - 0.12f, (z) + 0.12f, 0xff888888), \
	BOX(-1.25f, -0.50f, 1.66f, 1.86f, (z) - 0.12f, (z) + 0.12f, 0xff888888), \
	BOX(-0.50f, 0.50f, 1.94f, 2.14f, (z) - 0.12f, (z) + 0.12f, 0xff888888), \
	BOX(0.50f, 1.25f, 1.66f, 1.86f, (z) - 0.12f, (z) + 0.12f, 0xff888888), \
	BOX(1.25f, 2.00f, 1.12f, 1.32f, (z) - 0.12f, (z) + 0.12f, 0xff888888), \
	BOX(2.00f, 2.50f, 0.48f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff888888)
#define SLEEPER(z) \
	BOX(-2.15f, 2.15f, -1.43f, -1.28f, (z) - 0.10f, (z) + 0.10f, 0xff646c72)
#define GATE_FRAME_PANEL(x0, x1, c) \
	QUAD(c, x0, -1.48f, 0.0f, x1, -1.48f, 0.0f, \
		x1, 2.08f, 0.0f, x0, 2.08f, 0.0f)

static Vertex __attribute__((aligned(16))) tunnel_verts[] = {
	TUNNEL_SECTION(12.0f, 9.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(9.0f, 6.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(6.0f, 3.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(3.0f, 0.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(0.0f, -3.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-3.0f, -6.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-6.0f, -9.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-9.0f, -12.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-12.0f, -15.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-15.0f, -18.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-18.0f, -21.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-21.0f, -24.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-24.0f, -27.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-27.0f, -30.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-30.0f, -33.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-33.0f, -36.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-36.0f, -39.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-39.0f, -42.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-42.0f, -45.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-45.0f, -48.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-48.0f, -51.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-51.0f, -54.0f, 0xff858b90, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-54.0f, -57.0f, 0xff777d83, 0xff777777, 0xff777777),
	TUNNEL_SECTION(-57.0f, -60.0f, 0xff858b90, 0xff777777, 0xff777777),

	ARCH(8.0f), ARCH(0.0f), ARCH(-8.0f), ARCH(-16.0f),
	ARCH(-24.0f), ARCH(-32.0f), ARCH(-40.0f), ARCH(-48.0f),
	ARCH(-56.0f),

	SLEEPER(12.0f), SLEEPER(10.0f), SLEEPER(8.0f), SLEEPER(6.0f),
	SLEEPER(4.0f), SLEEPER(2.0f), SLEEPER(0.0f),
	SLEEPER(-2.0f), SLEEPER(-4.0f), SLEEPER(-6.0f), SLEEPER(-8.0f),
	SLEEPER(-10.0f), SLEEPER(-12.0f), SLEEPER(-14.0f), SLEEPER(-16.0f),
	SLEEPER(-18.0f), SLEEPER(-20.0f), SLEEPER(-22.0f), SLEEPER(-24.0f),
	SLEEPER(-26.0f), SLEEPER(-28.0f), SLEEPER(-30.0f), SLEEPER(-32.0f),
	SLEEPER(-34.0f), SLEEPER(-36.0f), SLEEPER(-38.0f), SLEEPER(-40.0f),
	SLEEPER(-42.0f), SLEEPER(-44.0f), SLEEPER(-46.0f), SLEEPER(-48.0f),
	SLEEPER(-50.0f), SLEEPER(-52.0f), SLEEPER(-54.0f), SLEEPER(-56.0f),
	SLEEPER(-58.0f),

	BOX(-2.42f, -2.25f, -1.50f, 2.15f, -59.45f, -59.15f, 0xff727b82),
	BOX(2.25f, 2.42f, -1.50f, 2.15f, -59.45f, -59.15f, 0xff727b82),
	BOX(-2.42f, 2.42f, 2.05f, 2.22f, -59.45f, -59.15f, 0xff727b82),
};

static Vertex __attribute__((aligned(16))) hall_verts[] = {
	HALL_SECTION(-60.0f, -63.0f, 0xff777d83),
	HALL_SECTION(-63.0f, -66.0f, 0xff858b90),
	HALL_SECTION(-66.0f, -69.0f, 0xff777d83),
	HALL_SECTION(-69.0f, -72.0f, 0xff858b90),
	HALL_SECTION(-72.0f, -75.0f, 0xff777d83),
	HALL_SECTION(-75.0f, -78.0f, 0xff858b90),
	HALL_SECTION(-78.0f, -81.0f, 0xff777d83),
	HALL_SECTION(-81.0f, -84.0f, 0xff858b90),
	HALL_SECTION(-84.0f, -87.0f, 0xff777d83),
	HALL_SECTION(-87.0f, -90.0f, 0xff858b90),
	HALL_SECTION(-90.0f, -93.0f, 0xff777d83),
	HALL_SECTION(-93.0f, -96.0f, 0xff858b90),
	HALL_SECTION(-96.0f, -99.0f, 0xff777d83),
	HALL_SECTION(-99.0f, -102.0f, 0xff858b90),
	HALL_SECTION(-102.0f, -105.0f, 0xff777d83),
	HALL_SECTION(-105.0f, -108.0f, 0xff858b90),
	HALL_SECTION(-108.0f, -110.0f, 0xff777d83),
	BOX(-4.65f, -4.05f, -1.50f, 2.50f, -66.6f, -65.8f, 0xff777777),
	BOX(4.05f, 4.65f, -1.50f, 2.50f, -66.6f, -65.8f, 0xff777777),
	BOX(-4.65f, -4.05f, -1.50f, 2.50f, -91.6f, -90.8f, 0xff777777),
	BOX(4.05f, 4.65f, -1.50f, 2.50f, -91.6f, -90.8f, 0xff777777),
};

static Vertex __attribute__((aligned(16))) intro_structure_verts[] = {
	INTRO_SECTION(54.0f, 51.0f, 2.5f),
	INTRO_SECTION(51.0f, 48.0f, 2.5f),
	INTRO_SECTION(48.0f, 45.0f, 2.5f),
	INTRO_SECTION(45.0f, 42.0f, 2.5f),
	INTRO_SECTION(42.0f, 39.0f, 4.5f),
	INTRO_SECTION(39.0f, 36.0f, 4.5f),
	INTRO_SECTION(36.0f, 33.0f, 4.5f),
	INTRO_SECTION(33.0f, 30.0f, 4.5f),
	INTRO_SECTION(30.0f, 27.0f, 4.5f),
	INTRO_SECTION(27.0f, 24.0f, 4.5f),
	INTRO_SECTION(24.0f, 21.0f, 5.0f),
	INTRO_SECTION(21.0f, 18.0f, 5.0f),
	INTRO_SECTION(18.0f, 15.0f, 5.0f),
	INTRO_SECTION(15.0f, 12.0f, 5.0f),
};

static Vertex __attribute__((aligned(16))) intro_partition_verts[] = {
	BOX(-4.5f, -2.5f, -1.5f, 2.4f, 35.9f, 36.1f, 0xff686868),
	BOX(-0.5f, 4.5f, -1.5f, 2.4f, 35.9f, 36.1f, 0xff686868),
	BOX(-2.5f, -0.5f, 1.55f, 2.4f, 35.9f, 36.1f, 0xff777777),
	BOX(-5.0f, -1.25f, -1.5f, 2.4f, 23.9f, 24.1f, 0xff686868),
	BOX(1.25f, 5.0f, -1.5f, 2.4f, 23.9f, 24.1f, 0xff686868),
	BOX(-1.25f, 1.25f, 1.55f, 2.4f, 23.9f, 24.1f, 0xff777777),
};

static Vertex __attribute__((aligned(16))) intro_wood_verts[] = {
	BOX(-2.20f, -0.75f, -1.36f, -1.12f, 50.2f, 53.0f, 0xff81552f),
	BOX(-2.22f, -0.72f, -1.11f, -0.88f, 50.2f, 53.0f, 0xffa77e55),
	BOX(0.55f, 1.85f, -0.72f, -0.58f, 50.0f, 52.3f, 0xff80512c),
	BOX(0.65f, 0.82f, -1.50f, -0.70f, 50.1f, 50.3f, 0xff654328),
	BOX(1.58f, 1.75f, -1.50f, -0.70f, 50.1f, 50.3f, 0xff654328),
	BOX(-2.28f, -2.05f, -1.32f, 0.25f, 48.0f, 48.15f, 0xff895d38),
	BOX(-2.35f, -1.98f, -1.30f, -1.10f, 47.96f, 48.19f, 0xffa77c4a),
	BOX(-2.34f, -2.12f, -0.08f, 0.02f, 48.0f, 48.14f, 0xff784c2c),
	BOX(-4.0f, -0.8f, -0.75f, -0.58f, 31.0f, 32.2f, 0xff76502e),
	BOX(-3.8f, -3.55f, -1.50f, -0.72f, 31.1f, 31.35f, 0xff67452b),
	BOX(-1.25f, -1.0f, -1.50f, -0.72f, 31.1f, 31.35f, 0xff67452b),
	BOX(0.8f, 4.0f, -0.75f, -0.58f, 27.0f, 28.2f, 0xff76502e),
	BOX(1.0f, 1.25f, -1.50f, -0.72f, 27.1f, 27.35f, 0xff67452b),
	BOX(3.55f, 3.8f, -1.50f, -0.72f, 27.1f, 27.35f, 0xff67452b),
	BOX(-4.0f, 4.0f, -0.68f, -0.50f, 17.2f, 19.0f, 0xff79512f),
	BOX(-3.85f, -3.60f, -1.50f, -0.68f, 17.4f, 17.7f, 0xff634127),
	BOX(3.60f, 3.85f, -1.50f, -0.68f, 17.4f, 17.7f, 0xff634127),
	BOX(-3.85f, -3.60f, -1.50f, -0.68f, 18.5f, 18.8f, 0xff634127),
	BOX(3.60f, 3.85f, -1.50f, -0.68f, 18.5f, 18.8f, 0xff634127),
};

static Vertex __attribute__((aligned(16))) intro_metal_verts[] = {
	BOX(1.05f, 1.35f, -0.45f, -0.34f, 51.0f, 51.3f, 0xff373b3d),
	BOX(1.16f, 1.24f, -0.34f, 0.22f, 51.08f, 51.22f, 0xff4b5051),
	BOX(1.02f, 1.38f, 0.22f, 0.34f, 50.95f, 51.35f, 0xff9a4c18),
	BOX(-0.15f, -0.02f, -0.28f, 0.65f, 48.0f, 48.11f, 0xff6a4428),
	BOX(-2.05f, -1.90f, -1.28f, 0.08f, 47.9f, 48.02f, 0xff79502e),
	BOX(-2.38f, -2.20f, -1.33f, 0.20f, 47.93f, 48.04f, 0xff805531),
	BOX(-3.20f, -2.55f, -0.45f, -0.28f, 17.7f, 18.0f, 0xff34383b),
	BOX(-3.02f, -2.92f, -0.28f, -0.17f, 17.2f, 17.92f, 0xff303438),
	BOX(-2.35f, -1.05f, -0.43f, -0.28f, 17.7f, 18.0f, 0xff34383b),
	BOX(-2.30f, -2.18f, -0.28f, -0.16f, 17.75f, 18.58f, 0xff555a5d),
	BOX(-1.20f, -0.55f, -0.46f, -0.30f, 17.7f, 18.0f, 0xff363a3d),
	BOX(-1.18f, -1.10f, -0.29f, -0.18f, 17.32f, 17.90f, 0xff555a5d),
	BOX(0.35f, 1.65f, -0.44f, -0.29f, 17.7f, 18.0f, 0xff363a3d),
	BOX(0.48f, 0.62f, -0.27f, -0.16f, 16.65f, 17.83f, 0xff555a5d),
	BOX(0.36f, 0.56f, -0.39f, -0.31f, 17.3f, 17.65f, 0xff80512e),
	BOX(2.0f, 3.15f, -0.44f, -0.29f, 17.7f, 18.0f, 0xff292d31),
	BOX(2.12f, 2.24f, -0.26f, -0.15f, 16.55f, 17.82f, 0xff555a5d),
	BOX(2.33f, 2.82f, -0.28f, -0.15f, 17.34f, 17.85f, 0xff292d31),
	BOX(-3.75f, -2.75f, 0.05f, 1.20f, 13.35f, 13.52f, 0xff62676a),
	BOX(-3.55f, -3.45f, -1.50f, 0.08f, 13.40f, 13.48f, 0xff484d50),
	BOX(-1.55f, -0.55f, 0.05f, 1.20f, 13.35f, 13.52f, 0xff62676a),
	BOX(-1.35f, -1.25f, -1.50f, 0.08f, 13.40f, 13.48f, 0xff484d50),
	BOX(0.55f, 1.55f, 0.05f, 1.20f, 13.35f, 13.52f, 0xff62676a),
	BOX(0.75f, 0.85f, -1.50f, 0.08f, 13.40f, 13.48f, 0xff484d50),
	BOX(2.75f, 3.75f, 0.05f, 1.20f, 13.35f, 13.52f, 0xff62676a),
	BOX(2.95f, 3.05f, -1.50f, 0.08f, 13.40f, 13.48f, 0xff484d50),
};

static Vertex __attribute__((aligned(16))) armory_weapon_verts[] = {
	/* Pistol */
	BOX(-3.85f, -3.48f, -0.44f, -0.34f, 17.45f, 17.58f, 0xff393e42),
	BOX(-3.76f, -3.60f, -0.34f, -0.23f, 17.48f, 17.55f, 0xff62676a),
	BOX(-3.72f, -3.62f, -0.57f, -0.42f, 17.49f, 17.56f, 0xff292d30),
	/* Assault rifle */
	BOX(-2.65f, -1.45f, -0.40f, -0.29f, 17.43f, 17.57f, 0xff373c40),
	BOX(-2.32f, -1.70f, -0.29f, -0.18f, 17.45f, 17.55f, 0xff555a5d),
	BOX(-2.59f, -2.47f, -0.53f, -0.36f, 17.46f, 17.56f, 0xff80512e),
	BOX(-2.08f, -1.98f, -0.56f, -0.39f, 17.46f, 17.56f, 0xff303438),
	/* Shotgun */
	BOX(-0.85f, 0.10f, -0.40f, -0.28f, 17.43f, 17.57f, 0xff42474b),
	BOX(-0.64f, -0.42f, -0.29f, -0.20f, 17.45f, 17.55f, 0xff62676a),
	BOX(-0.82f, -0.69f, -0.53f, -0.38f, 17.46f, 17.56f, 0xff80512e),
	BOX(-0.26f, -0.16f, -0.55f, -0.39f, 17.46f, 17.56f, 0xff34383b),
	/* Sniper rifle and scope */
	BOX(0.65f, 2.95f, -0.39f, -0.28f, 17.43f, 17.57f, 0xff34393d),
	BOX(0.92f, 1.42f, -0.28f, -0.17f, 17.45f, 17.55f, 0xff555a5d),
	BOX(0.75f, 1.02f, -0.55f, -0.38f, 17.46f, 17.56f, 0xff80512e),
	BOX(1.20f, 1.27f, -0.17f, -0.08f, 17.45f, 17.55f, 0xff25292d),
	BOX(1.15f, 1.36f, -0.08f, 0.02f, 17.44f, 17.56f, 0xff454a4e),
};

static Vertex __attribute__((aligned(16))) intro_lamp_verts[] = {
	BOX(1.02f, 1.38f, 0.31f, 0.39f, 51.0f, 51.3f, 0xffd02010),
	BOX(-3.28f, -2.72f, 1.72f, 1.80f, 30.0f, 30.16f, 0xffd02010),
	BOX(2.72f, 3.28f, 1.72f, 1.80f, 33.0f, 33.16f, 0xffd02010),
};
static Vertex __attribute__((aligned(16))) intro_target_markers_red[] = {
	QUAD(0xffff3020, -3.37f, 0.62f, 13.54f, -3.13f, 0.62f, 13.54f,
		-3.13f, 0.86f, 13.54f, -3.37f, 0.86f, 13.54f),
	QUAD(0xffff3020, -1.17f, 0.62f, 13.54f, -0.93f, 0.62f, 13.54f,
		-0.93f, 0.86f, 13.54f, -1.17f, 0.86f, 13.54f),
	QUAD(0xffff3020, 0.93f, 0.62f, 13.54f, 1.17f, 0.62f, 13.54f,
		1.17f, 0.86f, 13.54f, 0.93f, 0.86f, 13.54f),
	QUAD(0xffff3020, 3.13f, 0.62f, 13.54f, 3.37f, 0.62f, 13.54f,
		3.37f, 0.86f, 13.54f, 3.13f, 0.86f, 13.54f),
};
static Vertex __attribute__((aligned(16))) intro_target_markers_green[] = {
	QUAD(0xff40ff40, -3.37f, 0.62f, 13.55f, -3.13f, 0.62f, 13.55f,
		-3.13f, 0.86f, 13.55f, -3.37f, 0.86f, 13.55f),
	QUAD(0xff40ff40, -1.17f, 0.62f, 13.55f, -0.93f, 0.62f, 13.55f,
		-0.93f, 0.86f, 13.55f, -1.17f, 0.86f, 13.55f),
	QUAD(0xff40ff40, 0.93f, 0.62f, 13.55f, 1.17f, 0.62f, 13.55f,
		1.17f, 0.86f, 13.55f, 0.93f, 0.86f, 13.55f),
	QUAD(0xff40ff40, 3.13f, 0.62f, 13.55f, 3.37f, 0.62f, 13.55f,
		3.37f, 0.86f, 13.55f, 3.13f, 0.86f, 13.55f),
};

static Vertex __attribute__((aligned(16))) menu_structure_verts[] = {
	QUAD(0xff65696b, -4.5f, -1.5f, 1.0f, 4.5f, -1.5f, 1.0f,
		4.5f, -1.5f, -8.0f, -4.5f, -1.5f, -8.0f),
	QUAD(0xff55595c, -4.5f, -1.5f, -8.0f, -4.5f, -1.5f, 1.0f,
		-4.5f, 3.0f, 1.0f, -4.5f, 3.0f, -8.0f),
	QUAD(0xff55595c, 4.5f, -1.5f, 1.0f, 4.5f, -1.5f, -8.0f,
		4.5f, 3.0f, -8.0f, 4.5f, 3.0f, 1.0f),
	QUAD(0xff45494c, -4.5f, 3.0f, 1.0f, 4.5f, 3.0f, 1.0f,
		4.5f, 3.0f, -8.0f, -4.5f, 3.0f, -8.0f),
	QUAD(0xff626669, -4.5f, -1.5f, -8.0f, 4.5f, -1.5f, -8.0f,
		4.5f, 3.0f, -8.0f, -4.5f, 3.0f, -8.0f),
};

static Vertex __attribute__((aligned(16))) menu_wood_verts[] = {
	BOX(-2.2f, 2.2f, -0.82f, -0.66f, -4.0f, -1.7f, 0xff76502e),
	BOX(-2.05f, -1.82f, -1.50f, -0.82f, -3.8f, -3.55f, 0xff634127),
	BOX(1.82f, 2.05f, -1.50f, -0.82f, -3.8f, -3.55f, 0xff634127),
	BOX(-2.05f, -1.82f, -1.50f, -0.82f, -2.15f, -1.9f, 0xff634127),
	BOX(1.82f, 2.05f, -1.50f, -0.82f, -2.15f, -1.9f, 0xff634127),
	BOX(-1.0f, -0.48f, -0.64f, -0.48f, -3.15f, -2.45f, 0xff835a34),
	BOX(-0.88f, -0.72f, -0.48f, -0.12f, -3.05f, -2.95f, 0xffa77e55),
	BOX(0.35f, 1.05f, -0.62f, -0.47f, -3.35f, -2.70f, 0xff806038),
};

static Vertex __attribute__((aligned(16))) menu_metal_verts[] = {
	BOX(-0.85f, -0.37f, -0.58f, -0.28f, -3.08f, -2.94f, 0xff34383b),
	BOX(-0.78f, -0.44f, -0.68f, -0.56f, -3.14f, -2.88f, 0xff292d30),
	BOX(-0.70f, -0.57f, -0.77f, -0.66f, -3.12f, -2.91f, 0xff454a4d),
	BOX(-0.91f, -0.83f, -0.55f, -0.34f, -3.06f, -2.98f, 0xff565b5d),
	BOX(-0.39f, -0.31f, -0.55f, -0.34f, -3.06f, -2.98f, 0xff565b5d),
	BOX(0.42f, 0.49f, -0.46f, -0.20f, -3.20f, -3.13f, 0xffc7a857),
	BOX(0.62f, 0.69f, -0.46f, -0.20f, -3.20f, -3.13f, 0xffc7a857),
	BOX(0.82f, 0.89f, -0.46f, -0.20f, -3.20f, -3.13f, 0xffc7a857),
	BOX(1.18f, 1.48f, -0.64f, -0.48f, -3.25f, -2.75f, 0xff393c3e),
	BOX(1.25f, 1.41f, -0.48f, 0.15f, -3.15f, -2.85f, 0xff4b5051),
};

static Vertex __attribute__((aligned(16))) menu_red_lamp_verts[] = {
	BOX(1.18f, 1.48f, 0.15f, 0.23f, -3.18f, -2.82f, 0xffffa030),
};

static TexturedVertex __attribute__((aligned(16))) textured_menu_structure[
	sizeof(menu_structure_verts) / sizeof(Vertex)];
static TexturedVertex __attribute__((aligned(16))) textured_menu_wood[
	sizeof(menu_wood_verts) / sizeof(Vertex)];
static TexturedVertex __attribute__((aligned(16))) textured_menu_metal[
	sizeof(menu_metal_verts) / sizeof(Vertex)];
static TexturedVertex __attribute__((aligned(16))) textured_menu_lamp[
	sizeof(menu_red_lamp_verts) / sizeof(Vertex)];

static Vertex __attribute__((aligned(16))) ammo_crate_verts[] = {
	BOX(-4.25f, -3.25f, -1.50f, -0.72f, -74.0f, -73.0f, 0xff8b6038),
};
static TexturedVertex __attribute__((aligned(16))) textured_ammo_crate_verts[36];
static TexturedVertex __attribute__((aligned(16))) textured_intro_wood_verts[
	sizeof(intro_wood_verts) / sizeof(Vertex)];
static TexturedVertex __attribute__((aligned(16))) textured_intro_metal_verts[
	sizeof(intro_metal_verts) / sizeof(Vertex)];
static TexturedVertex __attribute__((aligned(16))) textured_armory_weapon_verts[
	sizeof(armory_weapon_verts) / sizeof(Vertex)];
static TexturedVertex __attribute__((aligned(16))) textured_intro_lamp_verts[
	sizeof(intro_lamp_verts) / sizeof(Vertex)];
static Vertex __attribute__((aligned(16))) barrel_raw_verts[BARREL_VERTEX_COUNT];
static TexturedVertex __attribute__((aligned(16))) barrel_verts[BARREL_VERTEX_COUNT];
static Vertex __attribute__((aligned(16))) barrel_fire_raw[] = {
	TRI(0xffffffff, -0.25f, 0.90f, 0.0f, 0.25f, 0.90f, 0.0f, 0.02f, 1.55f, 0.0f),
	TRI(0xffffffff, -0.17f, 0.90f, 0.015f, 0.18f, 0.90f, 0.015f,
		-0.04f, 1.30f, 0.015f),
	TRI(0xffffffff, -0.10f, 0.90f, 0.025f, 0.11f, 0.90f, 0.025f,
		0.04f, 1.12f, 0.025f),
};
static TexturedVertex __attribute__((aligned(16))) barrel_fire_verts[
	sizeof(barrel_fire_raw) / sizeof(Vertex)];
static Vertex __attribute__((aligned(16))) barrel_glow_raw[] = {
	QUAD(0x70ffffff, -0.85f, 0.40f, -0.08f, 0.85f, 0.40f, -0.08f,
		0.85f, 1.55f, -0.08f, -0.85f, 1.55f, -0.08f),
};
static TexturedVertex __attribute__((aligned(16))) barrel_glow_verts[
	sizeof(barrel_glow_raw) / sizeof(Vertex)];

#define TUNNEL_VERTEX_COUNT (sizeof(tunnel_verts) / sizeof(Vertex))
#define HALL_VERTEX_COUNT (sizeof(hall_verts) / sizeof(Vertex))
#define INTRO_VERTEX_COUNT (sizeof(intro_structure_verts) / sizeof(Vertex))
#define TUNNEL_EXPECTED_VERTEX_COUNT (TUNNEL_SECTION_COUNT * TUNNEL_SECTION_VERTEX_COUNT + \
	ARCH_VERTEX_COUNT + SLEEPER_VERTEX_COUNT + TUNNEL_END_STRUCTURE_VERTEX_COUNT)
#define HALL_EXPECTED_VERTEX_COUNT \
	(HALL_SECTION_COUNT * HALL_SECTION_VERTEX_COUNT + HALL_COLUMN_VERTEX_COUNT)
#define ARCHES_START (TUNNEL_SECTION_COUNT * TUNNEL_SECTION_VERTEX_COUNT)
#define SLEEPERS_START (ARCHES_START + ARCH_VERTEX_COUNT)
typedef char tunnel_vertex_capacity_check[
	(sizeof(tunnel_verts) / sizeof(tunnel_verts[0]) <= MAX_TUNNEL_VERTS) ? 1 : -1];
typedef char tunnel_vertex_layout_check[
	(sizeof(tunnel_verts) / sizeof(tunnel_verts[0]) == TUNNEL_EXPECTED_VERTEX_COUNT) ? 1 : -1];
typedef char hall_vertex_capacity_check[
	(sizeof(hall_verts) / sizeof(hall_verts[0]) <= MAX_TUNNEL_VERTS) ? 1 : -1];
typedef char hall_vertex_layout_check[
	(sizeof(hall_verts) / sizeof(hall_verts[0]) == HALL_EXPECTED_VERTEX_COUNT) ? 1 : -1];

static Vertex __attribute__((aligned(16))) gate_verts[] = {
	GATE_FRAME_PANEL(-2.30f, -1.40f, 0xff888888),
	GATE_FRAME_PANEL(-1.36f, -0.46f, 0xff888888),
	GATE_FRAME_PANEL(-0.42f, 0.42f, 0xff888888),
	GATE_FRAME_PANEL(0.46f, 1.36f, 0xff888888),
	GATE_FRAME_PANEL(1.40f, 2.30f, 0xff888888),
};

static Vertex __attribute__((aligned(16))) gate_frame_verts[] = {
	BOX(-2.50f, -2.28f, -1.52f, 2.18f, -0.16f, 0.16f, 0xff999999),
	BOX(2.28f, 2.50f, -1.52f, 2.18f, -0.16f, 0.16f, 0xff999999),
	BOX(-2.50f, 2.50f, -1.52f, -1.36f, -0.16f, 0.16f, 0xff999999),
	BOX(-2.50f, 2.50f, 2.00f, 2.22f, -0.16f, 0.16f, 0xff999999),
};

static Vertex __attribute__((aligned(16))) overlay_verts[16384];
static Vertex __attribute__((aligned(16))) overlay_dialogue_verts[1024];
static Vertex __attribute__((aligned(16))) overlay_complete_verts[1024];
static Vertex __attribute__((aligned(16))) overlay_khan_dialogue_verts[3][4096];
static Vertex __attribute__((aligned(16))) overlay_chapter2_complete_verts[2048];
static Vertex __attribute__((aligned(16))) overlay_intro_message_verts[3][2048];
static Vertex __attribute__((aligned(16))) overlay_weapon_name_verts[WEAPON_COUNT][1024];
static Vertex __attribute__((aligned(16))) overlay_watch_verts[512];
static Vertex __attribute__((aligned(16))) overlay_crosshair_verts[64];
static Vertex __attribute__((aligned(16))) overlay_weapon_pickup_hint[2048];
static Vertex __attribute__((aligned(16))) overlay_hp_verts[200];
static Vertex __attribute__((aligned(16))) overlay_ammo_full_verts[60];
static Vertex __attribute__((aligned(16))) overlay_ammo_empty_verts[60];
static Vertex __attribute__((aligned(16))) overlay_menu_title[4096];
static Vertex __attribute__((aligned(16))) overlay_chapter_heading[2048];
static Vertex __attribute__((aligned(16))) overlay_menu_version[1024];
static Vertex __attribute__((aligned(16))) overlay_menu_hint[2048];
static Vertex __attribute__((aligned(16))) overlay_main_options[3][2][2048];
static Vertex __attribute__((aligned(16))) overlay_chapter_options[3][2][2048];
static Vertex __attribute__((aligned(16))) overlay_controls_title[2048];
static Vertex __attribute__((aligned(16))) overlay_controls_text[8192];
static Vertex __attribute__((aligned(16))) overlay_pause_title[2048];
static Vertex __attribute__((aligned(16))) overlay_pause_options[2][2][2048];
static Vertex __attribute__((aligned(16))) overlay_cutscene_dialogue[2048];
static Vertex __attribute__((aligned(16))) overlay_fade_sprites[256][2];
static Vertex __attribute__((aligned(16))) overlay_menu_panel[2];
static Vertex *overlay_build_target;
static unsigned int *overlay_build_count;
static unsigned int overlay_build_capacity;
static unsigned int overlay_static_count;
static unsigned int overlay_dialogue_count;
static unsigned int overlay_complete_count;
static unsigned int overlay_khan_dialogue_counts[3];
static unsigned int overlay_chapter2_complete_count;
static unsigned int overlay_intro_message_counts[3];
static unsigned int overlay_weapon_name_counts[WEAPON_COUNT];
static unsigned int overlay_watch_count;
static unsigned int overlay_crosshair_count;
static unsigned int overlay_weapon_pickup_hint_count;
static unsigned int overlay_menu_title_count;
static unsigned int overlay_chapter_heading_count;
static unsigned int overlay_menu_version_count;
static unsigned int overlay_menu_hint_count;
static unsigned int overlay_main_option_counts[3][2];
static unsigned int overlay_chapter_option_counts[3][2];
static unsigned int overlay_controls_title_count;
static unsigned int overlay_controls_text_count;
static unsigned int overlay_pause_title_count;
static unsigned int overlay_pause_option_counts[2][2];
static unsigned int overlay_cutscene_dialogue_count;

enum {
	UI_MAIN_MENU,
	UI_CHAPTER_SELECT,
	UI_CONTROLS,
	UI_PAUSED,
	UI_CUTSCENE,
	UI_PLAYING
};

static const unsigned char font8x8[128][8] = {
	['!'] = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
	[','] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x10},
	['.'] = {0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18},
	[':'] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
	['-'] = {0x00,0x00,0x00,0x3e,0x00,0x00,0x00,0x00},
	['>'] = {0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00},
	['['] = {0x1c,0x10,0x10,0x10,0x10,0x10,0x1c,0x00},
	[']'] = {0x1c,0x04,0x04,0x04,0x04,0x04,0x1c,0x00},
	['1'] = {0x08,0x18,0x08,0x08,0x08,0x08,0x1c,0x00},
	['2'] = {0x1c,0x22,0x02,0x04,0x08,0x10,0x3e,0x00},
	['3'] = {0x3c,0x02,0x02,0x1c,0x02,0x02,0x3c,0x00},
	['0'] = {0x1c,0x22,0x26,0x2a,0x32,0x22,0x1c,0x00},
	['4'] = {0x04,0x0c,0x14,0x24,0x3e,0x04,0x04,0x00},
	['5'] = {0x3e,0x20,0x3c,0x02,0x02,0x22,0x1c,0x00},
	['6'] = {0x0c,0x10,0x20,0x3c,0x22,0x22,0x1c,0x00},
	['7'] = {0x3e,0x02,0x04,0x08,0x10,0x10,0x10,0x00},
	['8'] = {0x1c,0x22,0x22,0x1c,0x22,0x22,0x1c,0x00},
	['9'] = {0x1c,0x22,0x22,0x1e,0x02,0x04,0x18,0x00},
	['/'] = {0x02,0x04,0x04,0x08,0x10,0x10,0x20,0x00},
	['A'] = {0x1c,0x22,0x22,0x3e,0x22,0x22,0x22,0x00},
	['B'] = {0x3c,0x22,0x22,0x3c,0x22,0x22,0x3c,0x00},
	['C'] = {0x1c,0x22,0x20,0x20,0x20,0x22,0x1c,0x00},
	['D'] = {0x38,0x24,0x22,0x22,0x22,0x24,0x38,0x00},
	['E'] = {0x3e,0x20,0x20,0x3c,0x20,0x20,0x3e,0x00},
	['F'] = {0x3e,0x20,0x20,0x3c,0x20,0x20,0x20,0x00},
	['G'] = {0x1c,0x22,0x20,0x2e,0x22,0x22,0x1c,0x00},
	['H'] = {0x22,0x22,0x22,0x3e,0x22,0x22,0x22,0x00},
	['I'] = {0x1c,0x08,0x08,0x08,0x08,0x08,0x1c,0x00},
	['J'] = {0x0e,0x04,0x04,0x04,0x04,0x24,0x18,0x00},
	['K'] = {0x22,0x24,0x28,0x30,0x28,0x24,0x22,0x00},
	['L'] = {0x20,0x20,0x20,0x20,0x20,0x20,0x3e,0x00},
	['M'] = {0x22,0x36,0x2a,0x2a,0x22,0x22,0x22,0x00},
	['N'] = {0x22,0x32,0x2a,0x26,0x22,0x22,0x22,0x00},
	['O'] = {0x1c,0x22,0x22,0x22,0x22,0x22,0x1c,0x00},
	['P'] = {0x3c,0x22,0x22,0x3c,0x20,0x20,0x20,0x00},
	['Q'] = {0x1c,0x22,0x22,0x22,0x2a,0x24,0x1a,0x00},
	['R'] = {0x3c,0x22,0x22,0x3c,0x28,0x24,0x22,0x00},
	['S'] = {0x1e,0x20,0x20,0x1c,0x02,0x02,0x3c,0x00},
	['T'] = {0x3e,0x08,0x08,0x08,0x08,0x08,0x08,0x00},
	['U'] = {0x22,0x22,0x22,0x22,0x22,0x22,0x1c,0x00},
	['V'] = {0x22,0x22,0x22,0x22,0x22,0x14,0x08,0x00},
	['W'] = {0x22,0x22,0x22,0x2a,0x2a,0x36,0x22,0x00},
	['X'] = {0x22,0x22,0x14,0x08,0x14,0x22,0x22,0x00},
	['Y'] = {0x22,0x22,0x14,0x08,0x08,0x08,0x08,0x00},
	['Z'] = {0x3e,0x02,0x04,0x08,0x10,0x20,0x3e,0x00},
};


typedef struct {
	float x, z;
	int hits_left;
	int alive;
	int attack_cooldown;
} Enemy;

typedef struct {
	float px, pz, yaw;
	float walk_phase, mutant_phase, fire_phase, recoil;
	float room_door_open, range_door_open, gate_open, exit_gate_open;
	int flashlight, reload_frames, flash_frames, weapon_type, arsenal_acquired;
	int hp, ammo, ammo_capacity, chapter1_complete, khan_dialogue_line;
	int chapter2_complete, room_door_opened, range_door_opened;
	int intro_message, armory_near;
	int target_hits[4];
} PauseRenderState;

enum { ENEMY_COUNT = 7, CHAPTER1_ENEMY_COUNT = 3 };

static Vertex __attribute__((aligned(16))) mutant_verts[MUTANT_VERTEX_CAPACITY];
static unsigned int mutant_vertex_count;

static TexturedVertex __attribute__((aligned(16))) textured_mutant_verts[
	MUTANT_VERTEX_CAPACITY];
static TexturedVertex __attribute__((aligned(16))) textured_mutant_eye_verts[72];
static Vertex __attribute__((aligned(16))) mutant_eye_verts[] = {
	BOX(-0.16f, -0.055f, 1.52f, 1.59f, 0.50f, 0.55f, 0xffffffff),
	BOX(0.055f, 0.16f, 1.52f, 1.59f, 0.50f, 0.55f, 0xffffffff),
};
static Vertex __attribute__((aligned(16))) lamp_housing_verts[] = {
	BOX(-0.30f, 0.30f, -0.10f, 0.02f, -0.22f, 0.22f, 0xff3d3b39),
};
static Vertex __attribute__((aligned(16))) lamp_lens_verts[] = {
	BOX(-0.20f, 0.20f, -0.12f, -0.08f, -0.13f, 0.13f, 0xd01818ff),
};
static TexturedVertex __attribute__((aligned(16))) textured_lamp_housing_verts[36];
static TexturedVertex __attribute__((aligned(16))) textured_lamp_lens_verts[36];
static unsigned int texture_random(void)
{
	static unsigned int state = 0x6d2b79f5u;
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

#define SFX_SAMPLE_COUNT 4096
#define GATE_SFX_SAMPLE_COUNT 32768
static short __attribute__((aligned(16))) gunshot_samples[SFX_SAMPLE_COUNT];
static short __attribute__((aligned(16))) footstep_samples[SFX_SAMPLE_COUNT];
static short __attribute__((aligned(16))) gate_open_samples[GATE_SFX_SAMPLE_COUNT];
static int gunshot_channel = -1;
static int footstep_channel = -1;
static int gate_channel = -1;

static short clamp_audio_sample(float sample)
{
	if (sample > 32767.0f) sample = 32767.0f;
	if (sample < -32768.0f) sample = -32768.0f;
	return (short)sample;
}

static void generate_sound_effects(void)
{
	int i;

	for (i = 0; i < SFX_SAMPLE_COUNT; ++i) {
		float time = (float)i / (float)SFX_SAMPLE_COUNT;
		float noise = (float)(texture_random() & 0xffffu) - 32768.0f;
		float shot_envelope = expf(-4.0f * time);
		float step_envelope = expf(-7.0f * time);
		float shot_boom = sinf((float)i * 0.0105f);
		float step_thump = sinf((float)i * 0.0090f);

		gunshot_samples[i] = clamp_audio_sample(
			noise * shot_envelope * 0.58f +
			shot_boom * 18000.0f * shot_envelope);
		footstep_samples[i] = clamp_audio_sample(
			noise * step_envelope * 0.24f +
			step_thump * 21000.0f * step_envelope);
	}
}

static void generate_gate_sound(void)
{
	int i;

	for (i = 0; i < GATE_SFX_SAMPLE_COUNT; ++i) {
		float time = (float)i / 44100.0f;
		float noise = (float)(texture_random() & 0xffffu) - 32768.0f;
		float phase = 6.2831853f * (58.0f * time + 55.0f * time * time);
		float envelope = (time < 0.04f ? time * 25.0f : 1.0f) * expf(-1.6f * time);
		float motor = sinf(phase) * 14500.0f +
			sinf(phase * 2.4f) * 4800.0f;
		float clank = sinf((float)i * 0.121f) * 5500.0f * expf(-18.0f * time);

		gate_open_samples[i] = clamp_audio_sample(
			(motor + noise * 0.16f + clank) * envelope);
	}
}

static void play_sound_effect(int channel, short *samples)
{
	if (channel >= 0 && sceAudioGetChannelRestLen(channel) <= 0) {
		sceAudioOutput(channel, PSP_AUDIO_VOLUME_MAX, samples);
	}
}

static unsigned int texture_color(unsigned int r, unsigned int g, unsigned int b)
{
	return 0xff000000u | ((b & 0xffu) << 16) | ((g & 0xffu) << 8) | (r & 0xffu);
}

static void generate_textures(void)
{
	int x, y;

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 35u) - 17;
			int row = y / 16;
			int brick_edge = (y % 16 < 2) ||
				((x + (row & 1) * 16) % 32 < 2);
			int crack = 0;
			int r, g, b;

			if (y > 5 && y < 38 &&
				abs(x - (8 + ((y - 5) * 3) / 5)) < 2) {
				crack = 1;
			}
			if (y > 20 && y < 52 &&
				abs(x - (47 - ((y - 20) * 2) / 3)) < 2) {
				crack = 1;
			}
			r = 112 + noise;
			g = 117 + noise;
			b = 120 + noise;
			if (brick_edge) {
				r -= 27;
				g -= 27;
				b -= 26;
			}
			if (crack) {
				r -= 45;
				g -= 43;
				b -= 40;
			}
			texture_staging[TEXTURE_CONCRETE][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 51u) - 25;
			int rail = (x >= 20 && x <= 24) || (x >= 39 && x <= 43);
			int sleeper = (y % TEXTURE_SIZE < 7 || y % TEXTURE_SIZE > 60);
			int r = 76 + noise, g = 72 + noise, b = 66 + noise;

			if (sleeper) {
				r = 105 + noise;
				g = 72 + noise / 2;
				b = 46 + noise / 3;
			}
			if (rail) {
				r = 93 + noise / 2;
				g = 101 + noise / 2;
				b = 105 + noise / 2;
				if (x == 21 || x == 40) {
					r += 35;
					g += 35;
					b += 35;
				}
			}
			if (sleeper && (x == 20 || x == 24 || x == 39 || x == 43)) {
				r = 135;
				g = 128;
				b = 112;
			}
			texture_staging[TEXTURE_TRACK][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 41u) - 20;
			int edge = x < 14 || x > 49;
			int stripe = edge && (((x + y) / 8) & 1);
			int r = 103 + noise, g = 66 + noise / 2, b = 43 + noise / 3;

			if (edge) {
				if (stripe) {
					r = 218 + noise / 3;
					g = 172 + noise / 3;
					b = 39 + noise / 4;
				} else {
					r = 31 + noise / 3;
					g = 29 + noise / 3;
					b = 25 + noise / 3;
				}
			}
			texture_staging[TEXTURE_GATE][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 23u) - 11;
			int scratch = ((x * 3 + y * 5) % 47 == 0) ||
				((x + y * 7) % 61 == 0);
			int r = 188 + noise, g = 184 + noise, b = 180 + noise;

			if (scratch) {
				r += 32;
				g += 35;
				b += 38;
			}
			texture_staging[TEXTURE_WEAPON][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int grain = (int)(sinf((float)x * 0.42f + sinf((float)y * 0.16f) * 2.0f) * 13.0f);
			int noise = (int)(texture_random() % 19u) - 9;
			int r = 126 + grain + noise;
			int g = 77 + grain / 2 + noise;
			int b = 39 + grain / 4 + noise / 2;
			if ((x + y * 3) % 37 < 2) {
				r -= 20;
				g -= 12;
				b -= 5;
			}
			texture_staging[TEXTURE_WOOD][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 27u) - 13;
			int seam = (x % 16 < 2) || (y % 16 < 2);
			int r = 94 + noise, g = 98 + noise, b = 101 + noise;
			if (seam) {
				r -= 10;
				g -= 10;
				b -= 9;
			}
			texture_staging[TEXTURE_FABRIC][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 31u) - 15;
			int mottling = ((x * 7 + y * 11) % 23 < 5) ? 13 : 0;
			int r = 126 + noise + mottling;
			int g = 76 + noise / 2 + mottling / 2;
			int b = 48 + noise / 3 + mottling / 3;
			texture_staging[TEXTURE_MUTANT][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int noise = (int)(texture_random() % 23u) - 11;
			unsigned int patch = (unsigned int)(x / 8 + (y / 8) * 3) % 7u;
			int r, g, b;
			if (patch == 0u || patch == 5u) {
				r = 103 + noise; g = 92 + noise; b = 61 + noise / 2;
			} else if (patch == 2u || patch == 6u) {
				r = 66 + noise; g = 86 + noise; b = 55 + noise / 2;
			} else {
				r = 89 + noise; g = 111 + noise; b = 75 + noise / 2;
			}
			texture_staging[TEXTURE_ARMOR][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			int dx = x - TEXTURE_SIZE / 2;
			int dy = y - TEXTURE_SIZE / 2;
			int distance = dx * dx + dy * dy;
			int noise = (int)(texture_random() % 21u) - 10;
			int r = distance < 170 ? 255 : 168 + noise;
			int g = distance < 170 ? 34 + noise : 8;
			int b = distance < 170 ? 8 : 3;
			texture_staging[TEXTURE_EYE][y * TEXTURE_SIZE + x] =
				texture_color((unsigned int)r, (unsigned int)g, (unsigned int)b);
		}
	}

	for (y = 0; y < TEXTURE_SIZE; ++y) {
		for (x = 0; x < TEXTURE_SIZE; ++x) {
			float dx = ((float)x - 31.5f) / 31.5f;
			float dy = ((float)y - 31.5f) / 31.5f;
			float radius = sqrtf(dx * dx + dy * dy);
			float fade = (radius - 0.08f) / 0.92f;
			float strength;
			unsigned int alpha;
			if (fade < 0.0f) fade = 0.0f;
			if (fade > 1.0f) fade = 1.0f;
			strength = 1.0f - fade * fade * (3.0f - 2.0f * fade);
			alpha = (unsigned int)(255.0f * strength);
			unsigned int red = 255u;
			unsigned int green = (unsigned int)(95.0f + 150.0f * strength);
			unsigned int blue = (unsigned int)(20.0f + 45.0f * strength);
			texture_staging[TEXTURE_FLASH][y * TEXTURE_SIZE + x] =
				(alpha << 24) | (blue << 16) | (green << 8) | red;
		}
	}
}

static int tunnel_vertex_uses_track_texture(size_t index)
{
	if (index < ARCHES_START) {
		return index % TUNNEL_SECTION_VERTEX_COUNT < 78;
	}
	if (index >= SLEEPERS_START && index < SLEEPERS_START + SLEEPER_VERTEX_COUNT) {
		return 1;
	}
	return 0;
}

static void set_textured_vertex(TexturedVertex *destination, Vertex source, int texture)
{
	destination->color = source.color;
	destination->x = source.x;
	destination->y = source.y;
	destination->z = source.z;

	if (texture == TEXTURE_TRACK) {
		destination->u = (source.x + 2.5f) * 12.8f;
		destination->v = -source.z * 32.0f;
	} else if (texture == TEXTURE_GATE) {
		destination->u = (source.x + 2.5f) * 12.8f;
		destination->v = (source.y + 1.5f) * 18.0f;
	} else if (texture == TEXTURE_WEAPON) {
		destination->u = (source.x + 0.6f) * 100.0f;
		destination->v = (source.y + 0.3f) * 100.0f;
	} else {
		destination->u = source.z * 32.0f;
		destination->v = source.y * 32.0f;
	}
}

static int clip_polygon_axis(const Vertex *input, int input_count, Vertex *output,
	int axis, float boundary, int keep_greater)
{
	int output_count = 0;
	int i;
	for (i = 0; i < input_count; ++i) {
		Vertex current = input[i];
		Vertex previous = input[(i + input_count - 1) % input_count];
		float current_value = axis == 0 ? current.x : (axis == 1 ? current.y : current.z);
		float previous_value = axis == 0 ? previous.x : (axis == 1 ? previous.y : previous.z);
		int current_inside = keep_greater ?
			current_value >= boundary - 0.00001f : current_value <= boundary + 0.00001f;
		int previous_inside = keep_greater ?
			previous_value >= boundary - 0.00001f : previous_value <= boundary + 0.00001f;

		if (current_inside != previous_inside) {
			float t = (boundary - previous_value) / (current_value - previous_value);
			Vertex intersection = {
				previous.color,
				previous.x + (current.x - previous.x) * t,
				previous.y + (current.y - previous.y) * t,
				previous.z + (current.z - previous.z) * t
			};
			if (output_count < 16) output[output_count++] = intersection;
		}
		if (current_inside && output_count < 16) {
			output[output_count++] = current;
		}
	}
	return output_count;
}

static int append_grid_triangle(const Vertex triangle[3], int texture,
	TexturedVertex *destination, unsigned int *destination_count,
	unsigned int *base_colors, unsigned int capacity)
{
	float min_value[3] = {triangle[0].x, triangle[0].y, triangle[0].z};
	float max_value[3] = {triangle[0].x, triangle[0].y, triangle[0].z};
	int min_cell[3], max_cell[3];
	int axis, ix, iy, iz;

	for (axis = 0; axis < 3; ++axis) {
		int vertex;
		for (vertex = 1; vertex < 3; ++vertex) {
			float value = axis == 0 ? triangle[vertex].x :
				(axis == 1 ? triangle[vertex].y : triangle[vertex].z);
			if (value < min_value[axis]) min_value[axis] = value;
			if (value > max_value[axis]) max_value[axis] = value;
		}
		min_cell[axis] = (int)floorf(min_value[axis] / GRID_CELL_SIZE);
		max_cell[axis] = (int)ceilf(max_value[axis] / GRID_CELL_SIZE) - 1;
		if (max_cell[axis] < min_cell[axis]) max_cell[axis] = min_cell[axis];
	}

	for (ix = min_cell[0]; ix <= max_cell[0]; ++ix) {
		for (iy = min_cell[1]; iy <= max_cell[1]; ++iy) {
			for (iz = min_cell[2]; iz <= max_cell[2]; ++iz) {
				Vertex a[16], b[16];
				int count = 3;
				int point;

				for (point = 0; point < 3; ++point) a[point] = triangle[point];
				count = clip_polygon_axis(a, count, b, 0,
					(float)ix * GRID_CELL_SIZE, 1);
				count = clip_polygon_axis(b, count, a, 0,
					(float)(ix + 1) * GRID_CELL_SIZE, 0);
				count = clip_polygon_axis(a, count, b, 1,
					(float)iy * GRID_CELL_SIZE, 1);
				count = clip_polygon_axis(b, count, a, 1,
					(float)(iy + 1) * GRID_CELL_SIZE, 0);
				count = clip_polygon_axis(a, count, b, 2,
					(float)iz * GRID_CELL_SIZE, 1);
				count = clip_polygon_axis(b, count, a, 2,
					(float)(iz + 1) * GRID_CELL_SIZE, 0);
				for (point = 1; point + 1 < count; ++point) {
					Vertex clipped[3] = {a[0], a[point], a[point + 1]};
					float abx = clipped[1].x - clipped[0].x;
					float aby = clipped[1].y - clipped[0].y;
					float abz = clipped[1].z - clipped[0].z;
					float acx = clipped[2].x - clipped[0].x;
					float acy = clipped[2].y - clipped[0].y;
					float acz = clipped[2].z - clipped[0].z;
					float nx = aby * acz - abz * acy;
					float ny = abz * acx - abx * acz;
					float nz = abx * acy - aby * acx;
					int vertex;
					if (nx * nx + ny * ny + nz * nz < 0.00000001f) continue;
					if (*destination_count + 3 > capacity) return 0;
					for (vertex = 0; vertex < 3; ++vertex) {
						set_textured_vertex(&destination[*destination_count],
							clipped[vertex], texture);
						base_colors[*destination_count] = clipped[vertex].color;
						if (texture == TEXTURE_CONCRETE) {
							if (fabsf(ny) >= fabsf(nx) && fabsf(ny) >= fabsf(nz)) {
								destination[*destination_count].u = clipped[vertex].x * 32.0f;
								destination[*destination_count].v = clipped[vertex].z * 32.0f;
							} else if (fabsf(nx) >= fabsf(nz)) {
								destination[*destination_count].u = clipped[vertex].z * 32.0f;
								destination[*destination_count].v = clipped[vertex].y * 32.0f;
							} else {
								destination[*destination_count].u = clipped[vertex].x * 32.0f;
								destination[*destination_count].v = clipped[vertex].y * 32.0f;
							}
						}
						++*destination_count;
					}
				}
			}
		}
	}
	return 1;
}

static int append_mesh_triangle(const Vertex triangle[3], int texture)
{
	TexturedVertex *destination = texture == TEXTURE_TRACK ?
		tunnel_track_grid : tunnel_concrete_grid;
	unsigned int *destination_count = texture == TEXTURE_TRACK ?
		&tunnel_track_grid_count : &tunnel_concrete_grid_count;
	unsigned int *base_colors = texture == TEXTURE_TRACK ?
		tunnel_track_base_colors : tunnel_concrete_base_colors;
	float abx = triangle[1].x - triangle[0].x;
	float aby = triangle[1].y - triangle[0].y;
	float abz = triangle[1].z - triangle[0].z;
	float acx = triangle[2].x - triangle[0].x;
	float acy = triangle[2].y - triangle[0].y;
	float acz = triangle[2].z - triangle[0].z;
	float nx = aby * acz - abz * acy;
	float ny = abz * acx - abx * acz;
	float nz = abx * acy - aby * acx;
	int vertex;

	if (*destination_count + 3 > MAX_TUNNEL_VERTS) return 0;
	for (vertex = 0; vertex < 3; ++vertex) {
		Vertex source = triangle[vertex];
		set_textured_vertex(&destination[*destination_count], source, texture);
		base_colors[*destination_count] = source.color;
		if (texture == TEXTURE_CONCRETE) {
			if (fabsf(ny) >= fabsf(nx) && fabsf(ny) >= fabsf(nz)) {
				destination[*destination_count].u = source.x * 32.0f;
				destination[*destination_count].v = source.z * 32.0f;
			} else if (fabsf(nx) >= fabsf(nz)) {
				destination[*destination_count].u = source.z * 32.0f;
				destination[*destination_count].v = source.y * 32.0f;
			} else {
				destination[*destination_count].u = source.x * 32.0f;
				destination[*destination_count].v = source.y * 32.0f;
			}
		}
		++*destination_count;
	}
	return 1;
}

static int build_tunnel_mesh(void)
{
	size_t i;
	tunnel_track_grid_count = 0;
	tunnel_concrete_grid_count = 0;
	if (TUNNEL_VERTEX_COUNT != TUNNEL_EXPECTED_VERTEX_COUNT ||
	TUNNEL_VERTEX_COUNT + HALL_VERTEX_COUNT + INTRO_VERTEX_COUNT +
		sizeof(intro_partition_verts) / sizeof(Vertex) >
			MAX_TUNNEL_VERTS ||
	INTRO_VERTEX_COUNT != INTRO_SECTION_COUNT * INTRO_SECTION_VERTEX_COUNT ||
	sizeof(intro_partition_verts) / sizeof(Vertex) !=
		INTRO_PARTITION_COUNT * 36) {
		return 0;
	}
	for (i = 0; i < TUNNEL_VERTEX_COUNT; i += 3) {
		Vertex triangle[3] = {
			tunnel_verts[i], tunnel_verts[i + 1], tunnel_verts[i + 2]
		};
		int texture = tunnel_vertex_uses_track_texture(i) ?
			TEXTURE_TRACK : TEXTURE_CONCRETE;
		if (!append_mesh_triangle(triangle, texture)) return 0;
	}
	if (HALL_VERTEX_COUNT != HALL_EXPECTED_VERTEX_COUNT) return 0;
	for (i = 0; i < HALL_VERTEX_COUNT; i += 3) {
		Vertex triangle[3] = {
			hall_verts[i], hall_verts[i + 1], hall_verts[i + 2]
		};
		if (!append_mesh_triangle(triangle, TEXTURE_CONCRETE)) return 0;
	}
	for (i = 0; i < INTRO_VERTEX_COUNT; i += 3) {
		Vertex triangle[3] = {
			intro_structure_verts[i], intro_structure_verts[i + 1],
			intro_structure_verts[i + 2]
		};
		if (!append_mesh_triangle(triangle, TEXTURE_CONCRETE)) return 0;
	}
	for (i = 0; i < sizeof(intro_partition_verts) / sizeof(Vertex); i += 3) {
		Vertex triangle[3] = {
			intro_partition_verts[i], intro_partition_verts[i + 1],
			intro_partition_verts[i + 2]
		};
		if (!append_mesh_triangle(triangle, TEXTURE_CONCRETE)) return 0;
	}
	return 1;
}

static int build_subdivided_gate(void)
{
	size_t i;
	gate_panel_grid_count = 0;
	gate_frame_grid_count = 0;
	for (i = 0; i < sizeof(gate_verts) / sizeof(Vertex); i += 3) {
		Vertex triangle[3] = {gate_verts[i], gate_verts[i + 1], gate_verts[i + 2]};
		if (!append_grid_triangle(triangle, TEXTURE_GATE, gate_panel_grid,
			&gate_panel_grid_count, gate_panel_base_colors, 4096)) return 0;
	}
	for (i = 0; i < sizeof(gate_frame_verts) / sizeof(Vertex); i += 3) {
		Vertex triangle[3] = {
			gate_frame_verts[i], gate_frame_verts[i + 1], gate_frame_verts[i + 2]
		};
		if (!append_grid_triangle(triangle, TEXTURE_GATE, gate_frame_grid,
			&gate_frame_grid_count, gate_frame_base_colors, 4096)) return 0;
	}
	return 1;
}

static void prepare_surface_geometry(const Vertex *source, TexturedVertex *destination,
	size_t count, float texture_scale)
{
	size_t i;
	for (i = 0; i < count; ++i) {
		destination[i].color = source[i].color;
		destination[i].x = source[i].x;
		destination[i].y = source[i].y;
		destination[i].z = source[i].z;
		destination[i].u = (source[i].x + source[i].y * 0.37f) * texture_scale;
		destination[i].v = (source[i].z + source[i].y * 0.53f) * texture_scale;
	}
}

static int append_barrel_band(float y0, float y1, float radius0, float radius1,
	unsigned int color, unsigned int *vertex_count)
{
	int side;
	for (side = 0; side < BARREL_SEGMENTS; ++side) {
		int next = (side + 1) % BARREL_SEGMENTS;
		float angle0 = 6.28318530718f * (float)side / (float)BARREL_SEGMENTS;
		float angle1 = 6.28318530718f * (float)next / (float)BARREL_SEGMENTS;
		Vertex a = {color, cosf(angle0) * radius0, y0, sinf(angle0) * radius0};
		Vertex b = {color, cosf(angle1) * radius0, y0, sinf(angle1) * radius0};
		Vertex c = {color, cosf(angle1) * radius1, y1, sinf(angle1) * radius1};
		Vertex d = {color, cosf(angle0) * radius1, y1, sinf(angle0) * radius1};
		if (*vertex_count + 6 > BARREL_VERTEX_COUNT) return 0;
		barrel_raw_verts[(*vertex_count)++] = a;
		barrel_raw_verts[(*vertex_count)++] = b;
		barrel_raw_verts[(*vertex_count)++] = c;
		barrel_raw_verts[(*vertex_count)++] = a;
		barrel_raw_verts[(*vertex_count)++] = c;
		barrel_raw_verts[(*vertex_count)++] = d;
	}
	return 1;
}

static int prepare_blockpost_geometry(void)
{
	unsigned int barrel_vertex_count = 0;
	size_t i;
	if (!append_barrel_band(0.00f, 0.30f, 0.29f, 0.35f,
		0xff686d70, &barrel_vertex_count) ||
		!append_barrel_band(0.30f, 0.66f, 0.35f, 0.35f,
		0xff5d6265, &barrel_vertex_count) ||
		!append_barrel_band(0.66f, 0.95f, 0.35f, 0.29f,
		0xff686d70, &barrel_vertex_count) ||
		!append_barrel_band(0.17f, 0.22f, 0.36f, 0.37f,
		0xff33383a, &barrel_vertex_count) ||
		!append_barrel_band(0.72f, 0.77f, 0.36f, 0.37f,
		0xff33383a, &barrel_vertex_count) ||
		barrel_vertex_count != BARREL_VERTEX_COUNT) {
		return 0;
	}

	prepare_surface_geometry(ammo_crate_verts, textured_ammo_crate_verts,
		sizeof(ammo_crate_verts) / sizeof(Vertex), 22.0f);
	prepare_surface_geometry(barrel_raw_verts, barrel_verts,
		barrel_vertex_count, 24.0f);
	prepare_surface_geometry(barrel_fire_raw, barrel_fire_verts,
		sizeof(barrel_fire_raw) / sizeof(Vertex), 1.0f);
	prepare_surface_geometry(barrel_glow_raw, barrel_glow_verts,
		sizeof(barrel_glow_raw) / sizeof(Vertex), 1.0f);
	for (i = 0; i < sizeof(barrel_fire_raw) / sizeof(Vertex); ++i) {
		barrel_fire_verts[i].u = (barrel_fire_raw[i].x + 0.35f) * 64.0f / 0.70f;
		barrel_fire_verts[i].v = (1.60f - barrel_fire_raw[i].y) * 64.0f / 1.30f;
	}
	for (i = 0; i < sizeof(barrel_glow_raw) / sizeof(Vertex); ++i) {
		barrel_glow_verts[i].u = (barrel_glow_raw[i].x + 0.90f) * 64.0f / 1.80f;
		barrel_glow_verts[i].v = (1.70f - barrel_glow_raw[i].y) * 64.0f / 1.50f;
	}
	return 1;
}

static int prepare_textured_geometry(void)
{
	size_t i;

	if (!build_tunnel_mesh() || !build_subdivided_gate()) return 0;
	prepare_surface_geometry(mutant_eye_verts, textured_mutant_eye_verts,
		sizeof(mutant_eye_verts) / sizeof(Vertex), 18.0f);
	prepare_surface_geometry(lamp_housing_verts, textured_lamp_housing_verts,
		sizeof(lamp_housing_verts) / sizeof(Vertex), 20.0f);
	prepare_surface_geometry(lamp_lens_verts, textured_lamp_lens_verts,
		sizeof(lamp_lens_verts) / sizeof(Vertex), 20.0f);
	prepare_surface_geometry(intro_wood_verts, textured_intro_wood_verts,
		sizeof(intro_wood_verts) / sizeof(Vertex), 22.0f);
	prepare_surface_geometry(intro_metal_verts, textured_intro_metal_verts,
		sizeof(intro_metal_verts) / sizeof(Vertex), 24.0f);
	prepare_surface_geometry(armory_weapon_verts, textured_armory_weapon_verts,
		sizeof(armory_weapon_verts) / sizeof(Vertex), 26.0f);
	prepare_surface_geometry(intro_lamp_verts, textured_intro_lamp_verts,
		sizeof(intro_lamp_verts) / sizeof(Vertex), 20.0f);
	prepare_surface_geometry(menu_structure_verts, textured_menu_structure,
		sizeof(menu_structure_verts) / sizeof(Vertex), 18.0f);
	prepare_surface_geometry(menu_wood_verts, textured_menu_wood,
		sizeof(menu_wood_verts) / sizeof(Vertex), 22.0f);
	prepare_surface_geometry(menu_metal_verts, textured_menu_metal,
		sizeof(menu_metal_verts) / sizeof(Vertex), 24.0f);
	prepare_surface_geometry(menu_red_lamp_verts, textured_menu_lamp,
		sizeof(menu_red_lamp_verts) / sizeof(Vertex), 20.0f);
	for (i = 0; i < sizeof(mutant_eye_verts) / sizeof(Vertex); ++i) {
		float eye_center_x = mutant_eye_verts[i].x < 0.0f ? -0.1075f : 0.1075f;
		textured_mutant_eye_verts[i].u =
			((mutant_eye_verts[i].x - eye_center_x) / 0.105f + 1.0f) * 32.0f;
		textured_mutant_eye_verts[i].v =
			((mutant_eye_verts[i].y - 1.555f) / 0.035f + 1.0f) * 32.0f;
	}
	return 1;
}

static void overlay_sprite(float x0, float y0, float x1, float y1, unsigned int color)
{
	if (!overlay_build_target || !overlay_build_count ||
		*overlay_build_count + 2 > overlay_build_capacity) {
		return;
	}

	overlay_build_target[(*overlay_build_count)++] = (Vertex){color, x0, y0, 0.0f};
	overlay_build_target[(*overlay_build_count)++] = (Vertex){color, x1, y1, 0.0f};
}

static void overlay_text(const char *text, int x, int y, int scale, unsigned int color)
{
	int origin_x = x;

	while (*text) {
		unsigned char ch = (unsigned char)*text++;
		int row, column;

		if (ch == '\n') {
			x = origin_x;
			y += 9 * scale;
			continue;
		}

		if (ch >= 'a' && ch <= 'z') {
			ch = (unsigned char)(ch - 'a' + 'A');
		}
		if (ch < 128) {
			for (row = 0; row < 8; ++row) {
				unsigned char bits = font8x8[ch][row];
				for (column = 0; column < 8; ++column) {
					if (bits & (0x80u >> column)) {
						overlay_sprite((float)(x + column * scale),
							(float)(y + row * scale),
							(float)(x + (column + 1) * scale),
							(float)(y + (row + 1) * scale), color);
					}
				}
			}
		}
		x += 8 * scale;
	}
}

static void prepare_overlay_geometry(void)
{
	int i;
	unsigned int temporary_count = 0;

	overlay_build_target = overlay_verts;
	overlay_build_count = &overlay_static_count;
	overlay_build_capacity = sizeof(overlay_verts) / sizeof(overlay_verts[0]);
	overlay_static_count = 0;
	overlay_sprite(13.0f, 25.0f, 113.0f, 33.0f, 0xff303030);
	overlay_text("HP", 14, 12, 1, 0xffffffff);
	overlay_text("AMMO", 14, 39, 1, 0xffffffff);
	overlay_text("BUNKER D6 v5.0", 360, 12, 1, 0xffd0d0d0);
	overlay_text("DOWN: RELOAD", 360, 24, 1, 0xffd0d0d0);
	overlay_text("LTRIGGER: CYCLE", 354, 36, 1, 0xffd0d0d0);
	overlay_sprite(0.0f, 211.0f, 480.0f, 272.0f, 0xb0000000);

	overlay_build_target = overlay_dialogue_verts;
	overlay_build_count = &overlay_dialogue_count;
	overlay_build_capacity =
		sizeof(overlay_dialogue_verts) / sizeof(overlay_dialogue_verts[0]);
	overlay_dialogue_count = 0;
	overlay_text("ANNA: ARTYOM, CLEAR THE TUNNEL", 12, 222, 1, 0xffffffff);
	overlay_text("AND OPEN D6 GATE [TRIANGLE].", 12, 237, 1, 0xffffffff);

	overlay_build_target = overlay_complete_verts;
	overlay_build_count = &overlay_complete_count;
	overlay_build_capacity =
		sizeof(overlay_complete_verts) / sizeof(overlay_complete_verts[0]);
	overlay_complete_count = 0;
	overlay_text("GATE OPENED! CHAPTER 1 COMPLETE.", 20, 232, 1, 0xffffffff);

	{
		static const char *khan_lines[3][2] = {
			{"KHAN: YOU MADE IT ARTYOM.",
				"THE DARK ONES WENT TO THE SURFACE."},
			{"KHAN: GRAB AMMO FROM THE CRATE",
				"AND WATCH THE SHADOWS!"},
			{"KHAN: CLEAR THE TUNNEL AND REACH",
				"THE SURFACE AIRLOCK."}
		};
		for (i = 0; i < 3; ++i) {
			overlay_build_target = overlay_khan_dialogue_verts[i];
			overlay_build_count = &overlay_khan_dialogue_counts[i];
			overlay_build_capacity = sizeof(overlay_khan_dialogue_verts[i]) /
				sizeof(overlay_khan_dialogue_verts[i][0]);
			overlay_khan_dialogue_counts[i] = 0;
			overlay_text(khan_lines[i][0], 12, 220, 1, 0xffffffff);
			overlay_text(khan_lines[i][1], 12, 235, 1, 0xffffffff);
		}
	}

	overlay_build_target = overlay_chapter2_complete_verts;
	overlay_build_count = &overlay_chapter2_complete_count;
	overlay_build_capacity = sizeof(overlay_chapter2_complete_verts) /
		sizeof(overlay_chapter2_complete_verts[0]);
	overlay_chapter2_complete_count = 0;
	overlay_text("CHAPTER 2 COMPLETE - SURFACE REACHED!",
		20, 232, 1, 0xffffffff);

	{
		static const char *intro_messages[3][2] = {
			{"COMMANDER: WAKE UP! PASS",
				"THROUGH THE MESS HALL TO ARMORY."},
			{"GUARD: GOOD LUCK OUT THERE!", ""},
			{"ARMORY READY. LTRIGGER TO CYCLE.",
				"TEST THE WEAPONS, THEN OPEN THE GATE."}
		};
		for (i = 0; i < 3; ++i) {
			overlay_build_target = overlay_intro_message_verts[i];
			overlay_build_count = &overlay_intro_message_counts[i];
			overlay_build_capacity = sizeof(overlay_intro_message_verts[i]) /
				sizeof(overlay_intro_message_verts[i][0]);
			overlay_intro_message_counts[i] = 0;
			overlay_text(intro_messages[i][0], 12, 225, 1, 0xffffffff);
			overlay_text(intro_messages[i][1], 12, 240, 1, 0xffffffff);
		}
	}

	{
		static const char *weapon_names[WEAPON_COUNT] = {
			"1 PISTOL", "2 ASSAULT RIFLE", "3 SHOTGUN", "4 SNIPER RIFLE"
		};
		for (i = 0; i < WEAPON_COUNT; ++i) {
			overlay_build_target = overlay_weapon_name_verts[i];
			overlay_build_count = &overlay_weapon_name_counts[i];
			overlay_build_capacity = sizeof(overlay_weapon_name_verts[i]) /
				sizeof(overlay_weapon_name_verts[i][0]);
			overlay_weapon_name_counts[i] = 0;
			overlay_text(weapon_names[i], 330, 48, 1, 0xffe6d4a0);
		}
	}

	overlay_build_target = overlay_watch_verts;
	overlay_build_count = &overlay_watch_count;
	overlay_build_capacity = sizeof(overlay_watch_verts) / sizeof(overlay_watch_verts[0]);
	overlay_watch_count = 0;
	overlay_sprite(423.0f, 48.0f, 455.0f, 50.0f, 0xffa0a0a0);
	overlay_sprite(423.0f, 64.0f, 455.0f, 66.0f, 0xffa0a0a0);
	overlay_sprite(423.0f, 48.0f, 425.0f, 66.0f, 0xffa0a0a0);
	overlay_sprite(453.0f, 48.0f, 455.0f, 66.0f, 0xffa0a0a0);
	overlay_sprite(438.0f, 50.0f, 440.0f, 58.0f, 0xff80e070);
	overlay_text("STEALTH", 414, 70, 1, 0xff80e070);

	overlay_build_target = overlay_crosshair_verts;
	overlay_build_count = &overlay_crosshair_count;
	overlay_build_capacity =
		sizeof(overlay_crosshair_verts) / sizeof(overlay_crosshair_verts[0]);
	overlay_crosshair_count = 0;
	overlay_sprite(236.0f, 132.0f, 244.0f, 140.0f, 0xffffffff);
	overlay_sprite(239.0f, 126.0f, 241.0f, 146.0f, 0xffffffff);
	overlay_sprite(230.0f, 135.0f, 250.0f, 137.0f, 0xffffffff);

	overlay_build_target = overlay_weapon_pickup_hint;
	overlay_build_count = &overlay_weapon_pickup_hint_count;
	overlay_build_capacity = sizeof(overlay_weapon_pickup_hint) /
		sizeof(overlay_weapon_pickup_hint[0]);
	overlay_weapon_pickup_hint_count = 0;
	overlay_text("PRESS TRIANGLE TO EQUIP ALL WEAPONS",
		74, 190, 1, 0xffffd080);

	overlay_build_target = overlay_hp_verts;
	overlay_build_count = &temporary_count;
	overlay_build_capacity = sizeof(overlay_hp_verts) / sizeof(overlay_hp_verts[0]);
	temporary_count = 0;
	for (i = 0; i < 100; ++i) {
		float x = 14.0f + (float)i;
		overlay_sprite(x, 26.0f, x + 1.0f, 32.0f, 0xff3030e8);
	}

	overlay_build_target = overlay_ammo_full_verts;
	temporary_count = 0;
	overlay_build_count = &temporary_count;
	overlay_build_capacity =
		sizeof(overlay_ammo_full_verts) / sizeof(overlay_ammo_full_verts[0]);
	for (i = 0; i < 30; ++i) {
		float x = 14.0f + (float)i * 5.0f;
		overlay_sprite(x, 51.0f, x + 3.0f, 61.0f, 0xff20d8ff);
	}

	overlay_build_target = overlay_ammo_empty_verts;
	temporary_count = 0;
	overlay_build_capacity =
		sizeof(overlay_ammo_empty_verts) / sizeof(overlay_ammo_empty_verts[0]);
	for (i = 0; i < 30; ++i) {
		float x = 14.0f + (float)i * 5.0f;
		overlay_sprite(x, 51.0f, x + 3.0f, 61.0f, 0xff424242);
	}

	overlay_build_target = overlay_menu_title;
	overlay_build_count = &overlay_menu_title_count;
	overlay_build_capacity = sizeof(overlay_menu_title) / sizeof(overlay_menu_title[0]);
	overlay_menu_title_count = 0;
	overlay_text("BUNKER D6", 48, 36, 3, 0xffe6d4a0);
	overlay_build_target = overlay_chapter_heading;
	overlay_build_count = &overlay_chapter_heading_count;
	overlay_build_capacity = sizeof(overlay_chapter_heading) /
		sizeof(overlay_chapter_heading[0]);
	overlay_chapter_heading_count = 0;
	overlay_text("CHAPTER SELECT", 48, 78, 2, 0xffe6d4a0);
	overlay_build_target = overlay_menu_version;
	overlay_build_count = &overlay_menu_version_count;
	overlay_build_capacity = sizeof(overlay_menu_version) /
		sizeof(overlay_menu_version[0]);
	overlay_menu_version_count = 0;
	overlay_text("BUNKER D6 v5.0", 344, 230, 1, 0xffd0d0d0);
	overlay_build_target = overlay_menu_hint;
	overlay_build_count = &overlay_menu_hint_count;
	overlay_build_capacity = sizeof(overlay_menu_hint) /
		sizeof(overlay_menu_hint[0]);
	overlay_menu_hint_count = 0;
	overlay_text("CROSS/START: SELECT  CIRCLE: BACK",
		36, 230, 1, 0xffd0d0d0);
	overlay_menu_panel[0] = (Vertex){0xb0000000, 20.0f, 20.0f, 0.0f};
	overlay_menu_panel[1] = (Vertex){0xb0000000, 460.0f, 252.0f, 0.0f};

	{
		static const char *main_items[3] = {
			"NEW GAME", "CHAPTER SELECT", "CONTROLS"
		};
		static const char *chapter_items[3] = {
			"1. BUNKER PROLOGUE", "2. DARK TUNNEL", "BACK"
		};
		static const char *pause_items[2] = {"RESUME", "MAIN MENU"};
		int item, selected;
		for (item = 0; item < 3; ++item) {
			for (selected = 0; selected < 2; ++selected) {
				overlay_build_target = overlay_main_options[item][selected];
				overlay_build_count = &overlay_main_option_counts[item][selected];
				overlay_build_capacity =
					sizeof(overlay_main_options[item][selected]) /
					sizeof(overlay_main_options[item][selected][0]);
				overlay_main_option_counts[item][selected] = 0;
				overlay_text(selected ? "> " : "  ", 66, 122 + item * 40, 2,
					selected ? 0xff80e8ff : 0xffd0d0d0);
				overlay_text(main_items[item], 98, 122 + item * 40, 2,
					selected ? 0xff80e8ff : 0xffd0d0d0);

				overlay_build_target = overlay_chapter_options[item][selected];
				overlay_build_count =
					&overlay_chapter_option_counts[item][selected];
				overlay_build_capacity =
					sizeof(overlay_chapter_options[item][selected]) /
					sizeof(overlay_chapter_options[item][selected][0]);
				overlay_chapter_option_counts[item][selected] = 0;
				overlay_text(selected ? "> " : "  ", 28, 118 + item * 38, 2,
					selected ? 0xff80e8ff : 0xffd0d0d0);
				overlay_text(chapter_items[item], 60, 118 + item * 38, 2,
					selected ? 0xff80e8ff : 0xffd0d0d0);
			}
		}
		for (item = 0; item < 2; ++item) {
			for (selected = 0; selected < 2; ++selected) {
				overlay_build_target = overlay_pause_options[item][selected];
				overlay_build_count = &overlay_pause_option_counts[item][selected];
				overlay_build_capacity =
					sizeof(overlay_pause_options[item][selected]) /
					sizeof(overlay_pause_options[item][selected][0]);
				overlay_pause_option_counts[item][selected] = 0;
				overlay_text(selected ? "> " : "  ", 164, 120 + item * 42, 2,
					selected ? 0xff80e8ff : 0xffd0d0d0);
				overlay_text(pause_items[item], 196, 120 + item * 42, 2,
					selected ? 0xff80e8ff : 0xffd0d0d0);
			}
		}
	}

	overlay_build_target = overlay_controls_title;
	overlay_build_count = &overlay_controls_title_count;
	overlay_build_capacity =
		sizeof(overlay_controls_title) / sizeof(overlay_controls_title[0]);
	overlay_controls_title_count = 0;
	overlay_text("CONTROLS", 48, 34, 3, 0xffe6d4a0);
	overlay_build_target = overlay_controls_text;
	overlay_build_count = &overlay_controls_text_count;
	overlay_build_capacity =
		sizeof(overlay_controls_text) / sizeof(overlay_controls_text[0]);
	overlay_controls_text_count = 0;
	overlay_text("MOVE: ANALOG STICK\nTURN: SQUARE OR CIRCLE\nFIRE: CROSS\n"
		"RELOAD: DOWN\nFLASHLIGHT: R\nCHANGE WEAPON: LTRIGGER\n"
		"SNIPER ZOOM: UP\nINTERACT: TRIANGLE\nPAUSE: START\nBACK: CIRCLE",
		54, 82, 1, 0xffffffff);

	overlay_build_target = overlay_pause_title;
	overlay_build_count = &overlay_pause_title_count;
	overlay_build_capacity =
		sizeof(overlay_pause_title) / sizeof(overlay_pause_title[0]);
	overlay_pause_title_count = 0;
	overlay_text("PAUSED", 180, 62, 3, 0xffe6d4a0);

	overlay_build_target = overlay_cutscene_dialogue;
	overlay_build_count = &overlay_cutscene_dialogue_count;
	overlay_build_capacity =
		sizeof(overlay_cutscene_dialogue) / sizeof(overlay_cutscene_dialogue[0]);
	overlay_cutscene_dialogue_count = 0;
	overlay_text("COMMANDER: WAKE UP! WE HAVE ORDERS",
		12, 222, 1, 0xffffffff);
	overlay_text("TO MOVE OUT!", 12, 237, 1, 0xffffffff);

	for (i = 0; i < 256; ++i) {
		unsigned int color = (unsigned int)i << 24;
		overlay_fade_sprites[i][0] = (Vertex){color, 0.0f, 0.0f, 0.0f};
		overlay_fade_sprites[i][1] =
			(Vertex){color, 480.0f, 272.0f, 0.0f};
	}

	overlay_build_target = 0;
	overlay_build_count = 0;
}

static void draw_overlay(int hp, int ammo, int ammo_capacity, int chapter1_complete,
	int khan_dialogue_line, int chapter2_complete, int range_gate_open,
	int intro_message, int equipped, int weapon_type, int armory_near)
{
	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_FOG);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
	sceGuDrawArray(GU_SPRITES,
		GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		overlay_static_count, 0, overlay_verts);
	if (armory_near && !equipped) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_weapon_pickup_hint_count, 0,
			overlay_weapon_pickup_hint);
	}
	if (equipped) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_crosshair_count, 0, overlay_crosshair_verts);
		if (weapon_type >= 0 && weapon_type < WEAPON_COUNT) {
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				overlay_weapon_name_counts[weapon_type], 0,
				overlay_weapon_name_verts[weapon_type]);
		}
	} else {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_watch_count, 0, overlay_watch_verts);
	}
	sceGuDrawArray(GU_SPRITES,
		GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		(unsigned int)hp * 2, 0, overlay_hp_verts);
	sceGuDrawArray(GU_SPRITES,
		GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		(unsigned int)ammo * 2, 0, overlay_ammo_full_verts);
	if (ammo < ammo_capacity) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			(unsigned int)(ammo_capacity - ammo) * 2, 0,
			&overlay_ammo_empty_verts[ammo * 2]);
	}
	if (chapter2_complete) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_chapter2_complete_count, 0, overlay_chapter2_complete_verts);
	} else if (khan_dialogue_line >= 0 && khan_dialogue_line < 3) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_khan_dialogue_counts[khan_dialogue_line], 0,
			overlay_khan_dialogue_verts[khan_dialogue_line]);
	} else if (chapter1_complete) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_complete_count, 0, overlay_complete_verts);
	} else if (!range_gate_open) {
		if (intro_message >= 0 && intro_message < 3) {
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				overlay_intro_message_counts[intro_message], 0,
				overlay_intro_message_verts[intro_message]);
		}
	} else {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_dialogue_count, 0, overlay_dialogue_verts);
	}
	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuEnable(GU_FOG);
}

static float smoothstep(float edge0, float edge1, float value)
{
	float t = (value - edge0) / (edge1 - edge0);
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	return t * t * (3.0f - 2.0f * t);
}

static unsigned int multiply_channel(unsigned int channel, float brightness)
{
	float value = (float)channel * brightness;
	if (value > 255.0f) value = 255.0f;
	if (value < 0.0f) value = 0.0f;
	return (unsigned int)value;
}

static unsigned int lit_color(unsigned int color, float red, float green, float blue)
{
	unsigned int r = multiply_channel(color & 0xffu, red);
	unsigned int g = multiply_channel((color >> 8) & 0xffu, green);
	unsigned int b = multiply_channel((color >> 16) & 0xffu, blue);
	return (color & 0xff000000u) | r | (g << 8) | (b << 16);
}

static void append_mutant_triangle(Vertex a, Vertex b, Vertex c)
{
	mutant_verts[mutant_vertex_count++] = a;
	mutant_verts[mutant_vertex_count++] = b;
	mutant_verts[mutant_vertex_count++] = c;
}

static void append_mutant_ellipsoid(float cx, float cy, float cz,
	float rx, float ry, float rz, float rotation_z, unsigned int color)
{
	enum { LONGITUDE_SEGMENTS = 8, LATITUDE_SEGMENTS = 4 };
	Vertex rings[LATITUDE_SEGMENTS + 1][LONGITUDE_SEGMENTS + 1];
	float cos_rotation = cosf(rotation_z);
	float sin_rotation = sinf(rotation_z);
	int latitude, longitude;

	for (latitude = 0; latitude <= LATITUDE_SEGMENTS; ++latitude) {
		float phi = -1.570796327f +
			3.141592654f * (float)latitude / (float)LATITUDE_SEGMENTS;
		float ring_radius = cosf(phi);
		float normal_y = sinf(phi);

		for (longitude = 0; longitude <= LONGITUDE_SEGMENTS; ++longitude) {
			float theta = 6.283185307f * (float)longitude /
				(float)LONGITUDE_SEGMENTS;
			float normal_x = ring_radius * cosf(theta);
			float normal_z = ring_radius * sinf(theta);
			float rotated_x = normal_x * cos_rotation - normal_y * sin_rotation;
			float rotated_y = normal_x * sin_rotation + normal_y * cos_rotation;
			float diffuse = -rotated_x * 0.24f + rotated_y * 0.62f +
				normal_z * 0.76f;
			float brightness;

			if (diffuse < 0.0f) diffuse = 0.0f;
			brightness = 0.60f + diffuse * 0.38f;
			rings[latitude][longitude] = (Vertex){
				lit_color(color, brightness, brightness, brightness),
				cx + rx * normal_x * cos_rotation - ry * normal_y * sin_rotation,
				cy + rx * normal_x * sin_rotation + ry * normal_y * cos_rotation,
				cz + rz * normal_z
			};
		}
	}

	for (latitude = 0; latitude < LATITUDE_SEGMENTS; ++latitude) {
		for (longitude = 0; longitude < LONGITUDE_SEGMENTS; ++longitude) {
			Vertex a = rings[latitude][longitude];
			Vertex b = rings[latitude][longitude + 1];
			Vertex c = rings[latitude + 1][longitude + 1];
			Vertex d = rings[latitude + 1][longitude];

			append_mutant_triangle(a, b, c);
			append_mutant_triangle(a, c, d);
		}
	}
}

static void build_mutant_geometry(float animation_phase)
{
	float arm_angle = 0.314159265f;
	float stride = sinf(animation_phase * 2.0f) * 0.11f;
	int side;

	mutant_vertex_count = 0;
	append_mutant_ellipsoid(0.0f, 0.36f, -0.02f,
		0.32f, 0.28f, 0.26f, 0.0f, 0xff493123);
	append_mutant_ellipsoid(0.0f, 0.70f, -0.04f,
		0.35f, 0.43f, 0.29f, 0.0f, 0xff523723);
	append_mutant_ellipsoid(0.0f, 1.02f, -0.10f,
		0.48f, 0.38f, 0.34f, 0.0f, 0xff583a24);
	append_mutant_ellipsoid(0.0f, 1.20f, -0.16f,
		0.34f, 0.28f, 0.30f, 0.0f, 0xff63432a);
	append_mutant_ellipsoid(0.0f, 1.39f, 0.02f,
		0.17f, 0.20f, 0.18f, 0.0f, 0xff493123);
	append_mutant_ellipsoid(0.0f, 1.57f, 0.06f,
		0.31f, 0.19f, 0.36f, 0.0f, 0xff624229);
	append_mutant_ellipsoid(0.0f, 1.39f, 0.25f,
		0.22f, 0.12f, 0.20f, 0.0f, 0xff422b20);
	append_mutant_ellipsoid(0.0f, 1.68f, 0.24f,
		0.27f, 0.08f, 0.10f, 0.0f, 0xff725033);

	for (side = -1; side <= 1; side += 2) {
		float sign = (float)side;
		float angle = sign * (arm_angle + stride);

		append_mutant_ellipsoid(sign * 0.39f, 1.08f, 0.00f,
			0.26f, 0.27f, 0.25f, 0.0f, 0xff563923);
		append_mutant_ellipsoid(sign * 0.53f, 0.78f, 0.12f,
			0.17f, 0.37f, 0.18f, angle, 0xff4c3322);
		append_mutant_ellipsoid(sign * 0.66f, 0.39f, 0.23f + stride,
			0.14f, 0.29f, 0.15f, sign * 0.12f + stride, 0xff583a25);
		append_mutant_ellipsoid(sign * 0.69f, 0.13f, 0.35f + stride,
			0.16f, 0.16f, 0.17f, 0.0f, 0xff61432b);
		append_mutant_ellipsoid(sign * 0.21f, 0.32f, -0.03f - stride,
			0.21f, 0.29f, 0.22f, -sign * 0.08f - stride, 0xff493123);
		append_mutant_ellipsoid(sign * 0.24f, 0.15f, -0.01f - stride,
			0.15f, 0.19f, 0.16f, sign * 0.05f - stride, 0xff563923);
		append_mutant_ellipsoid(sign * 0.25f, 0.07f, 0.10f - stride,
			0.17f, 0.08f, 0.23f, 0.0f, 0xff422b20);
	}

	append_mutant_ellipsoid(-0.11f, 1.60f, 0.275f,
		0.060f, 0.050f, 0.035f, 0.0f, 0xff311d17);
	append_mutant_ellipsoid(0.11f, 1.60f, 0.275f,
		0.060f, 0.050f, 0.035f, 0.0f, 0xff311d17);
	append_mutant_triangle((Vertex){0xffb7b49b,
		-0.085f, 1.40f, 0.385f}, (Vertex){0xffb7b49b,
		-0.045f, 1.39f, 0.405f}, (Vertex){0xffb7b49b,
		-0.066f, 1.33f, 0.391f});
	append_mutant_triangle((Vertex){0xffb7b49b,
		0.045f, 1.39f, 0.405f}, (Vertex){0xffb7b49b,
		0.085f, 1.40f, 0.385f}, (Vertex){0xffb7b49b,
		0.066f, 1.33f, 0.391f});

	for (side = -1; side <= 1; side += 2) {
		float sign = (float)side;
		append_mutant_triangle((Vertex){0xff745032,
			sign * 0.56f, 0.98f, -0.08f},
			(Vertex){0xff745032, sign * 0.79f, 1.24f, -0.14f},
			(Vertex){0xff745032, sign * 0.38f, 1.12f, 0.08f});
		append_mutant_triangle((Vertex){0xff805637,
			sign * 0.57f, 0.13f, 0.38f},
			(Vertex){0xff805637, sign * 0.59f, 0.34f, 0.42f},
			(Vertex){0xff805637, sign * 0.64f, 0.12f, 0.35f});
	}
	append_mutant_triangle((Vertex){0xff815b3b, -0.25f, 1.18f, -0.24f},
		(Vertex){0xff815b3b, -0.12f, 1.48f, -0.21f},
		(Vertex){0xff815b3b, -0.08f, 1.18f, -0.16f});
	append_mutant_triangle((Vertex){0xff815b3b, 0.08f, 1.18f, -0.16f},
		(Vertex){0xff815b3b, 0.12f, 1.48f, -0.21f},
		(Vertex){0xff815b3b, 0.25f, 1.18f, -0.24f});
	append_mutant_triangle((Vertex){0xff815b3b, -0.12f, 0.94f, -0.34f},
		(Vertex){0xff815b3b, 0.00f, 1.22f, -0.40f},
		(Vertex){0xff815b3b, 0.12f, 0.94f, -0.34f});
}

static unsigned int light_vertex_color(Vertex source, float px, float py, float pz,
	float yaw, int flashlight, float world_x_offset, float world_z_offset)
{
	float dx = source.x + world_x_offset - px;
	float dy = source.y - py;
	float dz = source.z + world_z_offset - pz;
	float distance = sqrtf(dx * dx + dy * dy + dz * dz);
	float ambient = 0.50f + 0.34f * (1.0f - smoothstep(0.0f, 60.0f, distance));
	float beam = 0.0f;
	float red_glow = 0.0f;
	float fire_dx = source.x + world_x_offset - BARREL_WORLD_X;
	float fire_dy = source.y + 0.60f;
	float fire_dz = source.z + world_z_offset - BARREL_WORLD_Z;
	float fire_distance = sqrtf(fire_dx * fire_dx + fire_dy * fire_dy +
		fire_dz * fire_dz);
	float fire_glow = 1.0f - smoothstep(0.5f, 8.0f, fire_distance);
	float red, green, blue;
	int lamp;

	if (flashlight) {
		float sin_yaw = sinf(yaw);
		float cos_yaw = cosf(yaw);
		float forward = -dx * sin_yaw - dz * cos_yaw;
		float lateral = dx * cos_yaw - dz * sin_yaw;

		if (forward > 0.0f && forward < 42.0f) {
			float cone_width = 0.20f + forward * 0.42f;
			float edge = 1.0f - fabsf(lateral) / cone_width;
			beam = smoothstep(0.0f, 1.0f, edge) *
				(1.0f - smoothstep(30.0f, 42.0f, forward));
		}
	}

	for (lamp = 0; lamp < 4; ++lamp) {
		float lamp_dz = source.z + world_z_offset - (-7.5f - (float)lamp * 15.0f);
		float lamp_dx = source.x + world_x_offset;
		float lamp_dy = source.y - 1.92f;
		float lamp_distance = sqrtf(lamp_dx * lamp_dx +
			lamp_dy * lamp_dy + lamp_dz * lamp_dz);
		float glow = 1.0f - smoothstep(1.0f, 7.0f, lamp_distance);
		if (glow > red_glow) red_glow = glow;
	}

	red = ambient + beam * 1.55f + red_glow * 0.38f + fire_glow * 0.85f;
	green = ambient + beam * 1.22f + red_glow * 0.07f + fire_glow * 0.30f;
	blue = ambient + beam * 0.82f + fire_glow * 0.06f;
	return lit_color(source.color, red, green, blue);
}

static void warm_menu_geometry(TexturedVertex *vertices, size_t count)
{
	size_t i;
	for (i = 0; i < count; ++i) {
		float dx = vertices[i].x - 1.33f;
		float dy = vertices[i].y - 0.05f;
		float dz = vertices[i].z + 3.0f;
		float distance = sqrtf(dx * dx + dy * dy + dz * dz);
		float glow = 1.0f - smoothstep(0.35f, 4.5f, distance);
		unsigned int color = vertices[i].color;
		float red = (float)(color & 0xffu) * (0.50f + glow * 0.30f) +
			60.0f * glow;
		float green = (float)((color >> 8) & 0xffu) *
			(0.50f + glow * 0.20f) + 24.0f * glow;
		float blue = (float)((color >> 16) & 0xffu) *
			(0.52f - glow * 0.16f);
		vertices[i].color = (color & 0xff000000u) |
			(multiply_channel((unsigned int)red, 1.0f)) |
			(multiply_channel((unsigned int)green, 1.0f) << 8) |
			(multiply_channel((unsigned int)blue, 1.0f) << 16);
	}
}

static void update_lighting(float px, float pz, float yaw, int flashlight, float gate_open)
{
	size_t i;

	for (i = 0; i < tunnel_track_grid_count; ++i) {
		Vertex source = {tunnel_track_base_colors[i], tunnel_track_grid[i].x,
			tunnel_track_grid[i].y, tunnel_track_grid[i].z};
		tunnel_track_grid[i].color = light_vertex_color(source,
			px, 0.0f, pz, yaw, flashlight, 0.0f, 0.0f);
	}
	for (i = 0; i < tunnel_concrete_grid_count; ++i) {
		Vertex source = {tunnel_concrete_base_colors[i], tunnel_concrete_grid[i].x,
			tunnel_concrete_grid[i].y, tunnel_concrete_grid[i].z};
		tunnel_concrete_grid[i].color = light_vertex_color(source,
			px, 0.0f, pz, yaw, flashlight, 0.0f, 0.0f);
	}
	for (i = 0; i < gate_panel_grid_count; ++i) {
		Vertex source = {gate_panel_base_colors[i], gate_panel_grid[i].x,
			gate_panel_grid[i].y, gate_panel_grid[i].z};
		gate_panel_grid[i].color = light_vertex_color(source,
			px, 0.0f, pz, yaw, flashlight, gate_open * 4.0f, -59.25f);
	}
	for (i = 0; i < gate_frame_grid_count; ++i) {
		Vertex source = {gate_frame_base_colors[i], gate_frame_grid[i].x,
			gate_frame_grid[i].y, gate_frame_grid[i].z};
		gate_frame_grid[i].color = light_vertex_color(source,
			px, 0.0f, pz, yaw, flashlight, 0.0f, -59.15f);
	}
}

static void bind_texture(int texture)
{
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
	sceGuTexImage(0, TEXTURE_SIZE, TEXTURE_SIZE, TEXTURE_SIZE, textures[texture]);
	sceGuTexScale(1.0f / (float)TEXTURE_SIZE, 1.0f / (float)TEXTURE_SIZE);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
}

static void draw_tunnel(void)
{
	unsigned int vertex_format = GU_TEXTURE_32BITF | GU_COLOR_8888 |
		GU_VERTEX_32BITF | GU_TRANSFORM_3D;

	sceGuDisable(GU_CULL_FACE);
	bind_texture(TEXTURE_TRACK);
	sceGumDrawArray(GU_TRIANGLES, vertex_format,
		tunnel_track_grid_count, 0, tunnel_track_grid);

	bind_texture(TEXTURE_CONCRETE);
	sceGumDrawArray(GU_TRIANGLES, vertex_format,
		tunnel_concrete_grid_count, 0, tunnel_concrete_grid);
}

static int near_armory_table(float x, float z)
{
	float nearest_x = x;
	float nearest_z = z;
	float dx, dz;
	if (nearest_x < -4.0f) nearest_x = -4.0f;
	if (nearest_x > 4.0f) nearest_x = 4.0f;
	if (nearest_z < 17.2f) nearest_z = 17.2f;
	if (nearest_z > 19.0f) nearest_z = 19.0f;
	dx = x - nearest_x;
	dz = z - nearest_z;
	return dx * dx + dz * dz < 12.25f;
}

static int weapon_capacity(int weapon_type)
{
	static const int capacities[WEAPON_COUNT] = {8, 30, 6, 5};
	return capacities[weapon_type];
}

static int crosses_wall(float old_z, float new_z, float wall_z,
	float opening_min_x, float opening_max_x, float x)
{
	int crossed = (old_z >= wall_z && new_z < wall_z) ||
		(old_z <= wall_z && new_z > wall_z);
	return crossed && (x < opening_min_x || x > opening_max_x);
}

static int collides_with_intro_partition(float old_z, float new_x, float new_z,
	int room_door_opened, int range_door_opened)
{
	if (!room_door_opened &&
		crosses_wall(old_z, new_z, ROOM_DOOR_Z, -2.5f, 2.5f, new_x)) {
		return 1;
	}
	if (crosses_wall(old_z, new_z, 36.0f, -2.3f, -0.7f, new_x) ||
		crosses_wall(old_z, new_z, 24.0f, -1.05f, 1.05f, new_x)) {
		return 1;
	}
	if (!range_door_opened &&
		crosses_wall(old_z, new_z, RANGE_GATE_Z, -2.5f, 2.5f, new_x)) {
		return 1;
	}
	return 0;
}

static void shoot_range_targets(float px, float pz, float yaw, int target_hits[4])
{
	static const float target_x[4] = {-3.25f, -1.05f, 1.05f, 3.25f};
	float sin_yaw = sinf(yaw);
	float cos_yaw = cosf(yaw);
	int i;

	for (i = 0; i < 4; ++i) {
		float dx = target_x[i] - px;
		float dz = 13.42f - pz;
		float forward = -dx * sin_yaw - dz * cos_yaw;
		float lateral = dx * cos_yaw - dz * sin_yaw;
		if (forward > 0.0f && fabsf(lateral) < 0.62f) {
			target_hits[i] = 1;
		}
	}
}

static void shoot_enemy(Enemy *enemies, float px, float pz, float yaw,
	int weapon_type)
{
	int i;
	int target = -1;
	float nearest = weapon_type == WEAPON_SNIPER ? 65.0f : 22.0f;
	float sin_yaw = sinf(yaw);
	float cos_yaw = cosf(yaw);

	for (i = 0; i < ENEMY_COUNT; ++i) {
		float dx, dz, forward, lateral;
		if (!enemies[i].alive) {
			continue;
		}

		dx = enemies[i].x - px;
		dz = enemies[i].z - pz;
		forward = -dx * sin_yaw - dz * cos_yaw;
		lateral = dx * cos_yaw - dz * sin_yaw;
		if (forward > 0.0f && forward < nearest &&
			fabsf(lateral) < (weapon_type == WEAPON_SNIPER ? 0.42f : 0.75f)) {
			nearest = forward;
			target = i;
		}
	}

	if (target >= 0) {
		if (weapon_type == WEAPON_SNIPER) {
			enemies[target].hits_left = 0;
		} else if (weapon_type == WEAPON_SHOTGUN) {
			int pellet;
			for (pellet = -2; pellet <= 2; ++pellet) {
				float dx = enemies[target].x - px;
				float dz = enemies[target].z - pz;
				float lateral = dx * cos_yaw - dz * sin_yaw;
				if (fabsf(lateral + (float)pellet * 0.12f) < 0.48f) {
					--enemies[target].hits_left;
				}
			}
		} else {
			--enemies[target].hits_left;
		}
		if (enemies[target].hits_left <= 0) {
			enemies[target].alive = 0;
		}
	}
}

static void update_enemies(Enemy *enemies, float px, float pz, int *hp)
{
	int i;

	for (i = 0; i < ENEMY_COUNT; ++i) {
		float dx, dz, distance;
		if (!enemies[i].alive) {
			continue;
		}

		dx = px - enemies[i].x;
		dz = pz - enemies[i].z;
		distance = sqrtf(dx * dx + dz * dz);
		if (distance < 18.0f && distance > 0.01f) {
			if (distance > 1.25f) {
				enemies[i].x += dx / distance * 0.022f;
				enemies[i].z += dz / distance * 0.022f;
			} else if (enemies[i].attack_cooldown == 0) {
				*hp -= 10;
				if (*hp < 0) {
					*hp = 0;
				}
				enemies[i].attack_cooldown = 60;
			}
		}

		if (enemies[i].attack_cooldown > 0) {
			--enemies[i].attack_cooldown;
		}
	}
}

static void draw_enemies(const Enemy *enemies, float px, float pz,
	float animation_phase)
{
	int i;
	unsigned int vertex_count = mutant_vertex_count;
	bind_texture(TEXTURE_MUTANT);
	for (i = 0; i < ENEMY_COUNT; ++i) {
		ScePspFVector3 translation;
		float phase = animation_phase + (float)i * 2.1f;
		if (!enemies[i].alive) {
			continue;
		}

		translation.x = enemies[i].x;
		translation.y = -1.5f + sinf(phase * 1.6f) * 0.018f;
		translation.z = enemies[i].z;
		sceGumMatrixMode(GU_MODEL);
		sceGumPushMatrix();
		sceGumLoadIdentity();
		sceGumTranslate(&translation);
		sceGumRotateY(atan2f(px - enemies[i].x, pz - enemies[i].z) +
			sinf(phase) * 0.025f);
		sceGumRotateZ(sinf(phase * 0.7f) * 0.018f);
		sceGumDrawArray(GU_TRIANGLES,
			GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			vertex_count, 0, textured_mutant_verts);
		bind_texture(TEXTURE_EYE);
		sceGumDrawArray(GU_TRIANGLES,
			GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			sizeof(textured_mutant_eye_verts) / sizeof(TexturedVertex),
			0, textured_mutant_eye_verts);
		bind_texture(TEXTURE_MUTANT);
		sceGumPopMatrix();
	}
}

static void draw_emergency_lamps(void)
{
	unsigned int housing_count =
		sizeof(textured_lamp_housing_verts) / sizeof(TexturedVertex);
	unsigned int lens_count =
		sizeof(textured_lamp_lens_verts) / sizeof(TexturedVertex);
	int lamp;

	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	for (lamp = 0; lamp < 4; ++lamp) {
		ScePspFVector3 translation;
		translation.x = 0.0f;
		translation.y = 2.02f;
		translation.z = -7.5f - (float)lamp * 15.0f;
		sceGumMatrixMode(GU_MODEL);
		sceGumPushMatrix();
		sceGumLoadIdentity();
		sceGumTranslate(&translation);
		bind_texture(TEXTURE_WEAPON);
		sceGumDrawArray(GU_TRIANGLES,
			GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			housing_count, 0, textured_lamp_housing_verts);
		bind_texture(TEXTURE_EYE);
		sceGumDrawArray(GU_TRIANGLES,
			GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			lens_count, 0, textured_lamp_lens_verts);
		sceGumPopMatrix();
	}
	sceGuDisable(GU_BLEND);
}

static void draw_intro_props(const int target_hits[4])
{
	unsigned int format = GU_TEXTURE_32BITF | GU_COLOR_8888 |
		GU_VERTEX_32BITF | GU_TRANSFORM_3D;
	int target;

	bind_texture(TEXTURE_WOOD);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_intro_wood_verts) / sizeof(TexturedVertex),
		0, textured_intro_wood_verts);

	bind_texture(TEXTURE_WEAPON);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_intro_metal_verts) / sizeof(TexturedVertex),
		0, textured_intro_metal_verts);

	bind_texture(TEXTURE_WEAPON);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_armory_weapon_verts) / sizeof(TexturedVertex),
		0, textured_armory_weapon_verts);

	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	bind_texture(TEXTURE_EYE);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_intro_lamp_verts) / sizeof(TexturedVertex),
		0, textured_intro_lamp_verts);
	sceGuDisable(GU_BLEND);

	sceGuDisable(GU_TEXTURE_2D);
	for (target = 0; target < 4; ++target) {
		const Vertex *marker = target_hits[target] > 0 ?
			&intro_target_markers_green[target * 6] :
			&intro_target_markers_red[target * 6];
		sceGuDrawArray(GU_TRIANGLES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			6, 0, marker);
	}
}

static void draw_blockpost_props(float fire_phase, float yaw)
{
	ScePspFVector3 translation = {BARREL_WORLD_X, -1.5f, BARREL_WORLD_Z};
	ScePspFVector3 scale;

	sceGumMatrixMode(GU_MODEL);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	bind_texture(TEXTURE_WEAPON);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		BARREL_VERTEX_COUNT, 0, barrel_verts);
	sceGumPopMatrix();

	bind_texture(TEXTURE_WOOD);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		sizeof(textured_ammo_crate_verts) / sizeof(TexturedVertex),
		0, textured_ammo_crate_verts);

	sceGuEnable(GU_BLEND);
	bind_texture(TEXTURE_FLASH);
	translation.y = -1.5f;
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumRotateY(yaw);
	sceGumRotateZ(sinf(fire_phase * 5.0f) * 0.045f);
	scale.x = 0.85f + 0.10f * sinf(fire_phase * 7.0f);
	scale.y = 0.88f + 0.12f * sinf(fire_phase * 9.0f);
	scale.z = 1.0f;
	sceGumScale(&scale);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		sizeof(barrel_glow_verts) / sizeof(TexturedVertex),
		0, barrel_glow_verts);
	sceGumPopMatrix();

	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumRotateY(yaw);
	sceGumRotateZ(sinf(fire_phase * 6.0f) * 0.07f);
	scale.x = 0.78f + 0.10f * sinf(fire_phase * 8.0f);
	scale.y = 0.88f + 0.16f * sinf(fire_phase * 11.0f);
	sceGumScale(&scale);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		sizeof(barrel_fire_verts) / sizeof(TexturedVertex),
		0, barrel_fire_verts);
	sceGumPopMatrix();
	sceGuDisable(GU_BLEND);
}

static void draw_flashlight_glow(int flashlight)
{
	if (!flashlight) {
		return;
	}

	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_FOG);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDrawArray(GU_TRIANGLES,
		GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		glow_vertex_count, 0, glow_verts);
	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuEnable(GU_FOG);
}

static void prepare_flashlight_glow(void)
{
	enum { GLOW_SEGMENTS = 16 };
	const float inner_radius_x = 24.0f;
	const float inner_radius_y = 20.0f;
	const float outer_radius_x = 88.0f;
	const float outer_radius_y = 72.0f;
	const float angle_step = 6.28318530718f / (float)GLOW_SEGMENTS;
	int segment;

	glow_vertex_count = 0;
	for (segment = 0; segment < GLOW_SEGMENTS; ++segment) {
		float angle0 = (float)segment * angle_step;
		float angle1 = (float)(segment + 1) * angle_step;
		Vertex center = {0x3860b0ff, 240.0f, 136.0f, 0.0f};
		Vertex inner0 = {0x2660b0ff,
			240.0f + cosf(angle0) * inner_radius_x,
			136.0f + sinf(angle0) * inner_radius_y, 0.0f};
		Vertex inner1 = {0x2660b0ff,
			240.0f + cosf(angle1) * inner_radius_x,
			136.0f + sinf(angle1) * inner_radius_y, 0.0f};
		Vertex outer0 = {0x0060b0ff,
			240.0f + cosf(angle0) * outer_radius_x,
			136.0f + sinf(angle0) * outer_radius_y, 0.0f};
		Vertex outer1 = {0x0060b0ff,
			240.0f + cosf(angle1) * outer_radius_x,
			136.0f + sinf(angle1) * outer_radius_y, 0.0f};

		glow_verts[glow_vertex_count++] = center;
		glow_verts[glow_vertex_count++] = inner0;
		glow_verts[glow_vertex_count++] = inner1;
		glow_verts[glow_vertex_count++] = inner0;
		glow_verts[glow_vertex_count++] = outer0;
		glow_verts[glow_vertex_count++] = outer1;
		glow_verts[glow_vertex_count++] = inner0;
		glow_verts[glow_vertex_count++] = outer1;
		glow_verts[glow_vertex_count++] = inner1;
	}
}

static void draw_gate(float gate_open, float gate_z)
{
	ScePspFVector3 translation;

	bind_texture(TEXTURE_GATE);
	translation.x = gate_open * 4.0f;
	translation.y = 0.0f;
	translation.z = gate_z;
	sceGumMatrixMode(GU_MODEL);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		gate_panel_grid_count, 0, gate_panel_grid);
	sceGumPopMatrix();

	translation.x = 0.0f;
	translation.z = gate_z + 0.10f;
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		gate_frame_grid_count, 0, gate_frame_grid);
	sceGumPopMatrix();
}

static void draw_menu_overlay(int ui_state, int selection, int cutscene_frame)
{
	int item;

	sceGuDisable(GU_TEXTURE_2D);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_FOG);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

	if (ui_state != UI_CUTSCENE) {
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			2, 0, overlay_menu_panel);
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_menu_version_count, 0, overlay_menu_version);
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_menu_hint_count, 0, overlay_menu_hint);
	}

	switch (ui_state) {
	case UI_MAIN_MENU:
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_menu_title_count, 0, overlay_menu_title);
		for (item = 0; item < 3; ++item) {
			int highlighted = item == selection;
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				overlay_main_option_counts[item][highlighted], 0,
				overlay_main_options[item][highlighted]);
		}
		break;
	case UI_CHAPTER_SELECT:
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_menu_title_count, 0, overlay_menu_title);
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_chapter_heading_count, 0, overlay_chapter_heading);
		for (item = 0; item < 3; ++item) {
			int highlighted = item == selection;
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				overlay_chapter_option_counts[item][highlighted], 0,
				overlay_chapter_options[item][highlighted]);
		}
		break;
	case UI_CONTROLS:
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_controls_title_count, 0, overlay_controls_title);
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_controls_text_count, 0, overlay_controls_text);
		break;
	case UI_PAUSED:
		sceGuDrawArray(GU_SPRITES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			overlay_pause_title_count, 0, overlay_pause_title);
		for (item = 0; item < 2; ++item) {
			int highlighted = item == selection;
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				overlay_pause_option_counts[item][highlighted], 0,
				overlay_pause_options[item][highlighted]);
		}
		break;
	case UI_CUTSCENE:
		if (cutscene_frame >= 120) {
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				overlay_cutscene_dialogue_count, 0,
				overlay_cutscene_dialogue);
		}
		if (cutscene_frame < 120) {
			unsigned int alpha = 255u -
				(unsigned int)cutscene_frame * 255u / 120u;
			sceGuDrawArray(GU_SPRITES,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
				2, 0, overlay_fade_sprites[alpha]);
		}
		break;
	default:
		break;
	}

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuEnable(GU_FOG);
}

static void draw_menu_scene(float phase, int ui_state, int selection,
	int cutscene_frame)
{
	ScePspFVector3 camera_translation;
	unsigned int format = GU_TEXTURE_32BITF | GU_COLOR_8888 |
		GU_VERTEX_32BITF | GU_TRANSFORM_3D;

	sceGuStart(GU_DIRECT, list);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuEnable(GU_FOG);
	sceGuFog(2.0f, 18.0f, 0xff090a0d);
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumPerspective(62.0f, 16.0f / 9.0f, 0.4f, 30.0f);
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
	sceGumRotateY(sinf(phase * 0.45f) * 0.045f);
	sceGumRotateX(sinf(phase * 0.65f) * 0.018f);
	camera_translation.x = sinf(phase * 0.35f) * 0.035f;
	camera_translation.y = 0.04f + sinf(phase * 0.55f) * 0.025f;
	camera_translation.z = 0.0f;
	sceGumTranslate(&camera_translation);
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

	bind_texture(TEXTURE_CONCRETE);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_menu_structure) / sizeof(TexturedVertex),
		0, textured_menu_structure);
	bind_texture(TEXTURE_WOOD);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_menu_wood) / sizeof(TexturedVertex),
		0, textured_menu_wood);
	bind_texture(TEXTURE_WEAPON);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_menu_metal) / sizeof(TexturedVertex),
		0, textured_menu_metal);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	bind_texture(TEXTURE_EYE);
	sceGumDrawArray(GU_TRIANGLES, format,
		sizeof(textured_menu_lamp) / sizeof(TexturedVertex),
		0, textured_menu_lamp);
	sceGuDisable(GU_BLEND);

	draw_menu_overlay(ui_state, selection, cutscene_frame);
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

static void draw_cutscene_scene(float phase, float camera_y, int cutscene_frame,
	const int target_hits[4], float room_door_open)
{
	ScePspFVector3 camera_translation;

	sceGuStart(GU_DIRECT, list);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuEnable(GU_FOG);
	sceGuFog(6.0f, 50.0f, 0xff090a0d);
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumPerspective(70.0f, 16.0f / 9.0f, 0.4f, 100.0f);
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
	sceGumRotateY(sinf(phase) * 0.008f);
	camera_translation.x = 0.0f;
	camera_translation.y = -camera_y;
	camera_translation.z = -52.0f;
	sceGumTranslate(&camera_translation);
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

	draw_tunnel();
	draw_intro_props(target_hits);
	draw_emergency_lamps();
	companion_draw(0.8f, 46.2f, phase * 0.45f, textures);
	draw_gate(room_door_open, ROOM_DOOR_Z);
	draw_gate(0.0f, RANGE_GATE_Z);
	draw_menu_overlay(UI_CUTSCENE, 0, cutscene_frame);
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

static void draw_paused_game_scene(const PauseRenderState *state,
	const Enemy enemies[ENEMY_COUNT], int pause_selection)
{
	ScePspFVector3 camera_translation;

	sceGuStart(GU_DIRECT, list);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuEnable(GU_FOG);
	sceGuFog(6.0f, state->flashlight ? 90.0f : 75.0f, 0xff090a0d);
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumPerspective(70.0f, 16.0f / 9.0f, 0.4f, 100.0f);
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();
	sceGumRotateY(-state->yaw);
	camera_translation.x = -state->px;
	camera_translation.y = 0.0f;
	camera_translation.z = -state->pz;
	sceGumTranslate(&camera_translation);
	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();

	draw_tunnel();
	draw_intro_props(state->target_hits);
	draw_blockpost_props(state->fire_phase, state->yaw);
	draw_emergency_lamps();
	draw_enemies(enemies, state->px, state->pz, state->mutant_phase);
	if (!state->room_door_opened || state->pz > 44.0f) {
		companion_draw(0.8f, 46.2f, state->mutant_phase * 0.45f, textures);
	}
	if (state->room_door_opened) {
		companion_draw(-3.65f, 29.0f, state->mutant_phase * 0.55f, textures);
		companion_draw(3.65f, 34.0f, state->mutant_phase * 0.60f, textures);
	}
	if (state->chapter1_complete) {
		companion_draw(KHAN_WORLD_X, KHAN_WORLD_Z,
			state->mutant_phase * 0.5f, textures);
	}
	draw_gate(state->room_door_open, ROOM_DOOR_Z);
	draw_gate(state->range_door_open, RANGE_GATE_Z);
	draw_gate(state->gate_open, -59.25f);
	draw_gate(state->exit_gate_open, EXIT_GATE_WORLD_Z);
	draw_flashlight_glow(state->flashlight);
	sceGuClear(GU_DEPTH_BUFFER_BIT);
	weapon_draw(state->weapon_type, state->arsenal_acquired,
		state->walk_phase, state->recoil, state->reload_frames,
		state->flash_frames, textures);
	draw_overlay(state->hp, state->ammo, state->ammo_capacity,
		state->chapter1_complete, state->khan_dialogue_line,
		state->chapter2_complete, state->range_door_opened,
		state->intro_message, state->arsenal_acquired, state->weapon_type,
		state->armory_near);
	draw_menu_overlay(UI_PAUSED, pause_selection, 0);

	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

static int exit_callback(int arg1, int arg2, void *common)
{
	(void)arg1;
	(void)arg2;
	(void)common;
	sceKernelExitGame();
	return 0;
}

static int callback_thread(SceSize args, void *argp)
{
	int cbid;
	(void)args;
	(void)argp;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	if (cbid >= 0) {
		sceKernelRegisterExitCallback(cbid);
	}
	sceKernelSleepThreadCB();
	return 0;
}

int main(void)
{
	int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
	static const Enemy initial_enemies[ENEMY_COUNT] = {
		{-1.0f, -16.0f, 3, 1, 0},
		{1.1f, -29.0f, 3, 1, 0},
		{-0.4f, -43.0f, 3, 1, 0},
		{-2.4f, -87.5f, 3, 0, 0},
		{1.8f, -91.0f, 3, 0, 0},
		{-1.6f, -96.0f, 3, 0, 0},
		{2.8f, -101.0f, 3, 0, 0},
	};
	float px = 0.0f, pz = 52.0f, yaw = 0.0f;
	float walk_phase = 0.0f;
	float mutant_phase = 0.0f;
	float fire_phase = 0.0f;
	float menu_phase = 0.0f;
	float recoil = 0.0f;
	float room_door_open = 0.0f;
	float range_door_open = 0.0f;
	float gate_open = 0.0f;
	float exit_gate_open = 0.0f;
	int step_frames = 0;
	int flashlight = 1;
	int previous_r = 0;
	int previous_cross = 0;
	int previous_triangle = 0;
	int previous_ltrigger = 0;
	int previous_down = 0;
	int previous_start = 0;
	int previous_confirm = 0;
	int previous_menu_up = 0;
	int previous_menu_down = 0;
	int game_ui_state = UI_MAIN_MENU;
	int menu_selection = 0;
	int chapter_selection = 0;
	int pause_selection = 0;
	int cutscene_frame = 0;
	int flash_frames = 0;
	int hp = 100;
	int weapon_ammo[WEAPON_COUNT] = {0, 0, 0, 0};
	int reserve_ammo[WEAPON_COUNT] = {0, 0, 0, 0};
	int target_hits[4] = {0, 0, 0, 0};
	int guards_greeted[2] = {0, 0};
	int weapon_type = -1;
	int arsenal_acquired = 0;
	int fire_cooldown = 0;
	int intro_message = -1;
	int reload_frames = 0;
	int room_door_opening = 0;
	int room_door_opened = 0;
	int range_door_opening = 0;
	int range_door_opened = 0;
	int gate_opening = 0;
	int chapter1_complete = 0;
	int khan_dialogue_line = -1;
	int khan_dialogue_complete = 0;
	int ammo_crate_used = 0;
	int exit_gate_opening = 0;
	int chapter2_complete = 0;
	PauseRenderState pause_state;
	Enemy enemies[ENEMY_COUNT];
	SceCtrlData pad;

	memcpy(enemies, initial_enemies, sizeof(enemies));
	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	generate_textures();
	generate_sound_effects();
	generate_gate_sound();
	gunshot_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
		PSP_AUDIO_SAMPLE_ALIGN(SFX_SAMPLE_COUNT), PSP_AUDIO_FORMAT_MONO);
	footstep_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
		PSP_AUDIO_SAMPLE_ALIGN(SFX_SAMPLE_COUNT), PSP_AUDIO_FORMAT_MONO);
	gate_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
		PSP_AUDIO_SAMPLE_ALIGN(GATE_SFX_SAMPLE_COUNT), PSP_AUDIO_FORMAT_MONO);
	if (!prepare_textured_geometry()) {
		return -1;
	}
	warm_menu_geometry(textured_menu_structure,
		sizeof(textured_menu_structure) / sizeof(TexturedVertex));
	warm_menu_geometry(textured_menu_wood,
		sizeof(textured_menu_wood) / sizeof(TexturedVertex));
	warm_menu_geometry(textured_menu_metal,
		sizeof(textured_menu_metal) / sizeof(TexturedVertex));
	if (!prepare_blockpost_geometry()) {
		return -1;
	}
	weapon_init();
	build_mutant_geometry(0.0f);
	prepare_surface_geometry(mutant_verts, textured_mutant_verts,
		mutant_vertex_count, 20.0f);
	companion_init();
	prepare_overlay_geometry();

	sceGuInit();
	textures = (unsigned int (*)[TEXTURE_SIZE * TEXTURE_SIZE])(
		(unsigned char *)sceGeEdramGetAddr() +
		2 * PSP_FRAMEBUFFER_BYTES + PSP_DEPTH_BUFFER_BYTES);
	memcpy(textures, texture_staging, sizeof(texture_staging));
	prepare_flashlight_glow();
	update_lighting(px, pz, yaw, 0, gate_open);
	sceKernelDcacheWritebackInvalidateAll();

	sceGuStart(GU_DIRECT, list);
	sceGuDisable(GU_CULL_FACE);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
	sceGuDispBuffer(480, 272, (void *)0x88000, 512);
	sceGuDepthBuffer((void *)0x110000, 512);
	sceGuOffset(2048 - 240, 2048 - 136);
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(65535, 0);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuShadeModel(GU_SMOOTH);
	sceGuClearColor(0xff050508);
	sceGuClearDepth(0);

	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	while (1) {
		float ax, ay;
		int r_pressed, cross_pressed, triangle_pressed;
		int down_pressed;
		int is_moving;
		ScePspFVector3 camera_translation;

		sceCtrlReadBufferPositive(&pad, 1);
		{
			int start_pressed = (pad.Buttons & PSP_CTRL_START) != 0;
			int confirm_pressed = (pad.Buttons &
				(PSP_CTRL_CROSS | PSP_CTRL_START)) != 0;

			if (game_ui_state != UI_PLAYING) {
				int was_paused = game_ui_state == UI_PAUSED;
				int up_pressed = (pad.Buttons & PSP_CTRL_UP) != 0;
				int down_menu_pressed = (pad.Buttons & PSP_CTRL_DOWN) != 0;
				int circle_pressed = (pad.Buttons & PSP_CTRL_CIRCLE) != 0;
				int confirm = confirm_pressed && !previous_confirm;
				int start_mode = 0;
				if (game_ui_state == UI_CUTSCENE) {
					float rise;
					if (cutscene_frame < 270) {
						++cutscene_frame;
					}
					rise = smoothstep(120.0f, 270.0f, (float)cutscene_frame);
					draw_cutscene_scene((float)cutscene_frame * 0.02f,
						-0.85f + rise * 0.85f, cutscene_frame,
						target_hits, room_door_open);
					if (cutscene_frame >= 270) {
						game_ui_state = UI_PLAYING;
					}
					previous_start = start_pressed;
					previous_confirm = confirm_pressed;
					continue;
				}

				if (up_pressed && !previous_menu_up) {
					if (game_ui_state == UI_MAIN_MENU) {
						menu_selection = (menu_selection + 2) % 3;
					} else if (game_ui_state == UI_CHAPTER_SELECT) {
						chapter_selection = (chapter_selection + 2) % 3;
					} else if (game_ui_state == UI_PAUSED) {
						pause_selection = (pause_selection + 1) % 2;
					}
				}
				if (down_menu_pressed && !previous_menu_down) {
					if (game_ui_state == UI_MAIN_MENU) {
						menu_selection = (menu_selection + 1) % 3;
					} else if (game_ui_state == UI_CHAPTER_SELECT) {
						chapter_selection = (chapter_selection + 1) % 3;
					} else if (game_ui_state == UI_PAUSED) {
						pause_selection = (pause_selection + 1) % 2;
					}
				}

				if (circle_pressed && game_ui_state == UI_CHAPTER_SELECT) {
					game_ui_state = UI_MAIN_MENU;
					menu_selection = 1;
				} else if (circle_pressed && game_ui_state == UI_CONTROLS) {
					game_ui_state = UI_MAIN_MENU;
					menu_selection = 2;
				} else if (circle_pressed && game_ui_state == UI_PAUSED) {
					game_ui_state = UI_PLAYING;
					previous_cross = (pad.Buttons & PSP_CTRL_CROSS) != 0;
				} else if (confirm) {
					if (game_ui_state == UI_MAIN_MENU) {
						if (menu_selection == 0) {
							start_mode = 1;
						} else if (menu_selection == 1) {
							game_ui_state = UI_CHAPTER_SELECT;
							chapter_selection = 0;
						} else {
							game_ui_state = UI_CONTROLS;
						}
					} else if (game_ui_state == UI_CHAPTER_SELECT) {
						if (chapter_selection == 0) {
							start_mode = 1;
						} else if (chapter_selection == 1) {
							start_mode = 2;
						} else {
							game_ui_state = UI_MAIN_MENU;
							menu_selection = 1;
						}
					} else if (game_ui_state == UI_CONTROLS) {
						game_ui_state = UI_MAIN_MENU;
					} else if (game_ui_state == UI_PAUSED) {
						if (pause_selection == 0) {
							game_ui_state = UI_PLAYING;
							previous_cross = (pad.Buttons & PSP_CTRL_CROSS) != 0;
						} else {
							game_ui_state = UI_MAIN_MENU;
							menu_selection = 0;
						}
					}
				}

				if (start_mode != 0) {
					int weapon;
					px = 0.0f;
					pz = 52.0f;
					yaw = 0.0f;
					walk_phase = 0.0f;
					mutant_phase = 0.0f;
					fire_phase = 0.0f;
					recoil = 0.0f;
					room_door_open = 0.0f;
					range_door_open = 0.0f;
					gate_open = 0.0f;
					exit_gate_open = 0.0f;
					step_frames = 0;
					flashlight = 1;
					flash_frames = 0;
					hp = 100;
					memset(weapon_ammo, 0, sizeof(weapon_ammo));
					memset(reserve_ammo, 0, sizeof(reserve_ammo));
					memset(target_hits, 0, sizeof(target_hits));
					memset(guards_greeted, 0, sizeof(guards_greeted));
					weapon_type = -1;
					arsenal_acquired = 0;
					fire_cooldown = 0;
					intro_message = -1;
					reload_frames = 0;
					room_door_opening = 0;
					room_door_opened = 0;
					range_door_opening = 0;
					range_door_opened = 0;
					gate_opening = 0;
					chapter1_complete = 0;
					khan_dialogue_line = -1;
					khan_dialogue_complete = 0;
					ammo_crate_used = 0;
					exit_gate_opening = 0;
					chapter2_complete = 0;
					memcpy(enemies, initial_enemies, sizeof(enemies));
					previous_r = 0;
					previous_cross = (pad.Buttons & PSP_CTRL_CROSS) != 0;
					previous_triangle = (pad.Buttons & PSP_CTRL_TRIANGLE) != 0;
					previous_ltrigger = 0;
					previous_down = 0;

					if (start_mode == 1) {
						cutscene_frame = 0;
						game_ui_state = UI_CUTSCENE;
					} else {
						px = 0.0f;
						pz = -2.0f;
						room_door_open = 1.0f;
						room_door_opened = 1;
						range_door_open = 1.0f;
						range_door_opened = 1;
						arsenal_acquired = 1;
						weapon_type = WEAPON_RIFLE;
						for (weapon = 0; weapon < WEAPON_COUNT; ++weapon) {
							weapon_ammo[weapon] = weapon_capacity(weapon);
							reserve_ammo[weapon] = weapon_capacity(weapon) * 4;
						}
						game_ui_state = UI_PLAYING;
					}
					previous_confirm = confirm_pressed;
					previous_start = start_pressed;
				}

				previous_menu_up = up_pressed;
				previous_menu_down = down_menu_pressed;
				if (start_mode == 0) {
					previous_confirm = confirm_pressed;
				}
				previous_start = start_pressed;
				menu_phase += 0.018f;
				if (game_ui_state == UI_CUTSCENE) {
					draw_cutscene_scene(0.0f, -0.85f, 0, target_hits,
						room_door_open);
				} else if (game_ui_state == UI_PAUSED ||
					(was_paused && game_ui_state == UI_PLAYING)) {
					draw_paused_game_scene(&pause_state, enemies, pause_selection);
				} else {
					draw_menu_scene(menu_phase, game_ui_state,
						game_ui_state == UI_CHAPTER_SELECT ? chapter_selection :
						(game_ui_state == UI_PAUSED ? pause_selection : menu_selection),
						cutscene_frame);
				}
				continue;
			}

			if (start_pressed && !previous_start) {
				game_ui_state = UI_PAUSED;
				pause_selection = 0;
				previous_confirm = 1;
				pause_state.px = px;
				pause_state.pz = pz;
				pause_state.yaw = yaw;
				pause_state.walk_phase = walk_phase;
				pause_state.mutant_phase = mutant_phase;
				pause_state.fire_phase = fire_phase;
				pause_state.recoil = recoil;
				pause_state.room_door_open = room_door_open;
				pause_state.range_door_open = range_door_open;
				pause_state.gate_open = gate_open;
				pause_state.exit_gate_open = exit_gate_open;
				pause_state.flashlight = flashlight;
				pause_state.reload_frames = reload_frames;
				pause_state.flash_frames = flash_frames;
				pause_state.weapon_type = weapon_type;
				pause_state.arsenal_acquired = arsenal_acquired;
				pause_state.hp = hp;
				pause_state.ammo = weapon_type >= 0 ?
					weapon_ammo[weapon_type] : 0;
				pause_state.ammo_capacity = weapon_type >= 0 ?
					weapon_capacity(weapon_type) : 0;
				pause_state.chapter1_complete = chapter1_complete;
				pause_state.khan_dialogue_line = khan_dialogue_line;
				pause_state.chapter2_complete = chapter2_complete;
				pause_state.room_door_opened = room_door_opened;
				pause_state.range_door_opened = range_door_opened;
				pause_state.intro_message = intro_message;
				{
					pause_state.armory_near = !arsenal_acquired &&
						near_armory_table(px, pz);
				}
				memcpy(pause_state.target_hits, target_hits,
					sizeof(pause_state.target_hits));
				draw_paused_game_scene(&pause_state, enemies, pause_selection);
				previous_start = start_pressed;
				continue;
			}
			previous_start = start_pressed;
		}

		r_pressed = (pad.Buttons & PSP_CTRL_RTRIGGER) != 0;
		if (r_pressed && !previous_r) {
			flashlight = !flashlight;
		}
		previous_r = r_pressed;

		triangle_pressed = (pad.Buttons & PSP_CTRL_TRIANGLE) != 0;
		if (triangle_pressed && !previous_triangle) {
			float commander_dx = px - 0.8f;
			float commander_dz = pz - 46.2f;
			static const float guard_x[2] = {-3.65f, 3.65f};
			static const float guard_z[2] = {29.0f, 34.0f};
			int guard;

			if (!room_door_opened && !room_door_opening &&
				commander_dx * commander_dx + commander_dz * commander_dz < 9.0f) {
				room_door_opening = 1;
				intro_message = 0;
				play_sound_effect(gate_channel, gate_open_samples);
			} else if (room_door_opened && !range_door_opened) {
				for (guard = 0; guard < 2; ++guard) {
					float dx = px - guard_x[guard];
					float dz = pz - guard_z[guard];
					if (!guards_greeted[guard] && dx * dx + dz * dz < 9.0f) {
						guards_greeted[guard] = 1;
						intro_message = 1;
						break;
					}
				}
				if (guard == 2 && !arsenal_acquired &&
					near_armory_table(px, pz)) {
					arsenal_acquired = 1;
					weapon_type = WEAPON_RIFLE;
					weapon_ammo[WEAPON_PISTOL] = 8;
					weapon_ammo[WEAPON_RIFLE] = 30;
					weapon_ammo[WEAPON_SHOTGUN] = 6;
					weapon_ammo[WEAPON_SNIPER] = 5;
					reserve_ammo[WEAPON_PISTOL] = 32;
					reserve_ammo[WEAPON_RIFLE] = 120;
					reserve_ammo[WEAPON_SHOTGUN] = 24;
					reserve_ammo[WEAPON_SNIPER] = 20;
					intro_message = 2;
				} else if (arsenal_acquired && !range_door_opening &&
					!range_door_opened && fabsf(px) < 2.0f &&
					fabsf(pz - RANGE_GATE_Z) < 2.5f) {
					range_door_opening = 1;
					intro_message = -1;
					play_sound_effect(gate_channel, gate_open_samples);
				}
			}

			if (range_door_opened && !chapter1_complete && !gate_opening) {
				float gate_dz = pz + 59.25f;
				if (px * px + gate_dz * gate_dz < 25.0f) {
					gate_opening = 1;
					play_sound_effect(gate_channel, gate_open_samples);
				}
			} else if (chapter1_complete && !chapter2_complete) {
				float khan_dx = px - KHAN_WORLD_X;
				float khan_dz = pz - KHAN_WORLD_Z;
				float crate_dx = px - AMMO_CRATE_WORLD_X;
				float crate_dz = pz - AMMO_CRATE_WORLD_Z;
				float exit_dz = pz - EXIT_GATE_WORLD_Z;
				if (!khan_dialogue_complete &&
					khan_dx * khan_dx + khan_dz * khan_dz < 12.25f) {
					if (khan_dialogue_line < 2) {
						++khan_dialogue_line;
					}
					if (khan_dialogue_line == 2) {
						int enemy;
						khan_dialogue_complete = 1;
						for (enemy = CHAPTER1_ENEMY_COUNT;
							enemy < ENEMY_COUNT; ++enemy) {
							enemies[enemy].alive = 1;
						}
					}
				} else if (!ammo_crate_used &&
					crate_dx * crate_dx + crate_dz * crate_dz < 6.25f) {
					hp = 100;
					if (arsenal_acquired && weapon_type >= 0) {
						weapon_ammo[weapon_type] = weapon_capacity(weapon_type);
					}
					ammo_crate_used = 1;
				} else if (khan_dialogue_complete &&
					px * px + exit_dz * exit_dz < 12.25f &&
					!exit_gate_opening) {
					exit_gate_opening = 1;
					play_sound_effect(gate_channel, gate_open_samples);
				}
			}
		}
		previous_triangle = triangle_pressed;

		{
			int ltrigger_pressed = (pad.Buttons & PSP_CTRL_LTRIGGER) != 0;
			if (ltrigger_pressed && !previous_ltrigger && arsenal_acquired &&
				reload_frames == 0) {
				int offset;
				for (offset = 1; offset <= WEAPON_COUNT; ++offset) {
					int candidate = (weapon_type + offset) % WEAPON_COUNT;
					if (weapon_ammo[candidate] > 0 ||
						reserve_ammo[candidate] > 0) {
						weapon_type = candidate;
						intro_message = -1;
						break;
					}
				}
			}
			previous_ltrigger = ltrigger_pressed;
		}

		cross_pressed = (pad.Buttons & PSP_CTRL_CROSS) != 0;
		if (fire_cooldown > 0) {
			--fire_cooldown;
		}
		if (arsenal_acquired && weapon_type >= 0 &&
			weapon_ammo[weapon_type] > 0 && reload_frames == 0 &&
			fire_cooldown == 0 &&
			(cross_pressed &&
				(weapon_type == WEAPON_RIFLE || !previous_cross))) {
			static const int fire_intervals[WEAPON_COUNT] = {18, 8, 28, 42};
			--weapon_ammo[weapon_type];
			fire_cooldown = fire_intervals[weapon_type];
			flash_frames = 8;
			recoil = 1.0f;
			play_sound_effect(gunshot_channel, gunshot_samples);
			shoot_range_targets(px, pz, yaw, target_hits);
			shoot_enemy(enemies, px, pz, yaw, weapon_type);
		}
		previous_cross = cross_pressed;

		down_pressed = (pad.Buttons & PSP_CTRL_DOWN) != 0;
		if (down_pressed && !previous_down && arsenal_acquired &&
			weapon_type >= 0 && reload_frames == 0 &&
			weapon_ammo[weapon_type] < weapon_capacity(weapon_type) &&
			reserve_ammo[weapon_type] > 0) {
			reload_frames = 60;
		}
		previous_down = down_pressed;

		if (pad.Buttons & PSP_CTRL_SQUARE) yaw += 0.035f;
		if (pad.Buttons & PSP_CTRL_CIRCLE) yaw -= 0.035f;

		ax = (pad.Lx - 128) * 0.0078125f;
		ay = (pad.Ly - 128) * 0.0078125f;
		is_moving = fabsf(ax) > 0.2f || fabsf(ay) > 0.2f;
		{
			float old_z = pz;
			if (fabsf(ay) > 0.2f) {
				px -= sinf(yaw) * (-ay) * 0.12f;
				pz -= cosf(yaw) * (-ay) * 0.12f;
			}
			if (fabsf(ax) > 0.2f) {
				px += cosf(yaw) * ax * 0.08f;
				pz -= sinf(yaw) * ax * 0.08f;
			}
			if (collides_with_intro_partition(old_z, px, pz,
				room_door_opened, range_door_opened)) {
				pz = old_z;
			}
		}

		{
			float x_min, x_max;
			if (pz > 42.0f) {
				x_min = -2.1f;
				x_max = 2.1f;
			} else if (pz > 36.0f) {
				x_min = -4.1f;
				x_max = 2.1f;
			} else if (pz > 24.0f) {
				x_min = -4.1f;
				x_max = 4.1f;
			} else if (pz > 13.8f) {
				x_min = -4.8f;
				x_max = 4.8f;
			} else if (!range_door_opened) {
				x_min = -2.1f;
				x_max = 2.1f;
			} else {
				float x_limit = pz < -60.0f ? 5.0f : 1.9f;
				x_min = -x_limit;
				x_max = x_limit;
			}
			if (px < x_min) px = x_min;
			if (px > x_max) px = x_max;
		}
		if (pz > 53.0f) pz = 53.0f;
		{
			float min_z = !room_door_opened ? 45.7f :
				(!range_door_opened ? 12.7f :
					(chapter2_complete ? -110.0f :
						(chapter1_complete ? -106.0f : -57.0f)));
			if (pz < min_z) pz = min_z;
		}

		if (is_moving) {
			walk_phase += 0.22f;
			if (++step_frames >= 14) {
				play_sound_effect(footstep_channel, footstep_samples);
				step_frames = 0;
			}
		} else {
			walk_phase *= 0.85f;
			step_frames = 0;
		}
		if (room_door_opening && !room_door_opened) {
			room_door_open += 0.025f;
			if (room_door_open >= 1.0f) {
				room_door_open = 1.0f;
				room_door_opened = 1;
			}
		}
		if (range_door_opening && !range_door_opened) {
			range_door_open += 0.025f;
			if (range_door_open >= 1.0f) {
				range_door_open = 1.0f;
				range_door_opened = 1;
			}
		}
		if (gate_opening && !chapter1_complete) {
			gate_open += 0.0125f;
			if (gate_open >= 1.0f) {
				gate_open = 1.0f;
				chapter1_complete = 1;
			}
		}
		if (exit_gate_opening && !chapter2_complete) {
			exit_gate_open += 0.025f;
			if (exit_gate_open >= 1.0f) {
				exit_gate_open = 1.0f;
				chapter2_complete = 1;
			}
		}
		update_enemies(enemies, px, pz, &hp);
		mutant_phase += 0.035f;
		fire_phase += 0.12f;
		if (reload_frames > 0) {
			--reload_frames;
			if (reload_frames == 30) {
				int needed = weapon_capacity(weapon_type) -
					weapon_ammo[weapon_type];
				if (needed > reserve_ammo[weapon_type]) {
					needed = reserve_ammo[weapon_type];
				}
				weapon_ammo[weapon_type] += needed;
				reserve_ammo[weapon_type] -= needed;
			}
		}
		recoil *= 0.72f;
		if (flash_frames > 0) {
			--flash_frames;
		}

		sceGuStart(GU_DIRECT, list);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		sceGuEnable(GU_FOG);
		sceGuFog(6.0f, flashlight ? 90.0f : 75.0f, 0xff090a0d);

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumPerspective(weapon_type == WEAPON_SNIPER &&
			(pad.Buttons & PSP_CTRL_UP) ? 42.0f : 70.0f,
			16.0f / 9.0f, 0.4f, 100.0f);

		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();
		sceGumRotateY(-yaw);
		camera_translation.x = -px;
		camera_translation.y = 0.0f;
		camera_translation.z = -pz;
		sceGumTranslate(&camera_translation);

		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();
		draw_tunnel();
		draw_intro_props(target_hits);
		draw_blockpost_props(fire_phase, yaw);
		draw_emergency_lamps();
		draw_enemies(enemies, px, pz, mutant_phase);
		if (!room_door_opened || pz > 44.0f) {
			companion_draw(0.8f, 46.2f, mutant_phase * 0.45f, textures);
		}
		if (room_door_opened) {
			companion_draw(-3.65f, 29.0f, mutant_phase * 0.55f, textures);
			companion_draw(3.65f, 34.0f, mutant_phase * 0.60f, textures);
		}
		if (chapter1_complete) {
			companion_draw(KHAN_WORLD_X, KHAN_WORLD_Z, mutant_phase * 0.5f, textures);
		}
		draw_gate(room_door_open, ROOM_DOOR_Z);
		draw_gate(range_door_open, RANGE_GATE_Z);
		draw_gate(gate_open, -59.25f);
		draw_gate(exit_gate_open, EXIT_GATE_WORLD_Z);
		draw_flashlight_glow(flashlight);
		sceGuClear(GU_DEPTH_BUFFER_BIT);
		weapon_draw(weapon_type, arsenal_acquired, walk_phase, recoil,
			reload_frames, flash_frames, textures);

		draw_overlay(hp, weapon_type >= 0 ? weapon_ammo[weapon_type] : 0,
			weapon_type >= 0 ? weapon_capacity(weapon_type) : 0,
			chapter1_complete, khan_dialogue_line, chapter2_complete,
			range_door_opened, intro_message, arsenal_acquired, weapon_type,
			!arsenal_acquired && near_armory_table(px, pz));
		sceGuFinish();
		sceGuSync(0, 0);

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}

	sceGuTerm();
	sceKernelExitGame();
	return 0;
}
