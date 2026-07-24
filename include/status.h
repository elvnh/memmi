#pragma once


/*
  TODO:
  - Should this instead be just a single integer?
    MEMMI_OK could be == 0, partial success could be a bit that's set
  - strerror?
 */

typedef enum {
    MEMMI_OK                       =  0u,
    MEMMI_NO_SUCH_PROCESS          = (1u << 0u),
    MEMMI_INSUFFICIENT_PERMISSIONS = (1u << 1u),
    MEMMI_INVALID_ARGUMENTS        = (1u << 2u),
    MEMMI_ALLOCATION_FAILED        = (1u << 3u),
    MEMMI_PARTIAL_READ_OR_WRITE    = (1u << 4u),
} memmi_Status;
