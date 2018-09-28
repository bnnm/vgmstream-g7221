/*****************************************************************
******************************************************************
**
**   G.722.1 Annex B - G.722.1 Floating point implementation
**   > Software Release 2.1 (2008-06)
**
**	Filename : decode.c 
**
**   © 2000 PictureTel Coporation
**          Andover, MA, USA  
**
**	    All rights reserved.
**
******************************************************************
*****************************************************************/

/***************************************************************************
 Include files                                                           
***************************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "defs.h"

static int one = 0x0081;
static int zero = 0x007f;
static int frame_start = 0x6b21;
int read_ITU_format(short int [], int *, int, FILE *);

/************************************************************************************
 Extern function declarations                                             
*************************************************************************************/
extern void mlt_based_coder_init();
extern void decoder(Bit_Obj*, Rand_Obj*, int, float [], float [], int);
extern void rmlt_coefs_to_samples(float *, float *, float *, int);

/***************************************************************************
 Procedure/Function:     G722.1 main decoder function 

 Syntax:

 Description:  Main processing loop for the G.722.1 decoder

 Design Notes:
								
***************************************************************************/
			   
int main(argc, argv)
     int argc;
     char *argv[];
{
  FILE *fpout;
  FILE *fp_bitstream;

  int i;
  int nsamp1;
  int number_of_regions;
  short int output[MAX_DCT_SIZE];
  float decoder_mlt_coefs[MAX_DCT_SIZE];
  float old_decoder_mlt_coefs[MAX_DCT_SIZE] = {0};
  float float_old_samples[MAX_DCT_SIZE] = {0};
  float float_out_samples[MAX_DCT_SIZE];
  int sample_rate;
  int bit_rate;
  int number_of_bits_per_frame;
  int number_of_16bit_words_per_frame;
  short int out_words[MAX_BITS_PER_FRAME/16];
  int framesize;
  int bandwidth;
  int syntax;
  int frame_error_flag=0;
  Bit_Obj bitobj;
  Rand_Obj randobj;

  /* parse the command line input */
  if (argc < 6) {
    printf("Usage: decode 0(packed)/1 input-audio-file output-bitstream-file bit-rate bandwidth\n");
    exit(1);
  }

  syntax = atoi(*++argv);

  if ((syntax != 0) && (syntax != 1))
    {
      printf("syntax must be 0 for packed or 1 for ITU format\n");
      exit(1);
    }
  
    if((fp_bitstream = fopen(*++argv,"rb")) == NULL) {
      printf("codec: error opening %s.\n",*argv);
      exit(1);
    }
    if((fpout = fopen(*++argv,"wb")) == NULL) {
      printf("codec: error opening %s.\n",*argv);
      exit(1);
	}
  
  printf ("FLOATING POINT DECODE...\n");

  bit_rate = atoi(*++argv);
  bandwidth = atoi(*++argv);

  if ((bit_rate < 8000) || (bit_rate > 48000) ||
      ((bit_rate/800)*800 != bit_rate)) {
    printf("codec: Error. bit-rate must be multiple of 800 between 8000 and 48000\n");
    exit(1);
  }
  
 /* initializes bandwidth and sampling rate parameters */
  if (bandwidth == 7000) {
      number_of_regions = NUM_REGIONS;

      sample_rate = 16000;

      framesize = sample_rate/50;

      number_of_bits_per_frame = bit_rate/50;
  }
  else if (bandwidth == 14000) {
      number_of_regions = MAX_NUM_REGIONS;

      sample_rate = 32000;

      framesize = sample_rate/50;

      number_of_bits_per_frame = bit_rate/50;
  }
  else {
    printf("codec: Error. bandwidth must be 7000 or 14000\n");
    exit(1);
  }

  printf("decoder\n");
  printf("bandwidth = %d hz\n",bandwidth);
  printf("syntax = %d ",syntax);
  if (syntax == 0) printf(" packed bitstream\n");
  else if (syntax == 1) printf(" ITU selection test bitstream\n");
  printf("sample_rate = %d    bit_rate = %d\n",sample_rate,bit_rate);
  printf("framesize = %d samples\n",framesize);
  printf("number_of_regions = %d\n",number_of_regions);
  printf("number_of_bits_per_frame = %d bits\n",number_of_bits_per_frame);

  number_of_16bit_words_per_frame = number_of_bits_per_frame/16;

  mlt_based_coder_init();

  /* initialize the random number generator */
  randobj.seed0 = 1;
  randobj.seed1 = 1;
  randobj.seed2 = 1;
  randobj.seed3 = 1;

/* Read first frame of samples from disk. */
 
    if (syntax == 0)
      nsamp1 = fread(out_words, 2, number_of_16bit_words_per_frame, fp_bitstream);
	
	else
      nsamp1 = read_ITU_format(out_words,
			       &frame_error_flag,
			       number_of_16bit_words_per_frame,
			       fp_bitstream);
    		
	if (nsamp1 == number_of_16bit_words_per_frame) nsamp1 = framesize;
 
  while(nsamp1 == framesize) {
	 
    /* reinit the current word to point to the start of the buffer */
    bitobj.code_word_ptr = out_words;
    bitobj.current_word =  *out_words;
    bitobj.code_bit_count = 0;
    bitobj.number_of_bits_left = number_of_bits_per_frame;

    decoder(&bitobj, &randobj, number_of_regions,
						decoder_mlt_coefs,
						old_decoder_mlt_coefs,
						frame_error_flag);

	rmlt_coefs_to_samples(decoder_mlt_coefs, float_old_samples, float_out_samples, framesize);

	{
	  float ftemp0;
				 for (i=0; i<framesize; i++) 
				  {
				  ftemp0 = float_out_samples[i];
				
							if (ftemp0 >= 0.0) 
							{
							  if (ftemp0 < 32767.0)
								output[i] = (int) (ftemp0 + 0.5);
							  else
								output[i] = 32767;
							}	    
							
							else 
							{
							  if (ftemp0 > -32768.0)
								output[i] = (int) (ftemp0 - 0.5);
							  else
								output[i] = -32768;
							}

				  }
	}

	/* For ITU testing and off the 2 lsbs. */

    for (i=0; i<framesize; i++)
      output[i] &= 0xfffc;
    
    fwrite(output, 2, framesize, fpout);

	/* Read next frame of samples from disk. */
    
	if (syntax == 0)
	nsamp1 = fread(out_words, 2, number_of_16bit_words_per_frame, fp_bitstream);
      else
	nsamp1 = read_ITU_format(out_words,
				 &frame_error_flag,
				 number_of_16bit_words_per_frame,
				 fp_bitstream);
      if (nsamp1 == number_of_16bit_words_per_frame) nsamp1 = framesize;
    
  }

    fclose(fpout);
 
    fclose(fp_bitstream);

    return 0;
}


int read_ITU_format(out_words,
		    p_frame_error_flag,
		    number_of_16bit_words_per_frame,
		    fp_bitstream)
     short int out_words[MAX_BITS_PER_FRAME/16];
     int *p_frame_error_flag;
     int number_of_16bit_words_per_frame;
     FILE *fp_bitstream;
{
  int i,j;
  int nsamp;
  short int packed_word;
  int bit_count;
  int indicated_number_of_bits_in_frame;
  short int bit;
  short int in_array[MAX_BITS_PER_FRAME+2];

  nsamp = fread(in_array, 2, 2+16*number_of_16bit_words_per_frame, fp_bitstream);

  j = 0;
  bit = in_array[j++];
  if (bit != frame_start) {
    *p_frame_error_flag = 1;
	
  }
  
  else {

	*p_frame_error_flag = 0;
    
    indicated_number_of_bits_in_frame = in_array[j++];

    for (i=0; i<number_of_16bit_words_per_frame; i++) {
      packed_word = 0;
      bit_count = 15;
      while (bit_count >= 0) {
	bit = in_array[j++];
	if (bit == zero) 
	  bit = 0;
	else if (bit == one) 
	  bit = 1;
	else {
	  *p_frame_error_flag = 1;

	}
	packed_word <<= 1;
	packed_word += bit;
	bit_count--;
      }
      out_words[i] = packed_word;
    }
  }
  return((nsamp-1)/16);
}

