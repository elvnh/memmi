// TODO: is this file compatible with 32 bit x86?
// TODO: undefine these macros at end of file
#define DR7_ENABLE_BIT_BASE_INDEX   16u
#define DR7_ENABLE_BIT_STRIDE       2u
#define DR7_COND_BITS_BASE_INDEX    16u
#define DR7_COND_BITS_STRIDE        4u
#define DR7_LENGTH_BITS_BASE_INDEX  18u
#define DR7_LENGTH_BITS_STRIDE      4u

#define DR7_READ_WRITE_COND         0b11u
#define DR7_WRITE_COND              0b01u
#define DR7_SIZE_1_BYTES            0b00
#define DR7_SIZE_2_BYTES            0b01
#define DR7_SIZE_4_BYTES            0b11
#define DR7_SIZE_8_BYTES            0b10

typedef enum {
    DEBUG_REG_DR0,
    DEBUG_REG_DR1,
    DEBUG_REG_DR2,
    DEBUG_REG_DR3,
    DEBUG_REG_DR6,
    DEBUG_REG_DR7,
    DEBUG_REG_COUNT,
} DebugRegister;

typedef struct {
    memmi_Status status;
    uint64_t     values[DEBUG_REG_COUNT];
} DebugRegisters;

static DebugRegister debug_register_from_index(uint32_t index)
{
    DebugRegister result = zero_enum(DebugRegister);

    switch (index) {
        case 0: {
            result = DEBUG_REG_DR0;
        } break;

        case 1: {
            result = DEBUG_REG_DR1;
        } break;

        case 2: {
            result = DEBUG_REG_DR2;
        } break;

        case 3: {
            result = DEBUG_REG_DR3;
        } break;

        default: {
            ASSERT(0);
            result = DEBUG_REG_DR0;
        } break;
    }

    return result;
}

static uint64_t dr7_breakpoint_mask(uint32_t breakpoint_index)
{
    uint32_t result =
          (0b01u << (DR7_ENABLE_BIT_BASE_INDEX  + breakpoint_index * DR7_ENABLE_BIT_STRIDE))
        | (0b11u << (DR7_COND_BITS_BASE_INDEX   + breakpoint_index * DR7_COND_BITS_STRIDE))
        | (0b11u << (DR7_LENGTH_BITS_BASE_INDEX + breakpoint_index * DR7_LENGTH_BITS_STRIDE));

    return result;
}

static uint64_t dr7_local_enable_bit(uint32_t reg_index)
{
    uint64_t result = (uint64_t)((uint64_t)0x1u << ((uint64_t)reg_index * (uint64_t)DR7_ENABLE_BIT_STRIDE));

    return result;
}

static uint64_t dr7_condition_bits(uint32_t reg_index, memmi_BreakpointCondition condition)
{
    uint64_t bits = 0;

    switch (condition) {
        case MEMMI_BREAKPOINT_READ_WRITE: {
            bits = DR7_READ_WRITE_COND;
        } break;

        case MEMMI_BREAKPOINT_WRITE: {
            bits = DR7_WRITE_COND;
        } break;

        default: {
            ASSERT(0);
            bits = DR7_READ_WRITE_COND;
        } break;
    }

    uint64_t result = bits << (DR7_COND_BITS_BASE_INDEX + reg_index * DR7_COND_BITS_STRIDE);

    return result;
}

static uint64_t dr7_length_bits(uint32_t reg_index, memmi_BreakpointLength length)
{
    uint64_t bits = 0;

    switch (length) {
        case MEMMI_BREAKPOINT_1_BYTES: {
            bits = DR7_SIZE_1_BYTES;
        } break;

        case MEMMI_BREAKPOINT_2_BYTES: {
            bits = DR7_SIZE_2_BYTES;
        } break;

        case MEMMI_BREAKPOINT_4_BYTES: {
            bits = DR7_SIZE_4_BYTES;
        } break;

        case MEMMI_BREAKPOINT_8_BYTES: {
            bits = DR7_SIZE_8_BYTES;
        } break;
    }

    uint64_t result = bits << (DR7_LENGTH_BITS_BASE_INDEX + reg_index * DR7_LENGTH_BITS_STRIDE);

    return result;
}

static uint64_t dr7_set_breakpoint_value(uint64_t old_dr7, uint32_t index, memmi_BreakpointCondition cond, memmi_BreakpointLength length)
{
    ASSERT(index <= 3);

    uint64_t result =
        (old_dr7 & ~dr7_breakpoint_mask(index))
        | dr7_local_enable_bit(index)
        | dr7_condition_bits(index, cond)
        | dr7_length_bits(index, length);

    return result;
}

static int32_t get_dr6_breakpoint_index(uint64_t dr6)
{
    int32_t result = -1;

    if (dr6 & 0x1) {
        result = 0;
    } else if (dr6 & 0x2) {
        result = 1;
    } else if (dr6 & 0x4) {
        result = 2;
    } else if (dr6 & 0x8) {
        result = 3;
    }

    return result;
}
