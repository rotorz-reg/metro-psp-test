#ifndef RENDER_MODELS_H
#define RENDER_MODELS_H

#define TEXTURE_SIZE 64

enum {
	TEXTURE_CONCRETE,
	TEXTURE_TRACK,
	TEXTURE_GATE,
	TEXTURE_WEAPON,
	TEXTURE_WOOD,
	TEXTURE_FABRIC,
	TEXTURE_MUTANT,
	TEXTURE_ARMOR,
	TEXTURE_EYE,
	TEXTURE_FLASH,
	TEXTURE_COUNT
};

enum {
	WEAPON_PISTOL,
	WEAPON_RIFLE,
	WEAPON_SHOTGUN,
	WEAPON_SNIPER,
	WEAPON_COUNT
};

typedef struct {
	float u, v;
	unsigned int color;
	float x, y, z;
} TexturedVertex;

void weapon_init(void);
void weapon_draw(int weapon_type, int equipped, float walk_phase, float recoil,
	int reload_frames, int flash_frames,
	const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE]);

void companion_init(void);
void companion_draw(float x, float z, float breathing_phase,
	const unsigned int textures[TEXTURE_COUNT][TEXTURE_SIZE * TEXTURE_SIZE]);

#endif
