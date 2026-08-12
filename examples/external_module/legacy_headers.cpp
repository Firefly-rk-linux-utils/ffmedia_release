#include <module/module_media.hpp>

int ffmedia_legacy_include_layout_compiles()
{
    return FFMEDIA_MODULE_ABI_VERSION > 0 ? 0 : 1;
}
