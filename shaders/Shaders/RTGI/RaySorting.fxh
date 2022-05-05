/*=============================================================================

    Copyright (c) Pascal Gilcher. All rights reserved.
    CC BY-NC-ND 3.0 licensed.

=============================================================================*/

#pragma once

/*===========================================================================*/

    /* How this works:

        N frames before jitter wraps (8 here)
        M pixels (16x16 here)
        K samples per pixel

        total number of possible directions rays can have in a sqrt(M) sized box: 
        
            N * M * K

        To minimize noise, 2 things are important:

            1. each sample set (samples per pixel) must have unique sample locations over the N * M * K pool

                -> don't waste samples

            2. the sample sets must be evenly scattered over the N * M * K pool
              (e.g. for 1000 total samples, one pixel using sample 0 1 2 3 is bad but 0 250 644 899 is good)

                -> lower errors between pixels in uniform areas

        Ensuring the first is easy, just use a low discrepancy sequence
        Ensuring second one is more difficult - low discrepancy sequences tend to have this property 
        as well, but since we know the total sample count, use something that we know samples completely
        uniform over the N * M * K pool is better.

        Solution used here:

        Vogel spiral (golden angle)

        x(i) = sin(i * golden phi) * i / n
        y(i) = cos(i * golden phi) * i / n

        It has such properties and is also very easy to compute as opposed to some low discrepancy sequences
        which usually have a bigger overhead to compute each single sample, and also doesn't have to deal
        with big numbers if done right -> lower precision errors

        The trick is using a vogel spiral spanning all the N * M * K samples,
        in such a manner that each subset K (samples per pixel) is again a vogel spiral,
        and also that all K * N subsets are _again_ vogel spirals, does the trick.
        So each sample pattern of a pixel is a vogel spiral of a vogel spiral of a vogel spiral.
        The vogel properties of the smallest spiral are of not much relevance, as the spp rarely reach
        numbers where this is important.

        Given a vogel spiral of n points, some subsets(k, l) filled with points where n mod k == l
        have similar properties as the original vogel spiral and are thus ideal for this application.

        Using a 16x16 grid here -> M = 256
        Accumulating data of 8 frames here -> N = 8
    */

#define RAY_SORTING_PIXEL_GRID_DIM_2    256
#define RAY_SORTING_FRAME_HISTORY_SIZE  4

int permute_frame_index(in int framecounter)
{
    //Bayer sequence is only defined for powers of 2 but this can be 
    //substituted by some hand-crafted sequences, the only purpose is to permute the     
    //frame indices to prevent a rotation being visible on some surfaces
    //int permutation_table[RAY_SORTING_FRAME_HISTORY_SIZE] = {1, 5, 3, 7, 2, 6, 4, 8};
    int permutation_table[RAY_SORTING_FRAME_HISTORY_SIZE] = {1, 3, 2, 4}; //{1, 2, 4, 3};//
    //int permutation_table[RAY_SORTING_FRAME_HISTORY_SIZE] = {1, 2};
    //int permutation_table[RAY_SORTING_FRAME_HISTORY_SIZE] = {1};

    return permutation_table[framecounter % RAY_SORTING_FRAME_HISTORY_SIZE];
}

void ray_sorting(in VSOUT i,
                   in int framecounter,
                   in float random_seed,
                   out float2x2 raygen_matrix,
                   out float2 raygen_init,
                   out float sampleset_start_norm
                   )
{
    int num_sample_sets = RAY_SORTING_PIXEL_GRID_DIM_2 * RAY_SORTING_FRAME_HISTORY_SIZE;
    
    int permuted_frame = permute_frame_index(framecounter);    
    int permuted_pixel = random_seed * (RAY_SORTING_PIXEL_GRID_DIM_2 - 1);

    //revert the order to distribute the total samples over the considered frames evenly
    int sampleset_start = permuted_pixel * RAY_SORTING_FRAME_HISTORY_SIZE + permuted_frame;

    //provide the first location of the vogel spiral subspiral
    //that will then get projected onto unit hemisphere
    static const float golden_angle = 2.39996322972865;

    sincos(golden_angle * sampleset_start, raygen_init.x, raygen_init.y);
    sampleset_start_norm = sampleset_start / float(num_sample_sets);

    //sin/cos (golden_angle * npixels * nframes)
    //precomputed for above values for higher precision
    //raygen_matrix = float2x2(-0.10280598, -0.99470142, 0.99470142, -0.10280598);      //8 frames
    raygen_matrix = float2x2(0.669773849, -0.742565142, 0.742565142, 0.669773849);      //4 frames    
    //raygen_matrix = float2x2(-0.913721469, 0.406341083, -0.406341083, -0.913721469);  //2 frames
    //raygen_matrix = float2x2(0.20767, 0.978192586, -0.978192586, 0.20767);            //1 frame
                     
}