#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief 由宿主持有的有效切片 Profile 材料策略。 */
enum hostmaterialstrategy
{
    HOST_MATERIAL_RGB_SOLID = 0,
    HOST_MATERIAL_RGB_WHITE = 1,
    HOST_MATERIAL_RGB_VARNISH = 2,
    HOST_MATERIAL_RGB_WHITE_VARNISH = 3,
    HOST_MATERIAL_WHITE_SOLID = 4,
    HOST_MATERIAL_VARNISH_SOLID = 5
};

/** @brief 由宿主持有的输入材料映射默认角色。 */
enum hostmaterialrole
{
    HOST_MATERIAL_ROLE_RGB = 0,
    HOST_MATERIAL_ROLE_WHITE = 1,
    HOST_MATERIAL_ROLE_VARNISH = 2,
    HOST_MATERIAL_ROLE_IGNORE = 3,
    HOST_MATERIAL_ROLE_SUPPORT_CANDIDATE = 4
};

/** @brief 由宿主持有并写入有效 Profile 的支撑模式。 */
enum hostsupportmode
{
    HOST_SUPPORT_NONE = 0,
    HOST_SUPPORT_BOTTOM_PROJECTION = 1,
    HOST_SUPPORT_UNSUPPORTED_ONLY = 2,
    HOST_SUPPORT_BOTTOM_PLUS_UNSUPPORTED = 3,
    HOST_SUPPORT_FULL_VERTICAL_PROJECTION = 4
};

/** @brief 与 C 兼容的纹理应用模式。 */
enum hosttextureapplymode
{
    HOST_TEXTURE_SOLID_VOLUME_FROM_TOP = 0,
    HOST_TEXTURE_TOP_SURFACE_ONLY = 1,
    HOST_TEXTURE_TOP_SURFACE_BAND = 2
};

/** @brief 与 C 兼容的纹理采样器。 */
enum hosttexturesampler
{
    HOST_TEXTURE_NEAREST = 0,
    HOST_TEXTURE_BILINEAR = 1
};

/** @brief 与 C 兼容的 UV 寻址模式。 */
enum hosttextureuvaddressmode
{
    HOST_TEXTURE_UV_CLAMP = 0,
    HOST_TEXTURE_UV_REPEAT = 1
};

/** @brief 与 C 兼容的纹理缺失策略。 */
enum hosttexturemissingpolicy
{
    HOST_TEXTURE_WARN_AND_FALLBACK = 0,
    HOST_TEXTURE_FAIL_FAST = 1
};

/** @brief 与 C 兼容的非表面 RGB 策略。 */
enum hosttexturenonsurfacepolicy
{
    HOST_TEXTURE_NON_SURFACE_MODEL_MATERIAL = 0,
    HOST_TEXTURE_NON_SURFACE_EMPTY = 1
};

/** @brief 与 C 兼容的不可打印白色策略。 */
enum hosttexturewhitepolicy
{
    HOST_TEXTURE_WHITE_FAIL_CLOSED = 0,
    HOST_TEXTURE_WHITE_UNDERBASE = 1
};

/** @brief 与 C 兼容的几何占用采样策略。 */
enum hostgeometrysamplingstrategy
{
    HOST_GEOMETRY_SAMPLING_LEGACY_CENTER = 0,
    HOST_GEOMETRY_SAMPLING_SLAB_2X2_AT_LEAST_TWO = 1
};

/** @brief 与 C 兼容的 TIFF 压缩算法。 */
enum hosttiffcompression
{
    HOST_TIFF_COMPRESSION_NONE = 0,
    HOST_TIFF_COMPRESSION_PACKBITS = 1
};

/** @brief 用于构建有效切片 Profile 的 C 兼容输入。 */
struct hosteffectiveprofilesettings
{
    const char* modelpath;
    const char* modelformat;
    const char* packagedirectory;
    const char* profileid;
    int dpix;
    int dpiy;
    double layerthicknessmm;
    enum hostmaterialstrategy materialstrategy;
    int materialrolemappingenabled;
    enum hostmaterialrole materialdefaultrole;
    int mapwhitenames;
    int mapvarnishnames;
    int allowinputsupportmaterial;
    int whiteexpandpx;
    int whiteshrinkpx;
    int varnishtoplayers;
    int maxunexpectedoverlappixels;
    int supportenabled;
    enum hostsupportmode supportmode;
    double supportoffsetmm;
    int supportminareapx;
    int internalvoidenabled;
    int internalvoidminareapx;
    int baseprojectionenabled;
    int baseprojectionlayercount;
    int textureenabled;
    enum hosttextureapplymode textureapplymode;
    int texturetopsurfacelayers;
    enum hosttexturesampler texturesampler;
    enum hosttextureuvaddressmode textureuvaddressmode;
    int textureflipv;
    int texturefallbackred;
    int texturefallbackgreen;
    int texturefallbackblue;
    enum hosttexturemissingpolicy texturemissingpolicy;
    enum hosttexturenonsurfacepolicy texturenonsurfacepolicy;
    enum hosttexturewhitepolicy texturewhitepolicy;
    int texturewhiteinkthreshold;
    int texturewhitevalue;
    enum hosttiffcompression tiffcompression;
    enum hostgeometrysamplingstrategy geometrysamplingstrategy;
};

/**
 * @brief 为参考切片构建包含自身哈希的有效 Profile。
 * @param modelPath 规范化的模型绝对路径。
 * @param packageDirectory 规范化的 Package 绝对目录。
 * @param profileHash 接收 `sha256:` 与 64 个小写十六进制字符。
 * @param profileHashCapacity 输出缓冲区容量。
 * @return 由调用方持有的堆分配 JSON 字符串；失败时返回 NULL。
 */
char* HostBuildProfile(
    const char* modelPath,
    const char* packageDirectory,
    char* profileHash,
    unsigned long profileHashCapacity);

/**
 * @brief 使用显式层高构建包含自身哈希的有效 Profile。
 * @param modelPath 规范化的模型绝对路径。
 * @param packageDirectory 规范化的 Package 绝对目录。
 * @param layerThicknessMm 以毫米为单位的正数层厚。
 * @param profileHash 接收 `sha256:` 与 64 个小写十六进制字符。
 * @param profileHashCapacity 输出缓冲区容量。
 * @return 由调用方持有的堆分配 JSON 字符串；失败时返回 NULL。
 */
char* HostBuildProfileWithLayerThickness(
    const char* modelPath,
    const char* packageDirectory,
    double layerThicknessMm,
    char* profileHash,
    unsigned long profileHashCapacity);

/**
 * @brief 构建参数化且包含自身哈希的有效 Profile。
 * @param settings 有效的宿主侧模型、输出与材料设置。
 * @param profileHash 接收 `sha256:` 与 64 个小写十六进制字符。
 * @param profileHashCapacity 输出缓冲区容量。
 * @return 由调用方持有的堆分配 JSON 字符串；校验失败时返回 NULL。
 */
char* HostBuildEffectiveProfile(
    const struct hosteffectiveprofilesettings* settings,
    char* profileHash,
    unsigned long profileHashCapacity);

#ifdef __cplusplus
}
#endif
