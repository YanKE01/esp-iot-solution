#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

typedef enum {
    ARM_MATH_SUCCESS        =  0,
    ARM_MATH_ARGUMENT_ERROR = -1,
    ARM_MATH_LENGTH_ERROR   = -2,
    ARM_MATH_SIZE_MISMATCH  = -3,
    ARM_MATH_NANINF         = -4,
    ARM_MATH_SINGULAR       = -5,
    ARM_MATH_TEST_FAILURE   = -6
} arm_status;

typedef struct {
    uint16_t numRows;
    uint16_t numCols;
    float   *pData;
} arm_matrix_instance_f32;

inline void arm_mat_init_f32(arm_matrix_instance_f32 *S, uint16_t nRows, uint16_t nCols, float *pData)
{
    S->numRows = nRows;
    S->numCols = nCols;
    S->pData   = pData;
}

inline arm_status arm_mat_add_f32(const arm_matrix_instance_f32 *A, const arm_matrix_instance_f32 *B, arm_matrix_instance_f32 *C)
{
    if (!A || !B || !C) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    if (A->numRows != B->numRows || A->numCols != B->numCols ||
            A->numRows != C->numRows || A->numCols != C->numCols) {
        return ARM_MATH_SIZE_MISMATCH;
    }

    size_t n = (size_t)A->numRows * (size_t)A->numCols;
    const float *a = A->pData;
    const float *b = B->pData;
    float *c = C->pData;

    for (size_t i = 0; i < n; ++i) {
        c[i] = a[i] + b[i];
    }

    return ARM_MATH_SUCCESS;
}

inline arm_status arm_mat_sub_f32(const arm_matrix_instance_f32 *A, const arm_matrix_instance_f32 *B, arm_matrix_instance_f32 *C)
{
    if (!A || !B || !C) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    if (A->numRows != B->numRows || A->numCols != B->numCols ||
            A->numRows != C->numRows || A->numCols != C->numCols) {
        return ARM_MATH_SIZE_MISMATCH;
    }

    size_t n = (size_t)A->numRows * (size_t)A->numCols;
    const float *a = A->pData;
    const float *b = B->pData;
    float *c = C->pData;

    for (size_t i = 0; i < n; ++i) {
        c[i] = a[i] - b[i];
    }

    return ARM_MATH_SUCCESS;
}

inline arm_status arm_mat_scale_f32(const arm_matrix_instance_f32 *A, float k, arm_matrix_instance_f32 *C)
{
    if (!A || !C) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    if (A->numRows != C->numRows || A->numCols != C->numCols) {
        return ARM_MATH_SIZE_MISMATCH;
    }

    size_t n = (size_t)A->numRows * (size_t)A->numCols;
    const float *a = A->pData;
    float *c = C->pData;

    /* Safe for in-place because each element read once then written */
    for (size_t i = 0; i < n; ++i) {
        c[i] = k * a[i];
    }

    return ARM_MATH_SUCCESS;
}

inline arm_status arm_mat_mult_f32(const arm_matrix_instance_f32 *A, const arm_matrix_instance_f32 *B, arm_matrix_instance_f32 *C)
{
    if (!A || !B || !C) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    if (A->numCols != B->numRows ||
            C->numRows != A->numRows || C->numCols != B->numCols) {
        return ARM_MATH_SIZE_MISMATCH;
    }

    /* Prevent accidental in-place usage which would corrupt results */
    if (C->pData == A->pData || C->pData == B->pData) {
        return ARM_MATH_ARGUMENT_ERROR;
    }

    uint16_t M = A->numRows;
    uint16_t N = A->numCols;  /* == B->numRows */
    uint16_t P = B->numCols;

    const float *a = A->pData;
    const float *b = B->pData;
    float *c = C->pData;

    for (uint16_t i = 0; i < M; ++i) {
        for (uint16_t j = 0; j < P; ++j) {
            float acc = 0.0f;
            const float *a_row = a + (size_t)i * N;
            for (uint16_t k = 0; k < N; ++k) {
                acc += a_row[k] * b[(size_t)k * P + j];
            }
            c[(size_t)i * P + j] = acc;
        }
    }

    return ARM_MATH_SUCCESS;
}

inline arm_status arm_mat_trans_f32(const arm_matrix_instance_f32 *A, arm_matrix_instance_f32 *B)
{
    if (!A || !B) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    if (A->numRows != B->numCols || A->numCols != B->numRows) {
        return ARM_MATH_SIZE_MISMATCH;
    }

    const float *a = A->pData;
    float *b = B->pData;

    /* Generic O(mn) transpose; supports A and B distinct buffers.
    For safety, disallow true in-place unless it is a square matrix
    and exactly the same buffer (rare in your usage anyway). */
    if (a == b && A->numRows != A->numCols) {
        return ARM_MATH_ARGUMENT_ERROR;
    }

    uint16_t m = A->numRows;
    uint16_t n = A->numCols;

    if (a == b && m == n) {
        /* In-place square transpose (simple but O(n^2)) */
        for (uint16_t i = 0; i < n; ++i)
            for (uint16_t j = i + 1; j < n; ++j) {
                float tmp = b[(size_t)i * n + j];
                b[(size_t)i * n + j] = b[(size_t)j * n + i];
                b[(size_t)j * n + i] = tmp;
            }
        return ARM_MATH_SUCCESS;
    }

    /* Out-of-place */
    for (uint16_t i = 0; i < m; ++i)
        for (uint16_t j = 0; j < n; ++j) {
            b[(size_t)j * m + i] = a[(size_t)i * n + j];
        }

    return ARM_MATH_SUCCESS;
}
