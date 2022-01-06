// -----------------------------------------------------------------
// IMediaSideData interface and data structure definitions
// -----------------------------------------------------------------

#pragma once

#include <stdint.h>

// -----------------------------------------------------------------
// Interface to exchange binary side data
// -----------------------------------------------------------------
// This interface should be implemented in IMediaSample objects and accessed through IUnknown
// It allows binary side data to be attached to the media samples and delivered with them
// Restrictions: Only one side data per type can be attached
interface __declspec(uuid("F940AE7F-48EB-4377-806C-8FC48CAB2292")) IMediaSideData : public IUnknown
{
    // Set the side data identified by guidType to the data provided
    // The provided data will be copied and stored internally
    STDMETHOD(SetSideData)(GUID guidType, const BYTE *pData, size_t size) PURE;

    // Get the side data identified by guidType
    // The caller receives pointers to the internal data, and the pointers shall stay
    // valid for the lifetime of the object
    STDMETHOD(GetSideData)(GUID guidType, const BYTE **pData, size_t *pSize) PURE;
};

// -----------------------------------------------------------------
// High-Dynamic-Range (HDR) Side Data
// -----------------------------------------------------------------

// {53820DBC-A7B8-49C4-B17B-E511591A790C}
DEFINE_GUID(IID_MediaSideDataHDR, 0x53820dbc, 0xa7b8, 0x49c4, 0xb1, 0x7b, 0xe5, 0x11, 0x59, 0x1a, 0x79, 0xc);

#pragma pack(push, 1)
struct MediaSideDataHDR
{
    // coordinates of the primaries, in G-B-R order
    double display_primaries_x[3];
    double display_primaries_y[3];
    // white point
    double white_point_x;
    double white_point_y;
    // luminance
    double max_display_mastering_luminance;
    double min_display_mastering_luminance;
};
#pragma pack(pop)

// {ED6AE576-7CBE-41A6-9DC3-07C35DC13EF9}
DEFINE_GUID(IID_MediaSideDataHDRContentLightLevel, 0xed6ae576, 0x7cbe, 0x41a6, 0x9d, 0xc3, 0x7, 0xc3, 0x5d, 0xc1, 0x3e,
            0xf9);

#pragma pack(push, 1)
struct MediaSideDataHDRContentLightLevel
{
    // maximum content light level (cd/m2)
    unsigned int MaxCLL;

    // maximum frame average light level (cd/m2)
    unsigned int MaxFALL;
};
#pragma pack(pop)

// {183ED511-8910-4262-88F6-4946BC799C84}
DEFINE_GUID(IID_MediaSideDataHDR10Plus, 0x183ed511, 0x8910, 0x4262, 0x88, 0xf6, 0x49, 0x46, 0xbc, 0x79, 0x9c, 0x84);

#pragma pack(push, 1)
// HDR10+ metadata according to SMPTE 2094-40
// Refer to the specification for the meaning of the fields
//
// All pixel values are kept as-is, rational values are normalized as double-precision floating point
struct MediaSideDataHDR10Plus
{
    // number of windows (1-3)
    unsigned int num_windows;

    // processing windows
    struct
    {
        unsigned int upper_left_corner_x;
        unsigned int upper_left_corner_y;
        unsigned int lower_right_corner_x;
        unsigned int lower_right_corner_y;
        unsigned int center_of_ellipse_x;
        unsigned int center_of_ellipse_y;
        unsigned int rotation_angle;
        unsigned int semimajor_axis_internal_ellipse;
        unsigned int semimajor_axis_external_ellipse;
        unsigned int semiminor_axis_external_ellipse;
        unsigned int overlap_process_option;

        double maxscl[3];
        double average_maxrgb;

        unsigned int num_distribution_maxrgb_percentiles;
        struct
        {
            unsigned int percentage;
            double percentile;
        } distribution_maxrgb_percentiles[15];

        double fraction_bright_pixels;

        unsigned int tone_mapping_flag;

        double knee_point_x;
        double knee_point_y;

        unsigned int num_bezier_curve_anchors;
        double bezier_curve_anchors[15];

        unsigned int color_saturation_mapping_flag;
        double color_saturation_weight;
    } windows[3];

    double targeted_system_display_maximum_luminance;

    unsigned int targeted_system_display_actual_peak_luminance_flag;
    unsigned int num_rows_targeted_system_display_actual_peak_luminance;
    unsigned int num_cols_targeted_system_display_actual_peak_luminance;
    double targeted_system_display_actual_peak_luminance[25][25];

    unsigned int mastering_display_actual_peak_luminance_flag;
    unsigned int num_rows_mastering_display_actual_peak_luminance;
    unsigned int num_cols_mastering_display_actual_peak_luminance;
    double mastering_display_actual_peak_luminance[25][25];
};
#pragma pack(pop)

// {277EE779-13F4-434E-BDEC-3D6F8C0E15D2}
DEFINE_GUID(IID_MediaSideDataDOVIMetadata, 0x277ee779, 0x13f4, 0x434e, 0xbd, 0xec, 0x3d, 0x6f, 0x8c, 0xe, 0x15, 0xd2);

#pragma pack(push, 1)
// Dolby Vision metadata
// Refer to the specification for the meaning of the fields
struct MediaSideDataDOVIMetadata
{
    struct
    {
        uint8_t rpu_type;
        uint16_t rpu_format;
        uint8_t vdr_rpu_profile;
        uint8_t vdr_rpu_level;
        uint8_t chroma_resampling_explicit_filter_flag;
        uint8_t coef_data_type; /* informative, lavc always converts to fixed */
        uint8_t coef_log2_denom;
        uint8_t vdr_rpu_normalized_idc;
        uint8_t bl_video_full_range_flag;
        uint8_t bl_bit_depth;  /* [8, 16] */
        uint8_t el_bit_depth;  /* [8, 16] */
        uint8_t vdr_bit_depth; /* [8, 16] */
        uint8_t spatial_resampling_filter_flag;
        uint8_t el_spatial_resampling_filter_flag;
        uint8_t disable_residual_flag;
    } Header;

    struct
    {
        uint8_t vdr_rpu_id;
        uint8_t mapping_color_space;
        uint8_t mapping_chroma_format_idc;

        struct
        {
#define LAV_DOVI_MAX_PIECES 8
            uint8_t num_pivots;                           /* [2, 9] */
            uint16_t pivots[LAV_DOVI_MAX_PIECES + 1]; /* sorted ascending */
            uint8_t mapping_idc[LAV_DOVI_MAX_PIECES];     /* 0 polynomial, 1 mmr */

            /* polynomial */
            uint8_t poly_order[LAV_DOVI_MAX_PIECES];    /* [1, 2] */
            int64_t poly_coef[LAV_DOVI_MAX_PIECES][3]; /* x^0, x^1, x^2 */

            /* mmr */
            uint8_t mmr_order[LAV_DOVI_MAX_PIECES]; /* [1, 3] */
            int64_t mmr_constant[LAV_DOVI_MAX_PIECES];
            int64_t mmr_coef[LAV_DOVI_MAX_PIECES][3 /* order - 1 */][7];
        } curves[3]; /* per component */

        /* Non-linear inverse quantization */
        uint8_t nlq_method_idc; // -1 none, 0 linear dz
        uint32_t num_x_partitions;
        uint32_t num_y_partitions;
        struct
        {
            uint16_t nlq_offset;
            uint64_t vdr_in_max;

            /* linear dz */
            uint64_t linear_deadzone_slope;
            uint64_t linear_deadzone_threshold;
        } nlq[3]; /* per component */
    } Mapping;

    struct
    {
        uint8_t dm_metadata_id;
        uint8_t scene_refresh_flag;

        /**
         * Coefficients of the custom Dolby Vision IPT-PQ matrices. These are to be
         * used instead of the matrices indicated by the frame's colorspace tags.
         * The output of rgb_to_lms_matrix is to be fed into a BT.2020 LMS->RGB
         * matrix based on a Hunt-Pointer-Estevez transform, but without any
         * crosstalk. (See the definition of the ICtCp colorspace for more
         * information.)
         */
        double ycc_to_rgb_matrix[9]; /* before PQ linearization */
        double ycc_to_rgb_offset[3]; /* input offset of neutral value */
        double rgb_to_lms_matrix[9]; /* after PQ linearization */

        /**
         * Extra signal metadata (see Dolby patents for more info).
         */
        uint16_t signal_eotf;
        uint16_t signal_eotf_param0;
        uint16_t signal_eotf_param1;
        uint32_t signal_eotf_param2;
        uint8_t signal_bit_depth;
        uint8_t signal_color_space;
        uint8_t signal_chroma_format;
        uint8_t signal_full_range_flag; /* [0, 3] */
        uint16_t source_min_pq;
        uint16_t source_max_pq;
        uint16_t source_diagonal;
    } ColorMetadata;
};
#pragma pack(pop)

// -----------------------------------------------------------------
// 3D Plane Offset Side Data
// -----------------------------------------------------------------

// {F169B76C-75A3-49E6-A23A-14983EBF4370}
DEFINE_GUID(IID_MediaSideData3DOffset, 0xf169b76c, 0x75a3, 0x49e6, 0xa2, 0x3a, 0x14, 0x98, 0x3e, 0xbf, 0x43, 0x70);

#pragma pack(push, 1)
struct MediaSideData3DOffset
{
    // Number of valid offsets (up to 32)
    int offset_count;

    // Offset Value, can be positive or negative
    // positive values offset closer to the viewer (move right on the left view, left on the right view)
    // negative values offset further away from the viewer (move left on the left view, right on the right view)
    int offset[32];
};
#pragma pack(pop)

// -----------------------------------------------------------------
// EIA-608/708 Closed Caption Data
// -----------------------------------------------------------------

// {40FEFD7F-85DD-4335-A804-8A33B0BF7B81}
DEFINE_GUID(IID_MediaSideDataEIA608CC, 0x40fefd7f, 0x85dd, 0x4335, 0xa8, 0x4, 0x8a, 0x33, 0xb0, 0xbf, 0x7b, 0x81);

// There is no struct definition. The data is supplied as a list of 3 byte CC data packets (control byte + cc_data1/2)

// -----------------------------------------------------------------
// Media Control Flags
// -----------------------------------------------------------------

// {B5411CE3-3B21-4FD6-80A8-CE682969F795}
DEFINE_GUID(IID_MediaSideDataControlFlags, 0xb5411ce3, 0x3b21, 0x4fd6, 0x80, 0xa8, 0xce, 0x68, 0x29, 0x69, 0xf7, 0x95);

// No struct definition, just a single DWORD for additional flags
enum MediaSideDataControlFlags
{
    MediaSideDataControlFlags_EndOfSequence = (1 << 0),
};
