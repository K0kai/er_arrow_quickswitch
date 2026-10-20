Pinned source: https://github.com/richgel999/miniz/tree/3.0.2

Only the standalone inflater is built. The local change to miniz_tinfl.c replaces
its umbrella miniz.h include with miniz_tinfl.h. miniz_export.h supplies the
empty export definition for static linking. See LICENSE for redistribution.
