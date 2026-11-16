/* The interior models of the client. A new area clears the list of interior models and frees
 * the buffers of each model. The shared area code calls this function through sr_area_reset_interiors. */
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define NUM_INTERIOR_MODELS 0x1000

/* Buffers of one model: their places in the mesh words of the record. */
static const int buffer_words[] = {2, 4, 5, 6, 7, 8};

void area_reset_interiors(void)
{
    SrInteriorStore *store = &sr_data_interior;
    int i, k;

    store->count = 0;
    for (i = 0; i < NUM_INTERIOR_MODELS; i++) {
        SrInteriorModel *model = &store->records[i];

        model->sector = -1;
        for (k = 0; k < (int)(sizeof buffer_words / sizeof buffer_words[0]); k++) {
            os_free((void *)(uintptr_t)model->mesh[buffer_words[k]]);
            model->mesh[buffer_words[k]] = 0;
        }
    }
}
