/*****************************************************************
******************************************************************
**
**   G.722.1 Annex B - G.722.1 Floating point implementation
**   > Software Release 2.1 (2008-06)
**
**	Filename : decoder.c
**
**   © 2000 PictureTel Coporation
**          Andover, MA, USA  
**
**	    All rights reserved.
**
******************************************************************
*****************************************************************/
/*****************************************************************
  Filename:   decoder.c    
  Original Author:
  Creation Date:  
  
  Purpose:         Contains files used to implement
                   the G.722.1 decoder
******************************************************************/

/******************************************************************
 Include files                                                           
*******************************************************************/

#include <stdio.h>
#include <math.h>
#include "defs.h"
#include "huff_defs.h"

/* decalaration of external functions and variables */
extern float mlt_quant_centroid[NUM_CATEGORIES-1][MAX_NUM_BINS];
extern float region_standard_deviation_table[REGION_POWER_TABLE_SIZE];
extern int index_to_array(int, int[], int);

extern int region_size;
extern int differential_region_power_decoder_tree[MAX_NUM_REGIONS][DIFF_REGION_POWER_LEVELS-1][2];
extern int vector_dimension[NUM_CATEGORIES];
extern int number_of_vectors[NUM_CATEGORIES];
extern int table_of_decoder_tables[NUM_CATEGORIES-1];

/* decalaration of local functions and variables */
void rate_adjust_categories(int, int [], int []);
void decode_envelope(Bit_Obj*, int, float[], int[]);
void decode_vector_quantized_mlt_indices(Bit_Obj*, Rand_Obj*, int, float [], int [], float []);


/***************************************************************************
 Procedure/Function:  decoder

 Syntax:     void decoder(  number_of_regions,
							number_of_bits_per_frame,
							bitstream,
							decoder_mlt_coefs,
							frame_error_flag)

							int number_of_regions;
							int number_of_bits_per_frame;
							short int bitstream[];
							float decoder_mlt_coefs[MAX_DCT_SIZE];
							int frame_error_flag;
      
              inputs:    int number_of_regions
                         int number_of_bits_per_frame
						 int frame_error_flag
              
              outputs:   float *decoder_mlt_coefs[],    
                         short int bitstream[]          
                  
 Description:  
				
***************************************************************************/
extern void categorize(int, int, int[], int[], int[]);

void decoder(bitobj,randobj,number_of_regions,
	     decoder_mlt_coefs,
	     old_decoder_mlt_coefs,
	     frame_error_flag)
     Bit_Obj *bitobj;
     Rand_Obj *randobj;
     int number_of_regions;
     float decoder_mlt_coefs[MAX_DCT_SIZE];
     float old_decoder_mlt_coefs[MAX_DCT_SIZE];
     int frame_error_flag;

{

  int absolute_region_power_index[MAX_NUM_REGIONS];
  int decoder_power_categories[MAX_NUM_REGIONS];
  int decoder_category_balances[MAX_NUM_RATE_CONTROL_POSSIBILITIES-1];
  int rate_control;
  int num_rate_control_bits;
  int num_rate_control_possibilities;
  float decoder_region_standard_deviation[MAX_NUM_REGIONS];
  int number_of_coefs;
  int number_of_valid_coefs;


  number_of_valid_coefs = number_of_regions * region_size;

  if (number_of_regions==NUM_REGIONS) {
      num_rate_control_bits = 4;
      num_rate_control_possibilities = 16;
      number_of_coefs = DCT_SIZE;
  }
  else {
      num_rate_control_bits = 5;
      num_rate_control_possibilities = 32;
      number_of_coefs = MAX_DCT_SIZE;
  }



  if (frame_error_flag == 0) {

    decode_envelope(bitobj,number_of_regions,
		    decoder_region_standard_deviation,
		    absolute_region_power_index);

    {
      int i;
      rate_control = 0;
      for (i=0; i<num_rate_control_bits; i++) {
          get_next_bit(bitobj);
          rate_control <<= 1;
          rate_control += bitobj->next_bit;
      }
    }
    bitobj->number_of_bits_left -= num_rate_control_bits;

    categorize(number_of_regions,
           bitobj->number_of_bits_left,
	       absolute_region_power_index,
	       decoder_power_categories,
	       decoder_category_balances);

    rate_adjust_categories(rate_control,
			   decoder_power_categories,
			   decoder_category_balances);

    decode_vector_quantized_mlt_indices(bitobj,randobj,number_of_regions,
					decoder_region_standard_deviation,
					decoder_power_categories,
					decoder_mlt_coefs);

/* Test for bit stream errors. */

    if (bitobj->number_of_bits_left > 0) {
      {
	int i;
	for (i=0; i<bitobj->number_of_bits_left; i++) {
	  get_next_bit(bitobj);
	  if (bitobj->next_bit == 0) frame_error_flag = 1;
	}	
      }
    }
    else {
      if (rate_control < num_rate_control_possibilities-1) {
	if (bitobj->number_of_bits_left < 0)
	  frame_error_flag |= 2;
      }
    }
    {
      int region;
      for (region=0; region<number_of_regions; region++) {

	if ((absolute_region_power_index[region]+ESF_ADJUSTMENT_TO_RMS_INDEX > 31) ||
	    (absolute_region_power_index[region]+ESF_ADJUSTMENT_TO_RMS_INDEX < -8))
	  frame_error_flag |= 4;
      }
    }

  }


/* If both the current and previous frames are errored,
   set the mlt coefficients to 0. If only the current frame
   is errored, then repeat the previous frame's mlt coefficients. */
  {
    int i;

    if (frame_error_flag != 0) {

      for (i = 0; i < number_of_valid_coefs; i++)
	decoder_mlt_coefs[i] = old_decoder_mlt_coefs[i];

      for (i = 0; i < number_of_valid_coefs; i++)
	old_decoder_mlt_coefs[i] = 0;

    }

    else {

		/* Store in case next frame is errored. */
      
		for (i = 0; i < number_of_valid_coefs; i++)
	old_decoder_mlt_coefs[i] = decoder_mlt_coefs[i];

    }
  }


/* Zero out the upper 1/8 of the spectrum. */
  
  {
    int i;
    for (i = number_of_valid_coefs; i < number_of_coefs; i++)
      decoder_mlt_coefs[i] = 0;
  }


}

/***************************************************************************
 Procedure/Function:  decode_envelope

 Syntax:   void decode_envelope(number_of_regions,
								decoder_region_standard_deviation,
								absolute_region_power_index)
								int number_of_regions;
								float decoder_region_standard_deviation[MAX_NUM_REGIONS];
								int absolute_region_power_index[MAX_NUM_REGIONS];
																					   
              
              inputs:   int int number_of_regions
                                                
              outputs:  float decoder_region_standard_deviation[MAX_NUM_REGIONS];
						int absolute_region_power_index[MAX_NUM_REGIONS];
						
 Description:  Recover differential_region_power_index from code bits

 Design Notes:
 				
***************************************************************************/

void decode_envelope(bitobj,number_of_regions,
		     decoder_region_standard_deviation,
		     absolute_region_power_index)
     Bit_Obj *bitobj;
     int number_of_regions;
     float decoder_region_standard_deviation[MAX_NUM_REGIONS];
     int absolute_region_power_index[MAX_NUM_REGIONS];


{
  int region;
  int i;
  int index;
  int differential_region_power_index[MAX_NUM_REGIONS];

/* Recover differential_region_power_index[] from code_bits[]. */
  
  index = 0;
  for (i=0; i<5; i++) {
    get_next_bit(bitobj);
    index <<= 1;
    index += bitobj->next_bit;
  }

/* ESF_ADJUSTMENT_TO_RMS_INDEX compensates for the current (9/30/96)
   IMLT being scaled to high by the ninth power of sqrt(2). */
  
  differential_region_power_index[0] = index-ESF_ADJUSTMENT_TO_RMS_INDEX;
  bitobj->number_of_bits_left -= 5;

  for (region=1; region<number_of_regions; region++) {
    index = 0;
    do {
        get_next_bit(bitobj);
      if (bitobj->next_bit == 0)
	index = differential_region_power_decoder_tree[region][index][0];
      else
	index = differential_region_power_decoder_tree[region][index][1];
      bitobj->number_of_bits_left--;
    } while (index > 0);
    differential_region_power_index[region] = -index;
  }

/* Reconstruct absolute_region_power_index[] from differential_region_power_index[]. */
  
  absolute_region_power_index[0] = differential_region_power_index[0];
  
  for (region=1; region<number_of_regions; region++) {
    absolute_region_power_index[region] = absolute_region_power_index[region-1] +
      differential_region_power_index[region] + DRP_DIFF_MIN;
  }

/* Reconstruct decoder_region_standard_deviation[] from absolute_region_power_index[]. */

  for (region=0; region<number_of_regions; region++) {
    i = absolute_region_power_index[region]+REGION_POWER_TABLE_NUM_NEGATIVES;
    decoder_region_standard_deviation[region] = region_standard_deviation_table[i];
  }

}

/***************************************************************************
 Procedure/Function:  rate_adjust_categories

 Syntax:			void rate_adjust_categories(rate_control,
												decoder_power_categories,
												decoder_category_balances)
												int rate_control;
												int decoder_power_categories[MAX_NUM_REGIONS];
												int decoder_category_balances[MAX_NUM_RATE_CONTROL_POSSIBILITIES-1];     
 
   
               inputs:    int rate_control,   
                          int *decoder_power_categories,
                          int *decoder_category_balances
                          
               outputs:   int rate_control,   
                          int *decoder_power_categories,
 
 Description:     Adjust the power categories based on the categorization control
				
***************************************************************************/

void rate_adjust_categories(rate_control,
			    decoder_power_categories,
			    decoder_category_balances)
     int rate_control;
     int decoder_power_categories[MAX_NUM_REGIONS];
     int decoder_category_balances[MAX_NUM_RATE_CONTROL_POSSIBILITIES-1];
{
  int i;
  int region;

  i = 0;
  while (rate_control > 0) {
    region = decoder_category_balances[i++];
    decoder_power_categories[region]++;
    rate_control--;
  }

}

/* ************************************************************************************ */
/* ************************************************************************************ */


/***************************************************************************
 Procedure/Function:   decode_vector_quantized_mlt_indices

 Syntax:  void decode_vector_quantized_mlt_indices(number_of_regions,
							decoder_region_standard_deviation,
							decoder_power_categories,
							decoder_mlt_coefs)

							int number_of_regions;
							float decoder_region_standard_deviation[MAX_NUM_REGIONS];
							int decoder_power_categories[MAX_NUM_REGIONS];
							float decoder_mlt_coefs[MAX_DCT_SIZE];

			 inputs:   int    number_of_regions
                       float  *decoder_region_standard_deviation
                       int    *decoder_power_categories
            
             outputs:  float  decoder_mlt_coefs[MAX_DCT_SIZE]
             

 Description:  
				
***************************************************************************/

void decode_vector_quantized_mlt_indices(bitobj,randobj,number_of_regions,
					 decoder_region_standard_deviation,
					 decoder_power_categories,
					 decoder_mlt_coefs)
     Bit_Obj *bitobj;
     Rand_Obj *randobj;
     int number_of_regions;
     float decoder_region_standard_deviation[MAX_NUM_REGIONS];
     int decoder_power_categories[MAX_NUM_REGIONS];
     float decoder_mlt_coefs[MAX_DCT_SIZE];

{

  float standard_deviation;
  float *decoder_mlt_ptr;
  float decoder_mlt_value;
  float temp1;
  float noifillpos;
  float noifillneg;

  static float noise_fill_factor_cat5[20] = {0.70711, 0.6179, 0.5005, 0.3220,
					       0.17678, 0.17678, 0.17678, 0.17678,
					       0.17678, 0.17678, 0.17678, 0.17678,
					       0.17678, 0.17678, 0.17678, 0.17678,
					       0.17678, 0.17678, 0.17678, 0.17678};

  static float noise_fill_factor_cat6[20] = {0.70711, 0.5686, 0.3563, 0.25,
					       0.25, 0.25, 0.25, 0.25,
					       0.25, 0.25, 0.25, 0.25,
					       0.25, 0.25, 0.25, 0.25,
					       0.25, 0.25, 0.25, 0.25};


  int region;
  int category;
  int j,n;
  int k[MAX_VECTOR_DIMENSION];
  int vec_dim;
  int num_vecs;
  int index,signs_index = 0;
  int bit = 0;
  int num_sign_bits;
  int ran_out_of_bits_flag;
  int *decoder_table_ptr;

  int random_word;
  float scale_factor;
  float noise_scale_factor;

  /*
  ** This was changed to for fixed point interop
  ** A scale factor of 22.0 is used to adjust the decoded mlt value.
  */
  if (number_of_regions == NUM_REGIONS) {
      scale_factor = 22.0f;
      noise_scale_factor = 22.0f;
  }
  else {
      /* Original float ref.decoder didn't do 14000, so this is approximate.
       * 33.0f is too quiet vs int ref.decoder, while 36.0f is slightly louder. */
      scale_factor = 35.8f;
      noise_scale_factor = 35.8f;
  }


  ran_out_of_bits_flag = 0;
  for (region=0; region<number_of_regions; region++) {
    category = decoder_power_categories[region];
    decoder_mlt_ptr = &decoder_mlt_coefs[region*region_size];
    standard_deviation = decoder_region_standard_deviation[region];
    if (category < NUM_CATEGORIES-1)
      {
	decoder_table_ptr = (int *) table_of_decoder_tables[category];
 	vec_dim = vector_dimension[category];
	num_vecs = number_of_vectors[category];

	for (n=0; n<num_vecs; n++) {
	  index = 0;
	  do {
	    if (bitobj->number_of_bits_left <= 0) {
	      ran_out_of_bits_flag = 1;

	      break;
	    }

	    get_next_bit(bitobj);
	    if (bitobj->next_bit == 0)
	      index = *(decoder_table_ptr + 2*index);
	    else
	      index = *(decoder_table_ptr + 2*index + 1);


	    bitobj->number_of_bits_left--;
	  } while (index > 0);
	  if (ran_out_of_bits_flag == 1)
	    break;
	  index = -index;
	  num_sign_bits = index_to_array(index,k,category);

	  if (bitobj->number_of_bits_left >= num_sign_bits) {
	    if (num_sign_bits != 0) {
	      signs_index = 0;
	      for (j=0; j<num_sign_bits; j++) {
	    get_next_bit(bitobj);
       		signs_index <<= 1;
		signs_index += bitobj->next_bit;
		bitobj->number_of_bits_left--;
	      }
	      bit = 1 << (num_sign_bits-1);
	    }
	    for (j=0; j<vec_dim; j++) {

	      decoder_mlt_value = standard_deviation * mlt_quant_centroid[category][k[j]]*scale_factor;

	      if (decoder_mlt_value != 0) {
		if ( (signs_index & bit) == 0)
		  decoder_mlt_value *= -1;
		bit >>= 1;
	      }

	      *decoder_mlt_ptr++ = decoder_mlt_value;
	    }
	  }
	  else {
	    ran_out_of_bits_flag = 1;

	    break;
	  }
	}

	/* If ran out of bits during decoding do noise fill for remaining regions. */

	if (ran_out_of_bits_flag == 1) {
	  for (j=region+1; j<number_of_regions; j++)
	    decoder_power_categories[j] = NUM_CATEGORIES-1;
	  category = NUM_CATEGORIES-1;
	  decoder_mlt_ptr = &decoder_mlt_coefs[region*region_size];
	}
      }


    if (category == NUM_CATEGORIES-3)
      {

	decoder_mlt_ptr = &decoder_mlt_coefs[region*region_size];
	n = 0;
	for (j=0; j<region_size; j++) {
	  if (*decoder_mlt_ptr != 0) {
	    n++;
	    if (fabs(*decoder_mlt_ptr) > 44.0F*standard_deviation) {
	      n += 3;
	    }
	  }
	  decoder_mlt_ptr++;
	}
	if(n>19)n=19;
	temp1 = noise_fill_factor_cat5[n];

	decoder_mlt_ptr = &decoder_mlt_coefs[region*region_size];

/*	noifillpos = standard_deviation * 0.17678; */
	noifillpos = standard_deviation * temp1;

	noifillneg = -noifillpos;

/* This assumes region_size = 20 */
	random_word = get_rand(randobj);
	for (j=0; j<10; j++) {
	  if (*decoder_mlt_ptr == 0) {
	    temp1 = noifillpos;
	    if ((random_word & 1) == 0) temp1 = noifillneg;
	    *decoder_mlt_ptr = temp1*noise_scale_factor;
	    random_word >>= 1;
	  }
	  decoder_mlt_ptr++;
	}
	random_word = get_rand(randobj);
	for (j=0; j<10; j++) {
	  if (*decoder_mlt_ptr == 0) {
	    temp1 = noifillpos;
	    if ((random_word & 1) == 0) temp1 = noifillneg;
	    *decoder_mlt_ptr = temp1*noise_scale_factor;
	    random_word >>= 1;
	  }
	  decoder_mlt_ptr++;
	}

      }

    if (category == NUM_CATEGORIES-2)
      {
	
	decoder_mlt_ptr = &decoder_mlt_coefs[region*region_size];
	n = 0;
	for (j=0; j<region_size; j++) {
	  if (*decoder_mlt_ptr++ != 0)
	    n++;
	}
	temp1 = noise_fill_factor_cat6[n];

	decoder_mlt_ptr = &decoder_mlt_coefs[region*region_size];

	noifillpos = standard_deviation * temp1;

	noifillneg = -noifillpos;

/* This assumes region_size = 20 */
	
	random_word = get_rand(randobj);
	for (j=0; j<10; j++) {
	  if (*decoder_mlt_ptr == 0) {
	    temp1 = noifillpos;
	    if ((random_word & 1) == 0) temp1 = noifillneg;
	    *decoder_mlt_ptr = temp1*noise_scale_factor;
	    random_word >>= 1;
	  }
	  decoder_mlt_ptr++;
	}
	random_word = get_rand(randobj);
	for (j=0; j<10; j++) {
	  if (*decoder_mlt_ptr == 0) {
	    temp1 = noifillpos;
	    if ((random_word & 1) == 0) temp1 = noifillneg;
	    *decoder_mlt_ptr = temp1*noise_scale_factor ;
	    random_word >>= 1;
	  }
	  decoder_mlt_ptr++;
	}
      }

    if (category == NUM_CATEGORIES-1)
      {

	noifillpos =  (float)( standard_deviation * 0.70711) ;

	noifillneg = -noifillpos;

/* This assumes region_size = 20 */
	
	random_word = get_rand(randobj);
	for (j=0; j<10; j++) {
	  temp1 = noifillpos;
	  if ((random_word & 1) == 0) temp1 = noifillneg;
	  *decoder_mlt_ptr++ = temp1*noise_scale_factor;
	  random_word >>= 1;
	}
	random_word = get_rand(randobj);
	for (j=0; j<10; j++) {
	  temp1 = noifillpos;
	  if ((random_word & 1) == 0) temp1 = noifillneg;
	  *decoder_mlt_ptr++ = temp1*noise_scale_factor;
	  random_word >>= 1;
	}
      }

  }

  if (ran_out_of_bits_flag)
      bitobj->number_of_bits_left = -1;

}

/***************************************************************************
 Function:  get_next_bit

***************************************************************************/

void get_next_bit(Bit_Obj *bitobj)
{
    short temp;

    if (bitobj->code_bit_count == 0)
    {
        bitobj->current_word = *bitobj->code_word_ptr++;
        bitobj->code_bit_count = 16;
    }
    bitobj->code_bit_count--;
    temp = bitobj->current_word >> bitobj->code_bit_count;
    bitobj->next_bit = (short)(temp & 1);
}


/***************************************************************************
 Function:    get_rand

***************************************************************************/

int get_rand(Rand_Obj *randobj)
{
    int random_word; /* int decoder uses short */

    random_word = (randobj->seed0 + randobj->seed3);
    if ((random_word & 32768L) != 0)
        random_word++;

    randobj->seed3 = randobj->seed2;
    randobj->seed2 = randobj->seed1;
    randobj->seed1 = randobj->seed0;
    randobj->seed0 = random_word;

    return(random_word);
}
