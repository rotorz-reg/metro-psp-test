#include "render_models.h"

#include <pspgu.h>
#include <pspgum.h>
#include <math.h>

typedef struct {
	unsigned int color;
	float x, y, z;
} Vertex;

enum { COMPANION_VERTEX_CAPACITY = 4096, LIMB_SEGMENTS = 8 };

static TexturedVertex vertices[COMPANION_VERTEX_CAPACITY];
static unsigned int vertex_count;

static void append_triangle(Vertex a, Vertex b, Vertex c)
{
	if (vertex_count + 3 > COMPANION_VERTEX_CAPACITY) {
		return;
	}
	vertices[vertex_count++] = (TexturedVertex){
		(a.x + a.y * 0.37f) * 20.0f, (a.z + a.y * 0.53f) * 20.0f,
		a.color, a.x, a.y, a.z};
	vertices[vertex_count++] = (TexturedVertex){
		(b.x + b.y * 0.37f) * 20.0f, (b.z + b.y * 0.53f) * 20.0f,
		b.color, b.x, b.y, b.z};
	vertices[vertex_count++] = (TexturedVertex){
		(c.x + c.y * 0.37f) * 20.0f, (c.z + c.y * 0.53f) * 20.0f,
		c.color, c.x, c.y, c.z};
}

static void append_limb(float x0, float y0, float z0,
	float x1, float y1, float z1, float radius0, float radius1,
	unsigned int color)
{
	float ax = x1 - x0, ay = y1 - y0, az = z1 - z0;
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
	for (side = 0; side < LIMB_SEGMENTS; ++side) {
		int next = (side + 1) % LIMB_SEGMENTS;
		float a0 = 6.283185307f * (float)side / LIMB_SEGMENTS;
		float a1 = 6.283185307f * (float)next / LIMB_SEGMENTS;
		float c0 = cosf(a0), s0 = sinf(a0);
		float c1 = cosf(a1), s1 = sinf(a1);
		Vertex a = {color, x0 + radius0 * (sx * c0 + ux * s0),
			y0 + radius0 * (sy * c0 + uy * s0),
			z0 + radius0 * (sz * c0 + uz * s0)};
		Vertex b = {color, x0 + radius0 * (sx * c1 + ux * s1),
			y0 + radius0 * (sy * c1 + uy * s1),
			z0 + radius0 * (sz * c1 + uz * s1)};
		Vertex c = {color, x1 + radius1 * (sx * c1 + ux * s1),
			y1 + radius1 * (sy * c1 + uy * s1),
			z1 + radius1 * (sz * c1 + uz * s1)};
		Vertex d = {color, x1 + radius1 * (sx * c0 + ux * s0),
			y1 + radius1 * (sy * c0 + uy * s0),
			z1 + radius1 * (sz * c0 + uz * s0)};
		append_triangle(a, b, c);
		append_triangle(a, c, d);
	}
}

static void append_cylinder_y(float cx, float cz, float y0, float y1,
	float radius0, float radius1, unsigned int color)
{
	int side;
	for (side = 0; side < LIMB_SEGMENTS; ++side) {
		int next = (side + 1) % LIMB_SEGMENTS;
		float a0 = 6.283185307f * (float)side / LIMB_SEGMENTS;
		float a1 = 6.283185307f * (float)next / LIMB_SEGMENTS;
		Vertex a = {color, cx + cosf(a0) * radius0, y0,
			cz + sinf(a0) * radius0};
		Vertex b = {color, cx + cosf(a1) * radius0, y0,
			cz + sinf(a1) * radius0};
		Vertex c = {color, cx + cosf(a1) * radius1, y1,
			cz + sinf(a1) * radius1};
		Vertex d = {color, cx + cosf(a0) * radius1, y1,
			cz + sinf(a0) * radius1};
		append_triangle(a, b, c);
		append_triangle(a, c, d);
	}
}

static void append_tapered_prism(float cx, float cz, float y0, float y1,
	float width0, float width1, float depth, unsigned int color)
{
	Vertex bottom[4] = {
		{color, cx - width0, y0, cz - depth},
		{color, cx + width0, y0, cz - depth},
		{color, cx + width0, y0, cz + depth},
		{color, cx - width0, y0, cz + depth}
	};
	Vertex top[4] = {
		{color, cx - width1, y1, cz - depth * 0.82f},
		{color, cx + width1, y1, cz - depth * 0.82f},
		{color, cx + width1, y1, cz + depth * 0.82f},
		{color, cx - width1, y1, cz + depth * 0.82f}
	};
	int side;
	for (side = 0; side < 4; ++side) {
		int next = (side + 1) % 4;
		append_triangle(bottom[side], bottom[next], top[next]);
		append_triangle(bottom[side], top[next], top[side]);
	}
}

static void append_filter(float cx, float cy, float cz, float radius)
{
	enum { FILTER_SEGMENTS = 12 };
	int side;
	for (side = 0; side < FILTER_SEGMENTS; ++side) {
		int next = (side + 1) % FILTER_SEGMENTS;
		float a0 = 6.283185307f * (float)side / FILTER_SEGMENTS;
		float a1 = 6.283185307f * (float)next / FILTER_SEGMENTS;
		Vertex back0 = {0xff303634, cx + cosf(a0) * radius, cy + sinf(a0) * radius, cz};
		Vertex back1 = {0xff303634, cx + cosf(a1) * radius, cy + sinf(a1) * radius, cz};
		Vertex front0 = {0xff788076, back0.x, back0.y, cz + 0.09f};
		Vertex front1 = {0xff788076, back1.x, back1.y, cz + 0.09f};
		Vertex center = {0xffa3a99b, cx, cy, cz + 0.092f};
		append_triangle(back0, back1, front1);
		append_triangle(back0, front1, front0);
		append_triangle(center, front0, front1);
	}
}

static void bind_armor_texture(
	const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE])
{
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
	sceGuTexImage(0, TEXTURE_SIZE, TEXTURE_SIZE, TEXTURE_SIZE,
		textures[TEXTURE_ARMOR]);
	sceGuTexScale(1.0f / TEXTURE_SIZE, 1.0f / TEXTURE_SIZE);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
}

void companion_init(void)
{
	int side;
	vertex_count = 0;

	append_tapered_prism(0.0f, 0.0f, 0.35f, 1.08f, 0.31f, 0.23f,
		0.19f, 0xff59664a);
	append_tapered_prism(0.0f, 0.0f, 0.58f, 0.91f, 0.34f, 0.31f,
		0.22f, 0xff73805b);
	append_tapered_prism(0.0f, 0.0f, 0.39f, 0.98f, 0.29f, 0.27f,
		0.205f, 0xff4b5942);

	append_limb(-0.15f, 0.40f, 0.0f, -0.23f, 0.08f, 0.10f,
		0.15f, 0.10f, 0xff4c5941);
	append_limb(0.15f, 0.40f, 0.0f, 0.10f, 0.15f, 0.31f,
		0.15f, 0.09f, 0xff4c5941);
	append_limb(-0.10f, 0.20f, 0.08f, -0.15f, -0.39f, 0.02f,
		0.11f, 0.075f, 0xff46513d);
	append_limb(0.10f, 0.20f, 0.08f, 0.16f, -0.39f, 0.02f,
		0.11f, 0.075f, 0xff46513d);
	append_tapered_prism(-0.16f, 0.02f, -0.58f, -0.35f,
		0.13f, 0.09f, 0.16f, 0xff424b39);
	append_tapered_prism(0.16f, 0.02f, -0.58f, -0.35f,
		0.13f, 0.09f, 0.16f, 0xff424b39);
	append_limb(-0.16f, -0.46f, 0.03f, -0.18f, -0.78f, 0.16f,
		0.075f, 0.07f, 0xff3f4938);
	append_limb(0.16f, -0.46f, 0.03f, 0.18f, -0.78f, 0.16f,
		0.075f, 0.07f, 0xff3f4938);

	append_cylinder_y(0.0f, 0.0f, 1.02f, 1.32f, 0.15f, 0.17f,
		0xff46513e);
	append_cylinder_y(0.0f, 0.0f, 1.30f, 1.55f, 0.23f, 0.25f,
		0xff384335);
	append_tapered_prism(0.0f, 0.0f, 1.19f, 1.34f,
		0.18f, 0.16f, 0.20f, 0xff252c29);
	append_tapered_prism(0.0f, 0.0f, 1.25f, 1.31f,
		0.06f, 0.06f, 0.24f, 0xff59634d);
	append_filter(-0.12f, 1.22f, 0.17f, 0.085f);
	append_filter(0.12f, 1.22f, 0.17f, 0.085f);

	append_limb(-0.13f, 0.42f, 0.28f, 0.12f, 0.42f, 0.57f,
		0.045f, 0.035f, 0xff353a38);
	append_limb(0.12f, 0.42f, 0.29f, 0.12f, 0.42f, 0.57f,
		0.045f, 0.035f, 0xff353a38);
	append_limb(0.12f, 0.42f, 0.57f, 0.12f, 0.42f, 1.12f,
		0.027f, 0.018f, 0xff656a67);
	append_limb(0.12f, 0.47f, 0.57f, 0.12f, 0.47f, 1.10f,
		0.012f, 0.009f, 0xff252a29);
	append_tapered_prism(0.12f, 0.51f, 0.46f, 0.53f,
		0.045f, 0.035f, 0.16f, 0xff794a28);

	for (side = 0; side < vertex_count; ++side) {
		vertices[side].u = (vertices[side].x + vertices[side].y * 0.37f) * 20.0f;
		vertices[side].v = (vertices[side].z + vertices[side].y * 0.53f) * 20.0f;
	}
}

void companion_draw(float x, float z, float breathing_phase,
	const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE])
{
	ScePspFVector3 translation = {x,
		-0.65f + sinf(breathing_phase) * 0.012f, z};
	sceGumMatrixMode(GU_MODEL);
	sceGumPushMatrix();
	sceGumLoadIdentity();
	sceGumTranslate(&translation);
	sceGumRotateZ(sinf(breathing_phase) * 0.006f);
	bind_armor_texture(textures);
	sceGumDrawArray(GU_TRIANGLES,
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		vertex_count, 0, vertices);
	sceGumPopMatrix();
}
