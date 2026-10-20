#pragma once
#include <stdbool.h>
#include <GLES3/gl3.h>
#include "../nvn.h"

/* Creates a new GLES texture. Caller owns the returned GL name. */
bool gles_upload_nvn_texture(const NvnTexture *texture, GLuint *out_name);
