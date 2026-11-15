#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>

PSP_MODULE_INFO("Metro_PSP_D6", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
	unsigned int color;
	float x, y, z;
} Vertex;

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
	QUAD(left_color, -2.5f, -1.5f, z0, -2.5f, 0.65f, z0, \
		-2.5f, 0.65f, z1, -2.5f, -1.5f, z1), \
	QUAD(right_color, 2.5f, -1.5f, z1, 2.5f, 0.65f, z1, \
		2.5f, 0.65f, z0, 2.5f, -1.5f, z0), \
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
#define ARCH(z) \
	BOX(-2.50f, -2.26f, -1.50f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff523b68), \
	BOX(2.26f, 2.50f, -1.50f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff523b68), \
	BOX(-2.50f, -2.00f, 0.48f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff63476f), \
	BOX(-2.00f, -1.25f, 1.12f, 1.32f, (z) - 0.12f, (z) + 0.12f, 0xff63476f), \
	BOX(-1.25f, -0.50f, 1.66f, 1.86f, (z) - 0.12f, (z) + 0.12f, 0xff63476f), \
	BOX(-0.50f, 0.50f, 1.94f, 2.14f, (z) - 0.12f, (z) + 0.12f, 0xff63476f), \
	BOX(0.50f, 1.25f, 1.66f, 1.86f, (z) - 0.12f, (z) + 0.12f, 0xff63476f), \
	BOX(1.25f, 2.00f, 1.12f, 1.32f, (z) - 0.12f, (z) + 0.12f, 0xff63476f), \
	BOX(2.00f, 2.50f, 0.48f, 0.68f, (z) - 0.12f, (z) + 0.12f, 0xff63476f)
#define SLEEPER(z) \
	BOX(-2.15f, 2.15f, -1.43f, -1.28f, (z) - 0.10f, (z) + 0.10f, 0xff646c72)
#define GATE_FRAME_PANEL(x0, x1, c) \
	QUAD(c, x0, -1.48f, 0.0f, x1, -1.48f, 0.0f, \
		x1, 2.08f, 0.0f, x0, 2.08f, 0.0f)

static Vertex __attribute__((aligned(16))) tunnel_verts[] = {
	TUNNEL_SECTION(0.0f, -5.0f, 0xff777d83, 0xff44314c, 0xff49334d),
	TUNNEL_SECTION(-5.0f, -10.0f, 0xff858b90, 0xff503650, 0xff49334d),
	TUNNEL_SECTION(-10.0f, -15.0f, 0xff777d83, 0xff44314c, 0xff49334d),
	TUNNEL_SECTION(-15.0f, -20.0f, 0xff858b90, 0xff503650, 0xff49334d),
	TUNNEL_SECTION(-20.0f, -25.0f, 0xff777d83, 0xff44314c, 0xff49334d),
	TUNNEL_SECTION(-25.0f, -30.0f, 0xff858b90, 0xff503650, 0xff49334d),
	TUNNEL_SECTION(-30.0f, -35.0f, 0xff777d83, 0xff44314c, 0xff49334d),
	TUNNEL_SECTION(-35.0f, -40.0f, 0xff858b90, 0xff503650, 0xff49334d),
	TUNNEL_SECTION(-40.0f, -45.0f, 0xff777d83, 0xff44314c, 0xff49334d),
	TUNNEL_SECTION(-45.0f, -50.0f, 0xff858b90, 0xff503650, 0xff49334d),
	TUNNEL_SECTION(-50.0f, -55.0f, 0xff777d83, 0xff44314c, 0xff49334d),
	TUNNEL_SECTION(-55.0f, -60.0f, 0xff858b90, 0xff503650, 0xff49334d),

	ARCH(-5.0f), ARCH(-10.0f), ARCH(-15.0f), ARCH(-20.0f),
	ARCH(-25.0f), ARCH(-30.0f), ARCH(-35.0f), ARCH(-40.0f),
	ARCH(-45.0f), ARCH(-50.0f), ARCH(-55.0f),

	SLEEPER(-2.0f), SLEEPER(-4.0f), SLEEPER(-6.0f), SLEEPER(-8.0f),
	SLEEPER(-10.0f), SLEEPER(-12.0f), SLEEPER(-14.0f), SLEEPER(-16.0f),
	SLEEPER(-18.0f), SLEEPER(-20.0f), SLEEPER(-22.0f), SLEEPER(-24.0f),
	SLEEPER(-26.0f), SLEEPER(-28.0f), SLEEPER(-30.0f), SLEEPER(-32.0f),
	SLEEPER(-34.0f), SLEEPER(-36.0f), SLEEPER(-38.0f), SLEEPER(-40.0f),
	SLEEPER(-42.0f), SLEEPER(-44.0f), SLEEPER(-46.0f), SLEEPER(-48.0f),
	SLEEPER(-50.0f), SLEEPER(-52.0f), SLEEPER(-54.0f), SLEEPER(-56.0f),
	SLEEPER(-58.0f),

	BOX(-0.82f, -0.66f, -1.27f, -1.08f, -59.0f, 0.0f, 0xff9ba5ac),
	BOX(0.66f, 0.82f, -1.27f, -1.08f, -59.0f, 0.0f, 0xff9ba5ac),
	BOX(-2.42f, -2.25f, -1.50f, 2.15f, -59.45f, -59.15f, 0xff727b82),
	BOX(2.25f, 2.42f, -1.50f, 2.15f, -59.45f, -59.15f, 0xff727b82),
	BOX(-2.42f, 2.42f, 2.05f, 2.22f, -59.45f, -59.15f, 0xff727b82),
};

#define TUNNEL_VERTEX_COUNT (sizeof(tunnel_verts) / sizeof(Vertex))

static Vertex __attribute__((aligned(16))) gate_verts[] = {
	GATE_FRAME_PANEL(-2.30f, -1.40f, 0xff2727a7),
	GATE_FRAME_PANEL(-1.36f, -0.46f, 0xff20209a),
	GATE_FRAME_PANEL(-0.42f, 0.42f, 0xff3030b8),
	GATE_FRAME_PANEL(0.46f, 1.36f, 0xff20209a),
	GATE_FRAME_PANEL(1.40f, 2.30f, 0xff2727a7),
};

static Vertex __attribute__((aligned(16))) lit_verts[TUNNEL_VERTEX_COUNT];
static Vertex __attribute__((aligned(16))) overlay_verts[16384];

static const unsigned char font8x8[128][8] = {
	['!'] = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
	[','] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x10},
	['.'] = {0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18},
	[':'] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
	['['] = {0x1c,0x10,0x10,0x10,0x10,0x10,0x1c,0x00},
	[']'] = {0x1c,0x04,0x04,0x04,0x04,0x04,0x1c,0x00},
	['1'] = {0x08,0x18,0x08,0x08,0x08,0x08,0x1c,0x00},
	['2'] = {0x1c,0x22,0x02,0x04,0x08,0x10,0x3e,0x00},
	['3'] = {0x3c,0x02,0x02,0x1c,0x02,0x02,0x3c,0x00},
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

static unsigned int overlay_vertex_count;

typedef struct {
	float x, z;
	int hits_left;
	int alive;
	int attack_cooldown;
} Enemy;

static Vertex __attribute__((aligned(16))) mutant_verts[] = {
	BOX(-0.31f, -0.08f, 0.0f, 0.52f, -0.16f, 0.16f, 0xff171a20),
	BOX(0.08f, 0.31f, 0.0f, 0.52f, -0.16f, 0.16f, 0xff171a20),
	BOX(-0.36f, 0.36f, 0.48f, 1.24f, -0.20f, 0.20f, 0xff20232b),
	BOX(-0.58f, -0.32f, 0.48f, 1.12f, -0.14f, 0.14f, 0xff191c23),
	BOX(0.32f, 0.58f, 0.48f, 1.12f, -0.14f, 0.14f, 0xff191c23),
	BOX(-0.25f, 0.25f, 1.20f, 1.62f, -0.19f, 0.19f, 0xff29232d),
	BOX(-0.16f, -0.07f, 1.40f, 1.48f, 0.19f, 0.22f, 0xff2020b8),
	BOX(0.07f, 0.16f, 1.40f, 1.48f, 0.19f, 0.22f, 0xff2020b8),
};

static Vertex __attribute__((aligned(16))) weapon_verts[] = {
	BOX(-0.13f, 0.13f, -0.09f, 0.10f, -0.14f, 0.31f, 0xff292d32),
	BOX(-0.045f, 0.045f, -0.015f, 0.035f, -0.52f, -0.12f, 0xff858b90),
	BOX(-0.10f, 0.10f, -0.26f, -0.08f, 0.05f, 0.29f, 0xff202328),
	BOX(-0.075f, 0.04f, -0.28f, -0.08f, -0.03f, 0.15f, 0xff35383d),
};

static void overlay_sprite(float x0, float y0, float x1, float y1, unsigned int color)
{
	if (overlay_vertex_count + 2 > sizeof(overlay_verts) / sizeof(overlay_verts[0])) {
		return;
	}

	overlay_verts[overlay_vertex_count++] = (Vertex){color, x0, y0, 0.0f};
	overlay_verts[overlay_vertex_count++] = (Vertex){color, x1, y1, 0.0f};
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

static void draw_overlay(int hp, int ammo, int chapter_complete)
{
	int i;

	overlay_vertex_count = 0;
	overlay_sprite(13.0f, 25.0f, 113.0f, 33.0f, 0xff303030);
	overlay_sprite(14.0f, 26.0f, 14.0f + hp, 32.0f, 0xff3030e8);
	overlay_text("HP", 14, 12, 1, 0xffffffff);
	overlay_text("AMMO", 14, 39, 1, 0xffffffff);
	for (i = 0; i < 30; ++i) {
		unsigned int color = i < ammo ? 0xff20d8ff : 0xff424242;
		float x = 14.0f + (float)i * 5.0f;
		overlay_sprite(x, 51.0f, x + 3.0f, 61.0f, color);
	}

	overlay_sprite(236.0f, 132.0f, 244.0f, 140.0f, 0xffffffff);
	overlay_sprite(239.0f, 126.0f, 241.0f, 146.0f, 0xffffffff);
	overlay_sprite(230.0f, 135.0f, 250.0f, 137.0f, 0xffffffff);

	overlay_sprite(0.0f, 211.0f, 480.0f, 272.0f, 0xb0000000);
	if (chapter_complete) {
		overlay_text("GATE OPENED! CHAPTER 1 COMPLETE.", 20, 232, 1, 0xffffffff);
	} else {
		overlay_text("ANNA: ARTYOM, CLEAR THE TUNNEL", 12, 222, 1, 0xffffffff);
		overlay_text("AND OPEN D6 GATE [TRIANGLE].", 12, 237, 1, 0xffffffff);
	}

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
		overlay_vertex_count, 0, overlay_verts);
	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuEnable(GU_FOG);
}

static unsigned int scale_color(unsigned int color, float brightness)
{
	unsigned int r = (unsigned int)((color & 0xffu) * brightness);
	unsigned int g = (unsigned int)(((color >> 8) & 0xffu) * brightness);
	unsigned int b = (unsigned int)(((color >> 16) & 0xffu) * brightness);

	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;

	return (color & 0xff000000u) | r | (g << 8) | (b << 16);
}

static void update_flashlight_vertices(float px, float pz, float yaw, int flashlight)
{
	size_t i;
	float sin_yaw = sinf(yaw);
	float cos_yaw = cosf(yaw);

	for (i = 0; i < TUNNEL_VERTEX_COUNT; ++i) {
		float brightness = 1.0f;

		if (flashlight) {
			float dx = tunnel_verts[i].x - px;
			float dz = tunnel_verts[i].z - pz;
			float forward = -dx * sin_yaw - dz * cos_yaw;
			float lateral = dx * cos_yaw - dz * sin_yaw;

			if (forward > 0.0f && forward < 32.0f) {
				float cone_width = 0.15f + forward * 0.32f;
				float lateral_ratio = fabsf(lateral) / cone_width;

				if (lateral_ratio < 1.0f) {
					float edge_falloff = 1.0f - lateral_ratio;
					float distance_falloff = 1.0f - forward / 32.0f;
					brightness += 1.8f * edge_falloff * distance_falloff;
				}
			}
		}

		lit_verts[i] = tunnel_verts[i];
		lit_verts[i].color = scale_color(tunnel_verts[i].color, brightness);
	}
}

static void shoot_enemy(Enemy *enemies, float px, float pz, float yaw)
{
	int i;
	int target = -1;
	float nearest = 22.0f;
	float sin_yaw = sinf(yaw);
	float cos_yaw = cosf(yaw);

	for (i = 0; i < 3; ++i) {
		float dx, dz, forward, lateral;
		if (!enemies[i].alive) {
			continue;
		}

		dx = enemies[i].x - px;
		dz = enemies[i].z - pz;
		forward = -dx * sin_yaw - dz * cos_yaw;
		lateral = dx * cos_yaw - dz * sin_yaw;
		if (forward > 0.0f && forward < nearest && fabsf(lateral) < 0.75f) {
			nearest = forward;
			target = i;
		}
	}

	if (target >= 0 && --enemies[target].hits_left <= 0) {
		enemies[target].alive = 0;
	}
}

static void update_enemies(Enemy *enemies, float px, float pz, int *hp)
{
	int i;

	for (i = 0; i < 3; ++i) {
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

static void draw_enemies(Enemy *enemies, float px, float pz)
{
	int i;
	unsigned int vertex_count = sizeof(mutant_verts) / sizeof(Vertex);

	for (i = 0; i < 3; ++i) {
		ScePspFVector3 translation;
		if (!enemies[i].alive) {
			continue;
		}

		translation.x = enemies[i].x;
		translation.y = -1.5f;
		translation.z = enemies[i].z;
		sceGumMatrixMode(GU_MODEL);
		sceGumPushMatrix();
		sceGumLoadIdentity();
		sceGumTranslate(&translation);
		sceGumRotateY(atan2f(px - enemies[i].x, pz - enemies[i].z));
		sceGumDrawArray(GU_TRIANGLES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			vertex_count, 0, mutant_verts);
		sceGumPopMatrix();
	}
}

static void draw_gate(float gate_open)
{
	ScePspFVector3 translation;
	unsigned int vertex_count = sizeof(gate_verts) / sizeof(Vertex);

	if (gate_open >= 1.0f) {
		return;
	}

	translation.x = gate_open * 5.2f;
	translation.y = 0.0f;
	translation.z = -59.25f;
	sceGumMatrixMode(GU_MODEL);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumDrawArray(GU_TRIANGLES,
		GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		vertex_count, 0, gate_verts);
	sceGumPopMatrix();
}

static void draw_weapon(float walk_phase, int flash_frames)
{
	ScePspFVector3 translation;
	unsigned int vertex_count = sizeof(weapon_verts) / sizeof(Vertex);

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_FOG);
	sceGumMatrixMode(GU_VIEW);
	sceGumPushMatrix();
	sceGumLoadIdentity();

	translation.x = 0.53f;
	translation.y = -0.42f + sinf(walk_phase) * 0.025f;
	translation.z = -1.30f;
	sceGumMatrixMode(GU_MODEL);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumDrawArray(GU_TRIANGLES,
		GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		vertex_count, 0, weapon_verts);

	if (flash_frames > 0) {
		static Vertex __attribute__((aligned(16))) muzzle_flash[] = {
			BOX(-0.09f, 0.09f, -0.09f, 0.09f, -0.68f, -0.52f, 0xff20aaff),
		};
		unsigned int flash_vertex_count = sizeof(muzzle_flash) / sizeof(Vertex);
		sceGumDrawArray(GU_TRIANGLES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			flash_vertex_count, 0, muzzle_flash);
	}

	sceGumPopMatrix();
	sceGumMatrixMode(GU_VIEW);
	sceGumPopMatrix();
	sceGuEnable(GU_FOG);
	sceGuEnable(GU_DEPTH_TEST);
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
	float px = 0.0f, pz = -2.0f, yaw = 0.0f;
	float walk_phase = 0.0f;
	float gate_open = 0.0f;
	int flashlight = 1;
	int previous_r = 0;
	int previous_cross = 0;
	int previous_triangle = 0;
	int flash_frames = 0;
	int hp = 100;
	int ammo = 30;
	int gate_opening = 0;
	int chapter_complete = 0;
	Enemy enemies[3] = {
		{-1.0f, -16.0f, 3, 1, 0},
		{1.1f, -29.0f, 3, 1, 0},
		{-0.4f, -43.0f, 3, 1, 0},
	};
	SceCtrlData pad;

	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDisable(GU_CULL_FACE);
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
		int is_moving;
		unsigned int vertex_count = TUNNEL_VERTEX_COUNT;
		ScePspFVector3 camera_translation;

		sceCtrlReadBufferPositive(&pad, 1);

		r_pressed = (pad.Buttons & PSP_CTRL_RTRIGGER) != 0;
		if (r_pressed && !previous_r) {
			flashlight = !flashlight;
		}
		previous_r = r_pressed;

		cross_pressed = (pad.Buttons & PSP_CTRL_CROSS) != 0;
		if (cross_pressed && !previous_cross && ammo > 0) {
			--ammo;
			flash_frames = 8;
			shoot_enemy(enemies, px, pz, yaw);
		}
		previous_cross = cross_pressed;

		triangle_pressed = (pad.Buttons & PSP_CTRL_TRIANGLE) != 0;
		if (triangle_pressed && !previous_triangle && !gate_opening) {
			float gate_dx = px;
			float gate_dz = pz + 59.25f;
			if (gate_dx * gate_dx + gate_dz * gate_dz < 25.0f) {
				gate_opening = 1;
			}
		}
		previous_triangle = triangle_pressed;

		if (pad.Buttons & PSP_CTRL_SQUARE) yaw += 0.035f;
		if (pad.Buttons & PSP_CTRL_CIRCLE) yaw -= 0.035f;

		ax = (pad.Lx - 128) * 0.0078125f;
		ay = (pad.Ly - 128) * 0.0078125f;
		is_moving = fabsf(ax) > 0.2f || fabsf(ay) > 0.2f;
		if (fabsf(ay) > 0.2f) {
			px -= sinf(yaw) * (-ay) * 0.12f;
			pz -= cosf(yaw) * (-ay) * 0.12f;
		}
		if (fabsf(ax) > 0.2f) {
			px += cosf(yaw) * ax * 0.08f;
			pz -= sinf(yaw) * ax * 0.08f;
		}

		if (px < -1.9f) px = -1.9f;
		if (px > 1.9f) px = 1.9f;
		if (pz > -1.0f) pz = -1.0f;
		if (pz < (chapter_complete ? -70.0f : -57.0f)) {
			pz = chapter_complete ? -70.0f : -57.0f;
		}

		if (is_moving) {
			walk_phase += 0.22f;
		} else {
			walk_phase *= 0.85f;
		}
		if (gate_opening && !chapter_complete) {
			gate_open += 0.0125f;
			if (gate_open >= 1.0f) {
				gate_open = 1.0f;
				chapter_complete = 1;
			}
		}
		update_enemies(enemies, px, pz, &hp);
		if (flash_frames > 0) {
			--flash_frames;
		}

		update_flashlight_vertices(px, pz, yaw, flashlight);

		sceGuStart(GU_DIRECT, list);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		sceGuEnable(GU_FOG);
		sceGuFog(2.0f, flashlight ? 22.0f : 5.0f, 0xff050508);

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumPerspective(70.0f, 16.0f / 9.0f, 0.5f, 100.0f);

		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();
		sceGumRotateY(-yaw);
		camera_translation.x = -px;
		camera_translation.y = 0.0f;
		camera_translation.z = -pz;
		sceGumTranslate(&camera_translation);

		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();
		sceGumDrawArray(GU_TRIANGLES,
			GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			vertex_count, 0, lit_verts);
		draw_enemies(enemies, px, pz);
		draw_gate(gate_open);
		draw_weapon(walk_phase, flash_frames);

		draw_overlay(hp, ammo, chapter_complete);
		sceGuFinish();
		sceGuSync(0, 0);

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}

	sceGuTerm();
	sceKernelExitGame();
	return 0;
}
