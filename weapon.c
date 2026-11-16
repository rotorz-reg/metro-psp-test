#include "render_models.h"

#include <pspgu.h>
#include <pspgum.h>
#include <math.h>

typedef struct {
	unsigned int color;
	float x, y, z;
} Vertex;

#define BOX(x0, x1, y0, y1, z0, z1, c) \
	{c, x0, y0, z0}, {c, x1, y0, z0}, {c, x1, y1, z0}, \
	{c, x0, y0, z0}, {c, x1, y1, z0}, {c, x0, y1, z0}, \
	{c, x1, y0, z1}, {c, x0, y0, z1}, {c, x0, y1, z1}, \
	{c, x1, y0, z1}, {c, x0, y1, z1}, {c, x1, y1, z1}, \
	{c, x0, y0, z1}, {c, x0, y0, z0}, {c, x0, y1, z0}, \
	{c, x0, y0, z1}, {c, x0, y1, z0}, {c, x0, y1, z1}, \
	{c, x1, y0, z0}, {c, x1, y0, z1}, {c, x1, y1, z1}, \
	{c, x1, y0, z0}, {c, x1, y1, z1}, {c, x1, y1, z0}, \
	{c, x0, y0, z1}, {c, x1, y0, z1}, {c, x1, y0, z0}, \
	{c, x0, y0, z1}, {c, x1, y0, z0}, {c, x0, y0, z0}, \
	{c, x0, y1, z0}, {c, x1, y1, z0}, {c, x1, y1, z1}, \
	{c, x0, y1, z0}, {c, x1, y1, z1}, {c, x0, y1, z1}

static Vertex weapon_body[] = {
	BOX(-0.14f, 0.14f, -0.08f, 0.12f, -0.20f, 0.20f, 0xff292d32),
	BOX(-0.12f, 0.12f, 0.12f, 0.17f, -0.19f, 0.17f, 0xff383c40),
	BOX(-0.045f, 0.045f, 0.025f, 0.075f, -1.03f, -0.10f, 0xff858b90),
	BOX(-0.055f, 0.055f, 0.085f, 0.14f, -0.78f, -0.15f, 0xff45494b),
	BOX(-0.075f, 0.075f, 0.095f, 0.15f, -0.75f, -0.24f, 0xff35393c),
	BOX(-0.085f, 0.085f, 0.00f, 0.20f, -1.10f, -1.02f, 0xff45494b),
	BOX(-0.11f, -0.065f, 0.075f, 0.24f, -0.98f, -0.90f, 0xff4b5052),
	BOX(0.065f, 0.11f, 0.075f, 0.24f, -0.98f, -0.90f, 0xff4b5052),
	BOX(-0.08f, 0.08f, 0.16f, 0.22f, -0.27f, -0.19f, 0xff555a5d),
	BOX(-0.11f, 0.11f, 0.055f, 0.16f, -0.64f, -0.58f, 0xff303336),
	BOX(-0.08f, 0.08f, 0.12f, 0.22f, -0.88f, -0.80f, 0xff45494b),
	BOX(-0.065f, 0.065f, 0.17f, 0.23f, -0.91f, -0.86f, 0xff202326),
	BOX(-0.075f, 0.075f, -0.04f, 0.055f, 0.66f, 0.76f, 0xff303336),
	BOX(-0.09f, 0.09f, -0.07f, 0.04f, 0.55f, 0.88f, 0xff303336),
	BOX(-0.11f, 0.11f, -0.12f, 0.08f, 0.82f, 0.91f, 0xff555a5d),
	BOX(-0.09f, 0.09f, -0.14f, -0.08f, -0.04f, 0.18f, 0xff303336),
	BOX(-0.078f, 0.078f, -0.28f, -0.14f, -0.02f, 0.04f, 0xff35383d),
	BOX(-0.08f, 0.08f, -0.55f, -0.48f, 0.82f, 0.89f, 0xff303336),
	BOX(-0.10f, 0.10f, -0.30f, -0.23f, -0.98f, -0.90f, 0xff202326),
	BOX(-0.15f, -0.11f, -0.02f, 0.08f, -0.13f, 0.06f, 0xff505558),
	BOX(-0.045f, 0.045f, 0.16f, 0.27f, -0.10f, -0.04f, 0xff24282c),
	BOX(-0.055f, 0.055f, 0.25f, 0.34f, -0.88f, -0.80f, 0xff303336),
	BOX(-0.035f, 0.035f, 0.23f, 0.42f, -0.07f, -0.02f, 0xff303336)
};

static Vertex weapon_wood[] = {
	BOX(-0.09f, 0.09f, 0.04f, 0.16f, -0.72f, -0.22f, 0xffc1844b),
	BOX(-0.105f, 0.105f, -0.01f, 0.045f, -0.70f, -0.24f, 0xffb77a43),
	BOX(-0.075f, 0.075f, -0.04f, 0.055f, 0.16f, 0.69f, 0xffb87d46),
	BOX(-0.085f, 0.085f, -0.34f, -0.08f, 0.13f, 0.29f, 0xffa66c3b),
	BOX(-0.085f, 0.085f, -0.19f, -0.12f, -0.11f, -0.04f, 0xffa66c3b),
	BOX(-0.09f, 0.09f, -0.34f, -0.25f, -0.19f, -0.12f, 0xff9e6335),
	BOX(-0.095f, 0.095f, -0.48f, -0.39f, -0.25f, -0.17f, 0xff9e6335)
};

static Vertex weapon_magazine[] = {
	BOX(-0.085f, 0.085f, -0.13f, -0.22f, 0.02f, 0.18f, 0xff303438),
	BOX(-0.080f, 0.080f, -0.22f, -0.32f, -0.02f, 0.17f, 0xff303438),
	BOX(-0.073f, 0.073f, -0.32f, -0.42f, -0.08f, 0.15f, 0xff303438),
	BOX(-0.064f, 0.064f, -0.42f, -0.51f, -0.14f, 0.13f, 0xff303438),
	BOX(-0.054f, 0.054f, -0.51f, -0.57f, -0.19f, 0.11f, 0xff303438)
};

static Vertex pistol_model[] = {
	BOX(-0.12f, 0.12f, -0.03f, 0.13f, -0.58f, -0.12f, 0xff303439),
	BOX(-0.08f, 0.08f, 0.13f, 0.20f, -0.52f, -0.16f, 0xff24282c),
	BOX(-0.055f, 0.055f, 0.08f, 0.15f, -0.78f, -0.54f, 0xff565b5e),
	BOX(-0.085f, 0.085f, -0.04f, 0.04f, -0.12f, 0.06f, 0xff262a2d),
	BOX(-0.055f, 0.055f, -0.34f, -0.06f, -0.03f, 0.07f, 0xff292d30)
};

static Vertex shotgun_model[] = {
	BOX(-0.14f, 0.14f, -0.08f, 0.14f, -0.30f, 0.20f, 0xff34383b),
	BOX(-0.11f, 0.11f, 0.14f, 0.22f, -0.26f, 0.15f, 0xff292d30),
	BOX(-0.07f, 0.07f, 0.00f, 0.11f, -1.27f, -0.23f, 0xff62676a),
	BOX(-0.09f, 0.09f, -0.12f, -0.01f, -0.86f, -0.31f, 0xff34383b),
	BOX(-0.08f, 0.08f, -0.30f, -0.15f, -0.21f, 0.16f, 0xff754b2d),
	BOX(-0.07f, 0.07f, -0.41f, -0.28f, -0.29f, -0.18f, 0xff70472b)
};

static Vertex sniper_model[] = {
	BOX(-0.13f, 0.13f, -0.07f, 0.13f, -0.28f, 0.23f, 0xff292d31),
	BOX(-0.075f, 0.075f, 0.02f, 0.11f, -1.56f, -0.20f, 0xff5d6265),
	BOX(-0.10f, 0.10f, -0.31f, -0.20f, -0.18f, 0.18f, 0xff69452b),
	BOX(-0.045f, 0.045f, 0.13f, 0.29f, -0.18f, 0.04f, 0xff25292d),
	BOX(-0.10f, 0.10f, 0.27f, 0.35f, -0.16f, 0.02f, 0xff34383b),
	BOX(-0.06f, 0.06f, 0.31f, 0.38f, -0.145f, 0.005f, 0xff454a4e)
};

static Vertex right_hand[] = {
	BOX(-0.12f, 0.12f, -0.49f, -0.31f, -0.31f, -0.16f, 0xffb0b4b1),
	BOX(-0.15f, -0.08f, -0.52f, -0.32f, -0.32f, -0.16f, 0xffb0b4b1),
	BOX(-0.06f, 0.01f, -0.52f, -0.32f, -0.32f, -0.16f, 0xffb0b4b1),
	BOX(0.03f, 0.10f, -0.52f, -0.32f, -0.32f, -0.16f, 0xffb0b4b1),
	BOX(0.11f, 0.16f, -0.50f, -0.34f, -0.32f, -0.16f, 0xffb0b4b1)
};

static Vertex muzzle_flash[] = {
	BOX(-0.09f, 0.09f, -0.09f, 0.09f, -1.24f, -1.08f, 0xffa0e8ff)
};
static TexturedVertex textured_body[sizeof(weapon_body) / sizeof(Vertex)];
static TexturedVertex textured_wood[sizeof(weapon_wood) / sizeof(Vertex)];
static TexturedVertex textured_magazine[sizeof(weapon_magazine) / sizeof(Vertex)];
static TexturedVertex textured_pistol[sizeof(pistol_model) / sizeof(Vertex)];
static TexturedVertex textured_shotgun[sizeof(shotgun_model) / sizeof(Vertex)];
static TexturedVertex textured_sniper[sizeof(sniper_model) / sizeof(Vertex)];
static TexturedVertex textured_right_hand[sizeof(right_hand) / sizeof(Vertex)];
static TexturedVertex textured_arm[96];
static unsigned int arm_vertex_count;
static TexturedVertex textured_flash[sizeof(muzzle_flash) / sizeof(Vertex)];
static Vertex left_hand[sizeof(right_hand) / sizeof(Vertex)];
static TexturedVertex textured_left_hand[sizeof(right_hand) / sizeof(Vertex)];

static void prepare_vertices(const Vertex *source, TexturedVertex *destination,
	unsigned int count, float scale, float offset_x, float offset_y, float offset_z)
{
	unsigned int i;
	for (i = 0; i < count; ++i) {
		destination[i].color = source[i].color;
		destination[i].x = source[i].x + offset_x;
		destination[i].y = source[i].y + offset_y;
		destination[i].z = source[i].z + offset_z;
		destination[i].u = (source[i].x + source[i].y * 0.37f) * scale;
		destination[i].v = (source[i].z + source[i].y * 0.53f) * scale;
	}
}

static void bind_texture(const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE],
	int texture)
{
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
	sceGuTexImage(0, TEXTURE_SIZE, TEXTURE_SIZE, TEXTURE_SIZE, textures[texture]);
	sceGuTexScale(1.0f / TEXTURE_SIZE, 1.0f / TEXTURE_SIZE);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
}

static void append_arm(float start_x, float start_y, float start_z,
	float end_x, float end_y, float end_z, float start_radius, float end_radius,
	TexturedVertex *destination, unsigned int *vertex_count)
{
	enum { SIDES = 8 };
	float ax = end_x - start_x, ay = end_y - start_y, az = end_z - start_z;
	float length = sqrtf(ax * ax + ay * ay + az * az);
	float rx = 0.0f, ry = 0.0f, rz = 1.0f;
	float sx, sy, sz, ux, uy, uz;
	int side;
	ax /= length;
	ay /= length;
	az /= length;
	if (fabsf(az) > 0.95f) {
		ry = 1.0f;
		rz = 0.0f;
	}
	sx = ay * rz - az * ry;
	sy = az * rx - ax * rz;
	sz = ax * ry - ay * rx;
	length = sqrtf(sx * sx + sy * sy + sz * sz);
	sx /= length;
	sy /= length;
	sz /= length;
	ux = ay * sz - az * sy;
	uy = az * sx - ax * sz;
	uz = ax * sy - ay * sx;
	for (side = 0; side < SIDES; ++side) {
		int next = (side + 1) % SIDES;
		float angle0 = 6.283185307f * (float)side / (float)SIDES;
		float angle1 = 6.283185307f * (float)next / (float)SIDES;
		float c0 = cosf(angle0), s0 = sinf(angle0);
		float c1 = cosf(angle1), s1 = sinf(angle1);
		unsigned int color = (side & 1) ? 0xffb0b4b1 : 0xffd0d2cf;
		Vertex a = {color, start_x + start_radius * (sx * c0 + ux * s0),
			start_y + start_radius * (sy * c0 + uy * s0),
			start_z + start_radius * (sz * c0 + uz * s0)};
		Vertex b = {color, start_x + start_radius * (sx * c1 + ux * s1),
			start_y + start_radius * (sy * c1 + uy * s1),
			start_z + start_radius * (sz * c1 + uz * s1)};
		Vertex c = {color, end_x + end_radius * (sx * c1 + ux * s1),
			end_y + end_radius * (sy * c1 + uy * s1),
			end_z + end_radius * (sz * c1 + uz * s1)};
		Vertex d = {color, end_x + end_radius * (sx * c0 + ux * s0),
			end_y + end_radius * (sy * c0 + uy * s0),
			end_z + end_radius * (sz * c0 + uz * s0)};
		Vertex triangles[6] = {a, b, c, a, c, d};
		int vertex;
		for (vertex = 0; vertex < 6; ++vertex) {
			Vertex v = triangles[vertex];
			destination[*vertex_count] = (TexturedVertex){
				(v.x + v.y * 0.37f) * 24.0f,
				(v.z + v.y * 0.53f) * 24.0f,
				v.color, v.x, v.y, v.z};
			++*vertex_count;
		}
	}
}

static void bind_vertex_texture(const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE],
	int texture, unsigned int count, const TexturedVertex *vertices)
{
	bind_texture(textures, texture);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		count, 0, vertices);
}

void weapon_init(void)
{
	unsigned int count = 0;
	unsigned int i;
	append_arm(-0.90f, -1.12f, -0.50f, 0.02f, -0.34f, -1.62f,
		0.19f, 0.12f, textured_arm, &count);
	append_arm(0.98f, -1.14f, -0.48f, 0.44f, -0.66f, -1.00f,
		0.19f, 0.12f, textured_arm, &count);
	arm_vertex_count = count;
	prepare_vertices(weapon_body, textured_body,
		sizeof(weapon_body) / sizeof(Vertex), 100.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(weapon_wood, textured_wood,
		sizeof(weapon_wood) / sizeof(Vertex), 22.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(weapon_magazine, textured_magazine,
		sizeof(weapon_magazine) / sizeof(Vertex), 20.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(pistol_model, textured_pistol,
		sizeof(pistol_model) / sizeof(Vertex), 100.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(shotgun_model, textured_shotgun,
		sizeof(shotgun_model) / sizeof(Vertex), 100.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(sniper_model, textured_sniper,
		sizeof(sniper_model) / sizeof(Vertex), 100.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(right_hand, textured_right_hand,
		sizeof(right_hand) / sizeof(Vertex), 24.0f, 0.0f, 0.0f, 0.0f);
	prepare_vertices(muzzle_flash, textured_flash,
		sizeof(muzzle_flash) / sizeof(Vertex), 1.0f, 0.0f, 0.0f, 0.0f);
	for (i = 0; i < sizeof(muzzle_flash) / sizeof(Vertex); ++i) {
		textured_flash[i].u = (muzzle_flash[i].x / 0.18f + 0.5f) * TEXTURE_SIZE;
		textured_flash[i].v = (muzzle_flash[i].y / 0.18f + 0.5f) * TEXTURE_SIZE;
	}
	for (i = 0; i < sizeof(right_hand) / sizeof(Vertex); ++i) {
		left_hand[i] = right_hand[i];
		left_hand[i].x = -right_hand[i].x - 0.35f;
		left_hand[i].z = right_hand[i].z + 0.50f;
	}
	prepare_vertices(left_hand, textured_left_hand,
		sizeof(left_hand) / sizeof(Vertex), 24.0f, 0.0f, 0.0f, 0.0f);
}

void weapon_draw(int weapon_type, int equipped, float walk_phase, float recoil,
	int reload_frames, int flash_frames,
	const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE])
{
	ScePspFVector3 translation;
	float reload_progress = (float)(60 - reload_frames) / 60.0f;
	float reload_dip = reload_frames > 0 ?
		sinf(reload_progress * 3.14159265f) : 0.0f;
	float magazine_drop = reload_frames > 0 ?
		(1.0f - fabsf(reload_progress * 2.0f - 1.0f)) * 0.34f : 0.0f;
	float magazine_rotation = 0.0f;

	if (!equipped) {
		return;
	}

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_FOG);
	sceGumMatrixMode(GU_VIEW);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumMatrixMode(GU_MODEL);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	translation.x = sinf(walk_phase * 0.5f) * 0.018f;
	translation.y = sinf(walk_phase) * 0.022f - reload_dip * 0.20f;
	translation.z = recoil * 0.035f;
	sceGumTranslate(&translation);
	sceGumRotateX(recoil * -0.035f + reload_dip * 0.12f);
	bind_vertex_texture(textures, TEXTURE_FABRIC, arm_vertex_count, textured_arm);

	translation.x = 0.30f;
	translation.y = -0.45f;
	translation.z = -1.30f;
	sceGumTranslate(&translation);
	if (weapon_type == WEAPON_PISTOL) {
		bind_vertex_texture(textures, TEXTURE_WEAPON,
			sizeof(textured_pistol) / sizeof(TexturedVertex), textured_pistol);
	} else if (weapon_type == WEAPON_SHOTGUN) {
		bind_vertex_texture(textures, TEXTURE_WEAPON,
			sizeof(textured_shotgun) / sizeof(TexturedVertex), textured_shotgun);
	} else if (weapon_type == WEAPON_SNIPER) {
		bind_vertex_texture(textures, TEXTURE_WEAPON,
			sizeof(textured_sniper) / sizeof(TexturedVertex), textured_sniper);
	} else {
		bind_vertex_texture(textures, TEXTURE_WEAPON,
			sizeof(textured_body) / sizeof(TexturedVertex), textured_body);
		bind_vertex_texture(textures, TEXTURE_WOOD,
			sizeof(textured_wood) / sizeof(TexturedVertex), textured_wood);
	}

	if (weapon_type == WEAPON_RIFLE && reload_frames > 0) {
		if (reload_progress < 0.45f) {
			magazine_rotation = -0.85f * sinf(reload_progress / 0.45f * 1.5707963f);
		} else if (reload_progress > 0.58f) {
			magazine_rotation = 0.45f * sinf(
				(reload_progress - 0.58f) / 0.42f * 1.5707963f);
		}
	}
	sceGumPushMatrix();
	translation.x = 0.0f;
	translation.y = -0.13f;
	translation.z = 0.10f;
	sceGumTranslate(&translation);
	sceGumRotateX(magazine_rotation);
	translation.y = 0.13f - magazine_drop;
	translation.z = -0.10f;
	sceGumTranslate(&translation);
	if (weapon_type == WEAPON_RIFLE && !(reload_frames > 0 && reload_progress >= 0.45f &&
		reload_progress <= 0.58f)) {
		bind_vertex_texture(textures, TEXTURE_WEAPON,
			sizeof(textured_magazine) / sizeof(TexturedVertex), textured_magazine);
	}
	sceGumPopMatrix();

	bind_vertex_texture(textures, TEXTURE_FABRIC,
		sizeof(textured_right_hand) / sizeof(TexturedVertex), textured_right_hand);
	bind_vertex_texture(textures, TEXTURE_FABRIC,
		sizeof(textured_left_hand) / sizeof(TexturedVertex), textured_left_hand);
	if (flash_frames > 0) {
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		bind_texture(textures, TEXTURE_FLASH);
		sceGumDrawArray(GU_TRIANGLES,
			GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			sizeof(textured_flash) / sizeof(TexturedVertex), 0, textured_flash);
		sceGuDisable(GU_BLEND);
	}

	sceGumPopMatrix();
	sceGumMatrixMode(GU_VIEW);
	sceGumPopMatrix();
	sceGuEnable(GU_FOG);
	sceGuEnable(GU_DEPTH_TEST);
}
