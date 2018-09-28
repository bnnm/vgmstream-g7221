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
  Filename:    decode.c    

  Purpose:     Contains the main function for the G.722.1 Annex C decoder
		
  Design Notes:

***************************************************************************/
/***************************************************************************
 Include files                                                           
***************************************************************************/
#include <stdio.h>
#include <string.h>
#include "g7221i.h"
#include "defs.h"

#if defined(G7221_EXPORT)
    #define G7221_API __declspec(dllexport) /* when exporting/creating DLL */
#elif defined(G7221_IMPORT)
    #define G7221_API __declspec(dllimport) /* when importing/linking DLL */
#else
    #define G7221_API /* internal/default */
#endif

/************************************************************************************
 Local type declarations                                             
*************************************************************************************/

/* Holds all state needed from one frame to the next */
typedef struct
{
    Word16 decoder_mlt_coefs[MAX_DCT_LENGTH];
    Word16 mag_shift;
    Word16 old_mag_shift;
    Word16 old_decoder_mlt_coefs[MAX_DCT_LENGTH];
    Word16 old_samples[MAX_DCT_LENGTH>>1];
    Rand_Obj randobj;
} DECODER_STATE;

/* This object is used to control the decoder configuration */
typedef struct 
{
    Word16  syntax;
    Word32  bit_rate;
    Word16  bandwidth;
    Word16  number_of_bits_per_frame;
    Word16  number_of_regions;
    Word16  frame_size;
} DECODER_CONTROL;

/* The persistant object we hand back to the caller */
/* typedef g7221_handle_s g7221_handle; */
struct g7221_handle_s
{
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
    Word16 i;
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
            &handle->state.mag_shift,
            &handle->state.old_mag_shift,
            handle->state.old_decoder_mlt_coefs,
            0 // no frame error
            );
        
    /* convert unpacked frame to samples */
    rmlt_coefs_to_samples(
            handle->state.decoder_mlt_coefs,
            handle->state.old_samples,
            sample_buffer,
            handle->control.frame_size,
            handle->state.mag_shift);

    /* For ITU testing, off the 2 lsbs. */
    for (i=0; i<handle->control.frame_size; i++)
        sample_buffer[i] &= 0xfffc;
}

G7221_API
g7221_handle * g7221_init(int bytes_per_frame, int bandwidth) {
    g7221_handle * handle = NULL;
    Word32 bit_rate;

    handle = calloc(1, sizeof(g7221_handle));
    if (!handle) goto fail;

    /* determine format */
    bit_rate = (Word32)bytes_per_frame*8*50;
    if ((bit_rate < 16000) || (bit_rate > 48000) || ((bit_rate/800)*800 != bit_rate)) {
        fprintf(stderr,"codec: Error. bit-rate must be multiple of 800 between 16000 and 48000\n");
        goto fail;
    }

    handle->control.bit_rate = bit_rate;
    handle->control.number_of_bits_per_frame = bytes_per_frame * 8;
    handle->control.bandwidth = bandwidth;
    if (handle->control.bandwidth == 7000) {
        handle->control.number_of_regions = NUMBER_OF_REGIONS;
        handle->control.frame_size = MAX_FRAMESIZE >> 1;
    }
    else if (handle->control.bandwidth == 14000) {
        handle->control.number_of_regions = MAX_NUMBER_OF_REGIONS;
        handle->control.frame_size = MAX_FRAMESIZE;
    }
    else {
        fprintf(stderr,"codec: Error. bandwidth must be 7000 or 14000\n");
        goto fail;
    }

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
