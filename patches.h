/*
 * This file contains patch control flags.
 *
 * In principle you should be able to mix and match any patches
 * you may want. In cases where patches are logically incompatible
 * one patch may take precedence over the other as noted in the
 * relevant descriptions.
 */

/* Patches */

/* The alpha patch adds transparency for the terminal.
 * https://st.suckless.org/patches/alpha/
 */
#define ALPHA_PATCH 1

/* This patch allows st to reize to any pixel size rather than snapping to character width/height.
 * https://st.suckless.org/patches/anysize/
 */
#define ANYSIZE_PATCH 1