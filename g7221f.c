/* library interface for G.722.1, based on hcs's code and: */

/***************************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**
**   All rights reserved.
**
***************************************************************************/

/***************************************************************************
 Include files                                                           
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "g7221f.h"
#include "defs.h"

#if defined(G7221_EXPORT)
    #define G7221_API __declspec(dllexport) /* when exporting/creating DLL */
#elif defined(G7221_IMPORT)
    #define G7221_API __declspec(dllimport) /* when importing/linking DLL */
#else
    #define G7221_API /* internal/default */
#endif

/************************************************************************************
 Extern function declarations
*************************************************************************************/
extern void mlt_based_coder_init();
extern void imlt_window_init();
extern void dct4_init();
extern void decoder(Bit_Obj*, Rand_Obj*, int, float [], float [], int);
extern void rmlt_coefs_to_samples(float *, float *, float *, int);

/************************************************************************************
 Local type declarations                                             
*************************************************************************************/

/* Holds all state needed from one frame to the next */
typedef struct {
    float decoder_mlt_coefs[MAX_DCT_SIZE];
    float old_decoder_mlt_coefs[MAX_DCT_SIZE];
    float old_samples[MAX_DCT_SIZE >> 1];
    float out_samples[MAX_DCT_SIZE];
    int   frame_error_flag; /* not actually used */
    Rand_Obj randobj;
} DECODER_STATE;

/* This object is used to control the decoder configuration */
typedef struct  {
    int bit_rate;
    int bandwidth;
    int number_of_bits_per_frame;
    int number_of_regions;
    int frame_size;
} DECODER_CONTROL;

/* The persistant object we hand back to the caller */
struct g7221_handle_s {
    DECODER_CONTROL control;
    DECODER_STATE state;
};

/************************************************************************************
 Constant definitions                                                    
*************************************************************************************/
#define MAX_SAMPLE_RATE     32000
#define MAX_FRAMESIZE   (MAX_SAMPLE_RATE/50)

/***************************************************************************
 Local function declarations                                             
***************************************************************************/

G7221_API
void g7221_decode_frame(
    g7221_handle *handle,
    int16_t *code_words,
    int16_t *sample_buffer)
{
    int i;
    Bit_Obj bitobj;

    /* reinit the current word to point to the start of the buffer */
    bitobj.code_word_ptr = code_words;
    bitobj.current_word =  *code_words;
    bitobj.code_bit_count = 0;
    bitobj.number_of_bits_left = handle->control.number_of_bits_per_frame;

    /* process the out_words into decoder_mlt_coefs */
    decoder(
            &bitobj,
            &handle->state.randobj,
            handle->control.number_of_regions,
            handle->state.decoder_mlt_coefs,
            handle->state.old_decoder_mlt_coefs,
            handle->state.frame_error_flag);

    /* convert unpacked frame to samples */
    rmlt_coefs_to_samples(
            handle->state.decoder_mlt_coefs,
            handle->state.old_samples,
            handle->state.out_samples,
            handle->control.frame_size);

    /* float to samples */
    {
        for (i = 0; i < handle->control.frame_size; i++) {
            float f = handle->state.out_samples[i];

            if (f >= 0.0) {
                if (f < 32767.0)
                    sample_buffer[i] = (int) (f + 0.5);
                else
                    sample_buffer[i] = 32767;
            }
            else {
                if (f > -32768.0)
                    sample_buffer[i] = (int) (f - 0.5);
                else
                    sample_buffer[i] = -32768;
            }
        }
    }

    /* For ITU testing, off the 2 lsbs. */
    for (i = 0; i < handle->control.frame_size; i++) {
        sample_buffer[i] &= 0xfffc;
    }

}

G7221_API
g7221_handle * g7221_init(int bytes_per_frame, int bandwidth) {
    g7221_handle * handle = NULL;
    int bit_rate;

    handle = calloc(1, sizeof(g7221_handle));
    if (!handle) goto fail;

    /* determine format */
    bit_rate = bytes_per_frame * 8 * 50;
    if ((bit_rate < 16000) || (bit_rate > 48000) || ((bit_rate/800)*800 != bit_rate)) {
        fprintf(stderr,"codec: Error. bit-rate must be multiple of 800 between 16000 and 48000\n");
        goto fail;
    }

    handle->control.bit_rate = bit_rate;
    handle->control.number_of_bits_per_frame = bytes_per_frame * 8;
    handle->control.bandwidth = bandwidth;
    if (handle->control.bandwidth == 7000) {
        handle->control.number_of_regions = NUM_REGIONS;
        handle->control.frame_size = 16000/50;
    }
    else if (handle->control.bandwidth == 14000) {
        handle->control.number_of_regions = MAX_NUM_REGIONS;
        handle->control.frame_size = 32000/50;
    }
    else {
        fprintf(stderr,"codec: Error. bandwidth must be 7000 or 14000\n");
        goto fail;
    }

    /* initialize global tables */
    /* only needs to be called once per program rather than every g7221_init,
     * but should't matter or affect threads (as long as table = value assigns are atomic) */
    mlt_based_coder_init();
    imlt_window_init();
    dct4_init();

    g7221_reset(handle);

    return handle;

fail:
    free(handle);
    return NULL;
}

G7221_API
void g7221_reset(g7221_handle *handle) {

    /* initialize coefs and old values */
    memset(&handle->state, 0, sizeof(handle->state));

    /* initialize the random number generator */
    handle->state.randobj.seed0 = 1;
    handle->state.randobj.seed1 = 1;
    handle->state.randobj.seed2 = 1;
    handle->state.randobj.seed3 = 1;
}

G7221_API
void g7221_free(g7221_handle *handle) {
    free(handle);
}
