/*
 * Experimental NVN GLES color-clear execution.
 * Requires an active GLES context and a bound framebuffer.
 * Synthetic test only; not wired to gameplay.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "../nvn.h"

bool gles_execute_clear_color(const CmdRec *r)
{
    if (!r || r->op != CMD_CLEAR_COLOR)
        return false;

    if (r->size < sizeof(CmdRec) + 16u)
        return false;

    if (r->x[0] != 0 || r->x[5] != 0)
        return false;

    if ((r->x[2] & 0xFu) != 0xFu)
        return false;

    float color[4];
    memcpy(color, r->extra, sizeof(color));

    glClearBufferfv(GL_COLOR, 0, color);

    return glGetError() == GL_NO_ERROR;
}