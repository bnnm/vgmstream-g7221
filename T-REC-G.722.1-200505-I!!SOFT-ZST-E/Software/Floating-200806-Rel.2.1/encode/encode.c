/*****************************************************************
******************************************************************
**                     
**   G.722.1 Annex B - G.722.1 Floating point implementation
**   > Software Release 2.1 (2008-06)
**
**	Filename : encode.c
**
**   © 2000 PictureTel Coporation
**          Andover, MA, USA  
**
**	    All rights reserved.
**
******************************************************************
*****************************************************************/

/*************************************************************************************
  Filename:   encode.c    
  Original Author:
  Creation Date:
  Purpose:         Contains the main function for the G.722.1 encoder
*************************************************************************************/


/************************************************************************************
 Include files                                                           
*************************************************************************************/
#include <stdio.h>
#include <math.h>
#include "defs.h"

/************************************************************************************
 Local function declarations                                             
*************************************************************************************/
void write_ITU_format(short int [], int, int, FILE *);

/************************************************************************************
 Extern function declarations                                             
*************************************************************************************/
extern void mlt_based_coder_init();
extern void encoder(int, int, float [], short int []); 
extern void samples_to_rmlt_coefs(float *, float *, int);
			   
/************************************************************************************
 Procedure/Function:     G722.1 main encoder function 

 Syntax:        encode <file format> <input file> <output bitstream file> <bit rate>

                        <file format> - 0 for packed ITU format/1 for pcm
                        <input file>  - input audio file to be encoded
                        <output file> - encoded bitstream of the input file
                        <bit rate>    - 24 or 32 kbit
                        
 Description:  Main processing loop for the G.722.1 encoder

*************************************************************************************/

void main(argc, argv)
     int argc;
     char *argv[];

{
  FILE *fpin;
  FILE *fp_bitstream;

  int i;
  int nsamp1;
  int number_of_regions;
  short int input[MAX_DCT_SIZE];
  int sample_rate;
  int bit_rate;
  int number_of_bits_per_frame;
  int number_of_16bit_words_per_frame;
  short int out_words[MAX_BITS_PER_FRAME/16];
  int framesize;
  int bandwidth=7;
  int syntax;
  int frame_error_flag=0;
  float mlt_coefs[MAX_DCT_SIZE];
  float float_new_samples[MAX_DCT_SIZE];

  int frame_cnt=0;

  /* check usage */
  if (argc < 5) {
    printf("Usage: encode 0(packed)/1 input-audio-file output-bitstream-file bit-rate\n");
    exit(1);
  }

  syntax = atoi(*++argv);

  if ((syntax != 0) && (syntax != 1))
    {
      printf("syntax must be 0 for packed or 1 for ITU format\n");
      exit(1);
    }

    number_of_regions = 14;
    
    if((fpin = fopen(*++argv,"rb")) == NULL) {
      printf("codec: error opening %s.\n",*argv);
      exit(1);
    }
    if((fp_bitstream = fopen(*++argv,"wb")) == NULL) {
      printf("codec: error opening %s.\n",*argv);
      exit(1);
    }
  
  
  sample_rate = 16000;

  bit_rate = atoi(*++argv);

  if ((bit_rate < 8000) || (bit_rate > 48000) ||
      ((bit_rate/800)*800 != bit_rate)) {
    printf("codec: Error. bit-rate must be multiple of 800 between 8000 and 48000\n");
    exit(1);
  }

  framesize = sample_rate/50;
  number_of_bits_per_frame = bit_rate/50;

  printf("encoder\n");
  printf("bandwidth = %d khz\n",bandwidth);
  printf("syntax = %d ",syntax);
  if (syntax == 0) printf(" packed bitstream\n");
  else if (syntax == 1) printf(" ITU selection test bitstream\n");
  printf("sample_rate = %d    bit_rate = %d\n",sample_rate,bit_rate);
  printf("framesize = %d samples\n",framesize);
  printf("number_of_regions = %d\n",number_of_regions);
  printf("number_of_bits_per_frame = %d bits\n",number_of_bits_per_frame);

  number_of_16bit_words_per_frame = number_of_bits_per_frame/16;

  mlt_based_coder_init();

  /* Read first frame of samples from disk. */

  nsamp1=fread(input,2,framesize,fpin);


  while(nsamp1 == framesize) {

    for(i=0; i<framesize;i++)
		float_new_samples[i]= (float)input[i];


	    frame_cnt++;

        printf("Frame: %d ",frame_cnt);




	/* Convert input samples to rmlt coefs   */
    samples_to_rmlt_coefs(float_new_samples, mlt_coefs, framesize);

	/*
	**	This was added to for fixed point interop 
	*/
	for(i=0;i<framesize;i++)
		mlt_coefs[i] /= 22.0f; 

	/* Encode the mlt coefs  */
    encoder(number_of_regions,
	    number_of_bits_per_frame,
	    mlt_coefs,
	    out_words);

	/* write the output bitstream to the output file */
    if (syntax == 0)
	fwrite(out_words, 2, number_of_16bit_words_per_frame, fp_bitstream);
    else
    write_ITU_format(out_words,
		             number_of_bits_per_frame,
                     number_of_16bit_words_per_frame,
                     fp_bitstream);

	/* Read next frame of samples from disk. */
		
		nsamp1=fread(input,2,framesize,fpin);
		  }
  
	  close(fpin);
	  close(fp_bitstream);

}


/************************************************************************************
 Procedure/Function:     Write ITU format function 

 Syntax:                void write_ITU_format(Word16 *out_words,
                                              Word16 number_of_bits_per_frame,
                                              Word16 number_of_16bit_words_per_frame,
                                              FILE   *fp_bitstream)

 Description:           Writes file output in PACKED ITU format


************************************************************************************/

void write_ITU_format( short int *out_words,
                      int number_of_bits_per_frame,
                      int number_of_16bit_words_per_frame,
                      FILE   *fp_bitstream)

{
    int  frame_start = 0x6b21;
    int  one = 0x0081;
    int  zero = 0x007f;

    int  i,j;
    int  packed_word;
    int  bit_count;
    int  bit;
    short int out_array[MAX_BITS_PER_FRAME+2];

    j = 0;
    out_array[j++] = frame_start;
    out_array[j++] = number_of_bits_per_frame;

    for (i=0; i<number_of_16bit_words_per_frame; i++)
    {
        packed_word = out_words[i];
        bit_count = 15;
        while (bit_count >= 0)
        {
            bit = (int)((packed_word >> bit_count) & 1);
            bit_count--;
            if (bit == 0)
                out_array[j++] = zero;
            else
                out_array[j++] = one;
        }
    }

    fwrite(out_array, 2, number_of_bits_per_frame+2, fp_bitstream);
}

